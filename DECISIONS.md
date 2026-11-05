# Xteink AI Screensaver — Decision Record

Last updated: 2026-11-04

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

## Reader-to-screensaver lifecycle

- Capture the exact visible-page text and required book metadata while the reader is still alive.
- Copy the captured data into an independently owned context; the screensaver must not retain pointers/references into the reader or its `Section`.
- Replace the reader activity with the AI screensaver activity using `ActivityManager::replaceActivity()`; do not push the screensaver onto the activity stack and leave the reader underneath.
- Let the normal reader `onExit()` and destruction path run before screensaver file/network work begins. This flushes reader session state and releases reader-owned EPUB/Section objects and caches.
- The reader's normal bookmark/clipping SD writes are not evidence that arbitrary concurrent filesystem access is safe. Prefer the sequential ownership boundary above.
- AI activity owns the copied context and screensaver workflow. Network worker never renders or manipulates the display.
- On completion/failure, hand off to the existing main-loop deep-sleep orchestration. The AI activity must not duplicate hardware shutdown/deep-sleep steps.

**Source-verified, not device-tested (2026-11-02):** `ActivityManager::replaceActivity()` defers replacement, exits/destroys the outgoing activity, clears the stack, and enters the replacement. `ReaderActivity::onExit()` flushes reader-session state and performs reader cleanup. `EpubReaderActivity` owns the EPUB and current Section through smart pointers. `Section::~Section()` performs its cleanup/suspend path. Exact lower-level EPUB file-handle closure and the concrete `ReadingContext` API remain unverified.

## Architecture and rendering

- AI activity owns display rendering. Network code never draws to the display.
- Capture page text once and retain an owned copy.
- Use the application-lifetime `AiScreensaverService` for network work; activities do not own worker tasks.
- The worker never retains an Activity pointer, calls Activity methods, renders, or acquires `RenderLock`.
- Use existing activity stack and render-lock mechanisms. ActivityManager's pre-exit gate runs outside `RenderLock`; `onExit()` is never a blocking join point.
- Network polling and transition to deep sleep must be validated on the X4.

## Worker lifecycle contract (SETTLED; first implementation in PR #3)

- `AiScreensaverService` is application-lifetime and owns the worker handle, copied URL, cancellation token, result state, and Wi-Fi ownership. It is advanced only by the Arduino/main task.
- Main task owns Wi-Fi mode changes, credential loading, station connection, and cleanup. Reuse an already-connected STA. If Wi-Fi is off, load/copy the saved last-connected credentials and connect as STA. If Wi-Fi is already active but not connected, do not commandeer it. Keep a service-owned connection available for sequential diagnostic requests; release it only on activity exit or sleep. Never disconnect a connection the service did not establish.
- Worker performs one HTTP GET only. It does not access SD storage. It owns the HTTP call and its `WifiPowerSaveGuard` lifetime; all RAII cleanup completes before result publication.
- Worker publishes the result and suspends itself. The main task confirms the task is suspended and reaps it. The service-owned Wi-Fi connection remains available to the activity's sequential POST/BMP diagnostic steps, then is released by the main task on activity exit or sleep. Never forcibly delete an active HTTP task.
- Cancellation is a `std::atomic<bool>` token passed through `HttpDownloader::FetchOptions`. Result data is synchronized with a mutex. Do not share an ordinary `bool` between tasks.
- ActivityManager calls `prepareToExit()` outside `RenderLock` before Pop/Replace/ReplaceForSleep. Replace also gates all stacked activities that it will destroy. The outgoing activity requests cancellation and returns not-ready until service cleanup is complete; the pending transition remains queued while the activity continues to run.
- `onExit()` runs under `RenderLock` and never blocks, joins, or deletes a worker. `prepareForSleep()` only requests cancellation; main task waits for service idle outside the lock and before plugin-event network delivery.
- Keep HTTP single-flight. No second request or sleep-event HTTP drain may overlap the worker.
- Initial task stack is 8 KiB, with a high-water mark logged. This is provisional until measured on the X4; adjust from evidence, not copied examples.
- Initial local request timeout is 5,000 ms per socket/connect operation. This is not a strict total-operation deadline; Back/exit stays deferred until all cleanup completes. Reducing perceived exit delay is a future optimization.
- The initial service accepts only RFC1918 or link-local IPv4 plain-HTTP URLs, and redirects are disabled. This avoids the pinned SDK's synchronous DNS stage and prevents the diagnostic request from following an external redirect.

## Networking and worker cancellation

- Use existing `HttpDownloader`; do not introduce another HTTP stack. It remains single-flight and not thread-safe.
- `FetchOptions` now has a per-request `timeoutMs` (default 60,000 ms) and an optional pointer to a `std::atomic<bool>` cancellation token. The cancellable GET passes that token to the pinned SDK's `SecureHttpClient::GET` abort callback. Existing callers keep the 60-second default.
- Pinned SDK commit `5e94fe7480f019c89fbf691aef87523eb1b46704`: `SecureHttpClient::setTimeout()` applies to plain HTTP socket and connection timeout. Header/body read loops poll abort. Plain HTTP TCP connect does not poll cancellation; it is bounded by the connection timeout.
- Pinned Arduino Core 3.3.11 / pioarduino platform 55.03.311: `NetworkManager::hostByName()` parses an IP literal with `IPAddress.fromString()` before attempting DNS. The first local GET validates RFC1918/link-local IPv4, disables redirects, and uses a 5,000 ms per-operation timeout.
- The 5-second timeout is not a strict total request deadline. If a request or cleanup takes longer, activity exit and sleep wait until the worker is actually idle. Never kill the task to meet a timing target.
- Existing `downloadToFile()` and `postToFile()` still accept `const bool*`; do not use that pointer as cross-task cancellation. `fetchUrl()` with `FetchOptions` is the first cancellable path; extend other methods only when required.
- `WifiPowerSaveGuard` changes global Wi-Fi power-save mode. Keep HTTP single-flight and wait for its destructor before main-task Wi-Fi shutdown or sleep-event network delivery.
- HAL storage serializes SD operations with a recursive mutex, but SdFat itself is not thread-safe. Any future worker file access must use `Storage`/`HalFile`, with one owner per `HalFile`; this initial worker performs no file I/O.
- `ClockSyncActivity` and `FontDownloadActivity` still perform synchronous HTTP from activity/main-loop logic. That is existing behavior, not a model for new worker-backed flows.

## Image and storage

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


## Worker design investigation correction (2026-11-02)

- `std::atomic` is available at the configured `gnu++2a` language level (`platformio.ini`); target build/use remains untested. A FreeRTOS event/semaphore primitive is an alternative.
- Neither HTTPS nor plain HTTP can currently promise immediate cancellation during DNS/TCP connection setup. For HTTPS, cancellation is checked before DNS and after TCP, and is polled during TLS handshake; plain HTTP has no abort callback in the connection branch. The `Network.hostByName()` interface has no caller-supplied timeout/abort parameter.
- Any `ActivityWorker` must serialize HTTP and Wi-Fi power-save changes, never overlap main-task Wi-Fi disconnect/mode changes, and fully unwind request/file/Wi-Fi RAII state before reporting completion.
- The Worker wrapper is a proposed reconciliation of conflicting repository guidance, not an approved implementation. No raw task creation or `vTaskDelete()` policy has been committed.

## Image and storage

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

## Worker lifecycle review policy

For the Xteink AI Screensaver work in this personal fork, the user is the sole project owner and has explicitly waived the repository's four independent firmware review requirement. The assistant performs the main-agent source/API review and reports its limits honestly. Automated CI and X4 device validation remain required; this project-specific waiver does not alter the repository's general review policy for other work.
