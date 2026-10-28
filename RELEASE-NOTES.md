# Community simulator releases

## v0.1.2

This unofficial Windows simulator release adds a vector KICK panel and fixes sample playback, Modular effects and live Browser audition. It builds on the v0.1.1 audit repairs. The manufacturer's firmware sources still contain partial and missing features; this release does not certify physical hardware behavior.

- Reconstruct the supplied front-panel graphic as an embedded SVG. Pads, buttons, Q-links, jog/Enter, touch strip, touchscreen and LEDs use the existing firmware paths.
- Add a saved **Hardware / Original** toolbar choice. Switching layouts retains the live screen, selected project/sequence/track/program, control state and open dialogs. Hover controls for mouse/Shift help. MASTER and REC remain visual references for analog hardware controls.
- Give each sample voice its own cleared silent frame. Interpolate fractional loop boundaries across the trimmed region in both directions and exclude resampler padding from playback duration.
- Keep sample sources alive behind cached voice pointers and stop stale voices on source replacement. Correct embedded preview-source ownership during load, purge, reload and destruction.
- Apply Modular stereo effects and their release tails to mono output. Cache exact oscillator increments, filter coefficients and normalized parameter conversions until their inputs change.
- Synchronize live Browser audition loading and gain edits with rendering. Audio uses a try-lock and emits silence during a load. Stop on missing files, retain mute across source changes and advance muted one-shots to completion.
- Add the source-backed [functionality inventory](FEATURE-INVENTORY.md), refresh the manual and document remaining firmware gaps. Give the community simulator its own 0.1.2 application version; the device firmware's upstream version is separate.

The Windows archive includes runtime assets, templates/demos, manual, inventory, audit notes, licenses and build/checksum information. ASIO, the ASIO SDK, personal preferences and commercial sample kits are excluded. Extract the whole archive and run `KT_KickEM.exe` from its folder.

Validation on Windows (2026-10-28):

| Check | Result |
|---|---|
| Full Debug engine suite | 264 passed, 0 failed |
| Full AddressSanitizer engine suite | 264 passed, 0 failed; no new sanitizer report |
| Release optimization / audition regressions | 21 passed, 0 failed |
| Release panel: default, preference restart, Original override | 227 passed, 0 failed in each run |
| Release shared UI audit | 5 passed, 0 failed |
| Release Audio, popup in both layouts, instrument and Modular | All five smoke cases completed successfully |
| Distribution preflight | Windows/app version 0.1.2; ASIO/ASan disabled; 899 tracked runtime files match source |

Validation and release procedure are recorded in [the release guide](Kick/how-to/release-simulator.md). Physical CV, MIDI loopback, USB/UART, updater operations, Linux recovery and ARM callback timing still require the appropriate hardware/environment. See [known gaps](FEATURE-INVENTORY.md#repair-and-acceptance-order) before relying on an advertised control.

## v0.1.1

Published the completed audit repairs at `e5f5ea8c`: playback and MIDI ordering, sample editing/undo, UI lifetimes, file-operation checks, update/USB/UART parsing and desktop stability. Historical validation and limitations are preserved in [AUDIT-FIXES.md](AUDIT-FIXES.md).
