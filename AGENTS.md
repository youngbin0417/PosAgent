# Project working rules

## Complete a work unit

- Read `WORKLOG.log` before changing code. After finishing a work unit, append a dated entry with changed paths, behavior, verification commands and results, and remaining gaps. Do not rewrite prior entries.
- Run the relevant build and tests before committing. Windows is the primary verification target; WSL Ubuntu is secondary. Record an environment limitation when a check cannot run.
- Inspect `git status` and the staged diff. Stage only files that belong to the completed work unit. Preserve unrelated user changes. Never stage generated files under `build/`, credentials, tokens, or other secrets.
- Create a local Git commit after each completed, verified work unit without waiting for another request. Do not push unless the user asks. If Git cannot commit, report the exact blocker and leave the worktree intact.

## Commit messages

- Use Conventional Commits: `<type>(<scope>): <imperative summary>`. Omit scope when it adds no clarity.
- Choose `feat`, `fix`, `refactor`, `perf`, `docs`, `test`, `chore`, `build`, `ci`, `style`, or `revert` according to the change. Use a specific present-tense verb such as `add`, `fix`, or `document`.
- Keep the subject at most 50 characters when practical, never above 72; do not end it with a period.
- Add a body only when the reason is not clear from the subject, or for a breaking change, security fix, migration, or revert. Wrap body lines at 72 characters.
- Do not amend, squash, force-push, or include unrelated changes unless the user specifically asks.
