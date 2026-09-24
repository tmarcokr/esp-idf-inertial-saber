---
name: git-create-branch
description: Create a new Git branch following the project nomenclature. Use automatically at the start of every new task, before modifying any file, or when the user invokes /git-create-branch.
argument-hint: "[short description or task goal]"
---

# 🚀 Workflow: Branch Creation

Use this workflow whenever you start a new task.

## Naming Rules
- Mandatory prefix: **`feature/`**
- Use only lowercase and underscores.
- Keep descriptions short (1-3 words).

## Process Steps

1. **Information Gathering**:
   - Determine the core goal of the task (e.g., "Implementing I2C Sensor").
   - Task goal / short description provided by the user (if any): `$ARGUMENTS`

2. **Branch Formatting**:
   - Generate the branch name: `feature/<short_description>`
   - Example: `feature/i2c_sensor`

3. **Execution**:
   - Checkout to a new branch from `main`:
     ```bash
     git checkout -b feature/<your_description> main
     ```
   - *Note:* Always use plain `git` commands. Shell aliases (e.g. oh-my-zsh `gcb`, `gaa`, `gp`) may exist in the agent's shell but must not be used in skills or agent commands.
   - If the working tree has uncommitted changes, report them before switching; never stash or discard them silently.
