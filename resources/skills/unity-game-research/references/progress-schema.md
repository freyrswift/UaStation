# Progress Schema

## Purpose

Track what the research pipeline has already completed so later sessions can resume with minimal token cost.

## Recommended Shape

```yaml
input_root: /path/to/game
output_root: /path/to/game-knowledge
backend: managed
unity_confirmed: true
phases:
  detect: done
  extract: done
  index: done
  infer: partial
  write: partial
topics:
  economy:
    status: done
    doc: topics/economy.md
  skill-system:
    status: pending
    doc: topics/skill-system.md
artifacts:
  raw_strings: raw/strings.txt
  files_inventory: evidence/files-inventory.md
  type_index: indexes/types-index.md
last_updated: 2026-04-09
```

## Rules

- update this file at the end of each substantial phase
- read it before doing expensive work
- prefer append/update over rewrite-from-scratch
- keep statuses simple: `pending`, `partial`, `done`, `blocked`
