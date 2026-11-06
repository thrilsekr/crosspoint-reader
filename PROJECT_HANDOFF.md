# Xteink X4 AI Cover Art Project Handoff

Last updated: 2026-11-05

## Mission and constraints

Generate reading-context-aware, portrait AI cover art for the original Xteink X4 on CrossPoint 2.0.0. Use existing reader-originated `Cover+Custom` sleep rendering: AI image replaces the EPUB cover; missing/invalid image falls back to the book cover. System-menu custom screensaver remains unchanged.

- Preserve CrossPoint 2.0.0 and SDK pin `5e94fe7480f019c89fbf691aef87523eb1b46704`; no beta/RC/dev firmware.
- SD-card sideloading only. No USB serial.
- Work only in `thrilsekr/crosspoint-reader`; never touch upstream.
- Credentials must not enter firmware, commits, logs, screenshots, or build artifacts.
- Assistant owns repository documentation and project continuity.
- Do not repeat verified milestones or their device tests unless a later change affects them.

## Branches and verified build baseline

- X4 baseline workflow: [run 42132139366](https://github.com/thrilsekr/crosspoint-reader/actions/runs/42132139366), successful X4-only build.
- Exact workflow source: `25c6ccad1f3e0f82d271bd8243361467f5983841`, branch `feat/x4-reader-screensaver-transition`.
- Integration target branch HEAD before reconciliation: `f8a87112849ec7606ba47dc14763fc871f4a3a13`.
- Worker PR #3 source before reconciliation: `c87f195973a2a5a6feb06970c83f9289d4d387b2`.
- PR #3: https://github.com/thrilsekr/crosspoint-reader/pull/3. It remains a draft and targets `feat/x4-reader-screensaver-transition`.
- The merge commit on `feat/ai-worker-lifecycle` contains the integration branch as its second parent. Check the live PR/branch head at the start of the next session; do not assume a branch SHA from this note remains current.

## Completed and verified milestones

**DEVICE PASS (user-reported):**
1. Reader-only short-power trigger captures current-page text and book title/author; context preview displayed the correct information.
2. Application-lifetime worker lifecycle, cooperative atomic cancellation, cleanup/reaping, and sleep drain.
3. Local diagnostic GET, POST, BMP download and save.
4. Reader-originated `Cover+Custom` displays the downloaded BMP; missing/invalid BMP falls back to the EPUB cover; system-menu custom screensaver is unchanged.
5. Per-book image storage, isolation, persistence, lookup, and fallback. Book A/B/C sequence passed.

Do not repeat these tests. The current per-book storage uses the existing EPUB cache identity and keeps generated images outside the disposable EPUB layout cache. See `DECISIONS.md`.

**BUILD PASS:** workflow run 42132139366 at the exact baseline SHA above. Build success is not a new device-validation claim for later commits.

## Current milestone: Stage 1, direct AI provider contract

**Status: investigation; no provider request has been implemented or sent.**

AI Horde is the candidate direct provider. Its asynchronous flow is:
- Submit JSON to `POST /api/v2/generate/async`, receive a generation ID.
- Poll `GET /api/v2/generate/check/{id}`; retrieve completed results from `GET /api/v2/generate/status/{id}`.
- Best-effort cancellation uses `DELETE /api/v2/generate/status/{id}`.

Candidate image parameters: 384×640 (3:5 aspect ratio; service dimensions must be multiples of 64), one image, portrait prompt. Scale to the 480×800 device target locally. `shared:false` does not prevent volunteer workers from seeing submitted prompts. Verify account-key behavior and privacy expectations before sending page text.

### Blocking technical work before a real request

1. **TLS security:** inspect the current merged downloader and pinned SDK path. Do not send the API key while peer/hostname verification is disabled or unproven. Remove any insecure TLS configuration; validate CA trust and hostname verification against the exact pinned SDK.
2. **Hostname transport:** completed worker path was built for local IPv4 HTTP. AI Horde and R2 require hostname HTTPS. Design bounded/cancellable DNS and TLS connection setup without regressing the tested worker lifecycle.
3. **HTTP contract:** add response-bearing JSON POST and response GET/status handling, reusing `AiScreensaverService` and the single-flight worker. Do not recreate the worker/task lifecycle.
4. **Image format:** AI Horde returns WebP. Current firmware decoder factory registers JPEG, PNG and GIF, not WebP. Determine a memory-safe streaming/block conversion path for the X4 (about 380 KB usable RAM).
5. **Configuration:** locate/define an SD-card-only configuration for the API key. Never log the key or captured page text.

No bridge/proxy is justified yet. Prefer direct, minimal provider communication if the TLS, hostname, memory, and privacy constraints can be met.

## Next action

Complete a source-verified design for TLS trust + hostname verification and bounded hostname connection setup using the pinned SDK. In parallel, confirm the exact AI Horde request/status/result schema and account-key privacy behavior. Present the implementation plan before changing firmware code. No user device action is needed yet.
