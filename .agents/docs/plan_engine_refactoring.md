# Implementation Plan: Engine and Configuration Refactoring

## 1. Objective
1. Move the core `inertial_engine` (Audio and Visual continuous effects) into the `profiles/inertial` folder.
2. Decouple `InertialDefinition` from the Core layer by extracting a purely generic `PhysicsConfig` for the bus, and moving the specific `InertialDefinition` to `profiles/inertial/models`.
This ensures better cohesion and keeps the `core` folder strictly for abstract contracts.

## 2. Rationale
- `InertialSwingEffect` and `InertialLightEffect` are conceptually part of the "Inertial" identity.
- `InertialDefinition` contains profile-specific variables (colors, wav file counts) but is currently imported by `SaberActionBus`, violating the Core/Profile separation boundary.
- By extracting the 6 base kinetic parameters into a `PhysicsConfig`, `SaberActionBus` remains pure. `InertialDefinition` can then inherit `PhysicsConfig` and safely live in the `profiles/inertial/` folder without creating circular dependencies.

## 3. Execution Steps for the Developer AI

### Step 1: Engine Relocation
- Move the entire folder `main/core/effects/inertial_engine` to `main/profiles/inertial/engine`.
- If the folder `main/core/effects` is left completely empty, delete it.

### Step 2: Decouple SaberActionBus (PhysicsConfig)
- Create a new file `main/core/models/PhysicsConfig.hpp` (or `main/core/bus/PhysicsConfig.hpp`).
- Define `struct PhysicsConfig` containing only the following 6 fields:
  - `float kineticEnergyDeadbandG;`
  - `float rotationDeadbandDps;`
  - `float overloadThresholdG;`
  - `float overloadChargeRate;`
  - `float overloadDrainRate;`
  - `float burstCooldownMs;`
- Update `SaberActionBus::setPhysicsConfig` to accept `const Core::PhysicsConfig& def` instead of `InertialDefinition`.
- Update `#include` in `SaberActionBus.hpp` and `SaberActionBus.cpp` to point to `PhysicsConfig.hpp`.

### Step 3: Relocate InertialDefinition
- Modify `main/core/models/InertialDefinition.hpp`: Make `struct InertialDefinition` inherit from `Core::PhysicsConfig`. Remove the 6 fields from `InertialDefinition` since they are now in the base struct. Ensure it `#include`s `PhysicsConfig.hpp`.
- Move `main/core/models/InertialDefinition.hpp` to `main/profiles/inertial/models/InertialDefinition.hpp`.
- (Optional) Update the namespace to reflect its new location (e.g., `InertialSaber::Profiles::Inertial::Models`).

### Step 4: Update CMakeLists.txt
- Open `main/CMakeLists.txt`.
- Update the source paths:
  - Replace `core/effects/inertial_engine/audio/...` with `profiles/inertial/engine/audio/...`
  - Replace `core/effects/inertial_engine/visual/...` with `profiles/inertial/engine/visual/...`
- Update the `INCLUDE_DIRS`:
  - Remove `"core/effects"` (if it's now empty).
  - Replace `"core/effects/inertial_engine/audio"` with `"profiles/inertial/engine/audio"`.
  - Replace `"core/effects/inertial_engine/visual"` with `"profiles/inertial/engine/visual"`.
  - Add `"profiles/inertial/models"` and `"profiles/inertial/engine"` if they are not covered.

### Step 5: Update Includes Globally
- Perform a project-wide search in `main/` for `#include "core/effects/inertial_engine/...` and replace it with `#include "profiles/inertial/engine/...`.
- Perform a project-wide search in `main/` for `#include "core/models/InertialDefinition.hpp"` and replace it with `#include "profiles/inertial/models/InertialDefinition.hpp"`.
- Ensure all profile loaders, configs, and effects that used `InertialDefinition` now include the correct new path.

### Step 6: Verification
- Build the project to guarantee that all references, includes, and inheritances were successfully caught and updated.
- **CRITICAL**: Do not run `idf.py build` directly if the environment is not sourced. Look at `.vscode/tasks.json` (specifically the "Build ESP-IDF" task) for the exact compilation command. The correct command is: `source ~/esp/esp-idf/export.sh && idf.py build`.
