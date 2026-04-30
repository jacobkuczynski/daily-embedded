#!/usr/bin/env bash
#
# Publish today's generation to the GitHub remote.
#
# Invoked by .generator/prompt.md as the FINAL step on a successful run.
# Idempotent: if there is nothing to commit (e.g. the generator made no
# changes that survive .gitignore), it logs and exits clean.
#
# Refuses to push if CHECK_FAILED.md or GENERATOR_MISSING.md exists at the
# repo root — failures stay local and never ship.
#
# Cron-safe: sets a minimal PATH explicitly so launchd's stripped env
# doesn't break `git`. Auth comes from the SSH deploy key configured
# via ~/.ssh/config (see SETUP.md).

set -euo pipefail

# launchd / cron scrub PATH down to /usr/bin:/bin. Add the homebrew
# locations and /usr/local/bin so `git` resolves on any reasonable mac.
export PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

if [ ! -d .git ]; then
    echo "publish: not a git repo at $REPO_ROOT — skipping push" >&2
    exit 0
fi

if [ -f CHECK_FAILED.md ] || [ -f GENERATOR_MISSING.md ]; then
    echo "publish: failure marker present — refusing to push" >&2
    exit 0
fi

TODAY="$(date +%F)"

# Force-add today's freshly-generated stub files. The .gitignore line
# `*/src/` keeps your in-flight edits to OLD problems out of every
# commit; this targeted -f scope re-admits ONLY today's brand-new
# stubs so followers actually see the day's blank starter.
#
# The dated glob is the load-bearing part: it matches only the folder
# whose name starts with today's date, so older problem folders' src/
# stays gitignored and your mid-attempt edits stay local.
shopt -s nullglob
TODAY_SRC_DIRS=( "${TODAY}-"*/src )
shopt -u nullglob
if [ "${#TODAY_SRC_DIRS[@]}" -gt 0 ]; then
    git add -f "${TODAY_SRC_DIRS[@]}"
fi

# Stage everything else that survives .gitignore. attempts/, .private/,
# .pending-references/, build artefacts, and old folders' src/ are
# excluded so a blanket `git add -A` is safe at this point.
git add -A

if git diff --cached --quiet; then
    # Nothing to commit — fine on Sundays when only the index row changed
    # to a state that already matches HEAD, or on the first-time setup
    # where the user has already pushed manually. Fall through to the
    # skip-worktree step so the bookkeeping still gets applied.
    echo "publish: no staged changes — skipping commit/push" >&2
else
    # Commit message: date + short summary of today's added folder, if any.
    NEW_FOLDER="$(git diff --cached --name-only --diff-filter=A \
                  | awk -F/ '/^[0-9]{4}-[0-9]{2}-[0-9]{2}-/ {print $1}' \
                  | sort -u | head -n 1 || true)"

    if [ -n "${NEW_FOLDER:-}" ]; then
        MSG="prep: ${NEW_FOLDER}"
    else
        MSG="prep: ${TODAY} index update"
    fi

    git commit -m "$MSG"
    git push origin HEAD
fi

# Always (re)apply skip-worktree to every tracked */src/* file. Runs
# even when there was nothing to push, so the first-time bootstrap flow
# (manual `git push -u origin main` followed by running this script
# once) correctly shields every canonical stub going forward.
# Idempotent: the bit can be set repeatedly with no effect.
TRACKED_SRC="$(git ls-files | grep -E '^[^/]+/src/' || true)"
if [ -n "$TRACKED_SRC" ]; then
    echo "$TRACKED_SRC" | xargs git update-index --skip-worktree
fi
