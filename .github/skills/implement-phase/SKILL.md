---
name: implement-phase
description: >
  Implements the next phase of a game prototype. Reads plan.md to find the next
  unimplemented phase, opens its detail file, then follows a consistent workflow:
  create todos, implement code, build, verify, elicit human feedback at the GATE,
  record results, and commit. Use this skill when the user says "implement next phase",
  "start next phase", "continue implementation", "/implement-phase", or similar.
  Do NOT use for planning — only for executing an existing phase file.
---

# Implement Phase Skill

Execute a single phase of a game prototype implementation. Each phase file (in `plan/`) contains tasks, verification
checklist, and GATE for human feedback.

---

## Prerequisites

- `plan.md` exists with Implementation Progress table
- At least one phase file exists in `plan/`
- Each phase file has: `Depends on`, `Tasks`, `Verification Checklist`, `## 🛑 GATE`, `Git Checkpoint`

If any prerequisite is missing, stop and inform user.

---

## Step 1: Discover Next Phase

1. Read `plan.md` Implementation Progress table
2. Find first row with Status `🔄 In Progress` or `⬜ Not Started`
3. If all rows are `✅ Complete`, inform user the prototype is done and stop
4. Open the linked phase file (e.g., `plan/phase-name.md`)
5. If file doesn't exist, stop and inform user (broken link)

---

## Step 2: Validate Dependencies

Read the phase file's `Depends on` field. For each listed dependency, verify it's `✅ Complete` in plan.md. If not, stop
and inform user which phase must finish first.

If resuming (status was `🔄`), read the phase file's `Feedback` section and `plan.md` Cross-Session Handoff for context.

---

## Step 3: Initialize Todos

Update both files:

1. Phase file: change Status → `🔄 In Progress`
2. `plan.md`: change row Status → `🔄 In Progress`
3. `plan.md`: update Cross-Session Handoff with current session

Parse phase file's `## Tasks` section. For each task:

- Create a SQL todo with id, title, description, status='pending'
- Link any task dependencies via todo_deps
- If resuming, mark tasks as 'done' only if explicitly confirmed in Feedback section

---

## Step 4: Implement Each Task

For each pending todo (in dependency order):

1. Mark todo status → `in_progress`
2. Read task details from phase file
3. Implement the code:
  - Use `flecs-systems` skill for ECS systems, components, or singletons
  - Keep code consistent with project patterns (see AGENTS.md)
4. Build (fix any errors before next task)
5. Mark todo status → `done`

---

## Step 5: Build & Verify

1. Clean build using desktop and wasm presets
  - If build fails: read error, fix code, rebuild
  - If stuck after 2-3 attempts: stop and ask user for help
2. Walk through `## Verification Checklist` items — verify code implements each one (manually; you can't run interactive
   tests)
3. Read `## 🛑 GATE` section from phase file (must exist; if missing, stop and inform user)
4. Use `ask_user` with schema:
  - Boolean for each GATE question
  - Boolean "Any blocking issues?" (true = must fix now, false = note in feedback)
  - Free-text "Additional feedback"
5. **Wait for human response**

If blocking issues reported:

- Implement the fix
- Rebuild and verify
- Call ask_user again for re-verification

Record all feedback in phase file `## Feedback` section:

- Date, summary of issues, deferred items, tuning changes
- For deferred issues: create SQL todo with title "DEFER: <issue>" (don't implement now)

---

## Step 6: Git Commit

Read phase file's `## Git Checkpoint` section for:

- List of files to stage
- Commit message

Execute:

1. Stage only listed files: `git add <specific-file>` for each
2. Verify: `git diff --cached --stat`
3. Commit with message + Co-authored-by trailer (see AGENTS.md Git section)

---

## Step 7: Mark Phase Complete

1. Phase file: change Status → `✅ Complete`
2. `plan.md`: change row Status → `✅ Complete`
3. `plan.md`: update Cross-Session Handoff:
  - Last session date
  - What was accomplished
  - Next action (next phase name, or "Prototype complete")
4. Inform user of next step
