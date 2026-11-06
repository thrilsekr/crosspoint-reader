## 2026-11-05 — Reconcile X4 integration and worker branches — MERGE PUSHED; BUILD PENDING

- Pushed merge commit `9c81c626e537cc957b6697b84a429e773341ce56` to `feat/ai-worker-lifecycle`.
- Parents: worker branch `c87f195973a2a5a6feb06970c83f9289d4d387b2` and integration branch `f8a87112849ec7606ba47dc14763fc871f4f3a13`.
- Combined the worker lifecycle/service with the integration branch's SDK connect-timeout patch, atomic HTTP APIs, per-book image storage, and reader sleep rendering.
- Resolved overlapping C++ changes using a three-way merge; combined the diagnostic includes for storage validation and the worker service.
- Updated `PROJECT_HANDOFF.md`, `DECISIONS.md`, and `AGENTS.md`. The repository-specific integration target now names the X4 feature branch rather than `develop`.
- PR #3 remains open/draft. GitHub reports `mergeable=true`; source is 59 commits ahead and 0 behind the target.
- **No build or device test has run on the reconciliation commit.** Next: run the manual X4-only workflow against the current worker branch head, inspect all jobs and artifact, then review the PR. Do not repeat previously passed device tests unless this build exposes a regression.

## 2026-11-05 — Per-book image storage, isolation, persistence, and sleep rendering — DEVICE PASS (user-reported)

- Source commit: `39f9a2491e035e273eebae60d258e7e481443b25` on `feat/x4-reader-screensaver-transition`.
- X4-only GitHub Actions run: [42098749919](https://github.com/thrilsekr/crosspoint-reader/actions/runs/42098749919); job `127495649366`; result: success.
- Artifact ID: `13449687530`; artifact name: `crosspoint-2.0.0-x4-local-get-test`.
- User tested the Book A/B/C sequence and reported: “This all appears to work.”
- User-reported PASS outcomes: image is stored/promoted for the correct book; that book's image is used for reader-originated sleep; another book does not display the first book's image; returning to the first book retains its image; a book without a generated image uses its normal EPUB cover; system-menu screensaver behavior remains unchanged.
- **Result: DEVICE PASS** for per-book image identity/isolation, persistence, sleep lookup/rendering, and normal-cover fallback.
- Scope limit: no confirmation yet of a live generation request from captured page context, backend job polling, or the complete five-minute generation workflow. Those remain outstanding.
- Do not repeat this Book A/B/C test or the worker GET/POST/BMP transfer diagnostics unless a later code change affects them.

## 2026-11-05 — Downloaded BMP as Cover+Custom reader cover — DEVICE PASS (user-reported)

- Firmware: X4-only build from commit `46cb346c4204da55b04ecd2140f51f91564d3b8c`, workflow run `42085257638`, artifact `13443680344`.
- Binary SHA-256: `8317a1ef2b3c9b0a9dd1e36c8b6bce9b8d57bda21a7f4bff5dd38d0f4f25a6d8`.
- User reports all X4 tests passed:
  - Reader-originated sleep with `Cover+Custom` displays the downloaded `/x4-download-test.bmp`.
  - System-menu sleep retains the existing custom screensaver.
  - Missing-test-image fallback behavior returns to the original EPUB cover.
- **Result: DEVICE PASS** for the downloaded-BMP sleep-cover integration and preservation of the system-menu screensaver path.
- Scope limit: this is a static diagnostic BMP fixture. AI image generation, final per-book storage/lookup, and generated-image lifecycle are not implemented.
- No further device test is required for this milestone.

# Xteink X4 AI Cover Art — Test and Build Log

## 2026-11-05 — Downloaded BMP as Cover+Custom reader cover — BUILD PASS; DEVICE TEST PENDING

- Source branch: `feat/ai-worker-lifecycle`, PR #3 targeting `feat/x4-reader-screensaver-transition`.
- Build run: `42085257638`; original X4 only; unit tests, clang-format, cppcheck, and firmware build passed.
- Artifact ID: `13443680344`; firmware binary size: 5,567,904 bytes.
- SHA-256: `8317a1ef2b3c9b0a9dd1e36c8b6bce9b8d57bda21a7f4bff5dd38d0f4f25a6d8`.
- RAM: 66,680 / 327,680 bytes (20.3%). Flash: 5,553,375 / 6,553,600 bytes (84.7%).
- Implementation: reader-originated `COVER_CUSTOM` sleep prefers `/x4-download-test.bmp`; missing, invalid, or unrenderable file falls back to the original book cover. System-menu custom screensaver path is unchanged.
- No X4 device test of this display integration yet. User's next test is to sideload this build, sleep from an EPUB with Cover+Custom selected, then sleep from a system menu to verify both paths.

## 2026-11-05 — Worker lifecycle and local BMP transfer — DEVICE PASS (user-reported)

- Firmware tested: `x4-ai-worker-lifecycle-firmware.bin`, workflow run `42060939878`, artifact ID `13430903087`.
- User-reported results: GET Pass; POST Pass; GET BMP Pass; BMP Saved.
- Confirms the worker GET lifecycle and serialized POST/BMP download/save diagnostic. It does not validate the Cover+Custom display path.

## 2026-11-04 — X4 reader trigger + context preview — DEVICE PASS

- Firmware source commit: `3360e939950fbf5d35022a085ec213628b7ec287`; built by manual X4 workflow run `41773580611`, job `126388673607`.
- Artifact ID: `13296867371`; firmware binary SHA-256: `b76e31f9293d7d05fa83ae948dd76736ec1be39bea19e22a55a4b4b946dd1e5b`.
- User sideloaded the verified firmware onto the original X4.
- Short press in system menu: no action, as expected (trigger is reader-scoped).
- Short press in EPUB reader: temporary context preview opened successfully.
- Preview showed the correct book title and author and captured the current page's text.
- Generic preview UI displays on-screen button affordances and duplicated physical Back hints. **Not a defect in the customer flow:** this is temporary `DetailsActivity` test UI and will be replaced by the final screensaver activity. No cleanup required here.
- **Result: PASS** for reader-only trigger, activity handoff, book metadata capture, and current-page text capture.
- Networking, image generation, worker lifecycle, and sleep transition remain unimplemented/untested. No additional firmware build required for this documentation update.

## 2026-11-04 — Reader trigger + context preview build — BUILD PASS; device test pending

- Branch: `feat/x4-reader-screensaver-transition`; source commit: `3360e939950fbf5d35022a085ec213628b7ec287`.
- Workflow run: `41773580611`; job: `126388673607`; event: manual `workflow_dispatch`; result: **success**.
- CrossPoint 2.0.0 base and exact FreeInk SDK pin checks passed. PlatformIO build and artifact upload both completed successfully.
- Artifact: `crosspoint-2.0.0-x4-local-get-test`, ID `13296835671`; ZIP size 3,645,865 bytes; ZIP SHA-256 `4235fc8e59a8b23549890eed9da5715a90f95f3b107e9cbfc17d8d4284f748f0`.
- Extracted `firmware.bin`: 5,591,680 bytes; ESP32-C3 application image; project `crosspoint-reader`; embedded version `3360e939`; binary SHA-256 `b76e31f9293d7d05fa83ae948dd76736ec1be39bea19e22a55a4b4b946dd1e5b`.
- Build memory report: RAM 20.4% (66,712 / 327,680 bytes); Flash 85.1% (5,577,161 / 6,553,600 bytes).
- **Device status: NOT TESTED.** Successful build does not establish device behavior. Next: sideload the verified artifact only for the planned X4 test; check AI Screensaver setting, short-power trigger, captured book metadata/page text, and Back behavior. No firmware has been installed by this build.

## 2026-11-03 — X4 reader trigger/context preview implementation (not built)

- Branch: `feat/x4-reader-screensaver-transition`, based on baseline commit `23be366a9b5bac3d125b93901b5a884c3b6a2f0b`.
- Added persisted short-power action `AI_SCREENSAVER=10`; existing values 0–9 are unchanged.
- Added the option to the no-touch short-power setting list and English strings for the context preview.
- The X4 reader power-release path copies current-page text and book title/author into a `DetailsSpec`, then calls `ActivityManager::replaceActivity()` with the existing `DetailsActivity`.
- The preview is a temporary integration harness only. It does not start networking, image generation, a worker task, or simulated sleep.
- Source review confirms `replaceActivity()` defers the replacement until the current loop returns, then exits/destroys the reader before entering the preview.
- **Build status: NOT RUN. Device status: NOT TESTED.** No firmware artifact is available for this change yet.
- Next: perform the manual X4-only workflow build, inspect compiler output/artifact, then sideload and verify setting selection, power-button trigger, captured book metadata/page text, and Back behavior. Preserve the installed firmware until the build artifact is inspected.

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


## Per-book image storage implementation (2026-11-05)

- Added `AiScreensaverStorage.h` to map the existing EPUB cache directory basename to the persistent `/.crosspoint/ai-screensaver/` namespace.
- Updated `SleepActivity::renderCoverSleepScreen()` to parse and display the active book's `current.bmp` before falling back to the normal cover.
- Updated the X4 local GET diagnostic to download into the active book's `incoming.bmp`, validate with `Bitmap::parseHeaders()`, and promote to `current.bmp` with `previous.bmp` rollback.
- Updated finished-book move handling to rename the persistent AI directory alongside the existing EPUB cache directory.
- **Build:** pending manual `X4 local GET diagnostic build`; no build result claimed.
- **Device:** pending original X4 validation; no device result claimed.


## 2026-11-05 — Worker lifecycle/cancellation contract investigation

- Source verified: pinned SDK `5e94fe7480f019c89fbf691aef87523eb1b46704` calls `hostResolves()` before both HTTP and HTTPS connection setup. Platform pin `55.03.311` maps to Arduino-ESP32 3.3.11; its `NetworkManager::hostByName()` checks `IPAddress::fromString()` first and returns immediately for valid IP literals. Hostnames still use synchronous DNS without a caller abort/timeout API.
- Source verified: TCP connection timeout currently inherits the 60-second HTTP timeout. TLS handshake and response reads have cooperative abort polling; TCP connect itself does not.
- Decision recorded: worker URLs are IPv4 literals only; add a separate 2-second TCP connect timeout while retaining the 60-second response/handshake timeout. Cross-task cancellation uses an atomic token; worker shutdown is cooperative and tasks are reaped only after cleanup.
- Repository worker guidance reconciled in commits `7974116`, `b6af84d`, `a1ce6f0`. No firmware source or SDK pin changed. No build or device test performed.


## 2026-11-05 — Worker network foundation implementation (build pending)

- Added repository-owned pinned-SDK patch script `scripts/patch_sdk_worker_connect_timeout.py`. It asserts exact SDK SHA `5e94fe7480f019c89fbf691aef87523eb1b46704`; SDK source is patched only in the Actions workspace, leaving the submodule pin untouched.
- Patch adds a separate 2,000 ms TCP connect timeout for both `SecureClient` (TLS) and `SecureHttpClient` (plain HTTP), then restores the 60,000 ms socket I/O timeout after successful TCP connection. Existing SDK timeout behavior remains unchanged when no connect timeout is set.
- Patch adds `FetchOptions::followRedirects` (default true); worker GET sets false to prevent redirecting an IPv4-literal request to a hostname and re-entering DNS.
- `HttpDownloader` adds atomic-token overloads for streaming GET, form POST, staged GET-to-file, and staged POST-to-file. Worker overloads require IPv4-literal URLs, reject redirects, use 2-second connect timeout, and retain 60-second response timeout. Existing bool-pointer APIs remain unchanged.
- Workflow updated only to py-compile and apply the SDK patch before the existing X4 build. Trigger remains manual `workflow_dispatch`.
- **Verification status:** GitHub source edits committed; patch script and C++ changes have not yet run in CI. No build artifact or device test exists yet. Next required action is manually dispatching the X4 Actions workflow, then inspect its patch/build logs.


## 2026-11-05 — Worker network foundation build — PASS (not device-tested)

- Source commit: `25c6ccad1f3e0f82d271bd8243361467f5983841`.
- Workflow run #33: `42132139366`; manual `workflow_dispatch`; completed successfully.
- Exact CrossPoint 2.0.0 source and SDK pin checks passed. SDK patch script py_compile and application passed against pinned SDK `5e94fe7480f019c89fbf691aef87523eb1b46704`.
- PlatformIO X4 `default` environment compiled and linked successfully; `HttpDownloader.cpp` compiled. Firmware binary generated successfully.
- Artifact: `crosspoint-2.0.0-x4-local-get-test`, artifact ID `13464236915`, 3,648,605-byte ZIP; SHA-256 of uploaded ZIP: `5fdf976bfd639224ffa1a5640c830540aa1120d3151e1b59e20af9a79cec3212`.
- Build memory report: RAM 20.4%; Flash 85.2%.
- **Result: BUILD PASS.** Confirms source/API integration and pinned-SDK patch compile. Does not prove runtime cancellation latency or device sleep behavior.
- **Device: NOT TESTED.** No firmware installation requested or performed.
- **Next:** implement the activity-owned lifecycle-aware worker using the now-built atomic HTTP APIs and bounded IPv4 TCP-connect path. Validate its lifecycle and stop/reap behavior before device testing.
