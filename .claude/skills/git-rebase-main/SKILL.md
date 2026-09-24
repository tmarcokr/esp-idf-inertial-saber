---
name: git-rebase-main
description: Rebase the current branch with main to keep history clean and updated. Use before submitting a Pull Request (as part of /git-submit-pr), or when the user invokes /git-rebase-main.
argument-hint: "[branch name]"
---

# 🚀 Workflow: Rebase with Main

Use this workflow to synchronize your feature branch with the latest changes from `main` without creating merge commits.

## Steps to Follow

1. **Update Local Main**:
   - Switch to the `main` branch and pull the latest changes:
     ```bash
     git checkout main && git pull
     ```

2. **Rebase Feature Branch**:
   - Switch back to your feature branch and perform the rebase:
     ```bash
     git checkout <your_branch>
     git rebase main
     ```
   - Branch to rebase: `$ARGUMENTS` (if empty, use the branch that was checked out before step 1).

3. **Resolve Conflicts (If Any)**:
   - If Git reports conflicts, **stop and ask the user** to resolve them manually in their editor.
   - After the user confirms resolution, stage the files and continue:
     ```bash
     git add <resolved_files>
     git rebase --continue
     ```

4. **Update Remote**:
   - Since history has diverged, you must force push. **Ask the user first** (outward-facing action):
     ```bash
     git push --force-with-lease
     ```

## Why Rebase?
- Maintains a **linear history** (no "Merge branch 'main' into..." commits).
- Simplifies the final squash before the Pull Request.
- Ensures you are testing your changes against the most recent code.
