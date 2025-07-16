# CustomJVMTI

CustomJVMTI is a lightweight and modular library for dynamically modifying the bytecode of Java classes by binding the JVM's `DefineClass` function via the C++ and JNI interface, without needing the standard JVMTI API.

---
## Features

* **Method-Level Binding**: You can modify the bytecode of any class and method by binding it with C++ callback functions.
* **Converter Support**: First-level method binding, second-level additional manipulation with generic converter callbacks.
* **Event Notifications**: Capture class load, unload, and virtual machine startup/death events.
* **Thread Safe**: All critical sections are protected with `std::mutex`.
* **Zero Library Plus**: Uses only the Windows API and JNI.

---

## Getting Started

### 1. Adding to the Project

1. Copy the `CustomJVMTI.hpp` file to your project's include directory.
2. Ensure that the JNI and Windows SDK settings are configured in your project.
3. The JVM DLL (jvm.dll) path must be in the linker settings.

### 2. Agent/Load Code (Example)

```cpp
#include "CustomJVMTI.hpp"

// JNI Agent_OnLoad
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
JNIEnv* env;
if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
return JNI_ERR;
}

// Set up a hook for bytecode operations
CustomJVMTI::AddMethodHook(
"net/minecraft/client/Minecraft",
"runGameLoop",
"()V",
[](auto& cls, auto& mName, auto& mDesc, auto& data) {
// Example: examine the first 50 bytes
std::cout << "Hooked: " << cls << "#" << mName << mDesc << std::endl;
return data;
}
);

// Enable the JVMTI-like DefineClass hook
CustomJVMTI::init(env, vm);
return JNI_VERSION_1_8;
}
```

---
## API Usage

| Function | Description |
| ---------------------------- | ---------------------------------------------------------------------------- |
| `init(JNIEnv*, JavaVM*)` | Initializes hook operations. Must be called once. |
| `AddMethodHook(cls, mName, mDesc, cb)` | Adds a C++ callback to the specified class method. |
| `AddTransformer(cb)` | Adds a generic transformer callback to all defined classes. |
| `SetEventNotificationMode(...);` | Listens for events such as class loading, etc. |

---

## Things to Note

* Because the JVM patches the `DefineClass` code path, hook operations may fail depending on the version and environment.
* Security and stability testing is required in production environments.
* Bytecode incompatibility in hooked classes may cause a JVM crash.

---

## Contributors

* Veysel & Rakun & Batuzane & Axirus

---

## License

MIT © seksirakun48
