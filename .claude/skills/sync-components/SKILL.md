---
name: sync-components
description: Sync or update external components from the esp-idf-components repository. Use only when the user explicitly invokes /sync-components.
disable-model-invocation: true
---

# 🚀 Workflow: Components Sync / Update

Use this workflow to sync or update shared components from the `esp-idf-components` repository into this project.

## Important Constraints
- **Do not use `git checkout`** for bringing files from the components repo (e.g., `git checkout componentes/main -- components/`), as it will forcefully overwrite local changes without warning.
- **Use `git merge`** to bring in changes. The histories are unrelated (there is no common ancestor), so every changed component file is an add/add conflict; `-X theirs` resolves them to the upstream version, because `components/` must be an exact copy of `esp-idf-components` (CLAUDE.md rule 3: no local customizations).
- Focus updates specifically on the `components/` directory.
- **Guardrail interaction**: once the user has invoked `/sync-components`, the agent runs every step itself on a `feature/` branch. `.claude/hooks/guard-bash.py` asks the user to confirm each persisting step (reset, staging, restore, clean, commit, push) **only while every modified `components/` file is an exact copy of `componentes/main`**; any other `components/` change (manual edits, unresolved conflicts, missing `git fetch componentes`) is denied. If a step is denied, stop and report it to the user instead of working around it.

## Process Steps

1. **Information Gathering**:
   - The working tree must be clean, with no untracked files (step 4 runs `git clean -fd`); otherwise stop and ask the user:
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
   - To merge changes safely while only targeting the `components/` folder, execute a merge without committing automatically, resolving conflicts to the upstream version:
     ```bash
     git merge componentes/main --no-commit --no-ff --allow-unrelated-histories -X theirs
     ```
   - *Note:* Files deleted upstream are not removed by this merge (there is no common ancestor); remove them explicitly if needed. If conflicts remain within `components/`, stop and ask the user.

4. **Filtering and Atomic Cleanup**:
   - Isolate the desired changes and purge the rest of the template files that might have been brought in:
     ```bash
     # 1. Unstage everything
     git reset HEAD
     
     # 2. Add ONLY the components directory
     git add components/
     
     # 3. Restore all other tracked files to their original state
     git restore .
     
     # 4. Remove all unrelated new files from the merge
     git clean -fd
     ```

5. **Finalizing**:
   - Review the staged changes:
     ```bash
     git status
     git diff --staged
     ```
   - If everything looks correct, commit the sync:
     ```bash
     git commit -m "chore: sync components from esp-idf-components"
     ```
