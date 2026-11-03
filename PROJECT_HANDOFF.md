# Xteink X4 AI Screensaver Project Handoff

Last updated: 2026-11-02

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
- Latest branch HEAD at 2026-11-02 record update: `29b4bb1ced7fea7d3a0347e6dd8a03078179f50c` (documentation/workflow-only changes after the diagnostic firmware commit).
- Project records are versioned on the same branch; verify the live branch HEAD when resuming.
- Latest commit message: “Keep cumulative results in local X4 diagnostic”.
- Latest recorded GitHub Actions check: run/job `125165358875`, completed successfully at 2026-11-02 08:19:28 UTC.
- Workflow name still says “Build CrossPoint 2.0.0 X4 baseline”.
- Latest build artifact retrieved and inspected: artifact ID `13125857099`, ZIP `x4-cumulative-diagnostic.zip`.
- ZIP contains `firmware.bin`, 5,590,784 bytes; SHA-256: `df2b6cd68872c87635c055c4289b080b9dceef1ecbb07ec28dd451f0bc87bb1f`.
- This binary is built from code commit `98811e06`; installation and successful device test are confirmed in the current milestone below.

## Current milestone

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

### Next milestone

**Worker/lifecycle investigation — architecture direction identified; cancellation and policy details remain open.**

1. Proposed minimal abstraction: an activity-owned `ActivityWorker` wrapper that encapsulates task creation; the activity must not call raw `xTaskCreate`. Worker receives copied inputs, owns its result/progress state, and never retains or calls the Activity.
2. Worker performs one synchronous HTTP operation at a time. Activity `loop()` remains responsive and reads synchronized progress/results. Worker never renders or acquires `RenderLock`.
3. `prepareForSleep()` requests cooperative cancellation and waits for the HTTP operation to unwind before sleep-event delivery. The worker must not require `RenderLock`; `onExit()` performs idempotent final stop/reap before destruction. `vTaskDelete` may reap only a completed/parked task, never interrupt an active HTTP call.
4. Resolve thread-safe cancellation and bounded plain-HTTP connection cancellation. Existing cancellation accepts `const bool*`; `fetchUrl()` and `postForm()` lack cancellation; the pinned SDK's plain HTTP DNS/connect path does not poll the abort callback and uses a 60-second connection timeout.
5. Confirm Wi-Fi ownership and use only `HalStorage`/`HalFile` for cross-task file work. HAL serializes individual SD operations; direct SdFat use is not thread-safe.
6. Resolve conflicting repository worker guidance (`docs/activity-manager.md` versus `.agents/rules/ui-activities.md`) into an explicit contract before implementation.
7. Verify image-display path and main-loop deep-sleep handoff, then propose the smallest implementation slice. Build only when a firmware change is ready for a meaningful X4 device test.

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
- **Pinned SDK cancellation detail CONFIRMED:** HTTPS connection setup passes the abort callback through DNS/TCP/TLS; read loops also poll it. The plain HTTP path calls host resolution and TCP connect without the abort callback, using the configured connection timeout (currently 60 seconds). This is a concrete cancellation-latency gap for local plain-HTTP requests.

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
