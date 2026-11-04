## 2026-11-04 — Worker lifecycle/cancellation design and CI policy — SOURCE VERIFIED / DESIGN SETTLED

Pinned CrossPoint 2.0.0 branch `ai-screensaver/2.0.0-baseline`; pinned FreeInk SDK `5e94fe7480f019c89fbf691aef87523eb1b46704`; platform pioarduino `55.03.311` / Arduino Core `3.3.11`.

- **Activity lifecycle CONFIRMED:** `ActivityManager::loop()` processes pending Pop/Replace actions after the activity loop and acquires `RenderLock` for destruction. A nonblocking pre-exit readiness gate can be checked before acquiring the lock; while not ready, keep the pending action and allow the activity to continue looping.
- **Sleep ordering CONFIRMED:** `ActivityManager::prepareForSleep()` acquires `RenderLock` and calls activity hooks. `main.cpp` invokes it before emitting/draining sleep plugin events. Therefore the hook only requests cancellation; the main task must wait for worker completion and Wi-Fi cleanup after the hook returns and before event drain.
- **HTTP cancellation CONFIRMED:** `HttpDownloader` is single-flight/not thread-safe. Existing file-download cancellation uses an ordinary `bool*`; `fetchUrl()` and `postForm()` lack cancellation parameters. A synchronized atomic cancellation token and cancellable GET path are required.
- **Timeout path CONFIRMED:** pinned SDK `SecureHttpClient::setTimeout()` configures plain-HTTP connection and socket timeouts. The SDK polls abort during response reads; plain-HTTP TCP connect does not poll abort. The pinned Arduino Core 3.3.11 `NetworkManager::hostByName()` parses numeric IP literals before DNS, so the first local endpoint can use an IPv4 literal to avoid DNS.
- **Wi-Fi contract DECIDED:** main task loads/copies saved credentials, reuses an existing connected STA, and only connects when Wi-Fi is off. It does not commandeer an active non-connected mode. It disconnects only a connection established by the AI service. Worker performs HTTP only; sleep event drain starts after worker completion.
- **Worker lifecycle DECIDED:** application-lifetime service owns worker/task/request/result; worker has no Activity pointer or render access. ActivityManager defers exits until service idle. `onExit()` never blocks or forcibly deletes an active task. Worker unwinds RAII, publishes completion, and self-deletes.
- **Five-second target ACCEPTED:** use a 5,000 ms per-socket/connect timeout for the initial local request. It is not a strict total deadline; keep exit pending until cleanup really completes. Reducing the delay is a future optimization.
- **CI run inspected:** `41885073421` was a push-triggered `CI (build)` on `develop`. All eight board builds succeeded. `cppcheck` setup failed because Ubuntu 26.04 could not locate `libpcre3`; `Test Status` then failed. This is a CI environment/setup failure, not a firmware compile failure.
- CI policy is being narrowed to PR-only, heavy jobs only for relevant code changes, and X4-only firmware builds. PR firmware links will expose only X4.
- **Not tested:** no worker implementation, firmware build, or X4 device test has been performed for this milestone.

## 2026-11-02 — Worker design constraints: activity patterns, storage, and cancellation — SOURCE VERIFIED

Additional pinned-source inspection: CrossPoint 2.0.0 branch `ai-screensaver/2.0.0-baseline`; SDK submodule `5e94fe7480f019c89fbf691aef87523eb1b46704`.

- `ClockSyncActivity` and `FontDownloadActivity` execute synchronous HTTP calls from activity/main-loop logic after `requestUpdateAndWait()`; this displays progress before blocking but does not keep the activity loop responsive.
- `HalStorage` uses a recursive FreeRTOS mutex around SD operations. `HalFile` read/write/close calls are wrapped by the same lock. Its source explicitly states SdFat is not thread-safe. Cross-task file I/O must use `Storage`/`HalFile`, not direct SdFat; each `HalFile` object should have one owner.
- `WifiPowerSaveGuard` changes global Wi-Fi power-save mode for the request lifetime. `HalPowerManager::Lock` has an explicit current limitation: only one lock at a time.
- **Pinned SDK cancellation verified:** `SecureHttpClient::ensureConnected()` passes the abort callback through HTTPS DNS/TCP/TLS connection setup. Its plain HTTP branch calls `hostResolves()` and `WiFiClient::connect()` without the abort callback and uses the configured connection timeout (60,000 ms currently). HTTP read loops poll abort callbacks.
- **Architecture direction proposed, not implemented:** activity-owned `ActivityWorker` wrapper encapsulates task creation; worker receives owned inputs and owns synchronized progress/results; no Activity pointer, rendering, or RenderLock access; one HTTP request at a time; cooperative cancellation and completion handshake; `prepareForSleep()` cancels and waits before sleep-event delivery; `onExit()` idempotently reaps before destruction. `vTaskDelete` must never interrupt active HTTP/resource cleanup.
- **Still unresolved:** reconcile conflicting repository worker instructions; thread-safe cancellation API for all required HTTP methods; bounded cancellation during plain HTTP resolution/connect; Wi-Fi ownership; task priority/stack/reaping details.
- No code changes, firmware build, or device test.

## 2026-11-02 — Worker lifecycle and network cancellation source investigation — PARTIALLY RESOLVED (not device-tested)

Pinned source: CrossPoint 2.0.0 branch `ai-screensaver/2.0.0-baseline`; SDK submodule `5e94fe7480f019c89fbf691aef87523eb1b46704`.

- **Execution model CONFIRMED:** ESP32-C3 is single-core; the existing render task is owned by ActivityManager. No reusable general-purpose Worker implementation was found in the inspected firmware/SDK source trees.
- **Repository policy conflict CONFIRMED:** `docs/activity-manager.md` says do not call `xTaskCreate` inside activities and proposes a lifecycle-aware Worker abstraction; `.agents/rules/ui-activities.md` says workers require synchronization and `vTaskDelete()` in `onExit()`. Resolve this conflict and define a safe contract before creating a task.
- **Sleep hook CONFIRMED:** `ActivityManager::prepareForSleep()` invokes activity hooks while holding `RenderLock`. The main deep-sleep path calls it before sleep plugin delivery, then later replaces the activity via `goToSleep()`, shuts down Wi-Fi, and prepares storage/display for deep sleep.
- **Deadlock constraint:** worker shutdown from `prepareForSleep()` cannot depend on acquiring `RenderLock`. Worker completion must precede sleep-event delivery, which also uses network access.
- **HTTP serialization CONFIRMED:** `HttpDownloader.h` explicitly says it is not thread-safe and one fetch runs at a time.
- **Cancellation gap CONFIRMED:** `downloadToFile()` and `postToFile()` accept `const bool*`; plain shared-bool access is not a synchronized cross-task contract. `fetchUrl()` and `postForm()` expose no cancellation parameter.
- **SDK mechanism CONFIRMED:** secure resumable GET/download passes a cancellation callback into the pinned SDK. This does not establish safe cross-task flag access or bounded cancellation at every connection stage.
- HTTP timeout is 60,000 ms per socket operation, not a total request deadline and not proof of prompt cancellation.
- **Design direction (not approved contract):** activity-owned lifecycle-aware Worker; owned inputs/results; synchronized cooperative cancellation; single-flight network access; completion handshake before resources are released; no rendering in worker.
- **Unresolved:** task creation/priority/stack/cleanup policy; safe cancellation through HTTP APIs and plain-HTTP stages; bounded stop/wait; Wi-Fi ownership.
- No source files changed; no build; no device test. Documentation records only.

# Xteink AI Screensaver — Test and Build Log

Newest entries first. Record actual results only; label hypotheses explicitly.

## 2026-11-02 — Reader/screensaver integration source investigation — DESIGN PATH VERIFIED (not device-tested)

Pinned source: CrossPoint 2.0.0 branch `ai-screensaver/2.0.0-baseline` and SDK submodule `5e94fe7480f019c89fbf691aef87523eb1b46704`.

- **Current-page text CONFIRMED:** `Section::getTextFromSectionFile()` calls `loadPage(currentPage)` and extracts that page's text blocks; it does not return the whole chapter. It reconstructs words with spaces, so original layout/spacing and non-text elements are not preserved.
- Earlier user QR verification decoded text matching the visible page apart from formatting. QR remains out of the final workflow.
- **Replacement lifecycle CONFIRMED in source:** `ActivityManager::replaceActivity()` defers the replacement until the current activity loop returns, calls outgoing `onExit()`, destroys it, clears the activity stack, then enters the new activity.
- **Reader cleanup CONFIRMED in source:** `ReaderActivity::onExit()` flushes reader-session state; `EpubReaderActivity` owns its EPUB and Section through smart pointers; `Section` destructor runs its cleanup/suspend path. Exact low-level EPUB file-handle close behavior not independently verified.
- **X4 trigger path CONFIRMED in source:** X4 is a no-touch profile. Existing reader short-power handling uses `mappedInput.wasReleased(Button::Power)` and guards against simultaneous Down release. The touch-board-only bookmark/sync/reader-menu mapping is not the right path for original X4.
- Persisted `SHORT_PWRBTN` values 0–9 are occupied. Append `AI_SCREENSAVER=10`; add it to `shortPowerButtonOptions()`; do not renumber existing values. Keep long-press power-down unchanged.
- **Recommended capture handoff:** while reader is alive, copy page text and selected metadata into an independently owned `ReadingContext`; request `replaceActivity()`; begin screensaver file/network work only after reader teardown. Do not push the AI activity or retain pointers/references to reader objects.
- **Not device-tested:** no code changes or firmware build were made for this investigation.
- **Remaining investigation:** exact context field selection/API; lifecycle-safe network worker, cancellation and Wi-Fi ownership; existing image display and deep-sleep handoff.

## 2026-11-02 — Cumulative diagnostic: network transfer, save, and open — PASS

- Firmware: cumulative diagnostic from commit `98811e06eb687e83fba58eef2de86a8a2fb409f0`.
- Workflow run: `41411501888`; job/check: `125165358875`; successful.
- Artifact: `x4-cumulative-diagnostic.zip`, ID `13125857099`; binary size 5,590,784 bytes; SHA-256 `df2b6cd68872c87635c055c4289b080b9dceef1ecbb07ec28dd451f0bc87bb1f`.
- Receiver observed sleep-event `POST /event`: JSON body 235 bytes; HTTP 200.
- Receiver observed diagnostic `GET /event`: HTTP 200; X4 displayed **Get pass**.
- Receiver observed diagnostic `POST /event`: form body `test=x4-post-test` (17 bytes); HTTP 200; X4 displayed **Post pass**.
- Receiver observed `GET /test.bmp`: sent 960,054 bytes; HTTP 200; X4 displayed **Get test.bmp Pass**.
- X4 reported `Bmp file: SAVED`.
- User opened `x4-download-test.bmp` on-device; it processed and opened successfully.
- Diagnostic BMP was landscape. User confirmed portrait is preferred for generated screensaver images.
- **Result: PASS** for local event POST, diagnostic GET/POST, BMP HTTP transfer, filesystem save, and BMP processing/opening.
- **Scope limit:** this validates the downloaded BMP file and normal image-open path; it does not yet validate integration with the sleep/screensaver activity or reading-context capture.
- No additional build required for these record updates.


## 2026-11-02 — Cumulative diagnostic firmware build

- Commit: `98811e06eb687e83fba58eef2de86a8a2fb409f0`
- Message: “Keep cumulative results in local X4 diagnostic”.
- GitHub Actions check/job: `125165358875`.
- Workflow run: `41411501888`.
- Result: **completed / success**.
- Completed: 2026-11-02 08:19:28 UTC.
- Branch: `ai-screensaver/2.0.0-baseline`.
- Code change: GET, POST, and BMP attempts/results are tracked separately; result screen shows cumulative outcomes and BMP file status.
- Artifact retrieval and binary hash verification: completed; see test entry below.
- Device installation and cumulative diagnostic test: subsequently confirmed; see test entry below.

## 2026-11-02 — Cumulative diagnostic artifact retrieved and verified\n\n- Artifact ID: `13125857099`.\n- ZIP: `x4-cumulative-diagnostic.zip`.\n- Contents: `firmware.bin`.\n- Firmware binary size: 5,590,784 bytes.\n- Firmware binary SHA-256: `df2b6cd68872c87635c055c4289b080b9dceef1ecbb07ec28dd451f0bc87bb1f`.\n- Artifact is associated with code commit `98811e06eb687e83fba58eef2de86a8a2fb409f0`.\n- Build succeeded; artifact inspected. Device installation remains pending.\n\n## 2026-11-02 — Updated local receiver script provided

- User said they would paste the revised PowerShell script into the existing `.ps1` file.
- Script creates a valid 800×400, 24-bit BMP test image.
- Port: 8765.
- Routes: GET `/test.bmp`; GET/POST `/event`; all other routes return 404.
- Logs accepted connection, request line, headers, request body, response, and disconnect.
- Q stops receiver.
- User has not yet confirmed starting this revised receiver.

## 2026-11-02 — Latest BMP diagnostic behavior (earlier diagnostic firmware)

- User reported GET succeeded in the earlier stage.
- Latest BMP test firmware run did not succeed.
- Device reported HTTP 0 for BMP download.
- Receiver output showed a sleep-event POST and diagnostic GET before a read timeout; it did not show the diagnostic POST or GET `/test.bmp`.
- Interpretation: no HTTP response was obtained. This does not prove TCP connection was never established.
- Device diagnostic UI overwrote earlier messages; cumulative results were implemented in commit `98811e06` to prevent loss of stage evidence.
- Firmware commit used for that test: `6802c434874d9eb5f38107d5158f3fd69694e134`.
- Artifact previously identified: `x4-bmp-download-diagnostic.zip`, artifact ID `13124283681`, run `41408268961`.
- Previously verified binary from that earlier artifact: 5,591,024 bytes; SHA-256 `85f3f1849c4b2b1a429f16089eb63cdf19bd4008d83c0bbed2a447345b2f1530`.
- This hash belongs to the earlier BMP diagnostic build, not the newer cumulative-results build.

## Earlier — POST diagnostic

- Device POST reached receiver at `POST /event`.
- Content-Type: `application/x-www-form-urlencoded`.
- Content-Length: 17.
- Body: `test=x4-post-test`.
- Receiver returned HTTP 200.
- Result: POST path worked in that test.

## Earlier — GET diagnostic

- User confirmed GET succeeded.
- Device displayed HTTP 200 and response preview “x4 get test received successfully”.
- Result: GET path worked in that test.

## Earlier — sleep event

- Receiver observed POST `/event` with a JSON sleep event.
- Payload included event ID, timestamp, book path, percent, and end_percent.
- Confirms device sleep-event POST reached local receiver.

## Earlier — reader page text verification

- User scanned the reader QR and decoded text matched the visible page text, formatting aside.
- This confirmed `getTextFromSectionFile()` as the relevant reader source for the final integration.
- QR is not part of the final user workflow.

## Baseline repository/build setup

- Personal fork: `https://github.com/thrilsekr/crosspoint-reader`.
- Branch: `ai-screensaver/2.0.0-baseline`.
- CrossPoint 2.0.0 base commit: `42d58daaabcf469a2e5e1ba871714fb9dcf51d29`.
- SDK submodule: `5e94fe7480f019c89fbf691aef87523eb1b46704`.
- Baseline workflow passed.
- Build uses PlatformIO Core 6.1.19 and pioarduino 55.03.311; environment `default`.
- Full SD backup completed.
- Built-in firmware update activity validates image header/segments/checksum/SHA trailer/chip/board and uses OTA partition; it does not format SD.
- Official CrossPoint 2.0.0 X3/X4 recovery firmware:
  https://github.com/crosspoint-reader/crosspoint-reader/releases/download/2.0.0/crosspoint-2.0.0-x3-x4.bin

## Test recording template

For each future test, record:
- Date/time:
- Firmware commit and binary SHA-256:
- Receiver/script version:
- Exact action performed:
- Device result:
- Receiver result:
- Interpretation:
- Confirmed / hypothesis / untested:
- Next action:


## 2026-11-02 — Worker cancellation source investigation (no code/build)

- CrossPoint branch: `ai-screensaver/2.0.0-baseline`; firmware baseline CrossPoint 2.0.0. SDK submodule pinned at `5e94fe7480f019c89fbf691aef87523eb1b46704`.
- Confirmed no reusable general-purpose Worker in the inspected firmware/SDK tree. ActivityManager owns the shared render task only. Repository worker instructions remain contradictory and require an explicit wrapper/lifecycle contract.
- Corrected earlier cancellation interpretation: both HTTPS and plain HTTP perform DNS/TCP setup without polling the caller's cancellation callback throughout those blocking stages. HTTPS checks abort before DNS and after TCP, and polls during TLS handshake; plain HTTP has no abort callback on its DNS/TCP path. `Network.hostByName()` has no caller-supplied timeout/abort parameter. TCP connection timeout currently inherits the configured 60-second HTTP timeout.
- `std::atomic` is available at configured `gnu++2a` language level; no target compile test performed.
- `WifiPowerSaveGuard` changes global Wi-Fi power-save mode. Together with the non-thread-safe `HttpDownloader` contract, this reinforces single-flight HTTP and explicit Wi-Fi ownership.
- Result: Worker architecture remains a viable direction, but bounded sleep cancellation is not yet proven. Need choose between SDK transport changes for bounded/cancellable DNS/TCP or a constrained endpoint/connection policy. No source code changed, no build triggered, no device test performed.
