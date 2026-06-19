# GH Secret Fix

Use this when GitHub push protection blocks a push because a secret still exists in commit history.

## What happened

A secret can remain in:

- an older local commit
- a file that was deleted later but still exists in history
- a generated log or output file that got committed accidentally

If GitHub names a specific commit/path, the secret is still reachable from the branch tip.

## Fast recovery steps

1. Find the flagged file paths from the GitHub error.
2. Check whether the paths still exist in the current tip:

```bash
git ls-tree -r --name-only HEAD | rg 'secret|bot_keys|main_log|receive_log|pipe_output|grok-break'
```

3. If the secret is in history, rewrite the branch to remove the bad paths:

```bash
rm -f .git/filter-repo/already_ran
git filter-repo --force \
  --path-glob '**/#.bot_keys.txt' \
  --path-glob '**/main_log.txt' \
  --path-glob '**/receive_log.txt' \
  --path-glob '**/pipe_output.txt' \
  --path-glob '**/#.grok-break]b0*.txt' \
  --invert-paths
```

4. If `git filter-repo` removes `origin`, add it back:

```bash
git remote add origin git@github.com:<org>/<repo>.git
```

5. Refresh the remote tip:

```bash
git fetch origin main
```

6. Verify the cleaned branch:

```bash
git status --short --branch
git log --oneline --decorate --graph --max-count=5 --all
git ls-tree -r --name-only HEAD | rg 'secret|bot_keys|main_log|receive_log|pipe_output|grok-break'
```

7. Push the rewritten history:

```bash
git push --force-with-lease origin main
```

## Notes

- Use `--force-with-lease`, not plain `--force`, unless you intentionally want to override remote changes.
- If GitHub reports a new secret path after the first rewrite, add that path to the `git filter-repo` command and run it again.
- If you only need to remove one file from the last commit and history is otherwise clean, `git commit --amend` or an interactive rebase may be enough.
- If you are unsure which commit contains the secret, GitHub's error output usually gives the exact commit SHA and path.

## Prevent this next time

- Add generated logs and key files to `.gitignore`.
- Do not commit bot tokens, API keys, or output dumps.
- If a secret ever gets committed, rotate it even after removing it from Git history.
