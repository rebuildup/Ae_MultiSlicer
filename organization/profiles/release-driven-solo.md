# Release-driven solo development profile

- Status: Current default
- Constitutional authority: none; this profile must refine the Constitution
- Related: ADR-0004, ADR-0008, ADR-0012, ADR-0013, ADR-0016, ADR-0018, ADR-0022, ADR-0023

## Purpose

Ae_MultiSlicer の current release-driven development で使用する Operating Model を定義します。

このprofileはproject-initの唯一の正しいorganization topologyではありません。別profileまたはproject-specific modelがConstitutionを満たす場合は置換できます。

## Current topology

### Source and implementation state

- Git: source stateのcanonical authority
- GitHub Issues: durable implementation scope / acceptance criteria / dependency state
- GitHub Pull Requests: review / candidate integration / validation evidence
- `main`: released/integrated source state
- `release-x-y-z`: current release integration line

### Release planning

- GitHub Issues を durable implementation / dependency SoT とする
- external planning surface は明示的に採用された場合だけ canonical とする
- implementation/dependency facts の ownership を曖昧にしない

### Delivery defaults

- normal sprint cadence: 1 week
- production/stable release intent: major bump default
- normal sprint release intent: minor bump default
- within-sprint / post-introduction adjustment: patch bump default
- top-level durable work: GitHub Issue
- ticket branch: Issue number only
- independent ticket PR: target release branch
- hard dependency stack: immediate predecessor branchをbaseにできる
- first meaningful durable commit後はcanonical remote publication + Draft PRを行う
- release branchにmeaningful differenceが入ったらDraft release PRを維持する
- `main`へのnormal integrationはcurrent release branchからのrelease PRだけ
- PR landing method: merge commit only
- repository merge settings: `allow_merge_commit=true` / `allow_squash_merge=false` / `allow_rebase_merge=false`
- squash merge / rebase mergeは使用しない。branch-local `git rebase` はstack maintenance等のbranch mechanicsとして別扱い
- stacked/native landingはmerge commit semanticsを保持できる場合だけ使用する
- merge/landingはADR-0012のexplicit authorization boundaryを維持する

### Workspace/runtime defaults

- project-local toolchain/bootstrap frontend: mise
- runtime / development CLI prerequisitesは原則repository-controlledな `mise.toml` からmaterializeする
- fresh clone / CI / agentのbootstrapは `mise install`、committed mise lockfileをreproducibility mechanismとして使う場合は `mise install --locked`、command executionは `mise exec -- ...` / `mise run <task>` を標準入口とする
- required toolはexact pin、bounded version request + committed mise lockfile、または既存ecosystem-native canonical version sourceのいずれかでreproducibleにする
- `rust-toolchain.toml` 等のnative canonical version sourceがある場合、mise側へ独立した競合pinを作らない
- external/untrusted PR checkoutでrepository-controlledなmise config/taskをagentが実行する前に、documented trust reviewまたはbounded sandboxを必須とし、mise自体をisolation boundaryとして扱わない
- miseはOS/system package、container/sandbox、secret management、worker isolationの代替にしない
- mise unavailable/incompatible時は同等のversion/reproducibility guaranteeを持つ明示的fallbackを使用する
- WSL/Linux worktree frontend: Worktrunk
- Worktrunk unavailable/incompatible時: native Git worktree fallback
- worktree自体をruntime isolation proofとして扱わない
- implementation workerのmutable runtimeは適切に分離する
- parent/child handoffはimmutable identityへpinする

### Secret / environment defaults

- application / serviceのsecret valueはInfisicalをcurrent default SoTとする
- current default control planeはself-hosted `https://secrets.rebuildup.dev` とし、APIは `https://secrets.rebuildup.dev/api`
- current defaultで使用するprovider endpoint / project ID / environment/path mapping等のnon-secret pointerをrepositoryからdiscoverableにする
- wrapper / CI / runtimeはprovider endpointを明示し、managed Infisical Cloudへ暗黙fallbackしない
- required key / type / validation / non-secret metadataはrepository-controlled schemaとして保持する
- normal local workflowはCLI-firstとし、runtime injectionを優先する
- plaintext `.env` をcanonical storeにしない
- human / agent / CIはidentityを分離し、project / environment / path / actionをleast privilegeにする
- GitHub Actionsは利用可能ならOIDC + scoped Machine Identityを使用し、long-lived master credentialをdefaultにしない
- generic CIへproduction secretを渡さない
- NixOS / dotfiles / bootstrap等 encrypted secret-in-Git がexplicit requirementならSOPS等へdeviateできる
- provider-specific procedureは `secrets-management` Skillへprogressive disclosureする

### Quality / evidence

- project-specific adaptive quality profileをcompileする
- validation evidenceをcurrent artifact/SHAへbindする
- worker / integration / release gateの責務を分ける
- fixed universal required status check名は前提にしない

## Constitutional mapping

### Identity Integrity

- resolved immutable SHAへsnapshot/result/validationをpin
- predecessor change後にaffected validationを再実行
- execution generation/fencingでstale continuationを区別

### Authority Integrity

- PR readinessとmerge authorizationを分離
- merge method固定やrepository settingの整合からauthorizationを導出しない
- release/product/irreversible decisionは定義済みauthority boundaryへ従う

### Evidence Integrity

- current SHAのvalidation evidenceを使用
- false greenを禁止
- PR/release readinessはapplicable evidenceへ基づく

### Mutable Ownership Safety

- worktree-only isolationを認めない
- concurrent implementation workerはmutable runtimeを安全に分離
- DB/Redis/queue/port等のshared mutable stateを別途調停

### Organizational Continuity

- Issue / PR / Git ref / committed docs / immutable resultをdurable recovery sourceにする
- native conversation resumeをcanonical recovery stateにしない

### Canonical Consistency

- source / implementation / review / release-planning responsibilityをfield ownershipで分離
- 同一factの二重canonical化を避ける

### Progress

- 自明なimplementation decisionを不要にoperatorへ返さない
- review/validation完了後はauthorization等の実blockerがなければ次stateへ進める
- safety mechanismがdelivery deadlockを作る場合はOperating Modelをre-evaluateする

## Deviation

このprofileの具体的なtool/cadence/topologyから外れる場合でも、該当するConstitutional guaranteeを維持できれば許容します。

explicit user/project decisionとして固定されたrelease scope/version/public contract等は、単なるdefaultとは区別します。

## Re-evaluate / remove

次の場合はprofile全体または一部を見直します。

- agent/runtimeが同等以上のisolation/recovery/review semanticsをより単純に提供する
- GitHub/Linear/Worktrunk/miseの役割を別systemが置換する
- release branch modelがcontinuous delivery等に対して逆効果になる
- weekly cadenceがproject objectiveへ合わない
- eval/実績から特定procedureの追加価値が消えた


## Agent Skills lifecycle

- Project-local Agent Skills の導入・更新は **`bunx skills` を canonical mechanism** とする。手動 copy / 個別 agent directory の内容を upstream freshness の SoT にしない。
- Bun は Skills CLI 用の development prerequisite とする。Bun が application runtime / package manager でない repository でも、既存の project-local `mise` または established bootstrap から利用可能にし、application の native toolchain ownership は変更しない。
- project-init Skills は floating source `rebuildup/project-init` を追跡する。上記の `release-0-3-0` は Constitution / Operating Model の reconciliation baseline であり、Skills の freeze/pin ではない。freeze が必要なら ADR で明示する。
- canonical install layout は `.agents/skills/` と `.claude/skills/`。Codex / OpenCode / Cursor は universal `.agents/skills/` を共有し、Claude Code は `.claude/skills/` を使用する。
- 初回導入 / scope変更は `bunx skills add rebuildup/project-init --list` で候補を確認し、標準bootstrapは `bunx skills add rebuildup/project-init --skill '*' --agent claude-code --agent codex -y` とする。repositoryに `mise.toml` taskがある場合は `mise run skills-bootstrap` を同じ入口とする。
- project-local `skills-lock.json` は source / skill path / content hash の durable metadata として commit する。通常の追従更新は `bunx skills update -p -y`（または `mise run skills-update`）を使う。
- current Skills CLI の `skills install` は lock から universal `.agents/skills/` のみを復元するため、Claude Codeを含むfresh clone bootstrapの唯一の入口にはしない。
- `--global` は通常運用で使用しない。user-global Skills を repository policy / freshness evidence にしない。
- project固有 Skill や intentional customization は upstream update で無条件上書きせず、source ownershipを分けてreconcileする。

## Project-specific reconciliation

- AeSDK 2025 Examples Template 配下へ clone するという現状 README の前提だけを保持し、それ以上の未確認 toolchain/build contract は固定しない。
- SDK checkout と repository の mutable ownership boundary を明示的に扱う。

## Upstream source

`rebuildup/project-init@release-0-3-0` (`57fb4a2e5abe6f52e4fd9cb2cb88234496e47d4b`) を基準にする。
