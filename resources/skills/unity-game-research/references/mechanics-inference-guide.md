# Mechanics Inference Guide

## Strong Signals

- class names such as `InventoryManager`, `QuestService`, `BattlePassController`
- enums for item rarity, currency type, quest state, or reward type
- strings for store packs, missions, cooldowns, ads, analytics events, and remote config keys
- prefab, scene, and asset names
- localization keys that reveal player-facing systems
- JSON, protobuf, ScriptableObject, or config table fields

## Weak Signals

- genre assumptions
- icon filenames without supporting code or config
- isolated numeric constants with no surrounding context
- Unity boilerplate types with no game-specific references

## Inference Pattern

1. List observed facts.
2. Group them by likely system.
3. Write the smallest defensible conclusion.
4. Put missing proof into `unknowns.md`.

## Good Output Style

- `Fact: Found enum CurrencyType with values Gold, Gems, Energy.`
- `Inference: The game likely uses hard currency plus stamina gating.`
- `Unknown: No store price table found; monetization depth is still unclear.`

## Avoid

- claiming exact live-ops design from a few strings
- claiming balance values without the actual tables
- merging multiple possible systems into one confident narrative
