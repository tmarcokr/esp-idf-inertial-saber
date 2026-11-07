#!/usr/bin/env python3
"""Deny apply_patch writes touching this project's immutable components tree."""

import json
import sys

from patch_paths import PROJECT_ROOT, is_within, patch_paths, resolved_paths


def deny(reason):
    print(json.dumps({"hookSpecificOutput": {
        "hookEventName": "PreToolUse",
        "permissionDecision": "deny",
        "permissionDecisionReason": reason,
    }}))


def main():
    try:
        event = json.load(sys.stdin)
        paths = patch_paths(event)
        components = PROJECT_ROOT / "components"
        targets = (components, components.resolve())
        for name in paths:
            if any(is_within(path, target)
                   for path in resolved_paths(event, name) for target in targets):
                deny("components/** is immutable (AGENTS.md). Use the component public "
                     "API, or report the needed upstream change. Component synchronization "
                     "is an explicitly invoked user-only procedure.")
                return
    except (ValueError, OSError, RuntimeError) as error:
        deny("Cannot validate apply_patch paths against the components/** policy: " + str(error))


if __name__ == "__main__":
    main()
