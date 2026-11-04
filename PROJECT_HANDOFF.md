# Xteink X4 AI Screensaver Project Handoff

Last updated: 2026-11-04

## Mission

Build an AI-generated, reading-context-aware screensaver for the original Xteink X4 running **CrossPoint 2.0.0**.

The intended user flow is:
1. Read normally.
2. Deliberately trigger the AI screensaver with a short power-button press.
3. Capture the exact currently visible page text.
4. Generate a portrait-oriented image.
5. Deliver a validated BMP to the X4 and display it.
6. Fall back safely on failure or timeout.

The long-term design should not require a laptop. Laptop/PowerShell is currently for development diagnostics only.

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
- Working branch: `ai-screensaver/2.0.0-baseline`
- CrossPoint 2.0.0 base commit: `42d58daaabcf469a2e5e1ba871714fb9dcf51d29`
- Exact SDK submodule: `5e94fe7480f019c89fbf691aef87523eb1b46704`
- Latest firmware/source diagnostic commit: `98811e06eb687e83fba58eef2de86a8a2fb409f0`.
- Baseline branch HEAD before this documentation PR: `23be366a9b5bac3d125b93901b5a884c3b6a2f0b` (2026-11-03, “Record worker cancellation source investigation”).
- Project records are versioned on the same branch; verify the live branch HEAD when resuming.
- Latest commit message: “Keep cumulative results in local X4 diagnostic”.
- Latest recorded GitHub Actions check: run/job `125165358875`, completed successfully at 2026-11-02 08:19:28 UTC.
- Workflow name still says “Build CrossPoint 2.0.0 X4 baseline”.
- Latest build artifact retrieved and inspected: artifact ID `13125857099`, ZIP `x4-cumulative-diagnostic.zip`.
- ZIP contains `firmware.bin`, 5,590,784 bytes; SHA-256: `df2b6cd68872c87635c055c4289b080b9dceef1ecbb07ec28dd451f0bc87bb1f`.
- This binary is built from code commit `98811e06`; installation and successful device test are confirmed in the current milestone below.

## Current milestone

**Milestone: worker lifecycle and network-cancellation design — SETTLED (2026-11-04; implementation not started).**

### Settled architecture

- An application-lifetime `AiScreensaverService` owns the worker task, copied request input, cancellation state, synchronized result/progress, and task handle. The worker never stores or calls an Activity, renders, or acquires `RenderLock`.
- The service coordinates Wi-Fi, but the Arduino/main task performs Wi-Fi mode changes, connection setup, and cleanup. Reuse an already-connected station. If Wi-Fi is off, load and copy the existing saved credentials and connect as STA. Do not commandeer an active, non-connected Wi-Fi mode. Disconnect/turn off Wi-Fi only when this service started it.
- One HTTP request at a time. `HttpDownloader` is explicitly not thread-safe. Sleep-event network delivery must wait until the service is fully idle.
- Cancellation is cooperative and synchronized with an atomic token. The worker unwinds HTTP, stream/file, and `WifiPowerSaveGuard` resources before publishing completion, then self-deletes. The main-task service observes completion and clears the task handle. Never forcibly delete an active worker.
- Ordinary exit uses a nonblocking pre-exit gate in `ActivityManager`, called before acquiring `RenderLock` for Pop/Replace/ReplaceForSleep. The outgoing activity requests cancellation and reports not-ready until the worker and Wi-Fi cleanup are complete; the pending transition remains queued while the activity continues to run. `onExit()` only releases activity-local resources and never joins or blocks.
- Sleep's `prepareForSleep()` hook runs under `RenderLock` and only requests cancellation. The main task waits for service completion outside the lock and before plugin-event network delivery. Service-owned Wi-Fi is cleaned up before the event drain.
- Retain `WifiPowerSaveGuard`. Cross-task SD access must use `Storage`/`HalFile`, not direct SdFat.

### Cancellation and timing

- Add a per-request timeout override; leave the existing 60-second default unchanged for other firmware callers.
- Initial local HTTP request target: 5,000 ms per socket/connect operation, using an IPv4 literal. The pinned platform is 55.03.311 / Arduino Core 3.3.11; `NetworkManager::hostByName()` parses an IP literal with `IPAddress.fromString()` before attempting DNS. The pinned SDK's `SecureHttpClient::setTimeout()` applies to plain-HTTP connection and socket operations; HTTP read loops poll the abort callback.
- Five seconds is an initial transport/exit-latency target, not a strict total-request deadline. If cleanup takes longer, keep the transition deferred until the worker is actually finished; never kill it to meet the target. Reducing perceived Back/exit delay is a future optimization.
- Worker task priority/stack size remain implementation measurements: size from the real call chain and X4 stack high-water mark, not copied examples. Any future independent background HTTP client must join a shared network-serialization mechanism.

### Next engineering action

Implement the service, ActivityManager pre-exit gate, sleep drain wait, and atomic cancellable GET with a per-request timeout. Keep the first implementation to one local GET. Then build only the X4 when the code is ready for meaningful device validation. Device checks will cover Back cancellation, sleep cancellation before event drain, Wi-Fi reuse/cleanup, and no overlapping requests.

### GitHub Actions finding (2026-11-04)

- The X4 diagnostic workflow `.github/workflows/x4-baseline-build.yml` is `workflow_dispatch` only; merging PR #1 did not launch an X4 build.
- The separate `CI (build)` workflow was triggered by a direct push to `develop`. Run `41885073421` attempted all eight board builds (all succeeded), but the `cppcheck` job failed during setup because Ubuntu 26.04 could not locate `libpcre3`; the required `Test Status` job consequently failed.
- This PR updates the project branch CI policy to run on PRs, skip heavy checks for docs/workflow-only changes, and build only the X4 `default` environment. PR firmware links will list only the X4 artifact.
- No worker code, firmware build, or device test has been performed for this milestone.

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

**Confirmed from pinned CrossPoint 2.0.0 source inspection (2026-11-02):**

- `ActivityManager::replaceActivity()` is the appropriate transition mechanism: it defers replacement when called from the active activity, then calls the outgoing activity's `onExit()` and destroys it before entering the replacement.
- Replacement clears the activity stack. Do not push the AI activity on top of the reader; the reader should not remain underneath.
- `ReaderActivity::onExit()` flushes the reader session, emits the reader-exit event when subscribed, resets reader state, and releases reader-owned caches/state.
- `EpubReaderActivity` owns its EPUB via `std::shared_ptr<Epub>` and its current `Section` via `std::unique_ptr<Section>`. Destroying the reader releases those owned objects. The exact lower-level EPUB file-handle close behavior has not yet been independently verified.
- The activity manager uses the render lock while exiting/destroying the current activity, protecting the transition against concurrent rendering.
- Therefore the agreed lifecycle is: capture visible-page text and required metadata while the reader is alive; copy them into an independently owned context; request activity replacement; allow normal reader `onExit()` and destruction; then run the AI screensaver activity using only the copied context.
- The reader's existing bookmark, clipping, and progress persistence demonstrate ordinary SD writes during reader operation. This does not by itself prove arbitrary concurrent filesystem access is safe; the agreed replacement flow avoids needing reader/worker overlap.
- Existing `SleepActivity` handles `/sleep.bmp`, book-cover, and blank-screen behavior. Existing cover renderer supports EPUB/TXT/Markdown/XTC.
- Shared render task calls active activity `render()`; main task handles activity logic. `RenderLock` protects shared state/framebuffer.
- `requestUpdate()` and `requestUpdateAndWait()` are available.
- Network worker must never draw.
- `docs/activity-manager.md` advises against `xTaskCreate` inside activities and recommends a lifecycle-aware Worker abstraction. No general-purpose Worker abstraction was found in the exact SDK tree. Do not create an ad-hoc task without a lifecycle-safe design.
- The source-supported capture and replacement approach is established. Exact `ReadingContext` definition/constructor and metadata field selection remain design work. Lower-level EPUB file-handle close behavior has not been independently verified; object destruction and Section cleanup order are confirmed. Lifecycle-safe network worker remains unresolved.

### HTTP and Wi-Fi

- Exact `src/network/HttpDownloader.cpp/.h` inspected.
- `HttpDownloader::fetchUrl(url, DataCallback)` is synchronous.
- `postForm()`, `postToFile()`, and `downloadToFile()` are available.
- `downloadToFile()` writes a `.part` file and replaces destination only after success; failure removes the `.part` file.
- GET/download secure path uses `WifiPowerSaveGuard`, `freeink::FetchOptions`, `freeink::fetchResumable`, SecureHttpClient/wolfSSL; plain HTTP uses WiFiClient.
- `postForm()` separately creates a `WifiPowerSaveGuard`, sets form content type, and sends POST.
- Caller must establish Wi-Fi; downloader does not connect.
- HTTP timeout is 60 seconds; max redirects 5.
- HTTPS checks memory (40 KB free heap, 20 KB largest allocation).
- Do not issue concurrent requests.
- Shared cancellation bool would require synchronization.
- Main.cpp Wi-Fi connect path uses saved credentials, checks plugin connection needs/battery, connects, syncs clock, and drains events.
- `WifiCredentialStore`: `WIFI_STORE.loadFromFile()`, `WIFI_STORE.findCredential(WIFI_STORE.getLastConnectedSsid())`.
- `TaskWatchdog.h` has `resetTaskWatchdogIfSubscribed()`.

### Worker lifecycle and cancellation investigation (2026-11-02)

**CONFIRMED from pinned CrossPoint 2.0.0 source:**
- The ESP32-C3 is single-core. The main Arduino loop and ActivityManager render task are existing execution contexts; the render task is manager-owned.
- No reusable general-purpose Worker implementation was found in the inspected firmware and pinned SDK source trees.
- Repository guidance conflicts: `docs/activity-manager.md` explicitly says not to call `xTaskCreate` inside activities and to propose a lifecycle-aware Worker abstraction; `.agents/rules/ui-activities.md` says workers require synchronization and `vTaskDelete()` in `onExit()`. This policy conflict must be resolved before implementation; neither instruction alone defines a safe cancellation/join contract.
- `ActivityManager::prepareForSleep()` invokes activity hooks while holding `RenderLock`. The main deep-sleep path calls it before sleep plugin event delivery; `ActivityManager::goToSleep()` later replaces the activity, causing `onExit()` and destruction.
- A worker stop/wait from `prepareForSleep()` must not need `RenderLock`; otherwise it can deadlock. It must complete before sleep-event network delivery to preserve single-flight HTTP ownership.
- `HttpDownloader` documents itself as not thread-safe: one fetch at a time. `downloadToFile()` and `postToFile()` accept `const bool* cancelFlag`; the current plain-bool contract is not safe for unsynchronized cross-task access. `fetchUrl()` and `postForm()` do not expose cancellation.
- `runGetSecure()` passes a cancellation callback to the pinned SDK `fetchResumable()` path. This is useful existing machinery, but it does not by itself establish thread-safe cancellation or bounded cancellation latency for every network stage.
- HTTP timeout is configured to 60,000 ms per socket operation; do not treat this as a 60-second total request deadline or as proof that sleep cancellation is prompt.
- `main.cpp` performs `activityManager.prepareForSleep()`, sleep-event delivery, `goToSleep()`, Wi-Fi shutdown, and deep-sleep preparation in that order. AI activity must stop and finish its network work before the sleep-event drain starts.

**DESIGN DIRECTION (not yet an approved implementation contract):** a small lifecycle-aware Worker owned by the AI activity, with independently owned request input/result, synchronized cooperative cancellation, one network operation at a time, and a completion handshake before resources are released. No worker rendering and no forced task deletion while HTTP/C++ resources are active.

**UNRESOLVED:** exact task creation/priority/stack/cleanup policy; atomic cancellation integration through all HTTP APIs and SDK plain-HTTP connection stages; bounded completion strategy; Wi-Fi ownership and coordination. No code, build, or device test performed for this investigation.

- **Existing network activity pattern CONFIRMED:** `ClockSyncActivity` and `FontDownloadActivity` perform synchronous HTTP work from activity/main-loop logic after forcing a render. This blocks normal activity-loop processing and is not the desired AI screensaver model.
- **Storage concurrency boundary CONFIRMED:** `HalStorage` uses a recursive mutex around SD operations and `HalFile` read/write/close operations; its implementation explicitly notes SdFat is not thread-safe. Worker file I/O must go through `Storage`/`HalFile`, not direct SdFat, and a single `HalFile` instance should have one owner at a time.
- `WifiPowerSaveGuard` changes the global Wi-Fi power-save mode for the duration of a request. `HalPowerManager::Lock` currently supports only one lock at a time. Avoid concurrent network operations or overlapping power-lock assumptions.
- **Pinned SDK cancellation detail CORRECTED (2026-11-02):** both HTTPS and plain HTTP have uncancellable DNS/TCP setup. HTTPS passes the abort callback into `SecureClient`, but `SecureClient::connect()` checks it before `hostResolves()`, then performs `Network.hostByName()` and TCP connect synchronously; it checks again after TCP and polls during the TLS handshake. Plain HTTP calls `hostResolves()` and `WiFiClient::connect()` without an abort callback. The TCP connection timeout is configured from the HTTP timeout (currently 60 seconds); the `Network.hostByName()` API exposes no caller-supplied timeout/abort parameter. Do not claim bounded cancellation until this path is addressed or an explicit safe policy is established.

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

- Maximum generation wait: 5 minutes.
- Show fallback while waiting.
- Same book: retain existing generated image while waiting; replace when new image arrives.
- Different book: show current book cover art until new image arrives.
- Wi-Fi briefly on, check once per minute, off between checks.
- If image arrives sooner, install/show and sleep immediately.
- Timeout: keep cover, not previous unrelated generated image.
- Archive old image only after new one downloads and validates.
- Track active image book identity/path/hash and image ID/checksum/timestamp to avoid duplicates.
- Suggested SD layout:
  ```
  SD card/
  ├── sleep.bmp
  └── .crosspoint/
      └── ai-screensaver/
          ├── state.json
          ├── incoming.bmp
          └── archive/
  ```
- Archive outside `.sleep` to avoid random sleep image selection.
- `/sleep.bmp` priority over `.sleep` was believed from source; revalidate before final integration.
- Simulated sleep: short press triggers; device stays awake showing fallback while Wi-Fi polls every minute; on completion/timeout normal deep sleep. No timer wake required.
- Capture page text once at trigger. No page turning/interactivity during simulated sleep.

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


## 2026-11-02 — Worker/network cancellation follow-up

- **CONFIRMED:** no reusable general-purpose Worker exists in the inspected firmware/SDK tree. `ActivityManager` owns only the render task. The activity docs still conflict: one prohibits raw task creation in activities and calls for a lifecycle-aware Worker proposal; the agent rule requires worker shutdown/deletion in `onExit()`.
- **CONFIRMED:** `platformio.ini` sets `-std=gnu++2a`; `std::atomic` is available at the configured language level, though target compilation/use has not been tested. A FreeRTOS synchronization primitive is also viable.
- **CONFIRMED:** the pinned SDK has a `SecureHttpClient::connect(AbortCallback)` entry point, but it does not make DNS/TCP cancellable. `SecureClient::connect()` calls `hostResolves()` (`Network.hostByName`) before TCP connect, checks abort only before DNS and after TCP, then polls during TLS handshake. Plain HTTP has no abort callback in its DNS/TCP branch. The DNS API has no caller-supplied timeout/abort parameter; TCP connection timeout is currently 60 seconds.
- **CONFIRMED:** `WifiPowerSaveGuard` changes global Wi-Fi power-save state. Combined with `HttpDownloader`'s not-thread-safe contract, network requests must be single-flight and coordinated with main-task Wi-Fi mode/disconnect operations.
- **DESIGN DIRECTION:** an activity-owned `ActivityWorker` wrapper remains viable, but its cancellation contract cannot promise prompt sleep transition until DNS/TCP behavior is bounded or the request is constrained to a verified connection strategy. Worker must unwind RAII resources before completion; no forced deletion while network code is active.
- **NEXT:** decide whether to (A) extend the pinned SDK transport with cancellable/bounded DNS and TCP setup, or (B) constrain endpoint addressing and accept a documented connection timeout. Then settle worker completion/reap handshake, stack/priority budget, and Wi-Fi ownership. No source edits, build, or device test performed.
