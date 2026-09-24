---
name: git-squash-commits
description: Squash multiple local commits into a single cohesive commit before opening a Pull Request. Use as part of /git-submit-pr, or when the user invokes /git-squash-commits.
argument-hint: "[base branch, default main]"
---

# 🚀 Workflow: Squash Commits (Interactive Rebase)

Use this workflow to clean up your commit history (e.g., combining multiple "WIP" or "wp" commits into a single cohesive functional commit) before submitting a Pull Request to `main` (or `master`).

## Requirements
- Base branch: `$ARGUMENTS` (if empty, use `main`).

> **Note for Claude Code:** The Bash tool cannot drive interactive editors, so `git rebase -i` must not be run interactively. Steps 1–3 below describe the manual (human) flow; when executing this workflow as the agent, first list the commits to be squashed (`git log --oneline <base>..HEAD`, where `<base>` is the base branch above), show them to the user, and then perform the exact same `pick` + `fixup` rebase non-interactively:
> ```bash
> GIT_SEQUENCE_EDITOR="sed -i -e '2,\$ s/^pick /fixup /'" git rebase -i <base>
> ```
> This keeps `pick` on the first (top) commit and changes every subsequent `pick` to `fixup`, exactly as described in step 2. If the kept commit message needs rewording, use `git commit --amend -m "<message>"` afterwards.

## Steps to Follow

1. **Start Interactive Rebase**:
   - Run the command in your terminal targeting the base branch (in this case, `master` or `main`):
     ```bash
     git rebase -i main
     ```

2. **Edit the Rebase File**:
   - Your default terminal text editor (e.g., `nano` or `vim`) will open showing a list of your recent branch commits in chronological order.
   - Leave the word `pick` (or `p`) next to the **very first commit at the top** (this is the base commit you want to keep).
   - Change the word `pick` to `fixup` (or `f`) for all subsequent commits below it. `fixup` squashes the commit into the one above it and automatically discards its individual commit message (perfect for hiding "wp" logs).

   *Example snippet:*
   ```text
   pick 9e617f5 feat: implement polyphonic audio engine and AudioSample
   f e90b731 wp
   f 26efb3a wp
   f 216e696 FEATURE: Optimize polyphonic mixer levels
   ```

3. **Save and Close**:
   - Save the file and close the editor (if using `vim`, type `:wq` and press `Enter`).
   - Git will automatically process the rebase, squashing the marked commits into the top one, maintaining a single, clean timeline point.

4. **Force Push**:
   - Because you have rewritten your local commit history, a standard push will fail. **Ask the user first**, then force push your new unified history to remote:
     ```bash
     git push --force-with-lease
     ```

## Result
Your remote repository branch is now updated with a clean, linear history containing a single, highly descriptive commit. You are now ready to open the Pull Request.
