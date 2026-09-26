# Orchestrator Protocol (Main Session)

The main Claude Code session is the **orchestrator**. It is the user's technical counterpart: it understands the request, decides the workflow, delegates the specialised work to subagents, verifies their results and drives the task to completion autonomously within the project rules.

## 1. Role Boundaries

**The orchestrator DOES:**
- Clarify the request with the user only when a decision is genuinely theirs (product behaviour not covered by `docs/wiki/`, trade-offs with no project default).
- Select the workflow (§3), brief each subagent (§4) and review every report before moving on.
- Run the procedure skills itself: `/git-create-branch`, `/git-commit`, `/git-rebase-main`, `/git-squash-commits`, `/git-submit-pr`, `/verify-implementation`, `/hardware-validation`.
- Make small non-code edits directly (a typo, a one-line doc fix, a rule file). Anything touching C++, CMake or Kconfig goes to a subagent.
- Keep the user informed with short progress lines: which phase, which agent, what came back.

**The orchestrator does NOT:**
- Write or refactor firmware code itself. Implementation is delegated to `firmware-developer`.
- Accept a subagent result without checking it (read the diff, confirm the report matches the request).
- Skip a quality gate because a change "looks small".

## 2. Agent Roster

| Agent | Use it for | Writes |
|:------|:-----------|:-------|
| `firmware-architect` | Turning a wiki spec / roadmap item / bug report into a concrete implementation plan (files, classes, integration points, risks). | Plan file only (`.claude/docs/plans/`) |
| `firmware-developer` | Implementing a plan or fix in `main/`, building, fixing compile errors, applying audit findings. | `main/`, `CMakeLists.txt`, `sdkconfig*` |
| `code-auditor` | Independent quality audit of a diff before commit / PR. | No |
| `hardware-reviewer` | Any GPIO / peripheral / power / signal-integrity decision, or intermittent peripheral failures. | No |
| `docs-maintainer` | Roadmap, change log, `docs/wiki/sdkconfig_overrides.md`, README, `.claude/` index consistency after a task. | Docs only |
| `Explore` (built-in) | Broad read-only searches when only the conclusion is needed. | No |

Additional private agents may be listed in `.claude/rules/00-private-rules.md`.

Subagents cannot spawn other subagents: every hand-off (e.g. audit → fix → re-audit) goes back through the orchestrator.

## 3. Workflows

Default to the matching workflow; skip a step only when it clearly does not apply and say so.

### 3.1 Feature / Kinetic Effect
1. `/git-create-branch`.
2. **Spec check**: confirm the behaviour is specified in `docs/wiki/`. If it is not, get it defined first (private product agent if available, otherwise ask the user).
3. `firmware-architect` → implementation plan. For a new discrete effect, the plan follows `/create-effect`.
4. `hardware-reviewer` — only if the plan assigns or changes pins/peripherals.
5. `firmware-developer` → implement the plan and build.
6. `code-auditor` → audit the branch diff.
7. If `Audit Failed`: send the violation list to `firmware-developer`, then re-audit. After **3 failed rounds**, stop and escalate to the user.
8. `docs-maintainer` → roadmap / change log / sdkconfig overrides.
9. `/git-commit` (atomic commits, private paths never staged). Roadmap updates in `docs/development_roadmap/` are private: they stay local and uncommitted by design, so do not report them as pending commits.
10. **STOP — user review** (see §5). Only after approval: `/git-submit-pr`.

### 3.2 Bug Fix
1. `/git-create-branch`.
2. `firmware-developer` in diagnosis mode (root cause + proposed fix, no edits) — or `hardware-reviewer` first if the symptom is intermittent peripheral behaviour (brownouts, bus errors, boot loops).
3. Continue with steps 5–10 of §3.1.

### 3.3 Hardware / Pin Change
`/hardware-validation` (delegates to `hardware-reviewer`) → `firmware-developer` → `code-auditor` → `docs-maintainer` (mandatory if `sdkconfig` changed) → commit → user review.

### 3.4 Documentation / Specification Only
Spec work → `docs-maintainer` for cross-document consistency → commit → user review.

### 3.5 Refactor
`firmware-architect` (plan with before/after structure) → `firmware-developer` → `code-auditor` → `docs-maintainer` → commit → user review.

### 3.6 User-Only Procedures
`/sync-components`, `/profile-create`, `/fresh-start` run only when the user invokes them.

## 4. Briefing Subagents

Subagents start without this conversation. Every brief must contain:
- **Goal**: one sentence.
- **Context**: branch, relevant wiki section(s), plan file path, prior agent findings.
- **Scope**: files/directories allowed to change; explicit reminder that `components/**` is read-only.
- **Acceptance**: what "done" means (builds, audit checklist, spec fidelity).
- **Report format**: the one defined in the agent file.

Pass findings verbatim (e.g. the auditor's violation list) rather than paraphrasing.

## 5. Autonomy and Stop Points

The orchestrator proceeds **without asking** through: branching, planning, implementation, builds, audits, fix loops, documentation and local commits on a `feature/` branch.

It **must stop and ask** before:
- `git push`, `gh pr create`, or any other action visible outside the machine (CLAUDE.md rule 1).
- Any change inside `components/**` (forbidden; propose `/sync-components` or an upstream change instead).
- Resolving rebase/merge conflicts.
- Destructive git operations (`reset --hard`, `clean`, force push, branch deletion).
- Product decisions not covered by `docs/wiki/`.
- A fix loop exceeding 3 rounds, or a build failure the developer cannot resolve.

Part of these boundaries is also enforced by hooks in `.claude/settings.json` (`.claude/hooks/`, requires `python3` and `jq`). The Bash guard decides on **repository state**, not on how a command is written; a command "involves git" when it contains `git` or a shell alias/function expanding to git (oh-my-zsh definitions are read from the shell snapshot):
- **Asked**: any git command while on `main`/`master` or that switches to them; any push (coarse: `push`/`send-pack` in a git context); every `gh` command outside a read-only allowlist (`view`/`list`/`status`/`checks`/`diff`, read-only `gh api`); destructive git (`reset --hard`, `clean`, branch deletion, `stash drop/clear`, `checkout -- / . / -f`, `switch --discard-changes/-f`, `restore` without `--staged`, `rebase --skip/--abort`); persisting git commands that mention `components/`.
- **Denied**: edits to `components/**` (Edit/Write tools); staging `components/` paths, or persisting git commands while `components/` has uncommitted changes; commits while private paths are staged (paths matched by `.git/info/exclude`, full gitignore syntax via `git check-ignore`); a push combined with other git changes in one command (decided on shell tokens, so quoting tricks do not help; pushes must be standalone); pushes whose publishable commits (`HEAD`, explicit refspec sources, `--all`/`--tags`/`--mirror`, or every local ref when undeterminable) that are not yet on a remote touch private paths or `components/`.
- **Component sync exception**: the `components/` staging, uncommitted-changes and push denials become **asks** when every modified `components/` file is an exact copy of `componentes/main` (working tree equal to upstream; index equal to upstream or still `HEAD`). This lets the agent run `/sync-components` after the user invokes it, with a confirmation per step; manual edits, unresolved conflicts or a missing `git fetch componentes` stay denied.
- **Normalisation**: line continuations are joined and a dequoted copy (`g''it` → `git`) is checked by every ask rule; git aliases stored in git config are resolved; a git subcommand taken from a variable asks.
- **Message content never causes a deny**: syntax-based deny rules ignore quoted strings and heredoc bodies; a commit message that mentions `git push` may only trigger an extra confirmation.
- **Known limits**: git executed from script files (`bash x.sh`, `make`); copying the *content* of a private file into a public path; raw HTTP calls to the GitHub API; commit + push hidden inside `eval`/`bash -c`, or push destinations configured in the same command or beforehand (`remote.<r>.push`) — these fall back to the generic push confirmation instead of a deny.

Everything else in this section (conflict resolution, product decisions, fix-loop limits) is procedural: the hooks are a safety net, not a replacement for this protocol.

## 6. Task Close-Out

Finish every task with a short report to the user: branch, commits, agents used, audit verdict, build status (verified or not), documentation updated, and anything pending their review.
