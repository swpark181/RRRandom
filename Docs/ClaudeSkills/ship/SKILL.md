---
name: ship
description: Ships the current work end to end on GitHub — commits the changes, pushes the branch, opens (or reuses) a pull request, and merges it into main. Use this whenever the user asks for that whole flow in any wording, e.g. "/ship", "ship it", "commit, push, PR and merge", "커밋 푸시 PR 머지", "커밋하고 메인에 머지해줘", "PR 올리고 머지까지", "main 에 반영해줘". Do not use it when the user only wants a commit, only a push, or only a PR without merging; do those steps alone instead.
---

# Ship: commit → push → PR → merge into main

The user invoking this skill is their go-ahead for this one run of the whole flow, including the push and the merge,
which are visible to others and hard to undo. That go-ahead covers the normal path only. Anything outside it (force
pushing, overriding branch protection, resolving conflicts in ways that drop someone's work, merging with failing checks)
needs the user's explicit word first, so stop and ask.

## 0. Preconditions

Check these before touching anything, and stop with a short explanation if one fails:

- Inside a git work tree with an `origin` remote on GitHub (`git remote -v`).
- The GitHub CLI works: `gh --version` and `gh auth status`. If `gh` is missing, tell the user to install and log in
  themselves (it's interactive), e.g. `! winget install --id GitHub.cli -e` then `! gh auth login`, and stop. On Windows a
  fresh install may need a new terminal before `gh` is on the PATH; `"C:\Program Files\GitHub CLI\gh.exe"` works meanwhile.
- The default branch: `git symbolic-ref refs/remotes/origin/HEAD` (usually `main`). Call it MAIN below.

## 1. Look at what is being shipped

Run `git status`, `git diff --stat`, `git diff` (skim, more closely for small changes), and `git log --oneline -10`.
From these, understand what changed and why, so the commit message and PR describe the change, not the file list.

Decide what to stage. Stage the files that belong to this work by path, not with a blind `git add -A`, because a
working tree often holds strays:

- Leave out build output and caches even if they slipped past `.gitignore` (for Unreal: `Binaries/`, `Intermediate/`,
  `Saved/`, `DerivedDataCache/`), local settings, logs, scratch files.
- Never commit secrets (`.env`, keys, tokens, credentials). If one shows up in the diff, stop and tell the user.
- Large binaries (roughly over 50 MB) fail on GitHub unless tracked by Git LFS. Check `git lfs ls-files` /
  `.gitattributes`; if a big file isn't covered, ask before going on.
- If the tree mixes unrelated changes, ship only what the user meant and say what you left out. When unsure, ask.

If there is nothing to commit and no unpushed commits, say so and stop.

## 2. Branch

- On MAIN: create a branch first (`git switch -c <name>`), named after the change in short kebab-case. Follow the repo's
  existing branch naming if there is one (look at `git branch -a`), otherwise e.g. `ship/<short-description>`.
- On another branch: keep using it. Long-lived work branches that are merged repeatedly are normal; don't rename them.

## 3. Commit

Write the message in the repo's own style: read `git log` for language, tense, length and prefixes, and match it. One
short subject line that says what the change does, a blank line, then a body explaining what and why when the change is
more than trivial. If your harness gives commit attribution lines (for example `Co-Authored-By:` trailers), end the
message with them exactly as given.

Multi-line messages break easily in shell quoting, especially in PowerShell. Write the message to a temporary file (the
scratchpad directory if there is one) and use `git commit -F <file>`.

Let hooks run. If a pre-commit hook fails, fix the cause and commit again as a new attempt; don't use `--no-verify`.

## 4. Bring in MAIN and push

`git fetch origin`, then check whether the branch is behind `origin/MAIN` (`git rev-list --count HEAD..origin/MAIN`).
If it is, merge `origin/MAIN` into the branch (merging, not rebasing, so already-pushed history stays intact). If that
conflicts, stop, show the conflicting files, and ask how to resolve; don't pick a side on your own.

Push with `git push -u origin <branch>`. Never `--force`. If the push is rejected because the remote branch moved, fetch,
merge it, and push again; if that conflicts, stop and ask.

## 5. Pull request

Reuse an open PR for this branch if there is one: `gh pr view <branch> --json number,url,state`. Otherwise create one:

```
gh pr create --base MAIN --head <branch> --title "<title>" --body-file <file>
```

Title: like the commit subject (for several commits, a summary of all of them). Body: what changed and why in a few
bullets, how it was checked (builds, tests, runs), anything not verified. If your harness gives PR attribution lines, end
the body with them. Write the body to a file and pass `--body-file`, for the same quoting reason as the commit.

## 6. Checks

`gh pr checks <number>`. If the repo has no checks, move on. If checks are running, wait with
`gh pr checks <number> --watch`. If any fail, stop: report which ones and the link, and don't merge.

## 7. Merge

Match how this repo merges: if `git log --oneline origin/MAIN` shows "Merge pull request #N" commits, use a merge commit
(`--merge`); if it is a straight line of PR titles, it squashes (`--squash`); otherwise ask, or default to `--merge`.

```
gh pr merge <number> --merge
```

Don't add `--admin` to get around branch protection or required reviews, and don't add `--delete-branch` unless the user
asks; work branches here may be reused. If the merge is blocked (review required, conflicts, protection), stop and say why.

## 8. Wrap up

`git fetch origin` and confirm the PR shows as merged (`gh pr view <number> --json state,mergeCommit`). If a local MAIN
exists, fast-forward it without switching away from the work branch: `git fetch origin MAIN:MAIN` (skip if it isn't a
fast-forward). Stay on the work branch.

Report in a few lines, in the user's language: the commit(s) shipped, the PR link and number, the merge commit, and
anything left out or not verified.
