# Triage Labels

The skills speak in terms of five canonical triage roles. This file maps those roles to the actual label strings used in this repo's issue tracker.

| Label in mattpocock/skills | Label in our tracker | Meaning                                  |
| -------------------------- | -------------------- | ---------------------------------------- |
| `needs-triage`             | `needs-triage`       | Maintainer needs to evaluate this issue  |
| `needs-info`               | `needs-info`         | Waiting on reporter for more information |
| `ready-for-agent`          | `ready-for-agent`    | Fully specified, ready for an AFK agent  |
| `ready-for-human`          | `ready-for-human`    | Requires human implementation            |
| `wontfix`                  | `wontfix`            | Will not be actioned                     |
| *(terminal)*               | `done`               | All acceptance criteria met and merged   |

When a skill mentions a role (e.g. "apply the AFK-ready triage label"), use the corresponding label string from this table.

On GitHub (framework tickets) the role is a label, and the category labels are `bug`, `enhancement` and
`documentation`. In a local Game Target ticket it is the `Status:` line. See `issue-tracker.md`.

## Closing a ticket (`done`)

**GitHub:** `done` is a closed issue, not a label. The merging PR's `Closes #NN` closes it. Before merging, tick the
issue's checkboxes, adjust any acceptance criteria that changed (and say why in a comment), and let the PR carry the
branch, commits and files.

**Local Game Target ticket:** it is `done` when:

1. Every `- [ ]` checkbox is flipped to `- [x]`.
2. `**Status:** done` replaces the previous status line.
3. A `## Comments` section is appended (or extended) with:
   - The integration branch name and one commit hash per logical change.
   - Files created or significantly modified.
   - Any acceptance criteria that were intentionally adjusted during implementation (and why).

The `## Comments` block is the canonical record of what was actually shipped — future agents reading the ticket should be able to reconstruct what happened without checking git.
