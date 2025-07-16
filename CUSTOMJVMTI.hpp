#ifndef CUSTOM_JVMTI_HPP
#define CUSTOM_JVMTI_HPP

#include <windows.h>
#include <jni.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <iostream>

struct MethodHook {
    std::string className;
    std::string methodName;
    std::string methodDesc;
    std::function<std::vector<unsigned char>(const std::string& cls,
                                            const std::string& mName,
                                            const std::string& mDesc,
                                            const std::vector<unsigned char>& bytes)> converter;
};

class CustomJVMTI {
public:
    enum EventType { EV_CLASS_LOAD = 1, EV_CLASS_UNLOAD, EV_VM_INIT, EV_VM_DEATH };

    using TransformerCallback = std::function<std::vector<unsigned char>(const std::string&,
                                                                        const std::vector<unsigned char>&)>;
    using EventCallback = std::function<void(const std::string&)>;

    static void AddMethodHook(const std::string& cls,
                              const std::string& mName,
                              const std::string& mDesc,
                              MethodHook::converter_t cb) {
        std::lock_guard<std::mutex> lk(mutex_);
        methodHooks_.push_back({cls, mName, mDesc, cb});
    }

    static void init(JNIEnv* env, JavaVM* vm) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (initialized_) return;
        jnienv_ = env;
        jvm_ = vm;
        hookDefineClass();
        initialized_ = true;
    }

    static jint AddTransformer(TransformerCallback cb) {
        std::lock_guard<std::mutex> lk(mutex_);
        transformers_.push_back(cb);
        return JNI_OK;
    }

    static jint SetEventNotificationMode(bool enable, EventType event, EventCallback cb) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (enable) eventHandlers_[event] = cb;
        else eventHandlers_.erase(event);
        return JNI_OK;
    }

private:
    static JNIEnv* jnienv_;
    static JavaVM* jvm_;
    static bool initialized_;
    static std::vector<TransformerCallback> transformers_;
    static std::vector<MethodHook> methodHooks_;
    static std::unordered_map<EventType, EventCallback> eventHandlers_;
    static std::vector<jclass> loadedClasses_;
    static std::mutex mutex_;

    using FN_DefineClass = jclass(JNICALL*)(JNIEnv*, const char*, jobject, const jbyte*, jsize);
    static FN_DefineClass orig_DefineClass_;

    static std::vector<unsigned char> processBytecode(const std::string& clsName,
                                                      const std::vector<unsigned char>& original) {
        std::vector<unsigned char> data = original;
        for (auto& mh : methodHooks_) {
            if (mh.className == clsName) {
                try {
                    data = mh.converter(clsName, mh.methodName, mh.methodDesc, data);
                } catch (const std::exception& e) {
                    std::cerr << "[CustomJVMTI] Hook exception: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[CustomJVMTI] Hook unknown exception." << std::endl;
                }
            }
        }
        for (auto& tf : transformers_) {
            try {
                data = tf(clsName, data);
            } catch (...) {
                std::cerr << "[CustomJVMTI] Transformer exception." << std::endl;
            }
        }
        return data;
    }

    static jclass JNICALL Hooked_DefineClass(JNIEnv* env,
                                             const char* name,
                                             jobject loader,
                                             const jbyte* buf,
                                             jsize len) {
        std::string cls = name ? name : "";
        std::vector<unsigned char> bytes(buf, buf + len);

        {
            std::lock_guard<std::mutex> lk(mutex_);
            bytes = processBytecode(cls, bytes);
        }

        jclass clsObj = orig_DefineClass_(env, name, loader, bytes.data(), (jsize)bytes.size());
        if (clsObj) {
            std::lock_guard<std::mutex> lk(mutex_);
            loadedClasses_.push_back((jclass)env->NewGlobalRef(clsObj));
            auto it = eventHandlers_.find(EV_CLASS_LOAD);
            if (it != eventHandlers_.end()) it->second(cls);
        }
        return clsObj;
    }

    static void hookDefineClass() {
        HMODULE hJvm = GetModuleHandleA("jvm.dll");
        if (!hJvm) { std::cerr << "[CustomJVMTI] jvm.dll not found." << std::endl; return; }
        void* addr = GetProcAddress(hJvm, "JVM_DefineClass");
        if (!addr) { std::cerr << "[CustomJVMTI] JVM_DefineClass not found." << std::endl; return; }

        orig_DefineClass_ = reinterpret_cast<FN_DefineClass>(addr);
        DWORD old;
        BYTE patch[5] = {0xE9};
        intptr_t rel = reinterpret_cast<intptr_t>(&Hooked_DefineClass) - reinterpret_cast<intptr_t>(addr) - 5;
        memcpy(patch + 1, &rel, 4);
        if (VirtualProtect(addr, 5, PAGE_EXECUTE_READWRITE, &old)) {
            memcpy(addr, patch, 5);
            VirtualProtect(addr, 5, old, &old);
            std::cerr << "[CustomJVMTI] Hook installed." << std::endl;
        } else {
            std::cerr << "[CustomJVMTI] Failed to install hook." << std::endl;
        }
    }
};

JNIEnv* CustomJVMTI::jnienv_ = nullptr;
JavaVM* CustomJVMTI::jvm_ = nullptr;
bool CustomJVMTI::initialized_ = false;
std::vector<CustomJVMTI::TransformerCallback> CustomJVMTI::transformers_;
std::vector<MethodHook> CustomJVMTI::methodHooks_;
std::unordered_map<CustomJVMTI::EventType, CustomJVMTI::EventCallback> CustomJVMTI::eventHandlers_;
std::vector<jclass> CustomJVMTI::loadedClasses_;
std::mutex CustomJVMTI::mutex_;
CustomJVMTI::FN_DefineClass CustomJVMTI::orig_DefineClass_ = nullptr;

#endif // CUSTOM_JVMTI_HPP
