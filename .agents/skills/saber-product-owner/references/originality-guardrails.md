# Originality Guardrails: InertialSaber vs. Legacy Systems

## 1. The Core Mandate
InertialSaber OS is a **native-physics** operating system. While we respect legacy standards like ProffieOS for their market impact, this project **MUST NOT** copy their terminology, logic structures, or documentation style.

### Forbidden Terminology & Concepts
| Legacy Term (ProffieOS) | InertialSaber Equivalent (Original) | Why? |
| :--- | :--- | :--- |
| SmoothSwing V2 | **InertialSwing Engine** | We use linear kinetic energy, not just angular rotation. |
| Styles / BladeStyles | **Light Engines / Visual States** | Ours are procedural and reactive, not static layers. |
| Presets | **Profiles** | Focus on the configuration of the physics engine. |
| Clash / Stab / Swing | **Impact / Thrust / Kinetic Event** | Use physical action descriptors. |
| Accent LEDs | **Auxiliary Lumina** | |

## 2. Structural Originality
- **No "Styles":** We do not use "Styles" as code blobs. We use **Visual Specs** that describe how the `InertialLight Engine` reacts to `EnergiaTotal` and `TanqueOverload`.
- **Physics-First:** Documentation must always start with the physical trigger (G-Force, Inertia, Angle) before describing the output (Sound, Light).
- **Terminology Enforcement:** Any documentation generated must use "Inertial", "Kinetic", "Thermal", and "Overload" as its primary lexicon.

## 3. Documentation Style
- **Technical/Functional:** Documentation should be "Functional Specs" (what it does) with "Technical Details" (how the math works).
- **No Manual-like Tone:** Avoid "To do X, press Y". Instead, use "When state X is reached via metric Y, the system triggers Z".
