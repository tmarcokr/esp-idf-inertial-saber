---
name: fresh-start
description: Signal the start of a new task and re-evaluate core instructions with a clean context. Use only when the user explicitly invokes /fresh-start to begin a new, distinct task.
disable-model-invocation: true
---

# Workflow: Fresh Start

## Objective
This workflow is used to signal the start of a new, distinct task. It instructs the agent to metaphorically "clear its context" and begin fresh, re-evaluating the project's core instructions as if it were a new session.

The goal is to have a "new attitude and a clean mind" ("una actitud nueva y mente limpa") for the upcoming task, as requested by the user.

## Process
1.  **Acknowledge New Task:** Verbally confirm that you are starting a new task.
2.  **Re-read Core Instructions:** To ensure you have the latest and most accurate context, re-read the following mandatory files as specified in `CLAUDE.md`:
    - `CLAUDE.md`
    - `.claude/rules/01-project-instructions.md`
    - `.claude/rules/00-private-rules.md` (if it exists)
    - `docs/development_roadmap/project_analysis.md`
3.  **Wait for User Prompt:** After re-reading the instructions, wait for the user to provide the details of the new task.
