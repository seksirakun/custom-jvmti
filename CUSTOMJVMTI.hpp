#ifndef CUSTOM_JVMTI_HPP
#define CUSTOM_CUSTOMJVMTI_HPP

#include <windows.h>
#include <jni.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>

// seftali for sonoyuncu by veysel & rakun

class CustomJVMTI {
public:
    static void init(JNIEnv* env, JavaVM* vm) {
        std::lock_guard<std::mutex> lk(mutex_);
        jnienv = env;
        jvm = vm;
        capabilities.clear();
        eventHandlers.clear();
        transformers.clear();
        loadedClasses.clear();
        hookDefineClass();
    }

    // capabilities ve version dölü
    static jint GetVersion() {
        return jnienv->GetVersion();
    }
    static jint AddCapabilities(const std::vector<jlong>& caps) {
        std::lock_guard<std::mutex> lk(mutex_);
        capabilities.insert(capabilities.end(), caps.begin(), caps.end());
        return JNI_OK;
    }
    static jint GetCapabilities(std::vector<jlong>& out) {
        std::lock_guard<std::mutex> lk(mutex_);
        out = capabilities;
        return JNI_OK;
    }

    // transformer callback
    using TransformerCallback = std::function<std::vector<unsigned char>(const std::string&, const std::vector<unsigned char>&)>;
    static jint AddTransformer(TransformerCallback cb) {
        std::lock_guard<std::mutex> lk(mutex_);
        transformers.push_back(cb);
        return JNI_OK;
    }

    // API
    static jint GetLoadedClasses(std::vector<jclass>& classes) {
        std::lock_guard<std::mutex> lk(mutex_);
        classes = loadedClasses;
        return JNI_OK;
    }

    // Eventleri tetikle
    enum EventType { EV_CLASS_LOAD = 1, EV_CLASS_UNLOAD, EV_VM_INIT, EV_VM_DEATH };
    using EventCallback = std::function<void(const std::string&)>;
    static jint SetEventNotificationMode(bool enable, EventType event, EventCallback cb) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (enable) eventHandlers[event] = cb;
        else eventHandlers.erase(event);
        return JNI_OK;
    }

private:
    static JNIEnv* jnienv;
    static JavaVM* jvm;
    static std::vector<jlong> capabilities;
    static std::unordered_map<EventType, EventCallback> eventHandlers;
    static std::vector<TransformerCallback> transformers;
    static std::vector<jclass> loadedClasses;
    static std::mutex mutex_;

    typedef jclass (JNICALL *FN_DefineClass)(JNIEnv*, const char*, jobject, const jbyte*, jsize);
    static FN_DefineClass orig_DefineClass;

    static jclass JNICALL Hooked_DefineClass(JNIEnv* env,
                                             const char* name,
                                             jobject loader,
                                             const jbyte* buf,
                                             jsize len) {
        std::vector<unsigned char> bytes(buf, buf + len);
        // transformers
        {
            std::lock_guard<std::mutex> lk(mutex_);
            for (auto &t : transformers) {
                bytes = t(name ? name : "", bytes);
            }
        }
        // Call original
        jclass cls = orig_DefineClass(env, name, loader, bytes.data(), (jsize)bytes.size());
        if (cls) {
            std::lock_guard<std::mutex> lk(mutex_);
            loadedClasses.push_back((jclass)env->NewGlobalRef(cls));
            // CLASS_LOAD 
            auto it = eventHandlers.find(EV_CLASS_LOAD);
            if (it != eventHandlers.end()) it->second(name ? name : "");
        }
        return cls;
    }

    // JNI_DefineClass
    static void hookDefineClass() {
        HMODULE hJVM = GetModuleHandleA("jvm.dll");
        if (!hJVM) return;
        void* addr = (void*)GetProcAddress(hJVM, "JNI_DefineClass");
        if (!addr) return;
        DWORD old;
        BYTE patch[5] = {0xE9};
        FN_DefineClass target = (FN_DefineClass)addr;
        orig_DefineClass = target;
        DWORD_PTR rel = (BYTE*)&Hooked_DefineClass - (BYTE*)addr - 5;
        memcpy(patch + 1, &rel, 4);
        VirtualProtect(addr, 5, PAGE_EXECUTE_READWRITE, &old);
        memcpy(addr, patch, 5);
        VirtualProtect(addr, 5, old, &old);
    }
};

// tanimlamalar
JNIEnv* CustomJVMTI::jnienv = nullptr;
JavaVM* CustomJVMTI::jvm = nullptr;
std::vector<jlong> CustomJVMTI::capabilities;
std::unordered_map<CustomJVMTI::EventType, CustomJVMTI::EventCallback> CustomJVMTI::eventHandlers;
std::vector<CustomJVMTI::TransformerCallback> CustomJVMTI::transformers;
std::vector<jclass> CustomJVMTI::loadedClasses;
std::mutex CustomJVMTI::mutex_;
CustomJVMTI::FN_DefineClass CustomJVMTI::orig_DefineClass = nullptr;

#endif // CUSTOM_CUSTOMJVMTI_HPP

//aptal rakun