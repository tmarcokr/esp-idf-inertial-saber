#!/usr/bin/env python3
"""Remind Codex to document changed sdkconfig options after apply_patch."""

import json
import sys

from patch_paths import patch_paths, resolved_paths


def main():
    try:
        event = json.load(sys.stdin)
        names = sorted({path.name for name in patch_paths(event)
                        for path in resolved_paths(event, name)
                        if path.name.startswith("sdkconfig")})
    except (ValueError, OSError, RuntimeError):
        return
    if names:
        print(json.dumps({"hookSpecificOutput": {
            "hookEventName": "PostToolUse",
            "additionalContext": ", ".join(names) + " was modified. AGENTS.md: every changed "
            "option must be documented in docs/wiki/sdkconfig_overrides.md. Delegate "
            "the documentation to docs-maintainer and include it before delivery.",
        }}))


if __name__ == "__main__":
    main()
