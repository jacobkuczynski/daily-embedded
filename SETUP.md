# Setup — first-time GitHub publish

One-time steps to wire the daily generator up to a GitHub repo with an SSH deploy key. Once this is done, every successful run of `.generator/prompt.md` ends by pushing the day's commit unattended.

This file is for the repo owner (you). End users who clone the public repo to practice never run any of this.

## 1. Create the empty GitHub repo

On github.com, create a new repository — empty, no README, no `.gitignore`, no LICENSE (this repo brings its own). Name it whatever you want it to be public as (e.g. `embedded-interview-prep`). Note the SSH URL: `git@github.com:<you>/<repo>.git`.

## 2. Generate a deploy key dedicated to this repo

On the mac that runs the cron, in Terminal:

```bash
ssh-keygen -t ed25519 -C "embedded-prep deploy" -f ~/.ssh/embedded_prep_deploy -N ""
```

That creates two files:

- `~/.ssh/embedded_prep_deploy` (private key, never share)
- `~/.ssh/embedded_prep_deploy.pub` (public key, paste into GitHub)

The empty `-N ""` is intentional — a passphrase-protected key cannot be used by an unattended cron job.

## 3. Add the public key to the GitHub repo as a deploy key

In GitHub: repo → **Settings → Deploy keys → Add deploy key**.

- Title: `embedded-prep cron mac`
- Key: paste the contents of `~/.ssh/embedded_prep_deploy.pub`
- **Allow write access:** check this box. Required for `git push`.

Click *Add key*. This key is scoped to this one repo — it has no access to anything else in your GitHub account, which is the whole point of using a deploy key instead of your personal SSH key.

## 4. Add an SSH config alias

Edit `~/.ssh/config` (create it if missing) and append:

```
Host github.com-embedded-prep
    HostName github.com
    User git
    IdentityFile ~/.ssh/embedded_prep_deploy
    IdentitiesOnly yes
```

The host alias `github.com-embedded-prep` is what `publish.sh` will use as the remote host. SSH will see the alias, look up the matching key, and skip your personal key entirely. `IdentitiesOnly yes` is important — without it, SSH may try other keys first and hit GitHub's auth-attempt limit.

Test the alias resolves:

```bash
ssh -T git@github.com-embedded-prep
# Expected: "Hi <you>/<repo>! You've successfully authenticated, but GitHub does not provide shell access."
```

## 5. Initialise the local repo and add the remote

From the embedded-prep working directory:

```bash
cd ~/personal/embedded-prep
git init -b main
git add -f */src/        # canonical stubs are gitignored — force-add them
git add -A                # everything else (README, tests, hints, Makefile, index, ...)
git status                # SANITY CHECK — verify no attempts/, .private/, or .pending-references/ in the staged set
git commit -m "Initial public catalog of embedded prep problems"
git remote add origin git@github.com-embedded-prep:<you>/<repo>.git
git push -u origin main
./.generator/publish.sh   # one-time: applies skip-worktree to every tracked src/ file
```

The host portion of the remote URL must be the alias `github.com-embedded-prep` (NOT plain `github.com`) — that's what routes the push through the deploy key.

The two `git add` lines on first commit are load-bearing: `*/src/` is gitignored so canonical stub files would be skipped by a plain `git add -A`. The `-f */src/` force-add re-admits them. From the next commit onward, `publish.sh` does this dance for you automatically.

The closing `./.generator/publish.sh` line is the one-time bootstrap. Once stubs are tracked on the remote, this run sets `--skip-worktree` on every committed `src/*` file so all your future local edits to them stay invisible to git.

## 6. Sanity-check the cron path

Cron / launchd run with a stripped environment. `publish.sh` already exports an explicit `PATH`, so `git` resolves regardless. The only environment-sensitive piece is `HOME` — both cron and launchd set it correctly to your user's home, which is what SSH needs to find `~/.ssh/config`.

Verify by running `publish.sh` once manually:

```bash
cd ~/personal/embedded-prep
./.generator/publish.sh
# Should print "publish: no staged changes — nothing to push" if you've already pushed manually.
```

If it instead errors with "Permission denied (publickey)" or similar, the SSH alias isn't being resolved. Re-check step 4.

## 7. Done

The next successful run of the daily generator will commit and push automatically. Failures (CHECK_FAILED.md present) stay on the mac and never ship.

If you ever rotate the deploy key, repeat steps 2–4 with a new filename, then update the GitHub deploy key entry. Your past commits remain valid; only future pushes change auth.
