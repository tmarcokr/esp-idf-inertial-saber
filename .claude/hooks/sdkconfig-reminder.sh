#!/usr/bin/env bash
# PostToolUse (Edit|Write|MultiEdit): enforce CLAUDE.md rule 2 (document sdkconfig changes).
set -euo pipefail

command -v jq >/dev/null || { echo "sdkconfig-reminder.sh requires jq" >&2; exit 1; }

input="$(cat)"
file_path="$(jq -r '.tool_input.file_path // .tool_response.filePath // empty' <<<"$input")"
[[ -z "$file_path" ]] && exit 0

case "$(basename "$file_path")" in
  sdkconfig|sdkconfig.defaults|sdkconfig.defaults.*)
    jq -n --arg f "$(basename "$file_path")" '{hookSpecificOutput: {hookEventName: "PostToolUse",
      additionalContext: ("\($f) was modified. CLAUDE.md rule 2: every changed option must be documented in docs/wiki/sdkconfig_overrides.md (delegate to docs-maintainer or list the options in your report).")}}'
    ;;
esac
exit 0
