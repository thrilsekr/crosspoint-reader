# Xteink X4 AI Cover Art — Decision Record

Last updated: 2026-11-05

This file records decisions that should not be reopened without new evidence or an explicit user request.

## Project boundaries

- Target is the original Xteink X4 running CrossPoint 2.0.0.
- Keep the exact 2.0.0 firmware and SDK baseline. No beta, RC, dev firmware, or reinstall without an explicit planned test.
- Work only in the user's personal fork: `thrilsekr/crosspoint-reader`.
- Use SD-card sideloading for firmware.
- No USB serial access.
- Prefer local-only networking and minimal data exposure. Never ask for private LAN addresses or credentials.
- The user delegates project management and engineering execution. Continue without a “proceed” prompt unless user input, device action, or approval is required.
- Existing screensaver files do not need protection during diagnostics. This supersedes the earlier instruction to preserve them. Avoid unrelated destructive changes.

## Reader integration

- Trigger via configurable short power-button action, append persisted setting value `AI_SCREENSAVER=10` (values 0–9 are already occupied).
- Original X4 is a no-touch profile. Its existing reader loop handles short power release directly via `mappedInput.wasReleased(Button::Power)`; use this path, with the existing simultaneous-Down guard. Do not rely on the touch-board-only `MappedInputManager` bookmark/sync/menu mapping.
- Add the option to `SettingsList.h::shortPowerButtonOptions()`; leave long-press power-down unchanged.
- Do not renumber existing `SHORT_PWRBTN` values.
- Long press remains existing power-down behavior.
- Capture current-page text from the reader's `Section` using `getTextFromSectionFile()` at trigger time. In the pinned SDK, it loads `currentPage` and extracts that page's text blocks, not the whole chapter. It joins words with spaces and does not preserve all original layout/spacing or non-text elements.
- Do not use QR in the final flow; it was only a validation method.
- Do not depend on plugin/event payloads for reader text.
- Do not introduce `reader.session` unless user explicitly asks to revisit it.
- Automatic inactivity sleep is out of scope.

## Reader-to-cover workflow

- Capture the exact visible-page text and required book metadata while the reader is alive.
- Copy the context into independently owned data before replacing or destroying the reader.
- Use `ActivityManager::replaceActivity()`; do not push an AI activity on top of the reader.
- Let normal reader `onExit()` and destruction release the EPUB and current Section before network work proceeds.
- The eventual AI activity owns the copied context and UI. The application-lifetime network service never stores an activity pointer or draws to the display.
- The final generation flow must hand off to CrossPoint's existing main-loop deep-sleep orchestration; do not create a second shutdown path.
- The current `AI_SCREENSAVER=10` short-power action is still a temporary context-preview screen. It does not yet generate an image or initiate sleep.

## Architecture and rendering

- This is an EPUB cover-art replacement using CrossPoint's existing `Cover+Custom` sleep mode, not a new screensaver mode.
- Source-verified routing: when `APP_STATE.lastSleepFromReader` is true and the selected mode is `COVER_CUSTOM`, `SleepActivity` calls `renderCoverSleepScreen()`. When sleep originates from system menus, it calls `renderCustomSleepScreen()`.
- Prefer the downloaded/generated AI BMP only for reader-originated `COVER_CUSTOM` sleep. If it is missing, invalid, or cannot be rendered, fall back to the original book cover and then CrossPoint's existing no-cover fallback.
- Leave `renderCustomSleepScreen()` unchanged. Do not alter general sleep-mode selection, create a new mode, or overwrite `/sleep.bmp`.
- The current display milestone uses the temporary diagnostic file `/x4-download-test.bmp`. It is a test fixture, not the final per-book image store; stale-image identity and replacement rules remain future work.
- `renderBitmapSleepScreen()` supports zoom-to-fit. It now reports render success/failure so a failed downloaded BMP can fall back to the original book cover. Do not add dimension, orientation, or aspect-ratio rejection.
- Capture visible-page text and book metadata before reader teardown. Network code must not hold display locks or draw.
- Use existing activity replacement and main-loop sleep orchestration. No direct deep-sleep calls from the AI activity.

## Worker lifecycle contract (settled and implemented)

- `AiScreensaverService` is application-lifetime and owned by the main application, not by a reader or diagnostic activity.
- The main task owns service state and Wi-Fi lifecycle. The worker task receives copied request data and a cancellation token; it performs one HTTP GET and publishes a result.
- The worker does not access SD storage, draw to the display, acquire `RenderLock`, or retain activity pointers.
- The worker uses an 8 KiB stack at priority 1. Its result is reaped by the main task only after the worker has returned to its suspended completion state.
- Sleep preparation calls `ActivityManager::prepareForSleep()` under `RenderLock`, then `AiScreensaverService::waitUntilIdle()` outside the lock before plugin delivery and radio shutdown.
- Cancellation is cooperative. The current worker GET uses a 5-second HTTP socket timeout; Wi-Fi association can take up to 45 seconds when the service must join a saved network. These are not a single end-to-end deadline.
- The user accepted the current 5-second request delay for now; keep reducing cancellation latency on the future-improvement list.
- The current diagnostic activity performs POST and BMP file download sequentially on the main task after the worker GET completes. The worker itself does not download or save the BMP.

## Networking and worker cancellation

- `HttpDownloader` is single-flight and must not be called concurrently.
- `AiScreensaverService` currently supports a local IPv4 HTTP GET without redirects; DNS hostnames, HTTPS, and the final image-generation provider contract are not part of this diagnostic milestone.
- The service reuses an existing station connection or joins saved Wi-Fi credentials in STA mode. It refuses to take over AP or AP+STA mode.
- Worker cancellation uses an atomic flag checked by the downloader between network operations. Do not assume cancellation can interrupt every DNS or TCP connect operation instantly.
- `downloadToFile()` writes to a `.part` file and replaces the destination only after a successful download; failed partial files are removed.
- No concurrent GET, POST, or BMP download requests. The diagnostic sequence is deliberately serialized.

## Generated cover art and storage

- Generated screensaver imagery must be portrait-oriented (preferred device presentation). Landscape diagnostic fixtures are acceptable for transport/renderer tests only; they do not change the generation preference.
- 800×400 is a target, not a hard validation rule.
- BMP validation is permissive: validate readable header/format and complete pixel data, not dimensions/orientation/aspect ratio.
- Renderer supports zoom-to-fit.
- Archive outside `.sleep`.
- Intended generated image state tracks book identity/path/hash and image ID/checksum/timestamp.
- Do not archive/replace the current image until the incoming image has downloaded and validated.
- For same book, retain the existing generated image while waiting; for another book, show that book's cover until a new image arrives.
- Timeout target is five minutes; fallback should remain cover art, not an unrelated previous image.

## Diagnostic workflow

- Local Windows receiver uses port 8765.
- Diagnostic routes are strict: GET `/test.bmp`, GET/POST `/event`; unknown routes return 404.
- Receiver should log connection acceptance, request line, headers, body, response, and disconnect/timeout phase.
- Q should stop the receiver.
- Diagnostic image download must use `/x4-download-test.bmp`, not `/sleep.bmp`, unless a later test explicitly changes scope.
- Cumulative device diagnostic screen must retain outcomes from GET, POST, and BMP stages.
- Do not claim a transport failure means no TCP connection; HTTP 0 means no HTTP response was obtained.

## Collaboration

- Keep explanations short and actionable.
- Avoid repeating established context or asking the user to repeat completed tests.
- Ask only essential questions.
- Treat confirmed behavior, hypotheses, and untested ideas separately.
- Validate each milestone before moving on.


## Record format / maintenance policy
- These records are primarily an assistant-operational memory store, not user documentation.
- Optimize for retrieval precision, compactness, explicit state, and low ambiguity rather than polished prose.
- Prefer concise structured fields, stable labels, exact identifiers, dates, and explicit status tags: CONFIRMED / HYPOTHESIS / UNTESTED / BLOCKED / DONE.
- Avoid repeating the same facts across files; handoff stores current state, decisions stores durable constraints, test log stores chronological evidence.
- User is not expected to read or maintain these files. Assistant owns updates and may translate or summarize on request.


## Worker lifecycle implementation correction (2026-11-05)

The initial worker lifecycle investigation is resolved in the X4 integration branch. The application-lifetime service, cooperative cancellation, activity exit gate, and pre-sleep drain are implemented. User-reported X4 testing of the worker diagnostic firmware passed GET, POST, GET-BMP, and BMP-save checks. This validates transport and file saving, not the new sleep-cover renderer; that renderer has a separate build and device test.