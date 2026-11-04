# AI Screensaver: Deferred Alternative

## Status

**Parking-lot idea only. Not part of the current POC or implementation plan.**

The current approach remains the reader-exit flow: capture the visible reading context, leave the reader, and let the screensaver-side workflow handle generation and display. Do not switch to this alternative unless the current approach encounters a significant technical roadblock or proves impractical.

## Alternative: Manually pre-generate while reading

Keep the reader open and use a menu-button action to manually request an AI screensaver for the current page.

Possible user flow:

1. While reading, the user reaches a page or passage they want to use as inspiration.
2. The user opens the reader menu and manually starts generation.
3. The current page text/context is captured and sent to the generation workflow.
4. The user returns to reading while generation runs in the background.
5. The generated image is saved and made available as the next screensaver.
6. When the user later sleeps the device, the prepared image is already available.

The intention is that the user can initiate generation at a meaningful moment, then continue reading without waiting for the network or image generation.

## Why keep this in reserve?

This approach may avoid or simplify some of the harder parts of the automatic reader-exit design:

- It may reduce coupling between the reader's exit lifecycle and the generation workflow.
- It may avoid needing to keep the device in an intermediate waiting/sleep-like state while generation completes.
- It may simplify power-button behavior and the handling of accidental or intentional presses while generation is pending.
- It may avoid some of the complexity around transitioning away from the EPUB renderer before generation starts.

It could potentially reuse the same page-context capture, worker, generator, and image-delivery components. It is not yet established that those components can safely run while the reader remains active.

## Questions to validate if this alternative is ever pursued

- Can a worker perform the network request and receive/save the image safely while the EPUB reader remains active?
- Can this happen without disrupting page turns, reader responsiveness, memory use, or other reader activity?
- How should the user be informed that generation is in progress, has completed, or has failed?
- What should happen if the user leaves the book, sleeps the device, or starts another generation before the first one finishes?
- Where should the generated image be stored so the normal screensaver flow can use it without disturbing existing screensavers?

## Guardrails

- This is a fallback to revisit only if the current POC path hits serious roadblocks.
- Do not add this to the active milestone, change the current architecture, or implement menu behavior as part of the current POC.
- Preserve existing screensavers and the current firmware.
- Validate feasibility against the original Xteink X4 running CrossPoint 2.0.0 before making any implementation decisions.
