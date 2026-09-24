# AGENTS.md — Ae_MultiSlicer

## Governance

- Constitution: [`constitution/CONSTITUTION.md`](constitution/CONSTITUTION.md)
- Current Operating Model: [`organization/profiles/release-driven-solo.md`](organization/profiles/release-driven-solo.md)
- Project-specific architecture / build / host constraints in README and docs remain more specific authority when they preserve the Constitution.

- this repository currently uses `master` as released/integrated ref; project-init's `main` default is intentionally adapted to `master` until a separate branch migration decision is made.
- After Effects SDK template placement/build constraints remain project-specific.
- Bun is only Agent Skills tooling and does not imply a product build-system migration.

## Agent Skills lifecycle

- install/reconcile: `bunx skills add rebuildup/project-init --skill '*' --agent claude-code opencode codex -y`
- fresh clone: `bunx skills install`
- continuous update: `bunx skills update -p -y`
- use project scope only; global installation is not canonical
- commit CLI-generated `skills-lock.json`; do not hand-author source/hash entries
- do not modify upstream-managed project-init Skill files; keep local refinement in separate Skills/adapters/docs
