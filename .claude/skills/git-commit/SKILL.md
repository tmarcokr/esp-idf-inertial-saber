---
name: git-commit
description: Create a clean commit following the project's atomic commit policy and quality standards. Use whenever a logical unit of work is complete on a feature branch, or when the user invokes /git-commit.
argument-hint: "[commit description]"
---

# 🚀 Workflow: Creating a Commit

Use this workflow to record your progress after completing a specific sub-task or feature.

Commit description provided by the user (if any): `$ARGUMENTS`

## Steps to Follow

1. **Pre-Commit Validation**:
   - Before committing, you MUST ensure the code is functional and does not break the project.
   - Build with the ESP-IDF VS Code extension (**"ESP-IDF: Build your Project"**) or from a shell. ESP-IDF v6.1 (the CI version) is installed by the extension under `~/.espressif/`; source its environment only if `idf.py` is not already on PATH (if no ESP-IDF environment is available, state `Build: NOT VERIFIED` in the commit report; CI builds every PR):
     ```bash
     command -v idf.py >/dev/null || source ~/.espressif/tools/activate_idf_v6.1.sh
     idf.py build
     ```
   - *Note:* If the project includes unit tests, they should also be executed and passed.

2. **Stage Changes**:
   - Select the files to be included in the commit:
     ```bash
     git add <file_path>
     ```
   - Stage explicit paths only. Never use `git add -A`, `git add .` or `git commit -a`.
   - **Never stage private paths** listed in `.git/info/exclude` (even if they are tracked, e.g. `docs/development_roadmap/`).

3. **Format & Commit**:
   - Write a simple, descriptive commit message in a single sentence explaining the work done:
     ```bash
     git commit -m "feat: <simple_description>"
     ```
   - Use conventional prefixes (feat, fix, docs, chore, etc.).

4. **Verify State**:
   - Check the status to ensure everything is correctly recorded:
     ```bash
     git status
     ```
