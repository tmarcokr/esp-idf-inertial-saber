---
name: create-effect
description: Workflow for adding a new discrete kinetic effect (InertialEffect + optional SmartLed overlay + audio category) to the inertial profile. Use when implementing a Kinetic Effect from `docs/wiki/KineticEffects.md` (e.g. Lockup, Stab) or any new bus-triggered effect, or when the user invokes /create-effect.
argument-hint: "[effect name]"
---

# Workflow: Create a Kinetic Effect

Requested effect (if provided): `$ARGUMENTS`

This workflow is orchestrated from the main session (see `.claude/rules/02-orchestrator.md` §3.1). It captures the pattern used for `BlasterEffect`, `KineticImpactEffect` and `DragEffect`.

## 1. Specification Gate
- The effect MUST be specified in `docs/wiki/KineticEffects.md` (trigger, thresholds with units, priority, audio category, visual behaviour, interaction with other effects). If it is not, the spec is written first (§3.1 step 2).

## 2. Plan (`firmware-architect`)
The plan must cover every touch point of the pattern:

| # | Touch point | Reference implementation |
|:--|:------------|:-------------------------|
| 1 | Effect class `main/profiles/inertial/effects/<Name>Effect.{hpp,cpp}` deriving `Core::InertialEffect` (`Test()` / `Run()`, priority) | `BlasterEffect`, `DragEffect` |
| 2 | Active-state gating through `PowerToggleEffect&` (effects only fire when the blade is ON) | `BlasterEffect` |
| 3 | Trigger source: input state (`InputDescriptor` in `SaberDataPacket`) or kinetic metric (`KineticEnergy`, `AxisRotation`, `OrientationVector`, `InertialOverload`) | `DragEffect` (hold), `KineticImpactEffect` (G drop) |
| 4 | Visual overlay `main/profiles/inertial/effects/overlays/Blade<Name>.{hpp,cpp}` implementing the SmartLed overlay contract, self-removing via `isFinished()` | `BladeBlasterBlock`, `BladeDragEffect` |
| 5 | Audio: random file from the font sub-directory via `AudioEngine::play()`; looping vs one-shot | `BlasterEffect::buildPath`, `DragEffect` |
| 6 | Profile parameters in `InertialDefinition` (`font<Name>Count`, LED counts, thresholds) + parsing/defaults in `ProfileParser.cpp` + its self-test JSON | `fontDragCount`, `dragLedCount` |
| 7 | Registration in `ConfigurableProfile.cpp` (`bus.registerEffect(std::make_unique<...>)`); priority is declared by the effect (the bus re-sorts on registration, order among equal priorities is not guaranteed) | existing registrations |
| 8 | Sources added to `main/CMakeLists.txt` `SRCS` | existing entries |
| 9 | Physics thresholds that are not per-profile go to `main/core/PhysicsConfig.hpp` | — |

If the effect needs a new audio category, note that `.claude/skills/profile-create/scripts/create_profile.py` naming patterns must be extended (separate change, user-invoked tooling).

## 3. Implement (`firmware-developer`)
- Implement the plan, register the effect and build.
- `components/**` is read-only; use the SmartLed and Audio public APIs only.

## 4. Verify
- `code-auditor` on `git diff main...HEAD`. Fix loop through `firmware-developer` (max 3 rounds).
- Hardware test notes for the user: how to trigger the effect, expected audio/visual result, log lines to look for.

## 5. Document (`docs-maintainer`)
- `project_analysis.md`: status table, maturity matrix (Kinetic Effects %), Phase 4 task row, change log line.
- `docs/wiki/sdkconfig_overrides.md` if any `sdkconfig` option changed.

## 6. Close
- `/git-commit`, then stop for user review before `/git-submit-pr`.
