# Xteink X4 AI Cover Art Project Handoff

Last updated: 2026-11-05

## Mission

Build AI-generated, reading-context-aware EPUB cover art for the original Xteink X4 running CrossPoint 2.0.0.

Use CrossPoint's existing **Cover+Custom** mode:
- Sleeping from an EPUB reader displays the generated AI image in place of the normal book cover, with the original book cover as fallback.
- Sleeping from system menus continues to display the user's existing custom screensaver.
- This is a cover-art substitution in the reader sleep path, not a replacement for the normal screensaver.

Intended flow: capture the visible page text and book metadata while the reader is alive, generate a portrait-oriented image, deliver its BMP to the X4, and let the existing reader-originated Cover+Custom path display it.

## Non-negotiable project rules

- Preserve the installed CrossPoint 2.0.0 firmware baseline. No beta, RC, dev build, or firmware reinstall outside an explicitly planned diagnostic/implementation test.
- Use SD-card sideloading for firmware.
- Verify APIs against the exact CrossPoint 2.0.0 source and pinned SDK.
- Separate confirmed facts, hypotheses, and untested ideas.
- Work one milestone at a time and validate before advancing.
- Prefer local-only networking and minimal data exposure. Never ask the user to disclose private LAN IPs or credentials.
- Do not ask for GitHub passwords or tokens.
- Do not touch upstream repositories. Work only in the user's personal fork.
- Do not use USB serial; it is unavailable on the X4.
- The user wants the assistant to manage engineering, research, implementation, verification, and project records. Continue without asking for “proceed” at every step. Ask only when user input, a device test, or approval for a consequential action is genuinely required.
- The user has clarified that existing screensaver files do **not** need protection during diagnostics. This relaxes the previous preservation constraint. Still avoid unrelated destructive changes and explain any device-side file changes before asking the user to perform them.
- Keep instructions concise and actionable.
- Don't generate implementation code or firmware files until the approach is verified.

## Repository and pinned baseline

- Personal fork: https://github.com/thrilsekr/crosspoint-reader
- CrossPoint 2.0.0 base commit: `42d58daaabcf469a2e5e1ba871714fb9dcf51d29`; SDK submodule: `5e94fe7480f019c89fbf691aef87523eb1b46704`.
- Feature/base branch: `feat/x4-reader-screensaver-transition`, current base commit `13b0d8bd22f5d100eaf0b32364198bea2d911b91`.
- Integration/PR branch: `feat/ai-worker-lifecycle`, source commit `46cb346c4204da55b04ecd2140f51f91564d3b8c`; PR #3 remains draft and targets the feature branch.
- Build target is original X4 only: CI matrix contains one `default` target and `platformio.ini` defines `FREEINK_DEVICE_X4=1` without the X3 define. No other seven device targets are built.
- Latest X4-only CI run: [42085257638](https://github.com/thrilsekr/crosspoint-reader/actions/runs/42085257638), success. Unit tests, clang-format, cppcheck, and the single X4 firmware build passed.
- Artifact ID: `13443680344`; binary size 5,567,904 bytes; SHA-256 `8317a1ef2b3c9b0a9dd1e36c8b6bce9b8d57bda21a7f4bff5dd38d0f4f25a6d8`.
- Build usage: RAM 66,680 / 327,680 bytes (20.3%); flash 5,553,375 / 6,553,600 bytes (84.7%).

## Completed milestone

**Milestone: local networking + BMP download/save/open diagnostic — DONE (2026-11-02).**

### Confirmed device results

Cumulative diagnostic firmware from code commit `98811e06eb687e83fba58eef2de86a8a2fb409f0` was installed and tested on the original X4.

- Receiver accepted device connections and returned HTTP 200 for diagnostic GET/POST and BMP GET.
- Sleep-event POST `/event`: JSON body received (235 bytes); HTTP 200.
- Diagnostic GET `/event`: HTTP 200; X4 screen: **Get pass**.
- Diagnostic POST `/event`: form body `test=x4-post-test` received (17 bytes); HTTP 200; X4 screen: **Post pass**.
- GET `/test.bmp`: receiver sent 960,054 bytes; HTTP 200; X4 screen: **Get test.bmp Pass**.
- X4 file status: **Bmp file: SAVED**.
- User opened `x4-download-test.bmp` on the X4; it processed and opened successfully.
- The diagnostic fixture was landscape. This was a transport/render test only; generated screensaver imagery should be portrait-oriented.
- Receiver endpoint was local-only on port 8765. User's private LAN details are not part of project records.

### Build/artifact used

- Firmware source commit: `98811e06eb687e83fba58eef2de86a8a2fb409f0` (“Keep cumulative results in local X4 diagnostic”).
- Workflow run: `41411501888`; job/check: `125165358875`; result: success.
- Artifact ID: `13125857099`; ZIP: `x4-cumulative-diagnostic.zip`.
- Binary: `firmware.bin`, 5,590,784 bytes.
- SHA-256: `df2b6cd68872c87635c055c4289b080b9dceef1ecbb07ec28dd451f0bc87bb1f`.
- Branch HEAD at record update: `29b4bb1ced7fea7d3a0347e6dd8a03078179f50c` (“Make X4 diagnostic builds manual-only”).
- Workflow is `workflow_dispatch` only, X4-only; documentation commits/pushes do not trigger builds.
- No firmware rebuild is needed for this documentation update.

### Completed milestone: reader trigger + context preview — DEVICE TEST PASS (2026-11-04)

- Appended persisted setting `AI_SCREENSAVER=10`; values 0–9 remain unchanged.
- Short power release in the EPUB reader captures current-page text plus book title/author and replaces the reader with CrossPoint's existing `DetailsActivity` as a temporary context preview.
- User confirmed the preview displayed the correct book title and author and captured current-page text. This is test-harness UI only, not customer-facing behavior.

### Completed milestone: worker lifecycle + local BMP transfer — DEVICE TEST PASS (2026-11-05; user-reported)

- User tested `x4-ai-worker-lifecycle-firmware.bin`: GET Pass, POST Pass, GET BMP Pass, BMP Saved.
- This validates worker GET lifecycle and serialized POST/BMP transfer/save. It does not validate the new sleep-cover display path.

### Current milestone: downloaded BMP as EPUB cover — BUILD PASS; DEVICE TEST PENDING

- `SleepActivity::renderCoverSleepScreen()` now checks `/x4-download-test.bmp` only when sleep originates from the reader and `COVER_CUSTOM` is selected.
- Missing, invalid, or unrenderable test BMP falls back to the original book cover. The system-menu path still calls `renderCustomSleepScreen()`; `/sleep.bmp` is untouched.
- The test BMP was already saved to the SD card during the previous worker-lifecycle test. No need to repeat GET/POST/BMP transfer checks if that file is still present.
- The short-power `AI_SCREENSAVER` action remains the temporary DetailsActivity context preview. For this renderer test, use the normal long power press from inside an EPUB to sleep.
- The new X4-only firmware build passed CI. Device verification is exclusively the user's next step.

## Next milestone

After the user validates reader-originated Cover+Custom display and system-menu screensaver preservation, connect the actual image-generation provider and replace the diagnostic BMP path with per-book generated-image state. Do not start that milestone until the display test passes.

## Confirmed CrossPoint 2.0.0 findings

### Reader text and trigger

- `EpubReaderActivity` owns the current `Section`.
- `section->getTextFromSectionFile()` is page-specific: `Section.cpp` calls `loadPage(currentPage)` and extracts the text blocks from that page, not the whole chapter.
- The method reconstructs text by joining words with spaces; it does not preserve all EPUB layout/spacing or non-text elements. User's earlier QR decode matched the visible page text apart from formatting.
- Reuse this method for the first integration rather than adding a second EPUB text extractor. QR is verification only; final solution will not require QR.
- Plugin/event APIs cannot access the current `Section`, invoke QR generation, or retrieve page text.
- A firmware fork can reuse `getTextFromSectionFile()`.
- Main trigger is the short power-button action. Long press remains OEM power-down behavior.
- `SHORT_PWRBTN` values in `src/CrossPointSettings.h`: 0 Ignore, 1 Sleep, 2 Page Turn, 3 Force Refresh, 4 Footnotes, 5 Power Confirm, 6 Create Clipping, 7 Bookmark, 8 Sync, 9 Reader Menu. Append `AI_SCREENSAVER=10`; do not renumber existing persisted values.
- `SettingsList.h::shortPowerButtonOptions()` maps persisted values to the user-facing list; add the new option there. For the original X4 (no touch), existing bookmark/sync/reader-menu entries are not offered by the touch-board-only branch.
- The original X4 uses the direct short-power release path in `EpubReaderActivity::loop()`, alongside existing clipping/footnote handling. Follow that pattern and guard against simultaneous Down release. Keep long-press power-down behavior unchanged.
- Existing power-button duration handling distinguishes Sleep (10 ms) from other actions (400 ms).
- Automatic inactivity sleep is out of scope.

### Reader lifecycle and rendering

- `ActivityManager::replaceActivity()` calls the outgoing activity's `onExit()`, destroys it, then enters the replacement; replacement clears the stack. Do not push the AI activity on top of the reader.
- `ReaderActivity::onExit()` flushes reader session state and releases reader state. Capture the current page text and metadata before replacement; keep only independently owned copies afterward.
- Existing `COVER_CUSTOM` routing distinguishes reader-originated sleep from system-menu sleep: reader sleep calls `renderCoverSleepScreen()`; system-menu sleep calls `renderCustomSleepScreen()`.
- The new AI cover substitution is restricted to reader-originated `COVER_CUSTOM` sleep. The system-menu custom screen and `/sleep.bmp` path are unchanged.
- The cover renderer supports zoom-to-fit. The AI candidate is accepted only if its header parses and the renderer successfully reads/draws the image; otherwise the original book cover is attempted.

### HTTP and Wi-Fi

- `AiScreensaverService` is application-lifetime. Its worker performs one local IPv4 HTTP GET, without redirects; it does not write files or access the display.
- Wi-Fi connection and teardown remain on the main task. An existing station connection is reused; otherwise the service joins saved credentials in STA mode. AP/AP+STA mode is refused.
- Worker GET uses a 5-second HTTP socket timeout and cooperative atomic cancellation. Joining Wi-Fi can take up to 45 seconds if a connection is needed; do not treat the socket timeout as an end-to-end deadline.
- `HttpDownloader` is single-flight. The diagnostic sequence is serialized: worker GET, then main-task POST, then main-task BMP download.
- `downloadToFile()` writes a `.part` file and replaces the destination only after success. No concurrent HTTP requests.

### Worker lifecycle implementation (2026-11-05; resolved)

- `AiScreensaverService` is application-lifetime and owned by main.
- The worker task receives copied request data, performs one HTTP GET, and publishes its result. It does not retain activity pointers, access SD storage, draw, or acquire `RenderLock`.
- Worker task uses an 8 KiB stack at priority 1. Main task reaps it after it reaches its suspended completion state.
- Before sleep, `ActivityManager::prepareForSleep()` requests cancellation under `RenderLock`; `aiScreensaverService.waitUntilIdle()` then drains the worker outside the lock before plugin delivery and Wi-Fi shutdown.
- Worker GET uses a 5-second socket timeout; Wi-Fi association can take up to 45 seconds if needed. The user accepted the current 5-second delay for now.
- User-reported X4 test of `x4-ai-worker-lifecycle-firmware.bin`: GET Pass, POST Pass, GET BMP Pass, BMP Saved.
- The local diagnostic POST and BMP download run sequentially on the main task. This is not the final AI generation workflow.

### Event behavior

- Device `sleep.enter` event sends POST to the local PC; Wi-Fi activates automatically for subscribed event.
- Event contains ID, timestamp, book path, percent, and end_percent.
- Percentage can differ slightly from visible UI.
- Browser plugins and device event manifests are separate.
- No `{event.text}`, no arbitrary reader APIs; `reader.session` is excluded unless the user explicitly asks to revisit it.

### BMP validation

- Validation should be permissive: confirm readable BMP/header, supported format, pixel data exists, and file is not truncated/corrupt.
- Do not reject dimensions, orientation, aspect ratio, or exact file size.
- Renderer supports zoom-to-fit.
- Prompt for portrait orientation. 800×400 is a target, not a strict validation requirement.

## Intended runtime behavior (not yet implemented/verified)

- The eventual short-power AI action captures the visible page text and book metadata while the EPUB reader is alive.
- The image-generation request uses a portrait-oriented prompt; 800×400 is a target, not a BMP validation rule.
- During generation, retain the current book cover as fallback. Reuse the current book's last valid generated image while waiting; do not show an unrelated previous book's image.
- Poll the local worker once per minute, with Wi-Fi off between checks. Maximum wait is five minutes for now.
- When a valid image arrives, store it with book identity and image metadata. The final storage path/state schema is still to be settled.
- Reader-originated sleep with `Cover+Custom` displays the AI cover; system-menu sleep retains the user's custom screensaver.
- If generation times out or the image is invalid, use the original book cover. Do not alter automatic inactivity sleep or introduce a new sleep mode.
- The current `AI_SCREENSAVER` action is only a context preview. The generation and sleep handoff are future work.

## Working protocol

At the start of a new chat in this Project:
1. Read this file, DECISIONS.md, and TEST_LOG.md.
2. Verify live GitHub branch head and any in-progress workflow/build before claiming status.
3. Continue from “Current milestone / Next actions”.
4. Do not re-ask settled questions or repeat completed tests.
5. Update the records when a meaningful decision, implementation, build, or test result occurs.
6. Before a chat becomes unwieldy, update records and leave a concise next action.

## Document ownership

These files in the repository root are the persistent source of truth:
- `PROJECT_HANDOFF.md`: current state and next action.
- `DECISIONS.md`: settled decisions and constraints.
- `TEST_LOG.md`: chronological tests and build verification.

When records disagree, prefer the newest verified repository/device evidence and update all affected documents. Never silently turn an assumption into a confirmed fact.


## Record format / maintenance policy
- These records are primarily an assistant-operational memory store, not user documentation.
- Optimize for retrieval precision, compactness, explicit state, and low ambiguity rather than polished prose.
- Prefer concise structured fields, stable labels, exact identifiers, dates, and explicit status tags: CONFIRMED / HYPOTHESIS / UNTESTED / BLOCKED / DONE.
- Avoid repeating the same facts across files; handoff stores current state, decisions stores durable constraints, test log stores chronological evidence.
- User is not expected to read or maintain these files. Assistant owns updates and may translate or summarize on request.


## Historical worker/network investigation (superseded 2026-11-05)

The November 2 investigation below is superseded by the implemented application-lifetime worker contract documented above. The current X4 worker lifecycle and local BMP transfer have been device-tested; the new Cover+Custom image-display path has a separate build and user device test pending.