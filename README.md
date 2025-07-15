# CustomJVMTI

`CustomJVMTI` is a lightweight emulation of the JVM Tool Interface (JVMTI) functionality without relying on the official JVMTI library. Instead, it hooks into the JVM using Windows API inline hooks to capture class definitions and enable:

* **Class conversion via registered bytecode converters**
* **Event notifications for class loading and the virtual machine lifecycle**
* **Capabilities management** (adding/getting capabilities)
* **Getting loaded classes without using the ClassLoader API**

---

## Features

1. **Initialization**

* `CustomJVMTI::init(JNIEnv* env, JavaVM* vm)` – captures the JNI environment and the `JavaVM` pointer, clears the internal state, and loads the hook onto `JNI_DefineClass`.

2. **Capabilities**

* `AddCapabilities(const std::vector<jlong>&)`
* `GetCapabilities(std::vector<jlong>&)`

3. **Class File Transformers**

* `AddTransformer(TransformerCallback)` – Registers callbacks to modify raw class bytes before definition.

4. **Class Loading Capture**

* Uses `JNI_DefineClass` inline hooks to capture all class definitions.

* Implements transformers, registers global references to loaded classes, and triggers `EV_CLASS_LOAD` events.

5. **Loaded Classes**

* `GetLoadedClasses(std::vector<jclass>&)` – Returns a snapshot of all classes loaded via the hook.

6. **Event Notifications**

* `SetEventNotificationMode(bool enable, EventType, EventCallback)` – subscribe/unsubscribe:

* `EV_CLASS_LOAD`
* `EV_CLASS_UNLOAD` (reserved)
* `EV_VM_INIT` (reserved)
* `EV_VM_DEATH` (reserved)

---

## Getting Started

### Prerequisites

* Windows 7 or later
* Visual Studio 2017+ or compatible C++ compiler
* `jvm.dll` accessible from the Java JDK (OpenJDK/Oracle)

### Compiling

1. Create a new C++ DLL project.
2. Add `CUSTOM_CUSTOMJVMTI.hpp` to your project. 3. Ensure that your project is linked against the JVM import library (for example, `jvm.lib`).

4. Compile as a 64-bit DLL (appropriate for your JVM architecture).

### Usage

1. Place the generated DLL alongside your Java application.

2. The JVM will load your library (for example, via `-agentpath:your.dll`) or by placing it in the `java.library.path` directory.
3. While the library is loading, call `CustomJVMTI::init(env, vm)` manually (for example, inside the `JNI_OnLoad` directory) or use the link. 4. Register the transformers:

```cpp
CustomJVMTI::AddTransformer([](const std::string &name, const std::vector<unsigned char> &bytes) {
// replace bytes...
return bytes;
});
```
5. Subscribe to events:

```cpp
CustomJVMTI::SetEventNotificationMode(true, CustomJVMTI::EV_CLASS_LOAD,
[](const std::string &clsName) {
printf("Loaded class: %s\n", clsName.c_str());
}
);
```
6. Then, retrieve the loaded classes:

```cpp
std::vector<jclass> list;
CustomJVMTI::GetLoadedClasses(list);
// Use jclass references
```

---

## Notes and Limitations

* Supports Windows only; Linux/macOS require different binding mechanisms.
* `EV_CLASS_UNLOAD`, `EV_VM_INIT`, and `EV_VM_DEATH` are reserved but not fully implemented.
* Thread suspend/resume and full JVMTI stack trace inspection are not provided.
* Changes depend on inlining patching of `JNI_DefineClass`. Make sure your JVM exports this symbol.

---

## License

MIT License — feel free to adapt it to your own needs.

Used by veysel,batuzane,rakun,beyaz,axirus,serhat
