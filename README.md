# Embedded Prep

Daily exercises for embedded software engineer interviews. A new problem lands every weekday morning — bit manipulation, RTOS / ISR safety, embedded data structures, peripheral protocols, debugging scenarios. Pure C99, gcc + make, no Docker, no external libraries.

## Two ways to use this repo

- **Follow along.** `git pull` once a day, work the latest problem, come back tomorrow for the reference solution. No setup beyond a working C toolchain. This is the path most readers want.
- **Fork and run your own loop.** Everything that drives the daily generation lives in `.generator/`. Fork the repo, point a scheduler at `.generator/prompt.md`, and follow `SETUP.md` to wire your fork up to GitHub for unattended publishing. You'll have your own catalog growing on the same rails.

## Spoiler warning

Each problem folder may contain a `reference.md` with the canonical solution. **Don't open it before you've taken your own swing at the problem.** The point is the reps. References land the morning after the problem itself, so if you're following along live they'll trail by a day; if you're working through historical problems they're already there next to the README.

## Build requirements

- gcc (any version with C99 + `-Wall -Wextra -Wpedantic`)
- GNU make
- macOS or Linux. Tested on macOS Apple Silicon.

That's it. No package install, no toolchain bring-up.

## Running a problem

```bash
cd <date>-<dow>-<topic>/
make test         # builds with -Wall -Wextra -Wpedantic and runs the harness
```

Tests will be red until you fill in the `TODO`s in `src/`. Iterate until green. Stuck? Open `hints.md` and read one hint at a time — three progressive nudges per problem.

## Folder layout

Each problem folder contains:

- `README.md` — problem statement, scenario, register layouts or protocol details, task list, constraints
- `src/` — starter `.c` / `.h` with `TODO` markers and `(void)`-cast stubs
- `tests.c` — assert-style harness (`CHECK` / `CHECK_EQ_U` macros)
- `Makefile` — canonical hardened build (no built-in rules, accepts `make test` or `make tests`)
- `hints.md` — three progressive hints, read one at a time
- `reference.md` — canonical solution + interviewer notes + common mistakes (lands the day after the problem)

## Topic rotation

| Day | Topic |
|-----|-------|
| Mon | Bit manipulation, low-level C — registers, packed structs, alignment, popcount |
| Tue | Concurrency, RTOS, ISR safety — mutexes, race conditions, atomics |
| Wed | DS&A under embedded constraints — ring buffers, no-malloc allocators, state machines |
| Thu | Peripheral protocols & driver design — I2C/SPI/UART frame parsing, register-driven SMs |
| Fri | Debugging scenarios + small system design (first Friday each month: pseudocode-only "cold open") |
| Sat | Mixed harder problem, 45–60 min budget |
| Sun | Rest |

Difficulty target throughout: SWE I / new-grad / Associate level. Weekday warmups fit a 20-minute window; Saturday is a 45–60 minute round.

## Index

`index.md` is the running catalog of problems with date, day, topic, and difficulty.

## Bug reports and questions

Spot a bug, think a reference solution misses a cleaner idiom, or stuck on a problem in a way the hints don't address? Open an issue.

## The system, for forkers

The generator that writes each new problem is documented in `.generator/prompt.md` — the source of truth for folder layout, the canonical Makefile, the test-harness macros, the topic rotation, and the self-check gate that prevents broken problems from shipping. `SETUP.md` covers wiring a fork up to its own GitHub remote with an SSH deploy key so cron-driven runs can `git push` unattended.

If you fork and start running your own loop, I'd be happy to know — drop a link in an issue or in your fork's README.

## License

MIT — see `LICENSE`.
