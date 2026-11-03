# Game Knowledge Dossier Schema

## Root Files

- `manifest.yaml`: machine-readable inventory of inputs, backend, confidence, and generated documents
- `progress.yaml`: checkpoint state for resume across sessions
- `summary.md`: short overview of the game loop and key systems
- `technical-findings.md`: backend, artifact paths, extraction limits, and reverse-engineering notes
- `unknowns.md`: unresolved topics worth further research
- `glossary.md`: stable terms, currencies, entities, and abbreviations

## Evidence Folder

- `evidence/files-inventory.md`: notable files and why they matter
- optionally add more evidence notes when needed, but keep them source-focused

## Indexes Folder

- `indexes/assemblies-index.md`
- `indexes/types-index.md`
- `indexes/assets-index.md`
- `indexes/native-symbols.md`
- `indexes/network-endpoints.md`
- `indexes/strings-by-topic.md`
- `indexes/candidate-systems.md`
- `indexes/recovery-plan.md`
- `indexes/tool-readiness.md`
- `indexes/install-plan.md`

Optional but recommended for token efficiency:

- `indexes/topics/<slug>.md`: compact topic-local cache assembled from the most relevant indexes for one requested system

## Raw Folder

- `raw/tool-availability.txt`
- `raw/managed-identifiers.txt`
- `raw/native-symbols.txt`
- plus raw inventories and string/topic extracts

## Topics Folder

Use for user-requested research targets. Prefer this folder when the request is narrow or bespoke:

- `topics/economy.md`
- `topics/skill-system.md`
- `topics/hero-movement-on-battlefield.md`
- `topics/guild-system.md`

For developer reconstruction requests, also allow:

- `topics/<slug>-architecture.md`
- `topics/<slug>-call-flow.md`
- `topics/<slug>-reimplementation.md`

For dual-perspective requests, prefer:

- `topics/<slug>.md`
- `topics/<slug>-architecture.md`
- `topics/<slug>-reimplementation.md`

## Systems Folder

Use for larger loops and subsystems:

- `core-loop.md`
- `progression.md`
- `economy.md`
- `combat.md`
- `inventory.md`
- `quests.md`
- `monetization.md`
- `networking.md`
- `save-system.md`

## Mechanics Folder

Use for atomic rules and reusable sub-mechanics:

- `resources.md`
- `currencies.md`
- `upgrades.md`
- `timers.md`
- `gacha-or-loot.md`

## Entities Folder

Use for content catalogs:

- `items.md`
- `characters.md`
- `enemies.md`
- `skills.md`

## Minimal Frontmatter Pattern For Markdown Files

When helpful, begin each document with compact metadata:

```yaml
scope: economy
confidence: medium
sources:
  - assets/bin/Data/Managed/Assembly-CSharp.dll
  - assets/bin/Data/resources.assets
```

Use it only when it improves clarity.

## Token Efficiency Notes

- reason from `indexes/` first
- read `raw/` only when indexes are insufficient
- update existing topic docs rather than regenerating them
- keep narrow requests narrow
