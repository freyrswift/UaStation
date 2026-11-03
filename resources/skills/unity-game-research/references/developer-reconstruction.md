# Developer Reconstruction

## Purpose

Use this mode when the user wants to reimplement a mechanic as a developer rather than only understand it as a designer.

## What To Recover

- class and namespace inventory relevant to the topic
- base classes and interfaces
- manager/controller/service/model/config boundaries
- method entry points
- event flow and callbacks
- state ownership
- data transfer objects and enums
- likely reimplementation seams

## Priority Inputs

Read in this order unless the evidence is clearly elsewhere:

1. `indexes/topics/<slug>.md` if present
2. `indexes/types-index.md`
3. `indexes/native-symbols.md`
4. `indexes/strings-by-topic.md`
5. `indexes/assets-index.md`
6. `raw/managed-identifiers.txt` or `raw/native-symbols.txt` only if the indexes are insufficient

## Output Pattern

Preferred files:

- `topics/<slug>-architecture.md`
- `topics/<slug>-call-flow.md`
- `topics/<slug>-reimplementation.md`

Only create `topics/<slug>-reimplementation.md` when feasibility is high.

Use a compact structure:

### Purpose

What the subsystem is responsible for.

### Key Classes

- class name
- probable role
- evidence

### Responsibilities

What each class appears to own.

### Relationships

- ownership
- dependency
- inheritance
- event/listener links

### Method Flow

Ordered sequence such as:

1. input received
2. controller validates
3. service applies rules
4. model/state updates
5. view/network side effects happen

### State And Data Flow

Track where authoritative state likely lives and how it changes.

### Reimplementation Notes

- safest abstractions to recreate first
- unclear boundaries
- likely server-owned logic

### Unknowns

Keep implementation gaps explicit.

### Server-side

List behavior that is clearly authoritative or finalized outside the client.

## Rules

- prefer naming exact classes over hand-wavy architecture language
- keep facts and guesses separate
- keep server-owned behavior separate from generic unknowns
- if names are obfuscated, reconstruct responsibilities from strings, method groupings, and adjacency
- do not claim exact behavior if only the high-level shape is visible

## Reimplementation Feasibility

Use three levels:

- `high`: separate `topics/<slug>-reimplementation.md` is allowed
- `medium`: keep reimplementation notes inside `topics/<slug>-architecture.md`
- `low`: avoid concrete reimplementation guidance

Promote to `high` only when at least 4 of these are true:

1. key classes are identified
2. main execution flow is understood
3. state ownership is visible
4. config or rule inputs are visible
5. class relationships are identifiable
6. unknowns are narrow

Red flags:

- severe obfuscation
- no clear execution flow
- unclear state ownership
- evidence mostly consists of strings
- server-authoritative behavior dominates

If feasibility is not `high`, explicitly list:

- `What Is Missing`
- `What Could Be Safely Recreated`
