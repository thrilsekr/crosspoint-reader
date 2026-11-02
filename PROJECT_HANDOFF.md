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
- Latest code commit: `98811e06eb687e83fba58eef2de86a8a2fb409f0`.
- Project records are versioned on the same branch; verify the live branch HEAD when resuming.
- Latest commit message: “Keep cumulative results in local X4 diagnostic”.
- Latest recorded GitHub Actions check: run/job `125165358875`, completed successfully at 2026-11-02 08:19:28 UTC.
- Workflow name still says “Build CrossPoint 2.0.0 X4 baseline”.
- Latest build artifact retrieved and inspected: artifact ID `13125857099`, ZIP `x4-cumulative-diagnostic.zip`.
- ZIP contains `firmware.bin`, 5,590,784 bytes; SHA-256: `df2b6cd68872c87635c055c4289b080b9dceef1ecbb07ec28dd451f0bc87bb1f`.
- This binary is built from code commit `98811e06`; it has not yet been installed on the X4.

## Current milestone

**Milestone: complete the local BMP download diagnostic.**

### Current state

- A local Windows PowerShell receiver listens on port `8765`.
- The user has just said they will paste the revised receiver script into the existing `.ps1` file. Do not ask them to repeat this.
- Revised receiver behavior: creates a valid 800×400 24-bit BMP; logs accepted connections, request lines, headers, bodies, response status, and disconnects; serves only GET `/test.bmp`; supports GET/POST `/event`; returns 404 for unknown routes; Q stops the receiver.
- The receiver is not yet confirmed running with the revised script.
- Earlier GET `/event` test returned HTTP 200.
- Earlier POST `/event` test reached the receiver with form body `test=x4-post-test` and received HTTP 200.
- Latest BMP diagnostic test did not succeed: device reported HTTP 0 and receiver did not show a corresponding BMP request. This indicates no HTTP response, not necessarily that TCP connection was never made.
- A cumulative diagnostic UI change was committed at `98811e06`; it retains GET, POST, and BMP outcomes rather than overwriting earlier stage messages.
- The last known installed firmware is the earlier BMP-download diagnostic build (commit `6802c434874d9eb5f38107d5158f3fd69694e134`), not confirmed to be the cumulative-results build.
- No new firmware installation has been requested or confirmed after commit `98811e06`.

### Next actions (assistant-owned unless device interaction is needed)

1. Retrieve the successful GitHub Actions run details and artifact for commit `98811e06`.
2. Verify the artifact contents, firmware binary size, and SHA-256 before presenting an installation link.
3. Once the user has pasted and started the revised receiver, provide one concise device-test sequence using the verified build.
4. Interpret the cumulative X4 diagnostic screen together with receiver logs.
5. Fix the actual failure point; do not repeat tests without a specific reason.
6. Update this handoff and TEST_LOG.md with the outcome.

## Confirmed CrossPoint 2.0.0 findings

### Reader text and trigger

- `EpubReaderActivity` owns the current `Section`.
- `section->getTextFromSectionFile()` returns the exact rendered page text. The user verified decoded QR text against the visible page; formatting aside, the text matched.
- QR is verification only; final solution will not require QR.
- Plugin/event APIs cannot access the current `Section`, invoke QR generation, or retrieve page text.
- A firmware fork can reuse `getTextFromSectionFile()`.
- Main trigger is the short power-button action. Long press remains OEM power-down behavior.
- `SHORT_PWRBTN` values in `src/CrossPointSettings.h`: 0 Ignore, 1 Sleep, 2 Page Turn, 3 Force Refresh, 4 Links and Footnotes, 5 Power Confirm, 6 Create Clipping. Proposed new value: `AI_SCREENSAVER=7`; do not renumber existing values.
- `SettingsList.h` contains `shortPowerButtonOptions()`; `SettingsActivity.cpp` displays/saves the setting.
- Input loop handles short vs long press; existing threshold 400 ms, special 10 ms for Sleep.
- Automatic inactivity sleep is out of scope.

### Reader lifecycle and rendering

- Capture page text once at trigger time under the correct render lock and retain an owned copy.
- `ActivityManager` stack supports pushing an activity while reader remains underneath.
- Existing `SleepActivity` handles `/sleep.bmp`, book-cover, and blank-screen behavior. Existing cover renderer supports EPUB/TXT/Markdown/XTC.
- Shared render task calls active activity `render()`; main task handles activity logic.
- `RenderLock` protects shared state/framebuffer.
- `requestUpdate()` and `requestUpdateAndWait()` are available.
- Network worker must never draw.
- `docs/activity-manager.md` explicitly advises against `xTaskCreate` inside activities and recommends a lifecycle-aware Worker abstraction. No general-purpose Worker abstraction was found in the exact SDK tree. Do not create an ad-hoc task without a lifecycle-safe design.
- Network polling responsiveness and transition from AI activity to deep sleep remain unverified.

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
