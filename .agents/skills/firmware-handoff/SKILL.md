---
name: firmware-handoff
description: Final checks and human handoff for completed firmware/build changes. Use after implementation and self-review; excludes read-only audits and reviewer tasks.
---

# Firmware handoff

Run after implementation and main-agent self-review, before calling a firmware
change ready or making an approved local commit. The main agent owns the result;
reviewer output is evidence, not authority. This workflow has at most two review
rounds per logical change, even when slot limits require dispatching in batches.

## 1. Prepare the review candidate

Record the human requirement, comparison base, changed files, and current diff,
including uncommitted work and any SDK changes. Account for every hunk and
identify unrelated user work. Finish self-review, relevant tests, and
`./bin/clang-format-fix -g` before dispatch. Build early only when needed to
resolve a concrete compilation or build-configuration question; the required
final build belongs after review fixes.

**Done when:** the candidate meets the requirement, every hunk is accounted for,
and checks run or unavailable checks with their reasons are recorded.

## 2. Main-agent review

Review the complete candidate diff against the human requirement and relevant
repository rules. Inspect affected call paths, ownership, resource use, error
handling, and compatibility. Run targeted static checks or tests where available.
If a concrete concern benefits from a second opinion, request a focused,
read-only review; independent reviews are optional, not a release gate.

Record findings, rejected concerns, unresolved risks, and checks performed.

**Done when:** every hunk is accounted for, verified problems are fixed, and
remaining limitations are explicit.

## 3. Resolve findings

Verify each finding against reachable code. Explain rejected findings and keep
unresolved concerns visible. Decide which advisory cleanups belong to the
requirement; defer unrelated improvements. Apply accepted fixes together, then
verify the whole diff yourself. If a focused reviewer was used, re-check any
follow-up changes against that review's concern.

**Done when:** no verified problem remains unaddressed and substantive risks are
clearly recorded.

## 4. Complete final checks

Run formatting and any affected tests not already green for the final change.
Build each relevant firmware target once after the final substantive firmware or
build edit, including configuration, code-generation sources, and translations.
Follow the [testing rule](../../rules/testing-debugging.md). Preserve green
results after formatting, comments, or documentation alone; repeat a check only
when a change or failure invalidates it. Record commands, outcomes, and concrete
reasons for unavailable checks; an unavailable check is not a pass.

**Done when:** relevant tests, formatting, and final firmware builds have passed,
or missing verification is explicitly reported and handoff remains incomplete.

## 5. Hand off the actual design

Explain old and new behavior, state/resource ownership, affected files, the
important tradeoff, checks, and remaining risks in plain English. Point to the
diff. Ask the human to accept the specific behavior and design tradeoff just
explained, including responsibility for maintaining it. Name the relevant choice:
for example, bounded temporary storage and a fallback when allocation fails.
For a straightforward fix, ask whether the new behavior and its state ownership
match their intent; do not invent a tradeoff. A generic understanding/ownership
question does not complete handoff. Prescribe no confirmation phrase. Recognize
explicit acceptance already given for this logical change; record it once and
reopen only after a material design change. Continuation or commit approval alone
does not establish understanding and ownership.

Give applicable hardware steps with expected results and likely failure signs,
including orientations, resources, and caches when affected. Hardware testing is
the human's responsibility and must happen before a PR is opened. Never claim
hardware verification yourself. Publication follows the root guide's human
ownership policy and the [Git rule](../../rules/git-workflow.md).

**Done when:** checks cover the final change, no verified finding remains, and
explicit human acceptance covers the described behavior,
architecture, and maintenance responsibility. The hardware test plan is provided.
If the human rejects the architecture, ask what must change and revise before
calling it ready.
