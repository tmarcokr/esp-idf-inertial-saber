# Independent Codex setup

`AGENTS.md` loads the ordered project rules. `.agents/skills/` contains independent
skill copies; `.codex/agents/` contains role definitions. No symbolic links or
runtime dependency on another assistant configuration are required.

## Recovery status

The user removed `.codex/config.toml` after it blocked workspace/chat loading.
It has been replaced with a new, comment-only TOML file: no active keys or tables,
no model, permission, agent or hook overrides. Existing client settings remain
in effect. The removed file is unavailable, so its exact parsing or compatibility
error cannot be reconstructed from these files.
During recovery, the locally installed CLI reported `codex-cli 0.163.0`; that
historical check did not establish which binary or configuration schema the IDE
used. On 2026-11-07, a native runtime query identified `Codex Desktop/0.167.0`.

All seven local role files parse as TOML and all seventeen local skill entrypoints
pass the skill-creator validator. Parsing does not establish client support for
every role field. When custom roles are unavailable, the orchestrator supplies the
role instructions and required skill paths explicitly to a general subagent.
Role scope descriptions are instructions, not filesystem sandbox boundaries.

The replacement parsed to an empty TOML object and the native CLI instruction
assembly (`codex debug prompt-input`) completed in a disposable Git workspace
with those exact bytes before installation. The output was valid JSON and the
command exited successfully. The CLI needed access to its runtime cache outside
the restricted filesystem. This verifies local CLI configuration loading and
instruction assembly; it does not verify the IDE loader or model/chat operation.
Project behavior remains in `AGENTS.md` and the ordered `.codex/rules/` files.
Do not restore the removed configuration or enable experimental settings without
first testing the exact client schema in a disposable workspace.

## Project hooks

`.codex/hooks.json` registers `apply_patch` handlers for `PreToolUse` and
`PostToolUse`. The shell adapters forward events to the Python guard and
sdkconfig reminder; registry commands resolve the Git root, so nested working
directories and checkout paths with spaces are supported. Python 3 and Git are
required.

The guard denies patch paths in `components/`, including moves that touch that
tree, and denies malformed or unresolvable path events. The reminder reports
changes to `sdkconfig`-prefixed basenames. Targeted synthetic fixtures cover
path forms, moves, symlinks, malformed events and reminder deduplication.

On 2026-11-07, the native `Codex Desktop/0.167.0` hook query reported both project
hooks enabled and trusted, with no errors or warnings. Trust depends on review
of the exact hook definitions and the checkout; inspect `/hooks` in the runtime
in use. Discovery and trust do not establish execution: actual native tool-event
dispatch has not been separately demonstrated. These handlers cover
`apply_patch` only; shell writes and other tools are outside their coverage.

## Privacy and maintenance

Shared ignore files cover generated and local artifacts. Private skill and agent
exclusions belong in `.git/info/exclude`; new skills and agents are not ignored
by default. Stage explicit paths only and inspect the staged diff before
committing. Maintain these independent copies separately.
