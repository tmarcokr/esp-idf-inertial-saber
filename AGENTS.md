# InertialSaber — Codex project instructions

## Initialization

Before working, read these independent Codex instructions in order:
1. `.codex/rules/01-project-instructions.md`.
2. `.codex/rules/02-orchestrator.md`.
3. `.codex/rules/00-private-rules.md`, if present (local only).
4. `docs/development_roadmap/project_analysis.md`, if available (local project context).

Codex owns `AGENTS.md`, `.codex/` and `.agents/skills/`. These are independent
copies maintained separately. Do not read, modify, link to, or synchronize with
another assistant's configuration during normal work. Project source, wiki and
specifications remain the project's sources of truth.

## Essential rules

- Act as the orchestrator: delegate file changes to specialized subagents,
  inspect their results, and obtain an independent audit before delivery.
- Use `.codex/agents/*.toml` for roles and `.agents/skills/*/SKILL.md` for skills.
  When the client cannot select a custom role, spawn a general subagent with the
  corresponding TOML `developer_instructions` and required skill paths explicitly
  included in its brief. Subagents must not spawn further agents.
- Load each required skill before work; Codex does not implement automatic
  loading from another tool's agent frontmatter. Use `$skill-name` to invoke skills.
- Never modify `components/**`. Component synchronization is an explicitly
  invoked user-only procedure.
- Document every changed `sdkconfig*` option in `docs/wiki/sdkconfig_overrides.md`.
- Work on `feature/` branches; never commit directly to `main` or `master`.
- Proceed through planning, implementation, checks, documentation and local commits.
  Ask for user review before `git push` or `gh pr create`.
- Ask before destructive Git operations or resolving merge/rebase conflicts.
- Never stage private or ignored files, and never force-add them. Stage explicit
  paths only. Respect `.git/info/exclude` and the Codex directory ignore files.
- Technical artifacts (code, comments, configuration, commits and PRs) are English.
- Target ESP32-S3 with ESP-IDF v6.1. Observe the architecture and hardware rules
  in the required skills and verify hardware claims against target datasheets.

## Tooling maintenance

For changes to this Codex setup, delegate configuration/documentation changes to
`docs-maintainer` and validation to a separate `code-auditor`. The bootstrap
conversion may create these files before the roles can be discovered. Use
configuration and hook validation for tooling-only changes; a firmware build is
needed only when the change can affect the firmware.
