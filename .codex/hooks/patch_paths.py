"""Read apply_patch hook inputs without depending on external packages."""

import os
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PATH_MARKERS = ("*** Add File: ", "*** Update File: ", "*** Delete File: ", "*** Move to: ")


def patch_paths(event):
    """Return patch paths, including both sides of a move, or reject bad input."""
    if not isinstance(event, dict):
        raise ValueError("Expected a hook event object")
    tool_input = event.get("tool_input")
    if not isinstance(tool_input, dict):
        raise ValueError("Expected tool_input.command containing an apply_patch patch")
    patch = tool_input.get("command")
    if not isinstance(patch, str):
        raise ValueError("Expected tool_input.command containing an apply_patch patch")
    lines = patch.strip().splitlines()
    if not lines or lines[0] != "*** Begin Patch" or lines[-1] != "*** End Patch":
        raise ValueError("Expected a raw apply_patch patch")
    paths = []
    for line in lines:
        for marker in PATH_MARKERS:
            if line.startswith(marker):
                name = line[len(marker):]
                if not name or "\x00" in name:
                    raise ValueError("Invalid patch path")
                paths.append(name)
                break
    return paths


def resolved_paths(event, name):
    """Keep lexical and symlink-resolved paths to protect aliases and links out."""
    cwd = event.get("cwd")
    if not isinstance(cwd, str) or not Path(cwd).is_absolute():
        raise ValueError("Expected an absolute hook cwd")
    candidate = Path(name)
    if not candidate.is_absolute():
        candidate = Path(cwd) / candidate
    lexical = Path(os.path.abspath(candidate))
    try:
        candidate.resolve(strict=True)
    except FileNotFoundError:
        # New patch targets are expected not to exist yet; still resolve aliases
        # in their existing parent path using pathlib's non-strict behavior.
        pass
    return lexical, candidate.resolve()


def is_within(path, directory):
    return path == directory or directory in path.parents
