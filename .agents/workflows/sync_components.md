---
description: Sync or update external components from the esp-idf-components repository.
---

# 🚀 Workflow: Components Sync / Update

Use this workflow to sync or update shared components from the `esp-idf-components` repository into this project.

## Important Constraints
- **Do not use `git checkout`** for bringing files from the components repo (e.g., `git checkout componentes/main -- components/`), as it will forcefully overwrite local changes without warning.
- **Use `git merge`** to safely bring in changes. This ensures Git uses its merge engine to combine changes and alert about conflicts, preserving project-specific customizations if any.
- Focus updates specifically on the `components/` directory.

## Process Steps

1. **Information Gathering**:
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
   - To merge changes safely while only targeting the `components/` folder, execute a merge without committing automatically:
     ```bash
     git merge componentes/main --no-commit --no-ff --allow-unrelated-histories
     ```
   - *Note:* If conflicts occur within the `components/` directory, they must be resolved manually.

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
