# Embedded prep — daily generator prompt

This is the prompt invoked by the daily scheduled task. Edit here, not at the cron level. After editing, the next run picks up the new text.

---

**Objective:** Generate today's embedded software interview practice problem in Jacob's daily prep folder, following the canonical structure validated on 2026-04-27.

**Working directory:** `/Users/jkucz/personal/embedded-prep/`

**Canonical example to mirror:** `2026-04-27-mon-bit-manipulation/`. When in doubt about format (README structure, hints tone, test harness style, Makefile contents), match that folder exactly.

## Steps

1. Determine today's date as `YYYY-MM-DD` and day of week (Mon..Sun) in the user's local time.

2. **Drop yesterday's reference.** If a file exists at `.pending-references/<yesterday-date>-reference.md`, locate yesterday's problem folder by globbing `<yesterday-date>-*` and move the reference into it as `reference.md`. If no folder exists for yesterday (e.g., yesterday was Sunday), leave the reference in `.pending-references/`.

3. **If today is Sunday:** stop after step 2 (after dropping yesterday's reference and running `./.generator/publish.sh`). Append a row to `index.md` of the form `| <today-date> | Sun | Rest | — |`, then publish. No new problem on Sundays.

4. Read the last ~30 rows of `index.md` to see which topics and scenarios have run recently. **Avoid repeating any specific scenario from the last 4 weeks.**

5. Pick today's topic per the rotation:

   | Day | Topic | Notes |
   |-----|-------|-------|
   | Mon | Bit manipulation, low-level C | registers, packed structs, alignment, endianness, popcount, parsing |
   | Tue | Concurrency, RTOS, ISR safety | mutexes, semaphores, race conditions, priority inversion, ISR-vs-thread, atomics. Use a tiny mock scheduler/lock API so code runs deterministically under `make test` on a Mac. |
   | Wed | DS&A under embedded constraints | ring buffers, no-malloc allocators, fixed-size pools, state machines, lookup tables |
   | Thu | Peripheral protocols & driver design | I2C/SPI/UART frame parsing, register-driven state machines, DMA descriptors. Use a mock peripheral layer. |
   | Fri | Debugging scenarios + small system design | given symptoms X/Y/Z, what's wrong; design a watchdog/event-loop/bootloader stage. **First Friday of each month: "cold open" — pseudocode only, no compiler.** |
   | Sat | Mixed harder problem, 45–60 min budget | combine two topics from above |

6. **Generate the problem folder** at `<today-date>-<dow-3letter>-<topic-slug>/` (e.g., `2026-04-28-tue-rtos-mutex-priority`). The folder must contain:

   ```
   README.md       problem statement, scenario, layout/protocol details, 4-6 task list, constraints, build/test instructions
   src/            starter .c/.h files with TODO markers and (void)-cast stubs that return 0/false
   tests.c         assert-style harness using the CHECK / CHECK_EQ_U macros below
   Makefile        canonical hardened version (below)
   hints.md        exactly 3 progressive hints, "read one at a time" header
   ```

   ### Canonical Makefile (paste verbatim, edit only the SRCS/HEADERS file names)

   ```makefile
   CC      = gcc
   CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -O0 -g
   SRCS    = src/<name>.c tests.c
   HEADERS = src/<name>.h
   TARGET  = run_tests

   # Disable Make's built-in implicit rules so `make tests` (typo) can't
   # bypass the build by compiling tests.c alone.
   MAKEFLAGS += --no-builtin-rules
   .SUFFIXES:

   .PHONY: test tests clean

   test tests: $(TARGET)
   	@./$(TARGET)

   $(TARGET): $(SRCS) $(HEADERS)
   	$(CC) $(CFLAGS) -I. -o $@ $(SRCS)

   clean:
   	rm -f $(TARGET) tests
   ```

   ### Test harness macros (use verbatim)

   ```c
   static int tests_run = 0;
   static int tests_failed = 0;

   #define CHECK(expr) do {                                               \
       tests_run++;                                                       \
       if (!(expr)) {                                                     \
           tests_failed++;                                                \
           fprintf(stderr, "FAIL  line %d: %s\n", __LINE__, #expr);       \
       }                                                                  \
   } while (0)

   #define CHECK_EQ_U(actual, expected) do {                              \
       tests_run++;                                                       \
       unsigned long _a = (unsigned long)(actual);                        \
       unsigned long _e = (unsigned long)(expected);                      \
       if (_a != _e) {                                                    \
           tests_failed++;                                                \
           fprintf(stderr,                                                \
                   "FAIL  line %d: %s == %s  (got 0x%lx, expected 0x%lx)\n", \
                   __LINE__, #actual, #expected, _a, _e);                 \
       }                                                                  \
   } while (0)
   ```

   `main()` calls each test function then prints `N/M tests passed` and returns non-zero if any fail.

   ### Code conventions

   - Pure C99. No `malloc`, no stdlib beyond `<stdint.h>`, `<stdbool.h>`, `<stdio.h>`, `<assert.h>`.
   - Exact-width types (`uint8_t`, `uint16_t`, `uint32_t`).
   - Code must be correct on a 16-bit MCU — no assumptions that `int` is 32-bit. Promote masks to wider unsigned types when computing widths near the destination type's width.
   - For ISR-safety problems, mark anything an ISR touches `volatile` and check critical sections.

   ### Difficulty target

   SWE I / new-grad / Associate level. ~5 functions for weekday warmups (20-min budget); more functions or a longer scenario for Saturday (45–60 min).

7. **Self-check gate** — both must pass or do not deliver:

   a. From inside the new problem folder, run `make clean && make test` against the starter. The build must succeed with **zero warnings** under `-Wall -Wextra -Wpedantic`. Tests **should** fail (stubs return 0/false). Record how many fail.

   b. Synthesize a reference solution. Write it to a temp path, then `cp` it over `src/<name>.c`. Run `make clean && make test`. The build must succeed with **zero warnings**. **All** tests must pass (`exit 0`).

   c. Restore the starter src from the original.

   **If either gate fails:** do NOT deliver. Write `CHECK_FAILED.md` at the top of the folder explaining what failed (compile error, warning, test that should have failed but passed, test that should have passed but failed). Do not append to `index.md`. Exit non-zero so the failure is visible.

8. **Stash tomorrow's reference drop.** Save the reference solution as `.pending-references/<today-date>-reference.md`. Format it as a markdown document with three sections: (i) the canonical `src/<name>.c` body in a fenced code block, (ii) "What an interviewer is looking for" — bullet points calling out the embedded idioms (build the mask once, explicit `!= 0`, wider type for masks, exact-width types, ISR-safety, etc., as relevant to this problem), (iii) "Common mistakes worth flagging" — 3–5 numbered items.

9. Append a row to `index.md`:

   ```
   | <today-date> | <Dow> | <topic short description> | <Easy/Medium/Hard> |
   ```

   The public `index.md` schema is `Date | Day | Topic | Difficulty` — no Status or Self-rating columns. Personal status / ratings live in `.private/ratings.md`, which is gitignored.

10. **Publish to GitHub.** As the final step, run `./.generator/publish.sh` from the repo root. The script:
    - exits cleanly if not in a git repo (so the system still works pre-publish);
    - refuses to push if `CHECK_FAILED.md` or `GENERATOR_MISSING.md` exists at the repo root (failures stay local);
    - **force-adds today's freshly-generated `src/`** (the `.gitignore` line `*/src/` keeps in-flight edits to OLD problems out of every commit, so we re-admit just today's stubs by date glob);
    - stages everything else that survives `.gitignore` (so `attempts/`, `.private/`, `.pending-references/`, and build artefacts never ship);
    - commits with a message naming today's folder, then pushes via the SSH deploy key configured in `~/.ssh/config` (see `SETUP.md`);
    - **after the push, sets `git update-index --skip-worktree` on every tracked `*/src/*` file**, so subsequent local edits to canonical stubs don't show up in `git status` and can't be staged by tomorrow's `git add -A`.

    Skip this step only if you wrote `CHECK_FAILED.md` in step 7.

    The src-shielding scheme has three composable rules — never edit them in isolation:
    - `.gitignore`'s `*/src/` blocks new files inside any problem's `src/` from being tracked;
    - the dated `git add -f "${TODAY}-"*/src/` line in `publish.sh` re-admits only today's stubs;
    - skip-worktree on tracked stubs hides modifications from git.
    Removing any one of the three breaks the property "the public repo only ever shows the blank canonical starter."

## Constraints

- Pure C99 throughout. No C++.
- Build environment is gcc + make on macOS Apple Silicon. No Docker, no extra packages.
- Don't repeat scenarios from the last 4 weeks (per `index.md`).
- First Friday of each month: "cold open" pseudocode-only — problem statement + a `scratch.md` for handwritten reasoning. No `src/`, no `tests.c`, no Makefile.
- Folder slug: `YYYY-MM-DD-<dow-3letter>-<topic-slug>`. Use lowercase hyphenated topic slugs (e.g., `rtos-mutex-priority`, `ringbuffer-isr-safe`, `i2c-frame-parser`).
- Saturday gets a difficulty bump (combine two topics or include a stretch goal).

## Success criteria

Today's folder exists with all canonical files, the self-check has passed both gates with no warnings, `.pending-references/<today>-reference.md` exists for tomorrow's drop, and `index.md` has the new row. On failure, `CHECK_FAILED.md` captures the diagnosis and nothing is shipped to the user folder under false pretenses.
