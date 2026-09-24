# Project Context & Bootstrapper

## ⚠️ MANDATORY INITIALIZATION
The rule files below are loaded automatically by Claude Code at session start (every file in `.claude/rules/` without `paths:` frontmatter is always-on):
- `.claude/rules/01-project-instructions.md` (Global)
- `.claude/rules/02-orchestrator.md` (Orchestrator protocol: agent roster, workflows, stop points)
- `.claude/rules/00-private-rules.md` (Local/Private — present only in the maintainer's checkout)

**Stop.** Before performing any analysis or action, you **MUST** also have loaded:
- `docs/development_roadmap/project_analysis.md` (Project Analysis — imported below)

@docs/development_roadmap/project_analysis.md

This is a foundational requirement of the ecosystem template. These files contain your mandatory identity, communication protocols, and the logic for autonomous skill activation. Failure to load these rules will result in an incorrect execution state.

---

## 🏗️ Project-Specific Overrides
InertialSaber (esp-idf-inertial-saber) is a high-performance, open-source operating system for lightsabers based on the ESP-IDF framework. It is designed for ultra-low latency motion processing, high-fidelity audio mixing, and advanced visual effects on the ESP32-S3.

**Operating model:** the main session is an **orchestrator**. It plans with the user, delegates implementation, audits, hardware review and documentation to subagents, and works autonomously up to local commits on a `feature/` branch. See `.claude/rules/02-orchestrator.md`.

## 🛑 Critical Development Rules

Before contributing or modifying this project, the following rules **MUST** be strictly observed:

1. **Review Before Push:** Before pushing any changes to GitHub, it is mandatory to ask the user to review the changes.
2. **SDKConfig Tracking:** If any line of the `sdkconfig` is modified, it is mandatory to document the change in [`docs/wiki/sdkconfig_overrides.md`](./docs/wiki/sdkconfig_overrides.md).
3. **Components Immutability:** It is **strictly forbidden** to change any line of code within the `components/**` directory.

---

## 🧭 Claude Code Tooling Index
Pointers only — the detailed routing lives in `.claude/rules/02-orchestrator.md`.

- **Expertise skills** (`.claude/skills/`, auto-activated by description or invoked with `/name`): `/esp32-expert`, `/hardware-specialist`, `/quality-auditor`, `/cpp-components-expert`.
- **Procedure skills** (invoked with `/name`; `sync-*`, `fresh-start` and `profile-create` are user-only; `git-*` run automatically as part of every task): `/create-effect`, `/fresh-start`, `/git-commit`, `/git-create-branch`, `/git-rebase-main`, `/git-squash-commits`, `/git-submit-pr`, `/profile-create`, `/hardware-validation`, `/verify-implementation`, `/sync-components`.
- **Subagents** (`.claude/agents/`, delegated via the Agent tool): `firmware-architect`, `firmware-developer`, `code-auditor`, `hardware-reviewer`, `docs-maintainer`.
- **Guardrails** (`.claude/settings.json` + `.claude/hooks/`, require `python3` and `jq`): state-based Bash guard (asks for any git command on `main`, every push, outward-facing `gh` actions and destructive git; denies persisting git commands while `components/` is modified and commits/pushes that carry private paths or `components/` changes), Edit/Write deny on `components/**`, and a `sdkconfig` documentation reminder. Exact list in `.claude/rules/02-orchestrator.md` §5.
- **Reference docs**: datasheets in `.claude/docs/`, active plans in `.claude/docs/plans/`, executed plans in `.claude/docs/archive/`.
