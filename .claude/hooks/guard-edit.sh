#!/usr/bin/env bash
# PreToolUse (Edit|Write|MultiEdit|NotebookEdit): enforce CLAUDE.md rule 3 (components/** is immutable).
set -euo pipefail

command -v jq >/dev/null || { echo "guard-edit.sh requires jq" >&2; exit 2; }

input="$(cat)"
file_path="$(jq -r '.tool_input.file_path // .tool_input.notebook_path // empty' <<<"$input")"
[[ -z "$file_path" ]] && exit 0

project_dir="$(realpath -m "${CLAUDE_PROJECT_DIR:-$(pwd)}")"
case "$file_path" in
  /*) abs_path="$(realpath -m "$file_path")" ;;
  *)  abs_path="$(realpath -m "$project_dir/$file_path")" ;;
esac

if [[ "$abs_path" == "$project_dir/components/"* ]]; then
  jq -n '{hookSpecificOutput: {hookEventName: "PreToolUse", permissionDecision: "deny",
    permissionDecisionReason: "components/** is immutable (CLAUDE.md rule 3). Use the component public API, or report the needed change so it can go upstream and arrive via /sync-components."}}'
fi
exit 0
