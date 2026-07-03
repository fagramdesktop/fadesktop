# Test Loop Protocol (harness-neutral)

The portable core of autonomous, tested implementation. Both `/implement` (Claude Code)
and `$implement` (Codex) read and follow THIS file verbatim for the testing phase, so the
impl⇄test loop behaves identically across harnesses. The harness-specific wrappers own
project setup, task splitting, and the spawn/wait mechanics; this file owns everything from
"a single task's implementation is committed" onward.

## Vocabulary

- **task-runner** — the per-task agent (one spawn per task). Owns the loop below. Its context
  is disposable: only its compact final summary propagates up to the orchestrator.
- **impl agent / impl-fix agent** — sub-agents the task-runner spawns to write or fix the
  implementation. They never write test code.
- **test-author agent** — sub-agent that writes the ad-hoc test overlay and builds.
- **overlay** — the throwaway `#ifdef _DEBUG` test code for the current task. Never part of an
  implementation commit. Lives as a patch under the task folder between rounds.
- **golden tdata** — a read-only backup of the authed test account. Tests only ever copy FROM
  it; they never write to it.

## Inputs the wrapper passes in

- `TASK_DIR` — `.ai/<project>/<letter>/` for this task.
- `TASK_ID` — stable id used in commit trailers (e.g. the project + letter).
- **TASK SPEC** — the task's full description block (from `implementing.md`) and its referenced
  images (`images/<file>` design mockups / screenshots / graphic resources for this isolated task).
  This is half of what the tests are designed against (the diff is the other half); the design READS
  the images — they show what the result should look like.
- Config: `BUILD` (build command), `EXE` (built binary path), `MAX_ATTEMPTS` (default 4). The test
  account lives in `out/Debug/` as the portable-data folders described under "Test account" below;
  the wrapper has already confirmed the golden one exists (launch gate). All paths are relative to
  the current checkout — no worktrees are created; the run happens in whatever repository slot it
  was launched from.

## State machine (run by the task-runner)

Precondition: the implementation for this task is committed in the current checkout (impl agents
commit; they do not stash). Record that commit's SHA as **IMPL_SHA** — the reset after each test run
returns the checkout to exactly it. The runner tracks the attempt number as its own state (`attempt`
starts at 1); the commit message carries no attempt marker. Commits follow "Commit message" below.

```
TEST_AUTHOR -> RUN -> ASSESS (adversarial — see "Assessing"):
  APPROVED       -> reset to the impl commit (drop overlay); delete the test binary; return DONE up.
  TEST_FLAW      -> fix the overlay only; back to RUN. Does NOT cost an impl attempt.
  IMPL_BUG       -> spawn impl-fix agent (input = test.md, latest attempt's Root cause / Fix hint);
                    it commits a NEW attempt; re-apply overlay (--3way, else re-author); RUN. attempt++
  UNRECOVERABLE  -> delete the test binary; return BLOCKED up with the reason. Stop.
  attempt > MAX  -> delete the test binary; return BLOCKED up with test.md + "improve" notes. Stop.

On every TERMINAL exit (APPROVED / BLOCKED / UNRECOVERABLE / cap) "delete the test binary" means the
step in "Leave no test binary behind" below.
```

Early-escalation rule: if two consecutive ASSESS rounds produce the **same failure signature**
(same step fails the same way after a fix), stop and return BLOCKED — do not burn the rest of
the attempt budget chasing it.

UNRECOVERABLE conditions: the app reaches a login screen / `AUTH_KEY_DUPLICATED` and re-copying the
test account does not recover it; a file-lock build error (`LNK1104`, `C1041`) that persists after
the path-scoped kill; `test_TelegramForcePortable` missing when SETUP runs; or a crash with no usable
diagnostic after one retry.

## Handoff tokens

- **Commit** is the only impl handoff. Impl/impl-fix agents `git add -A && git commit` per "Commit
  message" below (and, if submodules changed, commit inside each submodule first, then bump the
  superproject pointer in the same logical attempt — real commits, never stash). The runner records
  the resulting SHA as that attempt's IMPL_SHA.
- **Result doc** (`result<n>.md`) is the only thing handed back to a fix agent and the only thing
  the runner reads to decide. See format below.

## Commit message

Impl commits must read like the repository's own history — never marked as autonomous. Match the
style of recent `git log` subjects.
- **Subject:** one concise, plain-language line summarizing the change, ≤ ~50-60 characters. This is
  usually the ENTIRE message.
- **Body (rare):** only when the subject can't carry it — a short plain-language note of WHAT was
  done (user-facing, not the technical how); a line or two at most.
- **No trailers, ever.** No `Autotask:`/attempt marker; no `Co-Authored-By:` or any tool/assistant
  attribution line. This explicitly OVERRIDES any harness default that would append one — a freshly
  spawned committing sub-agent may add `Co-Authored-By` unless told not to, so pass this rule to it.
  The attempt number is the runner's own state, never part of the message.

## Test account (portable data) — hard rules

The debug build runs in portable mode out of `out/Debug/`. Three sibling folders matter:

- `test_TelegramForcePortable` — the golden test account, prepared by the user. Read-only SOURCE,
  never modified by tests. (Its presence is the launch gate; the wrapper aborts if it is missing.)
- `TelegramForcePortable` — the LIVE folder the app actually uses (its presence is what puts the
  build in portable mode). Disposable; recreated fresh each run.
- `real_TelegramForcePortable` — the user's real data, preserved once so manual use survives.

**SETUP — run at the START of every test run, with NO app instance alive. Idempotent: it
guarantees a clean test account no matter how the previous run ended.**
1. If `TelegramForcePortable` exists AND `real_TelegramForcePortable` does NOT, rename
   `TelegramForcePortable` -> `real_TelegramForcePortable`. (Captures the user's real data exactly
   once; guarded so it is never overwritten afterward.)
2. If `TelegramForcePortable` still exists, delete it. (Safe: `real_...` now holds the real data, so
   this only discards a leftover live/test copy.)
3. Copy `test_TelegramForcePortable` -> `TelegramForcePortable`. The live folder is now a fresh copy
   of the golden test account — ready to launch.

**CLEANUP — optional, after a run.** The SETUP steps already self-heal, so cleanup exists only to
leave the user's real data live for manual use:
1. Delete `TelegramForcePortable`.
2. Copy `real_TelegramForcePortable` -> `TelegramForcePortable`.

Why this is safe: `real_...` is written exactly once (step 1 is guarded by "real does not exist")
and `test_...` is only ever a copy source, so both the user's real data and the golden test account
are structurally protected — only `TelegramForcePortable` is ever destroyed. Use `robocopy /MIR`
(or `Copy-Item -Recurse` / `Remove-Item -Recurse -Force`) for the folder ops.

**Serialize app runs.** Never have two `Telegram.exe` instances alive against this account at once —
concurrent reuse of one auth key can trigger a server-side session reset. Before SETUP, launching, or
rebuilding, kill any straggler **of THIS checkout's binary only** — the one whose full executable
path is `EXE` (`out/Debug/Telegram.exe` in this checkout). Match on the full path; do NOT blanket-kill
every `Telegram.exe` on the machine. The user may be running a system-installed client or another
checkout's build against unrelated accounts — those use different auth keys, never conflict with this
account, and MUST be left alive. On Windows, scope the kill by path:

    $exe = (Resolve-Path "$EXE").Path
    Get-CimInstance Win32_Process -Filter "Name = 'Telegram.exe'" |
      Where-Object { $_.ExecutablePath -eq $exe } |
      ForEach-Object { Stop-Process -Id $_.ProcessId -Force }

`taskkill /IM Telegram.exe /F` is forbidden here and anywhere else in this loop — it is image-name-wide
and takes down the user's unrelated clients. Every "kill stragglers" / "taskkill" step below means
this path-scoped kill.

**Avoid destructive calls.** The overlay must never trigger logout / session-termination /
account-deletion. Tests that genuinely need those use a separate burner account, not this one. (If a
permanent destructive-call fuse is later added to the debug build, this is enforced in code; until
then it is the test-author's responsibility.)

## Design the tests from THIS task (the crux)

The single most important rule: **tests are derived from what THIS task changed — not from generic
project navigation, and not reused from a previous task.** Different change → different checks. If
two tasks produce the same screenshots and the same assertions, the second test is a no-op. Before
writing any overlay:

1. **Read both sides of the task.** (a) The TASK SPEC — the task's full description block from
   `implementing.md` and its referenced design-mockup images (`images/<file>`); READ the images,
   they are the source of truth for what the result should look like. (b) The change under test —
   `git show <IMPL_SHA>` (the actual diff) and `<TASK_DIR>/plan.md`. List every concrete thing the
   diff changed and every surface the task (description + "Observable result") says it affects.
2. **Turn each into a falsifiable check with an ORACLE** — something that can come out FAIL. A check
   with no way to fail is not a test. By change type:
   - **String / text** → assert the EXACT expected text is present at runtime (dump the label/widget
     text to the log and compare) AND the old text is gone. Not "the screen opened".
   - **Visual / asset (icon, image, color, layout)** → the rendered target must MATCH the intended
     new artwork and DIFFER from the old. Both are available as files (intended art under
     `.ai/<project>/...`; the committed new file is `git show <IMPL_SHA>:<path>`; the old is
     `git show <IMPL_SHA>^:<path>`). Render those references to PNG and compare the tight crop
     against both. **If the rendered target matches the OLD art — or you cannot tell them apart —
     that is a FAIL, not a pass.** (This is the check that catches a change that never took effect,
     e.g. an asset that wasn't rebuilt into the binary.) For a task the wrapper marked
     `Visual: layout`, matching-the-art is necessary but NOT sufficient — also verify the numeric
     design contract in `<TASK_DIR>/visual.md` (sizes, spacings, alignment); see "Visual contract".
   - **Behavior** → drive the specific action and observe the concrete state/log/screenshot the
     change should produce, and confirm the pre-change behavior no longer happens.
3. **Cover every surface the task names.** If the Observable result lists a settings row, a balance
   header, a gift field, and a suggestion bar, each must be observed (or explicitly marked N/A with
   a reason). Do not stop at one or two.
4. **Write the checks into `<TASK_DIR>/test.md` BEFORE running** (format under "Test report"), so the
   design is explicit and Actual/Result can be filled in per check afterward.

## Visual contract (layout tasks)

When the wrapper marks a task `Visual: layout`, "looks right" is not a vibe — it is a small
computation, and the test MEASURES it. The wrapper's design-spec phase writes the contract to
`<TASK_DIR>/visual.md`; impl builds to it; this loop verifies it. (Tasks marked `Visual: appearance`
or unmarked use the ordinary visual/asset check above — this section does not apply.)

A mockup (usually mobile) gives RELATIONSHIPS, never pixels. The contract re-expresses those
relationships in desktop units by anchoring every quantity to a font metric or an existing tdesktop
`.style` token — so it auto-adjusts to desktop and reuses real components. The strongest anchor is an
existing widget: "the count badge IS the dialogs-list unread badge" pins font + height + padding to
`st::dialogsUnread*` and is self-correcting — far better than "a blue circle ~24px".

Write it as an ORDERED DERIVATION: each step resolves one quantity the next consumes, so impl and
test are both mechanical. Example — a glyph-on-rounded-square icon + title + count, in a bubble:

    Anchor:  T = st::<title>.font->height ;  Badge := the dialogs unread-badge metrics
    1. glyphH = 1.4·T              ±2px   — white glyph box height                    (from T)
    2. square = glyphH ÷ (2/3)    ±2px   — accent rounded-square side ; iconR = square·0.28
    3. margin m (equal on square's top/left/bottom) ; bubbleH = square + 2·m   ±1px
       bubbleR = bubbleH/2 ; iconR : bubbleR must read as in-sync (icon proportionally smaller)
    4. titleY = (bubbleH − T)/2    ±1px   — title vertically centered in the bubble
    5. badge = Badge (font+height+padding) ; vertically centered ; margins top=right=bottom equal ±1px

Then the RELATIONSHIP checks that catch what existence-checks miss — each falsifiable: `square ≤
bubbleH` (no overflow/overlap), the square's three margins equal, the two corner radii in sync, the
badge identical to a real chat-row unread badge. Note every mobile→desktop adjustment and which token
replaced each mobile measurement.

How TEST verifies it (numbers over eyes):
- **Measure, don't admire.** Have the overlay LOG the computed geometry — `font->height` and the
  `QRect` of each piece (glyph, square, bubble, title, badge) — and assert each derivation line
  arithmetically within tolerance. Live-widget geometry is the primary oracle; it deterministically
  catches "icon taller than the bubble", "square overflows", "badge oversized / cramped". Where a
  rect can't be logged, measure it from a tight crop by colour (accent square, badge, bubble outline
  are separable).
- **Same-scale side-by-side.** Build one composite — the mockup's bubble crop and the rendered crop
  scaled to EQUAL element height, side by side — and judge composition on THAT, never on a
  full-window screenshot (a 30px bubble in a 600px window is what rubber-stamps bad proportions).
- **Adversarial designer pass.** One final judgement framed to REJECT: "You are a product designer
  rejecting this PR — list every way these two differ in proportion, spacing, or alignment." Approve
  only if it finds nothing disqualifying. (The approval-framed "does it look OK?" is what passed the
  broken build.)
- **Existence ≠ sufficiency.** "Icon + title + count are all present" is a precondition, not a pass.
  A `Visual: layout` check APPROVES only when the measured geometry satisfies the contract; any line
  out of tolerance is an IMPL_BUG (report measured-vs-target) and loops like any other.

## Overlay mechanics

The overlay is ad-hoc, authored fresh against the CURRENT implementation, injected at the
highest level that still exercises the change (often a direct data-layer call like
`item->applyEdition(...)` rather than a faked MTP response). It must:

- Live entirely inside `#ifdef _DEBUG` blocks.
- Pick a **test strategy** and record it in the spec:
  `live-data` (use real account data) · `live-mutate` (really create an entity — prefer a
  throwaway target, clean up after) · `inject` (build fake local state without the network) ·
  `mock-api` (intercept specific requests, return canned responses — for payments/destructive).
  Prefer `inject` over `live-mutate` to avoid account/server accumulation and flake.
- Drive the scenario on the Qt event loop, preferring **condition-waits over fixed timers**
  (wait until the target widget/data actually exists, with a timeout fallback). Fixed sleeps are
  the main source of screenshot flake.
- Write a flushed log to `<TASK_DIR>/test_log.txt` (open Append|Text, flush after each write) and
  save screenshots to `<TASK_DIR>/screenshots/`. Delete the old log at the first step.
- **Capture the target tightly.** Grab the specific widget / row / glyph (or crop the saved PNG to
  it) so the target is unambiguously in frame at usable resolution. A full-window grab that leaves
  the target clipped, off-screen, or thumbnail-sized is NOT acceptable evidence — if the target
  isn't clearly captured, that is a TEST_FLAW (re-frame), never a pass.
- **Lay down the oracle's reference.** For an asset/visual check, also save the OLD and intended-NEW
  art as PNGs beside the crop (`screenshots/<name>_old.png`, `<name>_new.png`) so the assessment is
  a direct three-way comparison, not a memory test.
- Emit these markers, one per line:
  `TEST_STEP: <desc>` · `TEST_RESULT: PASS: <what>` / `TEST_RESULT: FAIL: <what> - <details>` ·
  `SCREENSHOT: <full path>` · `TEST_COMPLETE` (immediately before quit).
- Prefer asserting on **logged state** (log the actual value, assert on text — deterministic);
  reserve screenshots for genuinely visual checks where an eye is the right judge.
- **Watchdog:** install a `QTimer` at scenario start that force-quits (`Core::Quit()`, and if
  needed `std::abort` after a flush) at a hard wall-clock cap (default 120s). This guarantees the
  app never hangs holding a lock on the exe — independent of the runner's own timeout.
- End every path (success or assertion failure) by logging `TEST_COMPLETE` then `Core::Quit()`.

### Finding widgets in an overlay (CRITICAL — avoids a guaranteed crash)

Telegram's custom widgets (`Ui::InputField`, `Ui::FlatLabel`, `Ui::RpWidget`, boxes, buttons, …)
do **NOT** declare `Q_OBJECT` — they have no own meta-object. So `QObject::findChildren<T*>()` does
**not** filter by type for them: with no distinct meta-object it matches the nearest moc'd base
(`QWidget`), i.e. it returns **every** child widget blindly cast to `T*`. The moment you use one as
`T` (e.g. call `InputField::setFocused()` / `rawTextEdit()` on what is really a `VerticalLayout`) you
get a raw SIGSEGV — the debugger shows `this` with the *wrong* dynamic type. A clean rebuild does NOT
fix it; it is a real bug in the overlay, not a stale build.

- **Never** `findChildren<Ui::SomeCustomWidget*>()`. Instead enumerate `findChildren<QWidget*>()`
  (`QWidget` *is* `Q_OBJECT`, so that call is sound and returns all descendants) and
  `dynamic_cast<Ui::SomeCustomWidget*>()` each, keeping the non-null results — C++ RTTI identifies the
  real type regardless of `Q_OBJECT`. A reusable helper:
  ```cpp
  template <typename T>
  [[nodiscard]] std::vector<T*> FindWidgets(QWidget *root) {
      auto out = std::vector<T*>();
      for (const auto w : root->findChildren<QWidget*>()) {
          if (const auto t = dynamic_cast<T*>(w)) out.push_back(t);
      }
      return out;
  }
  ```
- Only genuine Qt `Q_OBJECT` types (`QWidget`, `QLabel`, `QLineEdit`, …) are safe to pass directly to
  `findChildren<T*>()`.

### Log to an ABSOLUTE path (the launcher chdir's)

The Windows launcher changes the working directory to the exe folder before the app runs, so a
**relative** overlay log path (`<TASK_DIR>/test_log.txt`) silently fails to write (`QFile` won't
create missing parents) — the run looks "clean" but produces no evidence. Resolve `<TASK_DIR>` to an
absolute path up front (e.g. `QDir::current().absoluteFilePath(...)` computed at inject time, or an
absolute path baked into the overlay) so flushes actually land; likewise for screenshots.

### Git mechanics for the overlay (no stash)

- After building, save the overlay as a patch: `git diff > <TASK_DIR>/test-overlay.patch`.
  Then **reset the checkout back to the implementation commit** so it stays impl-only:
  `git reset --hard <IMPL_SHA>` (and `git submodule update --init --recursive` if the overlay
  touched submodules). The overlay never enters an impl commit.
- Next round, re-apply on top of the new implementation: `git apply --3way
  <TASK_DIR>/test-overlay.patch`. This succeeds ~90% of the time when the tail change was small.
- On conflict, **re-author the conflicting hunk from the spec** in `test<n>.md` (which records
  intent: injection point, fake values, assertions) rather than fighting the conflict markers.
  Scenario steps that only call public APIs should live in their own block so they never conflict;
  only true in-situ injections land inside impl files.

## Build & run discipline

- Build with `BUILD`. A single changed TU compiles fast; only the overlay-touched files + link
  rebuild between rounds. On `LNK1104`/`C1041`, run the path-scoped kill (Test account → "Serialize
  app runs"), wait, retry once; if it persists -> UNRECOVERABLE.
- **Codegen does not track resource mtimes.** If the task changed only a resource the style codegen
  consumes (an icon `.svg`, etc.) without touching a `.style`, an incremental build will NOT re-pack
  it and the binary keeps the OLD asset. Before building such a task force regeneration — touch the
  referencing `.style` (or clean the codegen output) — so the change actually ships. A render that
  shows no difference from before is the symptom of skipping this.
- Run: run the SETUP steps (Test account) -> launch `EXE` **with `-testagent`** in the background,
  redirecting BOTH stdout and stderr to `<TASK_DIR>/app_stderr.txt` (see "Crashes & assertions"
  below — this flag is what stops a crash from hanging on a modal dialog, and the redirect is what
  captures the assertion text) -> **start a hard wall-clock deadline (~90s) from launch** -> poll
  `test_log.txt` every ~5s -> on each `SCREENSHOT:` read the image and judge it -> detect
  `TEST_COMPLETE` (success) or process death (crash) or no new output for the watchdog cap, or the
  hard deadline elapsing (hang) -> path-scoped kill of any straggler (Test account → "Serialize app
  runs") -> optional CLEANUP -> save the overlay (`git diff > <TASK_DIR>/test-overlay.patch`) ->
  THEN `git reset --hard <IMPL_SHA>` (back to impl-only — the patch must be saved before this reset).

    On Windows, launch and capture both streams like:

        $exe = (Resolve-Path "$EXE").Path
        Start-Process -FilePath $exe -ArgumentList '-testagent' `
          -RedirectStandardError "$TASK_DIR/app_stderr.txt" `
          -RedirectStandardOutput "$TASK_DIR/app_stdout.txt" -PassThru

### Crashes & assertions (always launch the test binary with `-testagent`)

A Debug build normally turns a failed `std::vector` bounds check, a bad iterator, an `assert()`, a
pure-virtual call, or `abort()` into a modal **Abort / Retry / Ignore** dialog. That dialog blocks
the process forever — the agent sees no `TEST_COMPLETE`, no process death, just a hang until the
watchdog cap, and learns nothing about the cause. **`-testagent` removes those dialogs.** With it
set, the binary:

- suppresses every CRT / STL / WER / `abort()` message box (no button to press, never hangs);
- converts any such assertion into a real crash that the crash reporter records, so the process
  **terminates immediately** instead of waiting;
- writes the assertion text (expression + file:line) to **stderr** — captured in
  `<TASK_DIR>/app_stderr.txt`, tagged `[testagent]`;
- also turns on debug logging (`-testagent` implies `-debug`).

**Do NOT key the crash decision on exit code.** Breakpad handles the crash and the process usually
exits **0** — exactly as tdesktop's own crash detection assumes. The reliable crash signals are: the
process is gone WITHOUT a `TEST_COMPLETE` marker, AND a fresh non-empty
`<workdir>/tdata/working` exists. So **always pass `-testagent`**, and on a crash gather diagnostics
in this order before deciding the verdict:

1. **`<TASK_DIR>/app_stderr.txt`** — the `[testagent] assert: …` line gives the failed expression and
   `file:line` (e.g. `vector(1931) : … vector subscript out of range`). Usually enough to localize.
2. **`<workdir>/tdata/working`** — the crash report the reporter wrote: the `Assertion:` /
   `CrtAssert:` annotations, the failed `file:line`, and `Caught signal …` / minidump id. Plain text;
   read it directly. `<workdir>` is the launch `-workdir` (in portable test runs,
   `out/Debug/TelegramForcePortable/`).
3. **`<workdir>/tdata/dumps/*.dmp`** — the minidump (full stack, needs symbols to read; note its path
   in `test.md`, don't try to symbolize inline).

A crash is an **IMPL_BUG** (the implementation tripped an assertion / dereferenced out of range), not
a TEST_FLAW, unless the overlay itself is what reached out of bounds — quote the `[testagent]` line
and the `tdata/working` excerpt in `test.md` as evidence, and feed the expression + file:line to the
impl-fix agent as the Root cause / Fix hint. Only a crash with NO usable diagnostic after one retry
is UNRECOVERABLE.

### Hangs & freezes (two layers, because they have two causes)

A run that never reaches `TEST_COMPLETE` and never dies is a hang. Two independent guards catch it:

- **Frozen main thread (in-app).** `-testagent` force-enables the built-in **DeadlockDetector** — a
  ping thread that, if the main/event loop stops responding (a genuine deadlock or an infinite loop
  on the UI thread), raises `Unexpected("Deadlock found!")` from a side thread. That crashes through
  the same reporter, so the **frozen main-thread stack is captured in the minidump** and the process
  exits on its own (key on the `tdata/working` report, not the exit code) — same diagnostics path as
  a crash above. No agent action needed beyond reading `tdata/working` / the dump. Detection is
  within ~30–90s of the stall.
- **Everything else (external hard cap).** The DeadlockDetector does NOT fire when the event loop is
  still alive but the test simply never finishes — e.g. a buggy overlay that loops forever, waits on
  a condition that never comes, or just never calls `Core::Quit()`. For that the **runner enforces a
  hard wall-clock deadline (~90s) from launch** and, when it elapses, does the path-scoped kill
  regardless of output. No legitimate auto-test runs anywhere near a minute, so this cap is pure
  backstop — but it is what guarantees the agent can never wedge forever.

Classify by which guard tripped: a DeadlockDetector crash with a real main-thread stack in app code
is an **IMPL_BUG**; the external cap firing is almost always a **TEST_FLAW** (the overlay didn't
drive to `TEST_COMPLETE`/quit) — re-author the overlay — unless the captured stack/log shows the
implementation itself wedged, in which case it is an IMPL_BUG. Two external-cap kills in a row with
the same signature → BLOCKED (early-escalation rule).

### Leave no test binary behind

The on-disk `EXE` (`out/Debug/Telegram.exe`) always contains the compiled overlay after a test run —
`git reset --hard` only reverts the source, not the built binary. So when the loop reaches a TERMINAL
verdict (APPROVED, BLOCKED, UNRECOVERABLE, or attempt cap), after the final path-scoped kill and
`git reset --hard <IMPL_SHA>`, **delete the built `EXE`** so no overlay-laden test binary is left for
the user to launch by mistake:

    Remove-Item -Force "$EXE"

A clean, feature-ready binary is one `BUILD` away on demand. (Delete only on terminal exit — between
attempts the next round rebuilds the overlay, so the binary is reused there.)

## Assessing (adversarial)

ASSESS decides APPROVED / TEST_FLAW / IMPL_BUG. Default to **not approved**; a check passes only on
positive, specific evidence — in the captured pixels or the log — that the change is present AND
correct.

- **No pass by inference.** "Same asset so it's fine", "probably", "looks like" are not evidence.
  Missing, clipped, or ambiguous evidence for a check → **TEST_FLAW**: re-frame/re-capture and run
  again. Never turn missing evidence into a PASS.
- **Judge the actual artifact.** State what is literally visible in the crop / present in the log,
  then compare to the oracle reference (`_old.png` vs `_new.png`). Do not narrate expectations.
- **Judge visually, never by hash.** Do NOT pixel-diff or hash images. Desktop renders differ from
  the mobile (iOS/Android) design mockups by platform, DPI, theme and antialiasing — the mockups
  convey the intended look, they are NOT pixel targets, so never fail a check merely for not matching
  a mockup pixel-for-pixel. The falsifiable signal is the on-screen crop against the OLD vs
  intended-NEW render (does it match the new and differ from the old?); the mockup informs what
  "correct" means. Read the images and decide like a designer reviewing the build. This bans
  pixel-diffing against the MOCKUP — not measuring your OWN render: for a `Visual: layout` task you DO
  assert the rendered widget's measured geometry against the desktop-unit contract in `visual.md`
  ("Visual contract"). That is numeric and falsifiable, and it is exactly the check that catches the
  wrong proportions/spacings an eye waves through.
- **No-difference = IMPL_BUG.** If a check detects no difference from the pre-change state (the glyph
  matches the OLD art; the string still shows the old word), the change did not take effect — return
  IMPL_BUG; do not approve.
- **A visual check with no baseline/target comparison cannot APPROVE** — with no oracle you have
  tested nothing.
- APPROVED requires every derived check to PASS with evidence; else IMPL_BUG (real defect) or
  TEST_FLAW (the test was wrong, not the code).

## Test report (`<TASK_DIR>/test.md`) — human-readable, append per attempt

The file the human opens to see how testing went. The test-author writes the checks (Expected /
Oracle / Observed via) BEFORE running; ASSESS fills Actual / Result and the verdict. Append a new
`## Attempt` section each round — never overwrite prior attempts.

```
# Test report — <project>/<letter>: <title>

## Attempt <n> — commit <sha> — strategy <...> — verdict: <APPROVED|TEST_FLAW|IMPL_BUG|UNRECOVERABLE>

### Test 1 — <aspect of THIS change>
- Expected: <observable effect the change should produce>
- Oracle: <what would make this check FAIL>
- Observed via: <surface + how captured: tight crop of widget X; refs _old/_new>
- Actual: <what is literally visible / logged>
- Screenshots: screenshots/<after>.png (refs: _old.png, _new.png)
- Result: PASS | FAIL

### Test 2 — ...

### Verdict reasoning
<1-3 lines tying the checks to the verdict>
### Root cause / Fix hint    (only if IMPL_BUG — the impl-fix agent reads this)
### Failure signature         (one line, for early-escalation comparison)
```

## Compact summary the task-runner returns up

```
TASK: <TASK_ID>
STATUS: <DONE|BLOCKED>
VERDICT: <APPROVED|reason if blocked>
ATTEMPTS: <n>
TOUCHED: <repo paths or none>
DISCOVERED: <new follow-up tasks to append to implementing.md, or none>
NOTES: <one or two lines, or none>
```

Detailed reasoning stays in `.ai/` artifacts. The chat reply is only this block.
