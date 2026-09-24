# Project Context & Bootstrapper

## ⚠️ MANDATORY INITIALIZATION
The rule files below are loaded automatically by Claude Code at session start (every file in `.claude/rules/` without `paths:` frontmatter is always-on):
- `.claude/rules/01-project-instructions.md` (Global)
- `.claude/rules/00-private-rules.md` (Local/Private — present only in the maintainer's checkout)

**Stop.** Before performing any analysis or action, you **MUST** also have loaded:
- `docs/development_roadmap/project_analysis.md` (Project Analysis — imported below)

@docs/development_roadmap/project_analysis.md

This is a foundational requirement of the ecosystem template. These files contain your mandatory identity, communication protocols, and the logic for autonomous skill activation. Failure to load these rules will result in an incorrect execution state.

---

## 🏗️ Project-Specific Overrides
InertialSaber (esp-idf-inertial-saber) is a high-performance, open-source operating system for lightsabers based on the ESP-IDF framework. It is designed for ultra-low latency motion processing, high-fidelity audio mixing, and advanced visual effects on the ESP32 family of microcontrollers.

## 🛑 Critical Development Rules

Before contributing or modifying this project, the following rules **MUST** be strictly observed:

1. **Review Before Push:** Before pushing any changes to GitHub, it is mandatory to ask the user to review the changes.
2. **SDKConfig Tracking:** If any line of the `sdkconfig` is modified, it is mandatory to document the change in [`docs/wiki/sdkconfig_overrides.md`](./docs/wiki/sdkconfig_overrides.md).
3. **Components Immutability:** It is **strictly forbidden** to change any line of code within the `components/**` directory.

---

## 🧭 Claude Code Tooling Index
Pointers only — the detailed routing lives in `.claude/rules/01-project-instructions.md`.

- **Expertise skills** (`.claude/skills/`, auto-activated by description or invoked with `/name`): `/esp32-expert`, `/hardware-specialist`, `/quality-auditor`, `/cpp-components-expert`.
- **Procedure skills** (invoked with `/name`; `sync-*`, `fresh-start` and `profile-create` are user-only; `git-*` run automatically as part of every task): `/fresh-start`, `/git-commit`, `/git-create-branch`, `/git-rebase-main`, `/git-squash-commits`, `/git-submit-pr`, `/profile-create`, `/create-sample`, `/hardware-validation`, `/verify-implementation`, `/sync-components`, `/sync-template`.
- **Subagents** (`.claude/agents/`, delegated via the Agent tool): `code-auditor`, `hardware-reviewer`.
- **Reference docs**: datasheets and refactoring plans in `.claude/docs/`.
