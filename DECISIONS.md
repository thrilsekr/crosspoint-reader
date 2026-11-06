# Xteink X4 AI Cover Art — Decision Record

Last updated: 2026-11-05

Durable decisions and constraints. Reopen only with new evidence or explicit user request.

## Project boundaries
- Target: original Xteink X4, CrossPoint 2.0.0, exact FreeInk SDK pin `5e94fe7480f019c89fbf691aef87523eb1b46704`.
- No beta, RC, development firmware, or firmware reinstall outside an explicitly planned test.
- Work only in personal fork `thrilsekr/crosspoint-reader`; do not touch upstream.
- Firmware delivery is SD-card sideloading. No USB serial.
- Prefer minimal data exposure. Never ask for private LAN details or credentials.
- Existing screensaver files need not be protected during diagnostics; avoid unrelated destructive changes.
- Assistant owns project records. Do not repeat completed tests without a concrete regression reason.

## Reader trigger and context
- Configurable short-power action is persisted as `AI_SCREENSAVER=10`; values 0–9 remain unchanged. Long press retains OEM power-down behavior.
- Original X4 is no-touch. Use the direct short-power release path in `EpubReaderActivity::loop()`, with the existing simultaneous-Down guard. Do not rely on touch-board-only mappings.
- Capture page text with the pinned SDK `Section::getTextFromSectionFile()` and book title/author while the reader is alive. It extracts the current page's text blocks, joins words with spaces, and does not preserve full layout or non-text elements.
- Copy context into independently owned data before `ActivityManager::replaceActivity()`; let reader `onExit()` and destruction run before network work. Never retain reader/Section pointers.
- Do not use QR in the final flow, event payloads, or `reader.session`. Automatic inactivity sleep is out of scope.

## Sleep rendering and per-book storage
- This is an EPUB cover substitution through existing reader-originated `Cover+Custom`, not a new screensaver mode.
- Reader-originated `COVER_CUSTOM` uses `renderCoverSleepScreen()`; system-menu sleep uses `renderCustomSleepScreen()`. Leave the latter and global `/sleep.bmp` path unchanged.
- Use the generated image only for the matching book. Missing, invalid, or unrenderable image falls back to that book's original cover.
- Per-book images live outside the disposable EPUB layout cache at `/.crosspoint/ai-screensaver/epub_<existing-cache-id>/`. Reuse the existing `Epub::getCachePath()` directory basename as identity.
- `current.bmp` is active; `incoming.bmp` is untrusted input; `previous.bmp` supports rollback during promotion. Do not replace a valid current image until the incoming image is fully downloaded and validated.
- Keep same-book previous image while waiting; never show another book's image. Cache clearing must not delete AI images. Current identity implementation covers reflowable books; XTC identity is out of scope until designed.
- Generate portrait art, target 480×800. This is a generation target, not a validation requirement. BMP validation stays permissive: readable supported header and complete pixel data only; do not reject dimensions, orientation, or aspect ratio. Existing renderer zoom-to-fit is retained.

## Worker lifecycle (implemented and device-tested)
- `AiScreensaverService` is application-lifetime and owned by main.
- Main task owns service state and Wi-Fi lifecycle. Worker receives copied request data and an atomic cancellation token; it performs one HTTP operation and publishes a result.
- Worker never retains Activity pointers, draws, accesses SD storage, or acquires `RenderLock`.
- Worker uses an 8 KiB stack at priority 1. Main reaps only after the worker has returned to its suspended completion state.
- Sleep preparation requests cancellation under `RenderLock`, then drains the service outside the lock before sleep-event delivery and radio shutdown.
- Cancellation is cooperative. Diagnostic GET uses a 5-second socket timeout; saved-network association may take up to 45 seconds. These are not an end-to-end deadline. User accepted current delay.
- HTTP is single-flight. Diagnostic POST and BMP download run sequentially on the main task after worker GET. Do not redesign the worker lifecycle for provider integration.

## HTTP/network foundation
- `HttpDownloader` is single-flight; never run concurrent requests.
- Existing staged downloads write to `.part` and replace the destination only after complete success; failed partial files are removed.
- Worker-safe atomic cancellation overloads exist for streaming GET, form POST, staged GET and staged POST. Legacy synchronous APIs remain source-compatible.
- Build-time SDK patch is pinned to the exact submodule SHA and adds a 2,000 ms TCP connect budget for worker operations while restoring the 60,000 ms socket I/O timeout after connect. Worker-safe requests require IPv4 literals and reject redirects. Existing callers retain prior timeout behavior.
- The X4 manual workflow syntax-checks/applies the SDK patch in the Actions workspace; do not change the SDK gitlink.
- **Security gate:** before sending provider credentials, verify the merged code does not disable TLS verification and confirm CA trust plus hostname verification against the pinned SDK. Do not use `setInsecure()` for authenticated HTTPS.
- Current diagnostic worker path was local IPv4 HTTP. Public hostnames, provider HTTPS, JSON API requests, and WebP are not yet implemented.

## Provider integration (proposed, not yet implemented)
- AI Horde is the candidate direct provider. Confirm current request, status, result, cancellation and privacy semantics before implementation.
- Candidate request is asynchronous: submit JSON, retain job ID, poll check endpoint no more than once per minute, retrieve result on completion, then download and convert image.
- `shared:false` is not a guarantee that volunteer workers cannot see prompt content. Verify registered-key behavior and user privacy expectations before sending page text.
- Keep provider credentials in SD-card configuration only. Never embed/log keys or page text.
- AI Horde image sizes must be multiples of 64; 384×640 is a candidate 3:5 request size, scaled to 480×800 locally.
- AI Horde output WebP is unsupported by the current JPEG/PNG/GIF decoder factory. Design memory-safe conversion before use.
- No bridge/proxy unless direct connectivity is shown to be infeasible.

## Collaboration
- Keep changes incremental and reviewable. Verify APIs against exact CrossPoint 2.0.0 and pinned SDK.
- Explain approach before implementation. Distinguish verified, implemented-unverified, proposed, and failed.
- Update `PROJECT_HANDOFF.md`, `DECISIONS.md`, and `TEST_LOG.md` with meaningful work.
