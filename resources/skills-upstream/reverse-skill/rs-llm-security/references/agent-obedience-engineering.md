# AI Agent Obedience Engineering — Making AI Actually Work After Reading the Workflow

> Source: 2026 multi-source synthesis (Anthropic Skill Engineering, Microsoft Code Words, Strands Steering Hooks, Gradient Flow Harness Engineering)
> Applicable scenario: AI coding agents (Claude Code / Codex / Cursor / Cline / Windsurf / Kiro, etc.) that only confirm without executing, skip steps, or take liberties omitting key operations after reading README/RULES.md

---

## Core Problem Diagnosis

The root cause of an AI agent "reading the workflow but not working" is not insufficient model capability, but **semantic escape room in natural-language instructions**:

| Root Cause | Explanation |
|------|------|
| **Context attention decay** | Content in the middle of long documents gets down-weighted by the LLM attention mechanism; the agent effectively only "sees" the beginning and the end |
| **Semantic override** | When optimizing for "helpfulness", the model creatively reinterprets explicit instructions (e.g., reading MUST DO X as "it is suggested to do X") |
| **Passive language read as optional** | "Ready for next step → invoke X" gets treated as a suggestion rather than an instruction |
| **No state enforcement** | No external state machine validates workflow order; the agent can skip steps undetected |
| **Silent state corruption** | The agent produces structurally correct but semantically wrong results, and errors accumulate silently |

---

## Technique 1: Instructions-First Principle (Critical-First Pattern)

**Put "what to do next" at the very top and the context after.**

```
WRONG (Agent ignores):
  [70 lines of project background and tool lists]
  → "Next step: run bootstrap to install missing tools"

CORRECT (Agent executes):
  "## Execute immediately: run `bootstrap-reverse.ps1` to check for and install missing tools
   → after completion, read routing.md to determine which skill to enter"
  [then the project background and tool lists]
```

**Principle**: LLMs give the highest attention weight to the beginning and end of a prompt. Middle content can be entirely ignored.

**Applied to this project**:
- The "routing entry" section of RULES.md should come after the trigger keywords and before the execution principles
- The first section of every SKILL.md should be "Execute immediately", not "Applicable scenarios"

---

## Technique 2: Directive Language Replacement (Directive Over Suggestive)

Replace all "suggestive" language with RFC 2119-grade directive language:

| Weak language (Agent may skip) | Strong language (Agent forced to execute) |
|---|---|
| "You can try..." | **MUST**: you must execute... |
| "Ready for next step → invoke X" | **NOW**: invoke X immediately, do not wait for confirmation |
| "It is recommended to read routing.md first" | **REQUIRED**: you must finish reading routing.md before entering any submodule |
| "If tools are missing you can bootstrap" | **NO EXCUSE**: when tools are missing the only correct action is invoking bootstrap; manual install guessing is forbidden |
| "Remember to update field-journal" | **CHECKLIST ENFORCED**: after task completion, tick off the Checklist item by item; do not claim the task is finished until everything is done |
| "should..." | **MUST** / **MUST NOT** |

**Key patterns**:
```
MUST — violation = task failure
MUST NOT — violation = safety violation
SHOULD — skipping requires a stated reason
MAY — genuinely optional
```

---

## Technique 3: Excuse Rebuttal Table

**This is the most critical patch for this project.** When an AI agent meets resistance, it automatically generates "reasonable excuses" to skip steps. Pre-list the common excuses and rebut each one:

| Common Agent Excuses | Rebuttal (enforced) |
|---|---|
| "This step can be skipped, I'll just..." | **Skipping forbidden.** Every step in the behavior chain is required. If you believe it can be skipped, first output the specific reason and let the user decide. |
| "In my judgment this is not necessary" | **Your judgment does not apply here.** List the specific criteria you used to judge, and explain why that criterion permits skipping a step that is explicitly written out. |
| "The user probably doesn't need this" | **Never decide for the user.** Present all options to the user; mark recommendations but do not hide alternatives. |
| "I already know how to do this, no need to read X" | **Read X before acting.** Even if you are sure you know how, X may contain constraints specific to this task. Reading it takes 2 seconds. |
| "To save time, I can skip in parallel..." | **The correct way to save time is executing independent steps in parallel, not skipping steps.** If two steps are independent, do them in parallel; if dependent, do them in order. |
| "I've used this tool before, I know the path" | **Guessing paths forbidden.** You must obtain the actual path from tool-index; install locations differ across machines. |
| "The task is basically done, no need for the checklist" | **The only definition of task completion is a fully ticked Checklist.** A task with an incomplete Checklist is not complete. |
| "I couldn't find tool-index, so I'll just guess paths" | **A missing file is 100 times safer than guessing a wrong path.** When tool-index is missing, first run refresh-tool-index.ps1 to generate it. |
| "The user didn't explicitly ask for a report, so I won't write one" | **Reporting is the default behavior, not optional.** After a security task completes, a report must be generated, unless the user explicitly says "no report". |
| "This is too simple, no need to record a journal" | **Simple tasks hold pitfall value too.** At minimum record: target type + what was used + any surprises; one line is fine. |
| "The user asked me to redo the import table/a step, but I did some other more useful step instead" | **Redo = redo the very step that was named** (or a legitimate prerequisite path confirmed by the user). MUST update the corresponding Evidence; substituting an unrelated step is forbidden, as is silently skipping. Unpacking is a **prerequisite** for a readable IAT, not a **substitute** for import-table Evidence. |
| "The user said for packed samples don't unpack yet, look at the import table first; I'll just submit the garbage table and call it done" | **Feasibility gate:** when X is blocked, you MUST state the blockage, give a recommended order, and **ask the user to confirm**. If the user insists, execute and mark `quality=unreadable/packed`; concluding from the garbage table that capabilities are absent is forbidden. |
| "It crashes after unpacking, so I'll keep grinding on editing the file on disk" | **Patch 6:** record E-self-check-crash / E-iat-repair-fail, switch to dynamic (bp CreateFile/GetFileSize). Infinite static file editing is forbidden. |
| "The IAT can't be fixed, let me statically try a few more unpacker tools to stall" | **IAT repair iron law:** prefer automatic/semi-automatic repair; if the tool errors or the result won't run → stop static IAT work immediately, record E-iat-repair-fail, switch to dynamic API breakpoints to capture. Infinite static grinding is forbidden. |
| ".NET / no import table, the hard gate doesn't apply, I'll skip" | **The equivalent anchor still applies:** for .NET, write a dnSpy/IL/metadata summary into the E-imports semantic slot; DLL/SYS must also include E-exports alongside. Passing through empty is forbidden. |


**Usage**: place this table near the end of RULES.md or another instruction file (a high-attention zone). The agent sees the rebuttals before looking for excuses.

---

## Technique 4: The Five Skill Engineering Patterns (Anthropic 2026 Official)

| Pattern | Applicable Scenario | Key Techniques |
|---|---|---|
| **Linear Flow** | Processes with clear steps (deployment, installation) | Provide safe defaults, use negative instructions ("MUST NOT use --force") |
| **Decision Tree** | Platform navigation, troubleshooting | Tree navigation + progressive loading from `references/` |
| **Iterative Loop** | TDD, review-fix loops | Hard rules up front + the **excuse rebuttal table** to block shortcuts |
| **Baton Loop** | Multi-session, multi-agent collaboration | Externalize state into `next-prompt.md` (MUST write before exiting) |
| **Multi-Phase + Checkpoints** | Multi-day complex workflows | Orchestrator "parent" skill + manual Go/No-Go checkpoints, with time cost labeled |

**Mapping to this project**:
- The full behavior chain = Linear Flow (15 steps executed in order)
- The routing matrix = Decision Tree (three-dimensional matching)
- The Checklist = Multi-Phase Checkpoint (every step must be ticked)
- The Field Journal = Baton Loop (cross-session state externalization)

---

## Technique 5: In-Band Enforcement Validation (Steering Hooks Idea)

Do not rely on AI "self-discipline"; instead embed self-validation instructions in the prompt:

```
Before each claim of "task complete", you MUST first self-check:
1. Did I skip any step in the behavior chain? Which one?
2. Did I guess any tool path? If so, what is the actual tool-index path?
3. Is the Checklist fully ticked? Why are some items unticked?
4. If any answer above is "yes"/"unticked", then the task is not complete;
   go back to the corresponding step and re-execute; do not declare completion.
```

This method makes the agent audit itself before saying "done", more immediate than external validation.

---

## Technique 6: Opaque Identifiers (Code Words) — for API/Tool Parameters

Microsoft 2026 research found: semantic parameter names trigger the model's tendency to "helpfully optimize".

```
WRONG: { "query": "...", "top": 9 }        → 68.4% parameter adherence
CORRECT: { "query": "...", "code": "alpha" } → 100% parameter adherence
```

**Application scenarios**:
- When passing precise configuration in bootstrap scripts, use short codes instead of semantic parameters
- For tool-call parameters needing strong guarantees, use code word mappings

---

## Technique 7: Dual-AI Review Loop (Dual Validation)

```
AI A (executor) writes the output
  ↓
AI B (reviewer) checks against the rules
  ↓ pass
output to the user
  ↓ fail
return to AI A for correction, with specific violation citations attached
```

**Application in this project**:
- Embed a "self-review" step in RULES.md: before outputting the report, the agent first checks itself item by item against the Checklist using its own capability
- If it finds unfinished items, it goes back and completes the corresponding steps itself

---

## Technique 8: Context Window Layout Optimization

LLM attention distribution (high→low):
```
[first 10%] ████████████ ← highest attention, place "act now" instructions
[middle 80%] ████░░░░░░░░ ← decaying attention, place reference material
[last 10%]  ████████████ ← attention rebounds, place "no skipping" and the Checklist
```

**Concrete application**:
1. **First 10%**: execute-immediately instructions + trigger keywords
2. **Middle 80%**: detailed workflows, reference links, tool lists
3. **Last 10%**: excuse rebuttal table + hard Checklist + forbidden-behavior list

---

## Battle-Tested Prompt Templates

### Template A: Forced Startup Template (embedded at the top of RULES.md)

```markdown
## CRITICAL: after reading this document you must immediately perform the following operations (do not just confirm — actually execute)

1. **NOW**: detect the directory containing this file → this is the package root
2. **NOW**: if this is first use, write these rules into the global configuration (see the global injection section)
3. **NEXT**: read `skills/SKILL.md` → `skills/routing.md` → determine which sub-skill to enter
4. **NEXT**: read `skills/tool-index.md` to confirm tool status
5. **THEN**: begin executing the actual task; do not linger in the "have read" state

If you only reply "read", "completed", "understood" without actually performing the steps above,
you have failed. What the user needs is tools installed, code analyzed, vulnerabilities verified —
not a confirmation message.
```

### Template B: Submodule Entry Template (embedded at the top of every SKILL.md)

```markdown
## ACTION REQUIRED (execute immediately after reading, do not wait)

After reading this file:
1. Confirm you understand this skill's applicable scenarios
2. Check whether this machine has the required tools (read `../tool-index.md`)
3. If tools are missing → invoke bootstrap
4. If tools are present → begin the first step of the workflow
5. If unsure → list the specific questions; do not go silent
```

### Template C: Task Completion Self-Check Template (embedded at the end of every SKILL.md)

```markdown
## Task Completion Self-Check (MUST confirm item by item before claiming completion)

□ I actually executed every step in the behavior chain (nothing skipped)
□ I did not guess any tool paths (all came from tool-index.md)
□ I produced reproducible commands/scripts/reports (not just descriptions of steps)
□ I updated the field-journal (if there were pitfalls)
□ I executed the post-completion Checklist (report + diagrams + experience write-back)
```

---

## Forbidden Behaviors (added from the agent-obedience perspective)

- Forbidden to reply only "understood, please tell me the specific task" after reading RULES.md
  → correct behavior: execute global injection → read SKILL.md → read routing.md → determine the entry point
- Forbidden to say "steps 1-4 are complete" when they were merely read once
  → correct behavior: distinguish "have read the docs" from "have executed actions"; only the latter produces real side effects
- Forbidden to say "task complete" without executing the Checklist
  → the Checklist is the only definition of task completion
- Forbidden to substitute "from experience" for reading tool-index
  → paths differ across machines; tool-index is the only way to locate them

---

## Summary: If You Can Change Only One Thing

**Add an "act immediately" instruction block at the very top of RULES.md**, using bold, CRITICAL, NOW, and other strong directive words.

This is the highest ROI change. Most agent "not working" behavior comes from automatically entering "wait for user instruction" mode after reading files. A mandatory "act immediately" instruction breaks this pattern.

If you can change a second thing: **add the excuse rebuttal table**. Agents look for excuses to stop at the first resistance; block these excuses in advance.
