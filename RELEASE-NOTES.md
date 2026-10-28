# Community simulator releases

## v0.1.3

Add the Linux x86_64 desktop simulator, built against Ubuntu 22.04/glibc 2.35, alongside Windows x64. Existing v0.1.2 tags and assets are preserved.

- Resolve desktop assets/templates/demos and project defaults beside the executable on Linux. Keep the preferences override and consistent `build/dist` output paths.
- Separate desktop behavior from embedded hardware: disable I2C/CV device access, USB gadget endpoints, device Wi-Fi/Ethernet configuration, firmware flashing, backlight/timezone and host power commands. I2C stubs report unavailable and clear failed reads.
- Preserve non-finite audio guards by removing GCC/Clang fast-math flags. Fix the GCC JUCE integer/SIMD overload conflict, missing thread headers and an incorrectly inline-declared LFO helper.
- Add `--version`, six desktop platform regressions and deterministic synthetic test-kit generation.
- Run the engine suite outside an asynchronous JUCE callback to prevent nested Linux event dispatch from hanging after completion. Trim quoted test-kit arguments before parsing on Windows.
- Extend the package tool to create Linux `.tar.gz` archives with executable launcher permissions, ELF/dependency checks, source/build metadata and checksums. Update Linux requirements and build/test instructions in the manual and release guide.

The Linux package is a desktop application for an X11/XWayland session, using system ALSA/JACK libraries. Embedded firmware paths remain separate. See [Linux setup](Kick/how-to/release-simulator.md#linux-desktop) and the [functionality inventory](FEATURE-INVENTORY.md) for setup and remaining feature/hardware limits.

Validation (2026-10-28):

| Check | Result |
|---|---|
| Windows full Debug engine suite | 270 passed, 0 failed, exit 0 |
| Windows full AddressSanitizer engine suite | 270 passed, 0 failed, exit 0; no new sanitizer report |
| Windows Release optimization/platform regressions | 27 passed, 0 failed, exit 0 |
| Native Linux Release full engine suite | 270 passed, 0 failed, exit 0 |
| Release UI on each platform | All nine cases pass: panel/preference/Original override, popups in both layouts, Audio, audit, instrument and Modular |
| Panel and shared UI assertions | 227 per panel run; 5 in the shared audit, on both platforms |
| Distribution preflight on each platform | Version 0.1.3; ASIO/ASan disabled; 899 tracked runtime files match source |
| Linux compatibility | x86_64 ELF; all linked libraries resolve; highest required glibc version 2.35 |

Linux engine/audio checks use a private PulseAudio null sink through ALSA, and GUI checks use Xvfb. They establish native desktop behavior without physical audio/MIDI or embedded-device acceptance. Windows engine suites run sequentially because their temporary project names overlap. Package contents, executable modes, manifest hashes and fresh-extraction smoke checks are verified before publication using [the release procedure](Kick/how-to/release-simulator.md).

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
