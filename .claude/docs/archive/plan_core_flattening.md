# Implementation Plan: Core Flattening

## 1. Objective
Eliminate the subdirectories within `main/core/` (`bus`, `interfaces`, `models`) and flatten the structure. With the removal of the engines and the specific profile definitions handled in previous plans, the remaining files form a single cohesive, tightly-coupled unit: the `SaberActionBus` and its immediate contracts.

## 2. Rationale
After executing `plan_engine_refactoring.md`, the `main/core/` folder will only contain roughly 5 files:
- `SaberActionBus.cpp`
- `SaberActionBus.hpp`
- `InertialEffect.hpp`
- `SaberDataPacket.hpp`
- `PhysicsConfig.hpp` (created in the previous plan)

Maintaining `bus/`, `interfaces/`, and `models/` for just 5 files is over-engineered. Flattening them into the root of `main/core/` simplifies navigation and reflects that they are all part of the same "Bus API" layer.

## 3. Execution Steps for the Developer AI

### Step 1: Flatten Directories
- Move `main/core/bus/SaberActionBus.cpp` and `.hpp` to `main/core/`.
- Move `main/core/interfaces/InertialEffect.hpp` to `main/core/`.
- Move `main/core/models/SaberDataPacket.hpp` to `main/core/`.
- (If `PhysicsConfig.hpp` was created in `main/core/models/` or `main/core/bus/` in the previous step, ensure it is moved to `main/core/`).
- Delete the now-empty directories: `main/core/bus/`, `main/core/interfaces/`, and `main/core/models/`.

### Step 2: Update CMakeLists.txt
- Open `main/CMakeLists.txt`.
- Update the `SRCS` list:
  - Replace `"core/bus/SaberActionBus.cpp"` with `"core/SaberActionBus.cpp"`.
- The `INCLUDE_DIRS` should already contain `"core"`. You can remove any references to `"core/bus"`, `"core/interfaces"`, or `"core/models"` if they exist.

### Step 3: Update Includes Globally
- Perform a project-wide search to update the includes for the flattened files:
  - Replace `#include "core/bus/SaberActionBus.hpp"` with `#include "core/SaberActionBus.hpp"`.
  - Replace `#include "core/interfaces/InertialEffect.hpp"` with `#include "core/InertialEffect.hpp"`.
  - Replace `#include "core/models/SaberDataPacket.hpp"` with `#include "core/SaberDataPacket.hpp"`.
- **Note on Relative Includes**: Inside `main/core/`, these files used relative paths (e.g., `#include "../models/SaberDataPacket.hpp"`). Ensure these are updated to flat relative paths (e.g., `#include "SaberDataPacket.hpp"`) or absolute paths (`#include "core/SaberDataPacket.hpp"`).

### Step 4: Verification
- Build the project to guarantee that all references and includes were successfully caught and updated.
- **CRITICAL**: Do not run `idf.py build` directly if the environment is not sourced. Look at `.vscode/tasks.json` (specifically the "Build ESP-IDF" task) for the exact compilation command. The correct command is: `source ~/esp/esp-idf/export.sh && idf.py build`.
