#!/usr/bin/env bash
# Codex apply_patch compatibility adapter; preserve the native hook event on stdin/stdout.
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$script_dir/sdkconfig-reminder.py"
