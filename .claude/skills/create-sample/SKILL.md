---
name: create-sample
description: Create a new modular feature (ISample) if the project uses the Sample architecture. Use when adding a new feature/sample to a project built on the `ISample` modular architecture, or when the user invokes /create-sample.
argument-hint: "[sample name]"
disable-model-invocation: true
---

# 🚀 Workflow: Creating a New Sample (Optional Architecture)

> **Not applicable to InertialSaber OS**: this project does not use the `ISample` architecture, and `components/**` is immutable. Kept for template compatibility; user-invocable only. For new effects use `/create-effect`.

Use this workflow when the current project utilizes the `ISample` modular architecture and you need to add a new feature.

Requested sample (if provided): `$ARGUMENTS`

1. **Component Definition (If needed)**:
   - Create a folder in `components/` if a new hardware wrapper is required.
   - Implement the wrapper in C++ within an appropriate namespace (e.g., `Espressif::Wrappers`).

2. **Sample Implementation**:
   - Create the `.hpp` and `.cpp` files in `main/samples/<new_sample>/`.
   - Inherit from the project's interface (e.g., `Espressif::App::ISample`).
   - Implement `setup()` (hardware initialization) and `run()` (loop/task logic).

3. **Main Integration**:
   - Include the header in `main/main.cpp`.
   - Instantiate the object (use `static` or dynamic allocation as per project rules).
   - Call `setup()`.
   - Launch the FreeRTOS task with `xTaskCreate`, passing the instance.

4. **Verification**:
   - Run `idf.py build` to ensure compilation succeeds.
