# AGENTS.md — Ae_MultiSlicer project dispatcher

Ae_MultiSlicer is an After Effects SDK project. The repository is intended to be cloned into the AeSDK 2025 Examples Template location described in `README.md`.

## Governance

- Top-level contract: [`constitution/CONSTITUTION.md`](constitution/CONSTITUTION.md)
- Current Operating Model: [`organization/profiles/release-driven-solo.md`](organization/profiles/release-driven-solo.md)
- Current repository facts: `README.md`, build/project files, source code, and accepted ADRs when added.

## Working rules

- Do not invent build, packaging, host-version, or SDK assumptions that are not evidenced by repository files or current official Adobe SDK documentation.
- Keep SDK/host-specific verification separate from source-only checks and bind evidence to the tested candidate.
- Preserve user changes and generated/vendor SDK content boundaries; do not treat the surrounding AeSDK checkout as repository-owned mutable state.
- Use GitHub Issues for durable work/dependencies and Pull Requests for review/integration state.
- Introduce architecture or toolchain decisions through durable project documentation rather than conversation-only state.
