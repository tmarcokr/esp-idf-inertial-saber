# Implementation Plan: System Module Refactoring

## 1. Objective
Refactor the `main/system` module to correctly align the physical directory structure with the logical architecture described in `project_analysis.md`. Specifically, eliminate the redundant `config` folder and ensure `adapters` and `hardware` remain strictly separated.

## 2. Rationale
- The `config` directory only contains a single file (`HardwareConfig.hpp`) and adds unnecessary nesting.
- `HardwareConfig.hpp` holds hardware-level constants (pins, core assignments), which logically belong inside the `hardware/` layer.
- The `adapters/` folder (containing `ImuAdapter` and `InputAdapter`) must remain separate from `hardware/` because it belongs to the Adapter Layer (translating physical signals into domain metrics), whereas the `hardware/` folder belongs to the Hardware Layer (dumb peripheral initialization).

## 3. Execution Steps for the Developer AI

### Step 1: Relocate HardwareConfig
- Move the file `main/system/config/HardwareConfig.hpp` to `main/system/hardware/HardwareConfig.hpp`.
- Delete the now-empty `main/system/config/` directory.

### Step 2: Update Namespace
- In the newly moved `main/system/hardware/HardwareConfig.hpp`, change the namespace from `InertialSaber::System::Config` to `InertialSaber::System::Hardware`.

### Step 3: Update Includes and References
- Search the entire `main/` directory for `#include "system/config/HardwareConfig.hpp"` and replace it with `#include "system/hardware/HardwareConfig.hpp"`.
- Search for usages of `Config::HardwareConfig` (or `System::Config::HardwareConfig`) and replace them with `Hardware::HardwareConfig` (or `System::Hardware::HardwareConfig`).
- Files expected to require updates include (but are not limited to):
  - `main/system/SaberSystem.cpp`
  - `main/system/hardware/*.cpp`
  - `main/system/adapters/*.cpp` and `.hpp`
  - `main/core/bus/SaberActionBus.cpp` and `.hpp`
  - `main/core/models/SaberDataPacket.hpp`
  - `main/profiles/ConfigurableProfile.cpp`
  - `main/profiles/inertial/effects/*.cpp`

### Step 4: Verification
- Build the project to guarantee that all references and includes were successfully caught and updated.
- **CRITICAL**: Do not run `idf.py build` directly if the environment is not sourced. Look at `.vscode/tasks.json` (specifically the "Build ESP-IDF" task) for the exact compilation command. The correct command is: `source ~/esp/esp-idf/export.sh && idf.py build`.
