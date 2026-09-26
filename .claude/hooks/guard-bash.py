#!/usr/bin/env python3
"""PreToolUse (Bash) guardrail driven by repository state instead of command syntax.

Command detection is deliberately coarse and conservative: a command "involves git" when the word
`git` appears in it, or a word that is a shell alias or function expanding to git (read from the
Claude Code shell snapshot, e.g. oh-my-zsh `gaa`, `gcam`, `gp`, `ggp`). Decisions then depend on state:

deny:
  - a push combined with any other persisting git verb in the same command (pushes must be standalone,
    so the state check sees exactly what will be published);
  - a push while unpushed commits (any ref: branches, tags, stash, HEAD, explicit sources) touch
    private paths, or touch `components/` with content that differs from `componentes/main`;
  - a commit while private paths are staged (paths matched by .git/info/exclude with full gitignore syntax);
  - staging `components/` paths, or any persisting git command while `components/` has uncommitted changes,
    unless every modified `components/` file is an exact copy of `componentes/main` (a component sync,
    /sync-components): those cases ask instead.
ask:
  - any git command while on main/master, or that switches to main/master;
  - any push (coarse: the word push/send-pack/http-push in a git context), any `gh` command that is not
    on the read-only allowlist, destructive git operations, and persisting git commands that mention
    `components/`.

Deny rules that depend on command syntax ignore quoted strings and heredoc bodies (commit messages),
so message content can only cause an extra ask, never a deny. The strictest decision wins.
Fails closed (exit 2) on internal errors.
Known limits: git executed from script files (`bash x.sh`, `make`); copying the content of a private
file to a public path; raw HTTP calls to the GitHub API; commit + push hidden inside `eval`/`bash -c`, and
push destinations configured in the same command or beforehand (`remote.<r>.push`) — these degrade to the
generic push confirmation instead of a deny.
"""
from __future__ import annotations

import glob
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

PROJECT_DIR = os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()
SNAPSHOT_GLOB = os.path.expanduser("~/.claude/shell-snapshots/*.sh")
SEVERITY = {"ask": 1, "deny": 2}
UPSTREAM_REF = "refs/remotes/componentes/main"  # esp-idf-components, remote added by /sync-components

WORD = re.compile(r"[A-Za-z0-9_.:!+-]+")
ALIAS_LINE = re.compile(r"^alias (?:-- )?([^=\s]+)='(.*)'$")
FUNCTION_START = re.compile(r"^([A-Za-z0-9_.:!+-]+) \(\) \{\s*$")
GIT_PREFIX = r"\bgit(?:\s+(?:-C|-c|--git-dir|--work-tree|--namespace)\s+\S+|\s+-{1,2}[\w-]+(?:=\S+)?)*\s+"
CLAUSE = r"[^;&|\n]*"


def git_verb(pattern: str) -> re.Pattern[str]:
    """`git [global options] <pattern>`: anchored form, used where a false positive would deny."""
    return re.compile(GIT_PREFIX + "(?:" + pattern + ")")


PERSISTING_WORDS = re.compile(
    r"\b(add|stage|commit|push|merge|rebase|cherry-pick|am|apply|stash|pull|revert|reset|update-index|"
    r"commit-tree|update-ref|mv|rm)\b")
PERSISTING_STRICT = git_verb(
    r"(add|stage|commit|merge|rebase|cherry-pick|am|apply|stash|pull|revert|reset|update-index|commit-tree|"
    r"update-ref|mv|rm)\b")
COMMIT_WORDS = re.compile(r"\b(commit|merge|cherry-pick|revert|am|pull|commit-tree)\b")
PUSH_WORDS = re.compile(r"\b(push|send-pack|http-push)\b")
SWITCH_TO_MAIN = git_verb(r"(checkout|switch)\s+(-\S+\s+)*(main|master)\b|worktree\s+add\b" + CLAUSE
                          + r"\b(main|master)\b")
STAGE_COMPONENTS = git_verb(r"(add|stage)\b" + CLAUSE + r"(^|[\s'\"/=])components(/|\s|$|['\"])")
COMPONENTS_MENTION = re.compile(r"(^|[^\w-])components(/|\s|$)")
DESTRUCTIVE = [
    (git_verb(r"reset\b" + CLAUSE + r"--hard"), "git reset --hard discards work"),
    (git_verb(r"clean\b"), "git clean deletes untracked files"),
    (git_verb(r"branch\b" + CLAUSE + r"(\s-[a-zA-Z]*[dD]\b|--delete)"), "branch deletion"),
    (git_verb(r"stash\s+(drop|clear)\b"), "stash deletion"),
    (git_verb(r"checkout\b" + CLAUSE + r"(\s--(\s|$)|\s\.(\s|$)|\s-[a-zA-Z]*f|--force)"),
     "checkout may discard changes"),
    (git_verb(r"switch\b" + CLAUSE + r"(--discard-changes|\s-[a-zA-Z]*f|--force)"), "switch may discard changes"),
    (git_verb(r"restore\b(?!" + CLAUSE + r"--staged)|restore\b" + CLAUSE + r"(--worktree|\s-[a-zA-Z]*W)"),
     "restore may discard working-tree changes"),
    (git_verb(r"rebase\b" + CLAUSE + r"--(skip|abort)"), "rebase conflict handling is reserved to the user"),
]
GH_CALL = re.compile(r"(?:^|[^\w.-])gh((?:\s+(?:-R|--repo|--hostname)\s+\S+|\s+-\S+)*)\s+([a-z][\w-]*)"
                     r"(?:\s+([a-z][\w-]*))?(" + CLAUSE + ")")
GH_READ_ONLY = {
    "pr": {"view", "list", "status", "checks", "diff"},
    "issue": {"view", "list", "status"},
    "run": {"view", "list", "watch", "download"},
    "workflow": {"view", "list"},
    "release": {"view", "list", "download"},
    "repo": {"view", "list"},
    "gist": {"view", "list"},
    "label": {"list"},
    "auth": {"status", "token"},
    "search": None,
    "status": None,
    "browse": None,
    "help": None,
    "version": None,
    "--version": None,
}
GH_FIELDS = re.compile(r"(^|\s)(-[fF]\S*|--field\b|--raw-field\b|--input\b)")
GH_METHOD = re.compile(r"(?:^|\s)(?:-X\s*|--method[=\s]+)(\w+)", re.IGNORECASE)


class Verdict:
    def __init__(self) -> None:
        self.decision: str | None = None
        self.reason = ""

    def add(self, decision: str, reason: str) -> None:
        if self.decision is None or SEVERITY[decision] > SEVERITY[self.decision]:
            self.decision, self.reason = decision, reason

    def emit(self) -> None:
        if self.decision:
            print(json.dumps({"hookSpecificOutput": {
                "hookEventName": "PreToolUse",
                "permissionDecision": self.decision,
                "permissionDecisionReason": self.reason,
            }}))


def git_output(*args: str) -> list[str]:
    result = subprocess.run(["git", "-C", PROJECT_DIR, *args], capture_output=True, text=True)
    return [line for line in result.stdout.splitlines() if line] if result.returncode == 0 else []


def git_paths(*args: str) -> list[str]:
    """Path list from a git command run with -z: NUL-separated, never quoted or escaped."""
    args = list(args)
    args.insert(args.index("--") if "--" in args else len(args), "-z")  # after `--` it would be a pathspec
    result = subprocess.run(["git", "-C", PROJECT_DIR, *args], capture_output=True, text=True,
                            errors="surrogateescape")
    return [path for path in result.stdout.split("\0") if path.strip("\n")] if result.returncode == 0 else []


def private_paths(paths: list[str]) -> list[str]:
    """Subset of `paths` matched by .git/info/exclude, with full gitignore semantics (globs, `**`, `!`).

    Evaluated by `git check-ignore` in an empty scratch repository holding only a copy of the exclude
    file, so .gitignore files and global excludes of the project do not interfere.
    """
    exclude = os.path.join(PROJECT_DIR, ".git", "info", "exclude")
    if not paths or not os.path.isfile(exclude):
        return []
    with tempfile.TemporaryDirectory() as scratch:
        subprocess.run(["git", "init", "-q", scratch], check=True, capture_output=True)
        shutil.copyfile(exclude, os.path.join(scratch, ".git", "info", "exclude"))
        result = subprocess.run(
            ["git", "-C", scratch, "-c", "core.excludesFile=", "check-ignore", "--no-index", "--stdin", "-z"],
            input="\0".join(paths) + "\0", capture_output=True, text=True, errors="surrogateescape")
    if result.returncode not in (0, 1):
        raise RuntimeError(f"git check-ignore failed: {result.stderr.strip()}")
    matched = set(result.stdout.split("\0"))
    return [path for path in paths if path in matched]


def is_components(path: str) -> bool:
    return path == "components" or path.startswith("components/")


def blob(spec: str) -> str | None:
    """Blob id of `<rev>:<path>` or `:<path>` (index, stage 0); None when absent."""
    return (git_output("rev-parse", "--verify", "--quiet", spec) or [None])[0]


def worktree_blob(path: str) -> str | None:
    """Blob id of the working-tree file; None when it does not exist as a regular file."""
    if not os.path.isfile(os.path.join(PROJECT_DIR, path)):
        return None
    return (git_output("hash-object", "--", path) or [None])[0]


def dirty_components_match_upstream(dirty: list[str]) -> bool:
    """True when every modified components/ path is an exact copy of UPSTREAM_REF (a component sync).

    The working tree must hold the upstream content (or lack the file when upstream lacks it); the
    index may hold either the upstream content or still the HEAD content (unstaged during the sync).
    Conflicted paths have no stage-0 entry and therefore never match.
    """
    if not dirty or not git_output("rev-parse", "--verify", "--quiet", UPSTREAM_REF):
        return False
    for path in dirty:
        if not is_components(path):
            return False
        upstream = blob(f"{UPSTREAM_REF}:{path}")
        if worktree_blob(path) != upstream or blob(f":{path}") not in {upstream, blob(f"HEAD:{path}")}:
            return False
    return True


def unpushed_components_match_upstream(calls: list[tuple[str, list[str]]] | None) -> bool:
    """True when every components/ file touched by an unpushed commit equals UPSTREAM_REF in that commit."""
    if not git_output("rev-parse", "--verify", "--quiet", UPSTREAM_REF):
        return False
    commit = None
    for line in git_output("log", "--name-only", "--no-renames", "--diff-merges=dense-combined", "--format=@%H",
                           *push_revisions(calls), "--not", "--remotes"):
        if line.startswith("@"):
            commit = line[1:]
        elif is_components(line) and blob(f"{commit}:{line}") != blob(f"{UPSTREAM_REF}:{line}"):
            return False
    return True


def load_shell_definitions() -> dict[str, str] | None:
    """Aliases and function bodies from the newest shell snapshot; None when no snapshot exists."""
    snapshots = sorted(glob.glob(SNAPSHOT_GLOB), key=os.path.getmtime)
    if not snapshots:
        return None
    definitions: dict[str, str] = {}
    function_name, body = None, []
    with open(snapshots[-1], encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.rstrip("\n")
            if function_name is not None:
                if line == "}":
                    definitions[function_name] = "\n".join(body)
                    function_name, body = None, []
                else:
                    body.append(line)
                continue
            alias = ALIAS_LINE.match(line)
            if alias:
                definitions[alias.group(1)] = alias.group(2).replace("'\\''", "'")
                continue
            function = FUNCTION_START.match(line)
            if function:
                function_name = function.group(1)
    return definitions


def effective_text(command: str, definitions: dict[str, str]) -> str:
    """Command text plus the expansion of every word that is a shell alias or function (bounded)."""
    parts, pending, seen = [command], [command], set()
    for _ in range(4):
        expansions = []
        for text in pending:
            for word in WORD.findall(text):
                if word in definitions and word not in seen:
                    seen.add(word)
                    expansions.append(definitions[word])
        if not expansions:
            break
        parts.extend(expansions)
        pending = expansions
    return "\n".join(parts)


def strip_literals(text: str) -> str:
    """Remove heredoc bodies and quoted strings: deny rules must never fire on message content."""
    text = re.sub(r"<<-?\s*(['\"]?)(\w+)\1[^\n]*\n.*?\n\s*\2(?=\n|$)", "<<", text, flags=re.DOTALL)
    return re.sub(r"'[^']*'|\"(?:\\.|[^\"\\])*\"", "''", text)


def involves_git(text: str) -> bool:
    return re.search(r"(^|[^\w.-])git([^\w-]|$)", text) is not None


def unknown_git_like_words(command: str) -> list[str]:
    """Without a snapshot, leading words that look like git aliases and are not real executables."""
    words = re.findall(r"(?:^|[;&|(\n]|\bthen\b|\bdo\b)\s*(g[a-z!]{1,8})(?=\s|$|[;&|)])", command)
    return [word for word in words if shutil.which(word) is None]


GIT_GLOBAL_OPTS_WITH_VALUE = {"-C", "-c", "--git-dir", "--work-tree", "--namespace", "--exec-path",
                              "--config-env", "--super-prefix"}
PERSISTING_SUBCOMMANDS = {"add", "stage", "commit", "merge", "rebase", "cherry-pick", "am", "apply", "stash",
                          "pull", "revert", "reset", "update-index", "commit-tree", "update-ref", "mv", "rm"}
PUSH_SUBCOMMANDS = {"push", "send-pack", "http-push"}
PUSH_OPTS_WITH_VALUE = {"--repo", "-o", "--push-option", "--receive-pack", "--exec"}


def strip_heredoc_bodies(text: str) -> str:
    return re.sub(r"<<-?\s*(['\"]?)(\w+)\1[^\n]*\n.*?\n\s*\2(?=\n|$)", "<<", text, flags=re.DOTALL)


def git_invocations(code: str) -> list[tuple[str, list[str]]] | None:
    """(subcommand, args) for every `git` call, from shlex tokens (quotes merged, messages kept whole).

    Returns None when the text cannot be tokenised. Aliases stored in git config are resolved.
    """
    lexer = shlex.shlex(code.replace("\n", " ; "), posix=True, punctuation_chars=True)
    lexer.whitespace_split = True
    try:
        tokens = list(lexer)
    except ValueError:
        return None
    calls, segment = [], []
    for token in tokens + [";"]:
        if set(token) <= set(";&|()"):
            for index, word in enumerate(segment):
                if os.path.basename(word) != "git":
                    continue
                cursor = index + 1
                while cursor < len(segment) and segment[cursor].startswith("-"):
                    cursor += 2 if segment[cursor] in GIT_GLOBAL_OPTS_WITH_VALUE else 1
                if cursor < len(segment):
                    subcommand, args = segment[cursor], segment[cursor + 1:]
                    stored = git_output("config", "--get", f"alias.{subcommand}")
                    if stored:
                        subcommand = stored[0].lstrip("!").split()[0] if stored[0].strip("! ") else subcommand
                    calls.append((subcommand, args))
            segment = []
        else:
            segment.append(token)
    return calls


def defines_git_alias(calls: list[tuple[str, list[str]]], code: str) -> bool:
    return any(sub == "config" and any(a.startswith("alias.") for a in args) for sub, args in calls) \
        or re.search(r"(^|\s)-c\s+alias\.", code) is not None


def push_revisions(calls: list[tuple[str, list[str]]] | None) -> list[str]:
    """Revisions a push can publish; falls back to every local ref when it cannot be determined."""
    everything = ["--all", "HEAD"]
    pushes = [args for sub, args in (calls or []) if sub in PUSH_SUBCOMMANDS]
    if not pushes:
        return everything
    revisions: list[str] = []
    for args in pushes:
        positionals, index = [], 0
        while index < len(args):
            arg = args[index]
            if arg in {"--all", "--branches"}:
                revisions.append("--branches")
            elif arg == "--mirror":
                revisions += ["--branches", "--tags"]
            elif arg == "--tags":
                revisions.append("--tags")
            elif arg in PUSH_OPTS_WITH_VALUE:
                index += 1
            elif not arg.startswith("-"):
                positionals.append(arg)
            index += 1
        refspecs = positionals[1:]
        if not refspecs and not revisions:
            revisions.append("HEAD")
        for refspec in refspecs:
            source = refspec.lstrip("+").split(":", 1)[0]
            if not source:
                continue
            if not git_output("rev-parse", "--verify", "--quiet", source):
                return everything
            revisions.append(source)
    return revisions or everything


def unpushed_files(calls: list[tuple[str, list[str]]] | None) -> list[str]:
    """Files touched by commits the push can publish that are not yet on any remote."""
    files = git_paths("log", "--name-only", "--no-renames", "--diff-merges=dense-combined", "--format=",
                      *push_revisions(calls), "--not", "--remotes")
    return sorted({path.strip("\n") for path in files})


def check_gh(text: str, verdict: Verdict) -> None:
    for match in GH_CALL.finditer(text):
        group, action, rest = match.group(2), match.group(3), match.group(4) or ""
        if group == "api":
            arguments = f"{action or ''} {rest}"
            method = GH_METHOD.search(arguments)
            read_only_graphql = ("graphql" in arguments and "{" in arguments and "mutation" not in arguments
                                 and "@" not in arguments and "$(" not in arguments and "`" not in arguments
                                 and "--input" not in arguments)
            if method:
                mutating = method.group(1).upper() not in {"GET", "HEAD"} and not read_only_graphql
            else:
                mutating = bool(GH_FIELDS.search(arguments)) and not read_only_graphql
            if mutating:
                verdict.add("ask", "Mutating GitHub API call: requires the user's confirmation.")
            continue
        if group in GH_READ_ONLY and (GH_READ_ONLY[group] is None or action in GH_READ_ONLY[group]):
            continue
        label = f"gh {group} {action or ''}".strip()
        verdict.add("ask", f"GitHub CLI command `{label}` is not on the read-only allowlist: requires the "
                           "user's confirmation.")


def analyse(command: str, verdict: Verdict) -> None:
    definitions = load_shell_definitions() or {}
    joined = command.replace("\\\n", " ")
    dequoted = re.sub(r"['\"\\\\]", "", joined)
    text = effective_text(joined + "\n" + dequoted, definitions)
    code = effective_text(strip_literals(joined), definitions)
    calls = git_invocations(effective_text(strip_heredoc_bodies(joined), definitions))
    git = involves_git(text)

    check_gh(text, verdict)
    if not definitions and unknown_git_like_words(command):
        verdict.add("ask", "Shell snapshot unavailable: possible git alias could not be resolved; confirm manually.")
    if not git:
        return

    branch = (git_output("rev-parse", "--abbrev-ref", "HEAD") or [""])[0]
    if branch in {"main", "master"}:
        verdict.add("ask", f"git command on {branch}: every git operation on the protected branch requires the "
                           "user's confirmation. Work on a feature/ branch (/git-create-branch).")
    if SWITCH_TO_MAIN.search(text):
        verdict.add("ask", "This command switches to main/master: every git operation there requires the user's "
                           "confirmation.")

    dirty_components: list[str] = []
    if STAGE_COMPONENTS.search(code) or PERSISTING_WORDS.search(text):
        dirty_components = [entry[3:] for entry in git_paths("status", "--porcelain", "--no-renames",
                                                             "--untracked-files=all", "--", "components")]
    is_sync = dirty_components_match_upstream(dirty_components)

    if STAGE_COMPONENTS.search(code):
        if is_sync:
            verdict.add("ask", "Component sync: staging components/ whose modified files all match "
                               "componentes/main. Confirm the sync (/sync-components).")
        else:
            verdict.add("deny", "Staging components/** is not allowed (CLAUDE.md rule 3) unless every modified "
                                "file matches componentes/main (/sync-components, after `git fetch componentes`).")
    elif PERSISTING_STRICT.search(text) and COMPONENTS_MENTION.search(text):
        verdict.add("ask", "Persisting git command in a command that mentions components/: confirm that no "
                           "component file is modified (CLAUDE.md rule 3).")
    if PERSISTING_WORDS.search(text) and dirty_components:
        if is_sync:
            verdict.add("ask", "Component sync in progress: every modified components/ file matches "
                               "componentes/main. Confirm this step of /sync-components.")
        else:
            verdict.add("deny", "components/ has uncommitted changes (e.g. " + dirty_components[0] + ") that do not "
                                "match componentes/main. components/** is immutable (CLAUDE.md rule 3): revert them "
                                "with `git restore -- components/` or ask the user.")

    if COMMIT_WORDS.search(text):
        staged_private = private_paths(git_paths("diff", "--cached", "--name-only"))
        if staged_private:
            verdict.add("deny", f"Private path staged: {staged_private[0]} (listed in .git/info/exclude). "
                                f"Unstage it with: git restore --staged -- {staged_private[0]}")

    if calls is not None and any(sub.startswith("$") for sub, _ in calls):
        verdict.add("ask", "git subcommand comes from a variable; the guardrail cannot resolve it. Confirm manually.")

    if PUSH_WORDS.search(text):
        subcommands = {sub for sub, _ in calls or []}
        if calls is None:
            verdict.add("ask", "Push in a command the guardrail cannot tokenise; confirm it manually.")
        elif subcommands & PUSH_SUBCOMMANDS and (subcommands & PERSISTING_SUBCOMMANDS
                                                 or defines_git_alias(calls, code)):
            verdict.add("deny", "Push combined with other git changes in one command. Run the push as a standalone "
                                "command so the guard can check exactly what will be published.")
        files = unpushed_files(calls)
        leaked = private_paths(files)
        touched_components = [f for f in files if is_components(f)]
        if leaked:
            verdict.add("deny", f"Unpushed commits contain a private path ({leaked[0]}). Rewrite those commits "
                                "before pushing; private files must never reach GitHub.")
        elif touched_components and not unpushed_components_match_upstream(calls):
            verdict.add("deny", f"Unpushed commits modify components/ ({touched_components[0]}) with content that "
                                "does not match componentes/main. components/** is immutable (CLAUDE.md rule 3).")
        target = " to main/master" if re.search(r"\b(main|master)\b", text) else ""
        verdict.add("ask", f"Outward-facing action: push{target} requires the user's review (CLAUDE.md rule 1).")

    for pattern, reason in DESTRUCTIVE:
        if pattern.search(text):
            verdict.add("ask", f"Destructive git operation ({reason}): requires the user's confirmation.")


GUARD_DISABLED = True


def main() -> None:
    if GUARD_DISABLED:
        return
    command = json.load(sys.stdin).get("tool_input", {}).get("command", "")
    if not command:
        return
    verdict = Verdict()
    analyse(command, verdict)
    verdict.emit()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:  # fail closed: a broken guard must not silently allow commands
        print(f"guard-bash.py internal error: {error}", file=sys.stderr)
        sys.exit(2)
