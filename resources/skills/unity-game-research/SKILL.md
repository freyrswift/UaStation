---
name: unity-game-research
description: Use when an agent needs to investigate an extracted Unity game, recover evidence from Mono or IL2CPP artifacts, and write reusable notes about specific gameplay systems such as economy, progression, combat, skills, inventory, networking, or monetization.
license: MIT
compatibility: Works in Agent Skills compatible clients with local filesystem access. Python 3 is recommended for the checkpoint script.
---

# Unity Game Research

## Overview

Investigate an extracted Unity game from the current working directory or from a user-provided path. Build or refresh a reusable `game-knowledge/` dossier with evidence-backed notes about only the systems the user asked to study.

## Default Behavior

- Treat the current working directory as the input root unless the user gives another path.
- Write outputs to `./game-knowledge/` unless the user specifies another destination.
- Resume from an existing `game-knowledge/` directory when possible.
- Prefer read-only analysis of the source tree and do not modify original game artifacts.
- Stop early if the folder does not look like a Unity game.
- Treat extraction, indexing, and resume as internal responsibilities of the skill.

## Core Workflow

1. Confirm Unity markers in the discovered tree:
   - `assets/bin/Data/`
   - `assets/bin/Data/Managed/`
   - `assets/bin/Data/il2cpp_data/`
   - search the tree for `libil2cpp.so`, `libunity.so`, and `global-metadata.dat`
   - `globalgamemanagers`, `resources.assets`, `sharedassets*.assets`
2. Classify the scripting backend:
   - managed/Mono if game assemblies exist in `Managed/`
   - IL2CPP if both `libil2cpp.so` and `global-metadata.dat` are present anywhere in the discovered tree
3. Build or refresh a checkpoint state under `game-knowledge/`:
   - `manifest.yaml`
   - `progress.yaml`
   - `raw/`
   - `indexes/`
4. Extract game-facing signals from the best available artifacts:
   - managed assemblies or IL2CPP recovery outputs
   - strings, endpoints, config blobs, enum names, type names
   - asset names, prefab names, scene names, resources, localization data
5. Convert large raw outputs into compact indexes before substantial reasoning.
6. Infer gameplay systems only from evidence. Separate facts from interpretation.
7. Write or update only the topic documents requested by the user, then refresh shared summary files as needed.

## Resume Rules

- If `game-knowledge/progress.yaml` exists, read it first.
- Do not redo expensive extraction if matching `raw/` and `indexes/` artifacts already exist and still cover the requested topics.
- If the user asks for a new topic later, continue from prior checkpoints instead of starting over.
- Only rebuild stale or missing phases.
- Preserve previous topic documents unless new evidence contradicts them.
- Record each researched topic in `progress.yaml`.
- If a topic document already exists, update it in place instead of rewriting it from scratch unless the evidence base changed substantially.

## Tool Orchestration

- If local reverse-engineering tools are available, use them.
- Prefer deterministic tools for extraction and indexing, then use LLM reasoning on the compact outputs.
- Good candidates include filesystem inventory, string extraction, assembly/type listing, IL2CPP recovery helpers, and asset name extraction.
- Do not block on a specific tool if equivalent evidence can be collected another way.
- When a tool produces a large output, save it under `raw/` and create a condensed index under `indexes/` before reasoning.
- Prefer running `scripts/unity_research_checkpoint.py` first to build or refresh `manifest.yaml`, `progress.yaml`, `raw/`, `indexes/`, and `evidence/files-inventory.md`.
- Treat the checkpoint script as the default low-cost entrypoint for repeated sessions.
- Use deeper adapters opportunistically:
  - managed: `ilspycmd` or `monodis` if available, otherwise printable-string identifier extraction
  - native/IL2CPP: `readelf` and `nm` if available, otherwise keep relying on file inventory and strings
- Respect environment-variable overrides for local tool paths when present.
- Always inspect `indexes/tool-readiness.md` after checkpoint creation to understand what parts of the toolchain are available.
- Inspect `indexes/install-plan.md` when readiness is insufficient and installation may be worthwhile.
- If required tools are missing, continue with available fallbacks instead of blocking the request.
- Only attempt tool installation when the user explicitly approves installation work. Do not silently install anything.
- If installation is approved, prefer deterministic package-manager or local-binary installation commands and record the result in `technical-findings.md`.

## Missing Tool Notifications

When the requested research topic would materially benefit from a missing tool, tell the user explicitly.

Use these priority levels:

- `Required`: the requested task is effectively blocked without the tool
- `Recommended`: the task can continue, but depth or confidence will be materially worse
- `Optional`: the tool would help, but current evidence is probably enough

The notification should say:

- which tool is missing
- why it matters for the current topic
- whether work can continue without it
- whether installation is recommended now

Examples:

- `Missing tool: Cpp2IL. Priority: Recommended. Needed for deeper IL2CPP reconstruction of the skill system. Work can continue with reduced architecture fidelity.`
- `Missing tool: Ghidra. Priority: Optional. Useful only if static IL2CPP recovery remains too shallow.`

## Capability Gap Detection

Do not assume the predefined tool list is complete.

If the current environment lacks a needed capability, but no exact tool is known or auto-detected, tell the user explicitly that a capability gap exists.

Examples of capability gaps:

- deeper native control-flow analysis
- Unity asset table extraction
- runtime memory dumping
- network traffic interception
- managed assembly decompilation beyond string-level recovery

For a capability gap, report:

- `Missing capability`
- why it matters for the requested topic
- whether the current pass can continue
- one or more candidate tools if known

Example:

- `Missing capability: runtime memory dumping. Priority: Recommended. Needed because static IL2CPP artifacts appear protected. Candidate tools may include Frida-based or debugger-assisted dumping workflows.`

## Token Efficiency Rules

- Read `manifest.yaml` and `progress.yaml` before opening any other files.
- Prefer `indexes/` over `raw/`. Do not read `raw/` unless the needed evidence is missing from indexes.
- Treat `raw/` as a last-resort archive, not the default reasoning source.
- For a narrow request, open only the indexes relevant to that topic.
- If the user requests one system, research one system. Do not broaden scope by default.
- Prefer updating existing topic files over regenerating them.
- Do not refresh `summary.md` for every narrow topic unless the global understanding materially changed.
- Do not rebuild checkpoint artifacts if current ones are sufficient and inputs do not appear to have changed.
- Maintain a small working set of files per pass. Avoid opening every index file in the dossier.
- Stop extraction once evidence is sufficient to support a stable draft for the requested topic.

## Topic Routing

Before substantial reasoning, map the user request to a narrow topic slug and load only the most relevant artifacts first.

Recommended priority order:

- `topics/<slug>.md` if it already exists
- `indexes/candidate-systems.md`
- `indexes/strings-by-topic.md`
- `indexes/types-index.md`
- `indexes/native-symbols.md`
- `indexes/assets-index.md`
- `indexes/network-endpoints.md`
- `raw/` only if the indexes are insufficient

When useful, create or update a compact topic cache under `indexes/topics/<slug>.md` so later sessions can resume without re-reading multiple indexes.

## Specialized Modes

- For developer reconstruction requests, use `references/developer-reconstruction.md`.
- For weak or partial evidence, use `references/mechanics-inference-guide.md`.
- For dossier layout and checkpoint file expectations, use `references/dossier-schema.md` and `references/progress-schema.md`.

## Evidence Rules

- Every non-trivial claim should point to evidence:
  - file path
  - assembly name
  - class/type name
  - string or config key
  - asset or scene path
- Distinguish four categories explicitly:
  - `Fact`: directly observed in files
  - `Inference`: likely interpretation of facts
  - `Unknown`: relevant but not confirmed from available evidence
  - `Server-side`: behavior is clearly authoritative or finalized outside the client, even if the exact implementation is unknown
- Never present guessed mechanics as certain just because the genre is obvious.
- If names are obfuscated, rely more on strings, resource names, endpoints, numeric tables, and cross-references.
- Use `Server-side` when the client clearly indicates backend ownership, validation, or final authority, instead of collapsing everything into `Unknown`.

## Required Output

Create or update these files under `game-knowledge/`:

- `manifest.yaml`
- `progress.yaml`
- `summary.md`
- `technical-findings.md`
- `unknowns.md`
- `glossary.md`
- `evidence/files-inventory.md`
- `indexes/`
- `raw/`

Expect these index artifacts when the checkpoint script has run:

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

Create topic documents under `game-knowledge/topics/` using stable slugs derived from the user's requested systems:

- `topics/economy.md`
- `topics/progression.md`
- `topics/hero-movement-on-battlefield.md`
- `topics/skill-system.md`
- `topics/inventory.md`
- `topics/guild-system.md`

Also create category documents such as `systems/`, `mechanics/`, or `entities/` only when they materially help organization for the current game and requested topics.

For developer reconstruction requests, also allow:

- `topics/<slug>-architecture.md`
- `topics/<slug>-call-flow.md`
- `topics/<slug>-reimplementation.md`

For dual-perspective requests, prefer:

- `topics/<slug>.md`
- `topics/<slug>-architecture.md`
- `topics/<slug>-reimplementation.md` when useful

Do not create empty filler documents. If a topic cannot be supported yet, record it in `unknowns.md` and explain what evidence is missing.

Only create `topics/<slug>-reimplementation.md` when reimplementation feasibility is high. Otherwise keep reimplementation notes inside `topics/<slug>-architecture.md`.

## Document Rules

- Start each topical document with:
  - scope
  - confidence
  - evidence summary
- Then describe the system in gamedesign language.
- Then structure the body as:
  - `Facts`
  - `Inferences`
  - `Unknowns`
  - `Server-side`
- End with `Files and Evidence`.
- Keep the documents useful for a later Q&A agent. Optimize for retrieval, not literary prose.
- For targeted subsystem requests, go as deep as evidence allows. Example: if the user asks for hero movement on the battlefield, trace movement-related classes, states, pathfinding hints, animation/state machine names, nav or grid clues, input bindings, skill-movement coupling, and any restrictions or combat interactions relevant to movement.

For developer reconstruction requests, prefer this structure:

- `Purpose`
- `Key Classes`
- `Responsibilities`
- `Relationships`
- `Method Flow`
- `State And Data Flow`
- `Reimplementation Notes`
- `Unknowns`
- `Files and Evidence`

For dual-perspective requests:

- keep `topics/<slug>.md` focused on mechanics, player-facing rules, progression hooks, restrictions, and outcomes
- keep `topics/<slug>-architecture.md` focused on classes, responsibilities, relationships, and flows
- avoid merging both views into one bloated document unless the topic is tiny

## Scope Requests

If the user asks for one or more specific systems:

1. Prioritize only those topics.
2. Still maintain `manifest.yaml`, `progress.yaml`, `summary.md`, `technical-findings.md`, and `unknowns.md`.
3. Do not spend time producing unrelated system documents unless strong evidence is already available at near-zero extra cost.
4. Treat a narrow request as permission to go deep, not broad.
5. If only one topic was requested, keep the pass single-topic unless blocking dependencies force a small amount of adjacent context.
6. If the request is framed as implementation or reconstruction work, switch to the developer reconstruction guidance from `references/developer-reconstruction.md`.
7. If the request explicitly asks for both design and code architecture, produce both views without broadening the topic scope.

## Escalation Path

- For managed builds, inspect `Managed/` assemblies and prioritize `Assembly-CSharp.dll`.
- For IL2CPP builds, inventory `libil2cpp.so`, `libunity.so`, and `global-metadata.dat`, then explain what can and cannot be recovered statically.
- If deeper backend-specific guidance is needed, check `indexes/tool-readiness.md` first, then use `references/tool-adapters.md` to pick the best available adapter or fallback; report any missing capability directly.

## Mobile Online RPG Bias

Expect frequent evidence for:

- progression and leveling
- currencies and sinks
- inventory and equipment
- skill trees or active/passive skills
- quests and mission chains
- guild, clan, party, or social systems
- PvE and PvP rules
- events, daily tasks, and retention loops
- monetization and live-ops
- client-versus-server authority boundaries

For live data or packed content, fall back to generic mobile signatures like `UnityCache/Shared`, `cache`, `files`, `AssetBundle`, `BundleFiles`, `__data`, or bundle-like files when the exact container layout is unclear.

Do not assume the server-side truth is fully visible in the client. For online RPGs, mark server-owned behavior as `Unknown` when the client only reveals hints.

## Validation And Networking

Use the smallest network surface that still proves the loop:

- start/finish DTOs and message types
- request/result types and error enums
- endpoint and config strings
- validator-like field paths and client-side compare helpers
- explicit client-versus-server authority boundaries

Treat client checks as local hints only when the server can still override the final state.

## References

- Read `references/dossier-schema.md` for the expected dossier layout.
- Read `references/progress-schema.md` for checkpoint file structure.
- Read `references/mechanics-inference-guide.md` when evidence is weak or partial.
- Read `references/developer-reconstruction.md` for architecture-focused reconstruction requests.
- Use `scripts/unity_research_checkpoint.py` before substantial reasoning when checkpoint artifacts are missing or stale.
- Read `references/tool-adapters.md` when selecting local reverse-engineering tools or fallbacks.

## Capability Notes

After checkpoint creation, inspect `indexes/tool-readiness.md` first. Use `references/tool-adapters.md` to pick the best available adapters and fallbacks; if a capability is missing, report the gap instead of assuming a deeper sibling workflow exists.
