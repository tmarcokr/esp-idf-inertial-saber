---
name: sync-components
description: Sync or update external components from the esp-idf-components repository. Use only when the user explicitly
  invokes $sync-components.
---

# 🚀 Workflow: Components Sync / Update

Use this workflow to sync or update shared components from the `esp-idf-components` repository into this project.

## Important Constraints
- **Do not use `git checkout`** for bringing files from the components repo (e.g., `git checkout componentes/main -- components/`), as it will forcefully overwrite local changes without warning.
- **Use `git merge`** to bring in changes. The histories are unrelated (there is no common ancestor), so changed component files can produce add/add conflicts; `-X theirs` resolves those conflicts to the upstream version, because `components/` must be an exact copy of `esp-idf-components` (AGENTS.md rule 3: no local customizations).
- Focus updates specifically on the `components/` directory.
- **Guardrails**: run this procedure only when the user explicitly invokes `$sync-components`, on a `feature/` branch. Proceed through the steps subject to the operation-specific approval gates below. `components/` must end as an exact copy of `componentes/main`; any other change in `components/` (manual edits, unresolved conflicts) means stop and report. Confirm with the user before the push.

Invocation authorizes this procedure only. Immediately before conflict resolution (including the automatic resolution performed by `-X theirs`), destructive cleanup or deletion of files absent upstream, show the concrete affected paths and obtain confirmation for that specific operation. A generic invocation or earlier approval for another operation does not satisfy these gates. Never run blanket cleanup or restore commands.

## Process Steps

1. **Information Gathering**:
   - The working tree must be clean, with no untracked files; otherwise stop and ask the user:
     ```bash
     git status --porcelain --untracked-files=all
     ```
   - Check if the `componentes` remote already exists:
     ```bash
     git remote -v
     ```
   - If not, add the components repository as a remote:
     ```bash
     git remote add componentes https://github.com/tmarcokr/esp-idf-components.git
     ```

2. **Fetching Updates**:
   - Fetch the latest changes from the components repository:
     ```bash
     git fetch componentes
     ```

3. **Merging Changes (Safe approach)**:
   - Inspect the local and upstream trees and contents without changing files. Show the concrete paths that may be resolved to the upstream version, including any overlapping paths outside `components/` affected by the repository-wide merge. Explain that `-X theirs` performs automatic conflict resolution and obtain confirmation for that exact merge immediately before running it:
     ```bash
     git merge componentes/main --no-commit --no-ff --allow-unrelated-histories -X theirs
     ```
   - Files deleted upstream are not removed by this merge (there is no common ancestor). Identify their exact paths under `components/` for the separate deletion gate in step 4. If conflicts remain, stop and report them; do not resolve them without fresh confirmation for their concrete paths.

4. **Filtering and Atomic Cleanup**:
   - Review the merge result and enumerate concrete paths: desired component changes, tracked files outside `components/` to restore, unrelated new files to remove, and component files absent from upstream to delete. Immediately before each destructive operation, show its exact path list and obtain confirmation for that operation. Skip operations with no affected paths; stop if the scope is unclear or differs from the approved list.
   - Unstage the merge result, then stage only the explicitly listed component paths:
     ```bash
     git reset HEAD
     # component_paths must contain the concrete component paths reviewed above.
     test "${#component_paths[@]}" -gt 0 && git add -- "${component_paths[@]}"
     ```
   - Populate each array below only with the concrete, nonempty path list just approved by the user for that specific operation. Do not use `.` or broad directory scopes. Run each command only after its own confirmation:
     ```bash
     # Restore only approved tracked paths outside components/.
     test "${#approved_tracked_paths[@]}" -gt 0 && git restore --worktree -- "${approved_tracked_paths[@]}"

     # Preview the exact unrelated new-file paths, confirm them, then remove them.
     test "${#approved_new_paths[@]}" -gt 0 && git clean -nd -- "${approved_new_paths[@]}"
     test "${#approved_new_paths[@]}" -gt 0 && git clean -fd -- "${approved_new_paths[@]}"

     # Delete only approved component paths confirmed absent from upstream.
     test "${#approved_deleted_component_paths[@]}" -gt 0 && git rm -- "${approved_deleted_component_paths[@]}"
     ```
   - Skip operations with no affected paths. If the approved paths change, obtain fresh confirmation before proceeding.

5. **Finalizing**:
   - Review the staged changes and verify that `components/` exactly matches `componentes/main`, with no staged changes outside `components/`; otherwise stop and report:
     ```bash
     git status
     git diff --staged
     ```
   - If everything looks correct, commit the sync:
     ```bash
     git commit -m "chore: sync components from esp-idf-components"
     ```
