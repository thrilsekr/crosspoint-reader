# Xteink AI Screensaver — Test and Build Log

Newest entries first. Record actual results only; label hypotheses explicitly.

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
