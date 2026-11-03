# Xteink AI Screensaver — Decision Record

Last updated: 2026-11-02

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

- AI activity owns display rendering.
- Network code must not draw to the display.
- Capture page text once and retain an owned copy.
- Do not create an ad-hoc FreeRTOS task inside an activity. The pinned source contains conflicting worker guidance: `docs/activity-manager.md` says not to call `xTaskCreate` in activities and to propose a lifecycle-aware Worker abstraction; `.agents/rules/ui-activities.md` says workers should be stopped/deleted in `onExit()` with `vTaskDelete()`. No reusable general-purpose Worker implementation was found in the inspected firmware/SDK tree. Resolve this conflict and define an explicit lifecycle/cancellation contract before implementation.
- Use existing activity stack and render-lock mechanisms.
- Network polling and transition to deep sleep must be validated.

## Networking and worker cancellation

- Caller establishes Wi-Fi; `HttpDownloader` does not connect.
- Use existing `HttpDownloader` APIs rather than inventing another HTTP stack.
- Do not run concurrent HTTP requests. `HttpDownloader` explicitly documents itself as not thread-safe and permits one fetch at a time.
- Existing `downloadToFile()` and `postToFile()` cancellation parameters are `const bool*`; this is not a thread-safe cross-task cancellation contract. `fetchUrl()` and `postForm()` have no cancellation parameter. Do not share an ordinary `bool` unsynchronized between tasks.
- The secure GET/download path passes a cancellation callback into the pinned SDK's resumable fetch implementation. This is useful but does not prove prompt, thread-safe cancellation across every connection stage.
- HTTP timeout is 60 seconds per socket operation, not a total request deadline or a guarantee of bounded sleep cancellation.
- `ActivityManager::prepareForSleep()` invokes activity hooks while holding `RenderLock` and is called before sleep plugin event delivery. Worker cancellation/completion must not need `RenderLock`, must finish before sleep-event network delivery, and must have a bounded wait.
- `onExit()` is also a required stop-and-wait fallback for non-sleep activity transitions. Do not forcibly delete a task while its network call or C++ resource cleanup is active.
- The exact worker creation/priority/stack/cleanup policy remains unresolved because repository guidance conflicts; resolve it before implementation.
- Respect `downloadToFile()` staging behavior (`.part`, replace on success, remove partial on failure).
- Keep service local-only where practical; no public exposure of the local receiver.

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
