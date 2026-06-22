---
name: implement
description: Autonomously implement a request on this repository (Telegram Desktop). Accept an inline description or a task-list file, normalize it into a project with a testability-split task list, then drive each task to test-approval through an isolated per-task runner subagent that does context, planning, implementation, build, a single review pass, and an in-app test loop. Use when the user wants one prompt — ideally under Goal run mode — to carry work all the way to a tested, approved state with persistent .ai/ artifacts and a thin parent thread. Reuses task-think's phase prompts and the shared test-loop protocol; prefers spawn_agent/wait_agent and keeps the main thread lean.
---

# Implement Pipeline

The tested superset of `task-think`. The parent thread stays lean: it holds only the task list and
one compact summary per task. Each task is driven to test-approval by an isolated **task-runner**
subagent that spawns its own phase subagents.

This skill does not re-specify the implementation phases or the test loop. It reuses:
- `.agents/skills/task-think/PROMPTS.md` — Phase 0-6 prompt templates and the Codex execution-mode
  / wait-ladder / progress-heartbeat / compact-reply rules.
- `.agents/shared/test-loop.md` — the harness-neutral impl⇄test loop (state machine, commit
  handoff, overlay mechanics, `--3way`/re-author, test-account swap, watchdog, escalation).

Read both before orchestrating.

## Inputs

`$ARGUMENTS` = ONE of: an inline task description; a path to a task-list file (e.g.
`.ai/communities/tasks.txt`); or an existing project name to resume, optionally followed by extra
work. May reference attached screenshots.

## Config

Runs in the **current checkout** — wherever the skill is invoked. No worktrees are created; paths
below are relative to that repository root.

```
BUILD         = cmake --build ./out --config Debug --target Telegram
EXE           = ./out/Debug/Telegram.exe   (or ./out/Debug/Telegram on WSL/Linux — verify the tree)
TEST_ACCOUNT  = ./out/Debug/test_TelegramForcePortable   (user-prepared golden; launch gate aborts if absent)
MAX_ATTEMPTS  = 4
```

Tasks run **sequentially** in this one checkout. App runs must serialize (one client per account).
To parallelize, run the skill in a different checkout/slot (e.g. `C:\Telegram\tdesktop`,
`D:\Telegram\tdesktop`, `D:\Telegram\twin`); give parallel slots separate test accounts so two test
runs never share one auth key.

## Artifacts (per project)

- `.ai/<project>/implementing.md` — the canonical, final, testability-split task list (descriptions
  + status); the main thread is its only writer.
- `.ai/<project>/images/` — illustrations referenced by tasks.
- `.ai/<project>/<letter>/` — per-task artifacts (context, plan, review, test, result, overlay, logs).
- `.ai/<project>/about.md` — project blueprint (task-think convention).

## Done (for Goal run mode)

Done when every task in `implementing.md` has `Status: approved` or `Status: blocked: <reason>`.
Resumable: re-invoking with the project name reads `implementing.md` and continues from the first
unfinished task.

## Phase A: Setup & input resolution (main thread)

1. Record `$START_TIME`.
2. **Test-account gate (hard precondition — before any work).** If
   `out/Debug/test_TelegramForcePortable` does not exist, STOP the entire skill immediately and tell
   the user to prepare it (a portable-data folder authed to a throwaway test account); autonomous
   testing is impossible without it.
3. **Resolve `$ARGUMENTS` into (project, SOURCE, mode) — without reading task files or images**
   (the main thread never loads task prose or assets; resolving needs only paths and existence
   checks). SOURCE ends as EITHER inline text OR a confirmed file path that the planner will read.
   - **File input** — first token is a path: confirm it exists (do NOT read it), SOURCE = that path;
     if under `.ai/<name>/`, project = `<name>`, else derive a short kebab name from the filename;
     mode = **extend** if that project already has `implementing.md`, else new.
   - **Existing project** — else if `.ai/<FIRST_TOKEN>/` exists: project = `FIRST_TOKEN`; empty
     remainder AND `implementing.md` present → mode = **resume**; non-empty remainder → mode =
     **extend**, SOURCE = the remainder, OR — if the remainder is itself a path to an existing file —
     SOURCE = that path (confirm it exists, do NOT read it).
   - **New inline** — else SOURCE = all of `$ARGUMENTS`; pick a unique short kebab-case name.
   After this step you have a project name and a SOURCE (inline text or a confirmed path), having read
   neither the file nor any image.
4. Create `.ai/<project>/` and `.ai/<project>/images/` if new.
5. Images must be on disk: the planner reads them as files and subagents cannot see chat
   attachments, so each image a task needs must exist as a file (referenced by the SOURCE file or
   under `.ai/<project>/images/`). If the user only pasted an image into the chat, get it onto disk —
   ask them to drop it into `.ai/<project>/images/` as a file — or, as a lossy fallback, write a
   textual description for the planner. Don't claim to have saved a paste you can't. Images the
   SOURCE file *references by path* are the planner's job, not yours.
6. If mode = **resume**, skip Phase B and go to Phase C.

## Phase B: Planning & testability split (delegate)

Spawn a worker subagent (`fork_context: false`). Give it the SOURCE — EITHER the inline request OR a
path to a task-list file (the main thread has NOT read it): tell the worker to READ the file itself
if SOURCE is a path, READ every image the source references (resolve relative to the SOURCE file's
dir), and COPY each into `.ai/<project>/images/` with a descriptive kebab-case name (the main thread
did not read or move them). Then read AGENTS.md, gauge scope, and produce the FINAL ordered task list
where EVERY task satisfies both constraints:
- **Implementable in one pass** — a single agent with a ~200k-token budget can implement it fully
  without context compaction (a bounded change across a handful of files, not a sweep across dozens);
  split anything bigger.
- **Independently testable** — each task yields an observable behavior the test agent can drive from
  an in-app debug overlay and verify via log/screenshot; split on testable seams.
Minimal number of tasks subject to both; preserve dependency order. If the SOURCE is already a list
(inline or in the file the worker read), respect its breakdown and refine only as needed (split
too-big/untestable entries; optionally merge
trivially tiny adjacent ones if still one testable unit). Distribute the provided images across the tasks they pertain to: every
task that changes UI / visual / asset behavior MUST cite the specific mockups/resources it must match
via its `Images:` line (caption = what to match), and leave no provided image referenced by no task.
These per-task references are the oracle the test phase verifies against — be specific and per-task,
not one shared dump.

Write `.ai/<project>/implementing.md`:

```
# Implementing: <project>

## Goal
<one-line overall goal>

## Tasks

### a: <imperative title>
Status: todo
<2-4 line self-contained description: what to implement and the observable, testable result>
Images: images/<file> — <caption>      (only if the task uses an image)

### b: <imperative title>
Status: todo
<...>
```

For **extend** mode, APPEND new lettered tasks after the existing ones, leaving prior entries and
statuses untouched. The worker replies with ONLY a compact confirmation (`ready — <N> tasks`, or
extend: `ready — appended <letters>`), never echoing the task list or image contents. Then the main
thread reads `implementing.md` back ONCE (its first and only load of the task prose; it never reads
the images).

## Phase C: Per-task loop (main thread orchestrates)

For each task whose `Status` is not `approved`/`blocked`, in order:

1. Set `Status: in-progress`. Spawn ONE **task-runner** worker (`fork_context: false`, request
   `model: gpt-5.4`, `reasoning_effort: xhigh` when supported) with the prompt below. Apply
   task-think's wait ladder (5-min waits while in progress, 1-2 min near completion; inspect the
   task's progress/result artifacts on timeout; one follow-up then one fresh retry before escalating).
2. Read only its compact reply block. Detail is in `.ai/`.
3. Update the task's `Status:` — `approved` (STATUS DONE) or `blocked: <reason>`.
4. Append any `DISCOVERED` tasks as new lettered `### <letter>:` blocks (`Status: todo`) after the
   remaining ones. The main thread is the only writer of `implementing.md`.
5. On BLOCKED, stop and report — do not start the next task.

### task-runner prompt

```
You are a task-runner for ONE task in an autonomous implement-and-test workflow on Telegram
Desktop (C++ / Qt). You own this task end to end and MUST use subagents (spawn_agent/wait_agent)
for each phase, keeping the parent thread lean.

PROJECT: <project>   TASK: <letter> — <title>
TASK DESCRIPTION:
<the task's full description block from implementing.md>
IMAGES: <referenced .ai/<project>/images/* paths, or none>
TASK_DIR: .ai/<project>/<letter>/   TASK_ID: <project>-<letter>
Config (paths relative to this checkout): BUILD/EXE/MAX_ATTEMPTS = <values>. Test account = the
out/Debug/ portable-data folders (see test-loop.md "Test account").

Read first: AGENTS.md; REVIEW.md; `.agents/skills/task-think/PROMPTS.md` (Phase 1-6 templates +
execution rules); `.agents/shared/test-loop.md` (testing). Read any IMAGES listed above. For a
follow-up letter, also read `about.md` and the previous letter's `context.md`.
Create `<TASK_DIR>/` and `<TASK_DIR>/logs/`.

Pipeline for THIS task only, one fresh subagent per phase, writing prompt/progress/result logs per
task-think:
1. CONTEXT  — task-think Phase 1 (or 1F) -> context.md (+ about.md).
2. PLAN     — Phase 2 -> plan.md.
3. ASSESS   — Phase 3.
4. IMPLEMENT— Phase 4, one unit per plan phase. Do not commit in these units.
5. BUILD    — Phase 5 (prefer same-thread build; fix errors; file-lock -> stop).
6. REVIEW   — Phase 6 but a SINGLE pass (one 6a, one 6b if NEEDS_CHANGES, rebuild).
7. COMMIT   — git commit with a concise plain-language subject (≤ ~50-60 chars, matching recent
   `git log` style; usually the whole message — short plain body only if needed). NO `Autotask:`/
   attempt trailer and NO `Co-Authored-By:`/attribution line (overrides the default; see test-loop.md
   "Commit message"). Submodules first, then superproject pointer. Record the commit SHA as IMPL_SHA
   (track the attempt number yourself).
8. TEST     — run `.agents/shared/test-loop.md` to APPROVED / BLOCKED / attempt cap. Spawn a
   test-author subagent and feed it BOTH sides per test-loop.md "Design the tests from THIS task":
   (1) the TASK SPEC — this task's full description block above PLUS its referenced IMAGES (have it
   READ the mockups; they show the intended result), and (2) the implementation — `git show
   <IMPL_SHA>` + touched files. It designs a falsifiable oracle per change and writes the plan into
   `<TASK_DIR>/test.md` BEFORE running (visual/asset changes compare the tight crop vs old vs
   intended-new art — judged VISUALLY, never by hash/byte; mobile mockups are not pixel targets),
   covers every surface the task names, and never reuses another task's navigate+screenshot. Drive
   RUN/ASSESS adversarially (no pass-by-inference; missing evidence = TEST_FLAW;
   no-difference-from-before = IMPL_BUG) and keep the human-readable `<TASK_DIR>/test.md` report. Spawn an impl-fix subagent on IMPL_BUG (commits the
   next attempt → new IMPL_SHA). After each run, save the overlay patch and `git reset --hard
   <IMPL_SHA>`. Run the test-account SETUP before each launch; honor every test-account hard rule.

Skip TEST only for docs/config-only tasks (say so). On Windows, after approval run task-think
Phase 7 (CRLF / no-BOM) on the task's touched source/config files.

Reply with only the compact summary block from test-loop.md
(TASK/STATUS/VERDICT/ATTEMPTS/TOUCHED/DISCOVERED/NOTES).
```

## Completion

Per-task summary (approved/blocked, attempts, touched files, key evidence), discovered tasks added,
the project name for follow-ups, total elapsed time, and a note that overlays are saved as
`<TASK_DIR>/test-overlay.patch` with the checkout reset to each task's implementation commit.

## Error handling

- Follow task-think's retry ladder for stuck phases; a task-runner returning BLOCKED stops the loop.
- Never push past a file-lock build error.
- The launch gate (Phase A) guarantees the test account exists before any work begins.
- Keep `.ai/` artifacts and edited text files LF/no-BOM on WSL; run CRLF normalization only on a
  native Windows checkout.

## User invocation

`Use local implement skill: <request or path to a task file>` — ideally under Goal run mode so it
loops to a tested state. Resume/extend: `Use local implement skill: <project> [additional change]`.
