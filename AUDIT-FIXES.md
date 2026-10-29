# Audit repairs

Repair pass for [AUDIT-REVIEW.md](AUDIT-REVIEW.md), based on desktop-build `1a6f1769`.
All 62 original audit IDs are accounted for below. Final sanitizer validation also found H9, documented below. Implementation and desktop verification are complete. This report accompanies the repair commit on `desktop-build`.
No hardware flashing was performed during the repair pass.

## Verification status

| Validation | Result |
|---|---|
| Original audit baseline | 196 engine checks passed. |
| Audit repair Debug engine suite (2026-10-27) | **243 passed, 0 failed**, exit 0. |
| Audit repair AddressSanitizer engine suite (2026-10-27) | **243 passed, 0 failed**, exit 0; no report from the repaired rerun. The initial run found H9 and its report is retained. |
| Simulator audit mode | **5 passed, 0 failed**, exit 0. |
| Simulator instrument / Modular | Both exit 0 and reach `SELFTEST done` on the final build. |
| Patch whitespace check | Passed. |

The 47 checks beyond the original baseline cover the reproduced memory/lifetime failures, note undo and concurrent snapshots, live routing, first-tick and delayed ratchet scheduling, sustained/retriggered notes, MIDI batches and clock timestamps, slice restore and reverse/fractional rendering, firmware/UART bounds, preset collisions, write failure, exact edit undo/redo, unreadable preview, and song MIDI export.

The graph crossfade intentionally changes structural-edit transients. The existing spectral comparison resets after installing its patch so it measures settled timbre; its comparison and tolerance are unchanged. The export fixture explicitly uses the sequence length, avoiding an incorrect assumption about how many times a shorter track repeats.

Original repair-pass desktop DSP workload: one second of 16-voice oscillator/ladder/drive/delay plus chorus took **344.05 ms in Debug** (218.79 ms in RelWithDebInfo with ASan). This is a development-build workload measurement, not an ARM deadline or individual-function profile; it does not justify replacing accurate DSP functions with approximations.

Original repair logs and result snapshots are in `../devtools/audit-fixes-*`; test result files remain beside their executables. The original repair suite had 44 audit checks and 243 total checks. Version 0.1.2 also includes 21 optimization/release regressions: `KICK_TEST_AUDIT_ONLY=1` runs 65 checks, `KICK_TEST_OPTIMIZE_ONLY=1` runs 21, and the ordinary suite runs all 264. Use `KICKEM_SELFTEST=audit` for the five shared UI lifetime/empty-state checks.

## Programs and DSP

| IDs | Change / disposition |
|---|---|
| A1 | Preserve physical held keys separately from arp-owned voices; switch arp routing without orphaning a direct voice; route carried keys through the new arp. |
| A2 | Release voices and clear mono held state when play mode changes. |
| A3 | Reset engines and note bookkeeping on prepare, including the same sample rate; reset arp and held state. |
| A4 | Inspect every voice-mix sample; stop runaway voices on nonfinite output or absolute amplitude above 32. |
| A5 | Delay reset invalidates ring contents in constant time; unwritten positions read as zero. |
| A6 | Add the normalized filter-envelope amount and filter-LFO amount independently. Pitch-LFO depth no longer moves cutoff. Stored envelope depth uses the MIDI scale. This intentionally changes affected patches' sound. |
| A7 | Crossfade structural graph changes over 20 ms, retaining the previous graph for release/effect output and destroying it on the editing thread. This smooths the transition; it does not preserve an arbitrarily long delay/reverb tail. |
| A8 | Retain fractional arp countdown overshoot. |
| A9 | Remove obsolete deferred releases when an input note retriggers. |
| A10 | Zero cached cable samples beyond a short arp sub-block. |
| A11 | Retry graph preparation outside the audio lock if the sample rate changed during construction. |
| A12 | Profiling candidate, not an established defect: retain accurate sine/tanh/filter functions. An explicit 16-voice oscillator/ladder/drive/delay/chorus desktop workload now reports elapsed time. ARM profiling and sonic comparisons are required before choosing approximations. |
| A13 | Queue CC64 sustain with note events; pedal-up releases sustained keys, including through a muted track. All-notes-off clears the pedal state. |
| A14 | Preserve presets whose names sanitize to the same filename; replace matching named patches transactionally; check the patch tag before deletion. |

## Engine and persistence

| IDs | Change / disposition |
|---|---|
| E1 | ValueTree listeners update the playback index for add/remove/property edits, undo, redo and direct tree operations. |
| E2 | The playback index contains copied plain note data behind one lock; playback never retains editable note/map pointers. |
| E3 | Share a sequence lifetime lock with transport processing; detach the actual playing sequence before deletion; invalidate queued numeric sequence references. Stop clears the playing pointer. Unrelated playback can continue when another sequence is deleted. Clock callbacks publish ticks; transport graph work runs on the tick worker. |
| E4 | Remember the destination note/channel for every live input key. Note-off and aftertouch use that destination after routing/transpose changes; stop flushes all destinations. |
| E5 | Schedule the inclusive starting boundary, including tick zero and seek/loop offsets. |
| E6 | Schedule ratchets from each note's actual tick and subdivision boundaries; consume queued events in chronological order across delayed tick batches. Bound subdivisions by note duration. |
| E7 | Validate readers/streams; propagate read/write/flush errors; replace audio through checked temporary files. Undo/redo own exact before/after snapshots; eviction no longer rewrites a shared backup. A failing output stream test verifies the original survives byte-for-byte. |
| E8 | Reject unreadable preview sources; guard publication/rendering with one source lock; clamp ranges; fix reverse block ranges, destination offsets and millisecond conversion. |
| E9 | Owning value containers replace raw allocated note/map objects. |
| E10 | Use the MIDI-port setter argument. |
| E11 | Scope track/program and sequence/track-name signal subscriptions; disconnect before track/sequence teardown. Scope manager name subscriptions as well. |
| F1 | Clear dispatched MIDI batches; snapshot input names while the MIDI source is alive so hotplug cannot invalidate subsequent routing reads. |
| F2 | Reverse starts on the last valid sample and subtracts its step; bound interpolation taps and robustly wrap large steps. Preserve fractional position through the final sample. Empty/unreadable slices stay silent. |
| F3 | Distinguish binding a saved source from replacing it; preserve and validate persisted trim boundaries on restore/copy. |
| F4 | Append four distinct layer nodes. |
| F5 | Add an explicit persisted Internal/MIDI Input clock choice. Route Clock/Start/Stop/Continue from the selected input; correct units and timestamp order, preserving input reception timestamps across message-thread batching; guard empty histories, invalid PPQN and invalid tempo; keep external timing driven by received clocks. |

## UI and file operations

| IDs | Change / disposition |
|---|---|
| U1 | Dispatch the complete learn operation once to the message thread using a safe component pointer. Local program/bus/selection state is resolved and used synchronously there. Remove seven callbacks retaining stack locals and guard absent program types. |
| U7 | Reject invalid/empty/path-containing project names and existing destinations in both new-project entry points and template copy. Check template presence, directory creation and copy success before loading. |
| U8 | Trim owned table rows on shrink, including empty/hidden tables, and clamp/clear selection. |
| U9 | Bounds-check recent-item clicks/highlighting/scrolling after removal or with an empty installation. |
| U10 | Marshal project-loaded preference and animation/navigation updates onto the message thread. Prevent a second loader start while its worker runs. |
| U11 | Populate row metadata immediately. Calculate folder sizes on a bounded worker pool; reject stale/deleted row callbacks. Use 64-bit totals and one recursive scan, without following symlinks. Stop the loading overlay after directory enumeration, independently of size jobs. |
| U12 | Copy only new recorder samples under the recorder's buffer lock and add incremental thumbnail blocks at the actual sample rate. Reset thumbnail state when recording changes. |
| U14 | Protect the active project and its ancestors by resolved path ancestry; revalidate at confirmation time. |
| U15 | Read the mono meter's valid channel/peak and synchronize the reverse button. |
| U16 | Implement sampler Export/Clear. Song Export explicitly exports MIDI notes, tempo/time signature and sequence repeats into the project Exports folder, with checked file creation and feedback. Recorder save now propagates errors and uses transactional replacement; failed export retains the recording. |
| U17 | Guard the selected program before assigning a saved recording. |
| U18 | Free split/rename dialog content on Cancel as well as Apply; fix the updater's always-true status expression. |
| U19 | Capture delete-dialog ownership safely; snapshot selected paths; revalidate the current project; avoid indexing an empty refreshed directory. |
| P1 | Apply message-thread list selection synchronously; deferred selections use safe pointers and revisions. Rebinding invalidates queued selections. |
| P2 | Use one inline theme signal across translation units and weak component subscriptions that disconnect after owner destruction. Look-and-feel owns a scoped connection. Also bound cyclic-state normalization at 1.0 and handle empty states. |
| P3 | Return safely from empty mixer knob handlers. |
| P4 | Map filtered MIDI row indexes to sequence track indexes and back. |
| P5 | Remove deselected pads from the shared edit selection. |

## Hardware, update flow and IPC

| IDs | Change / disposition |
|---|---|
| U2 | Route SYSTEM software-update replies before normal navigation. Replace the MCU busy wait with an event wait and 10-second acknowledgement timeout. |
| U3 | Copy update status into safe message-thread callbacks; own popup pointers by value. Image validation and flashing run outside the message thread. |
| U4 | Register scan completion before starting; keep populated device data alive; snapshot the selected device name before confirmation. |
| U5 | Suppress hardware navigation on the update screen and guard concurrent firmware workers centrally; avoid re-launch on visibility callbacks. Stop the USB timeout timer and ignore late USB status once flashing takes ownership. |
| U6 | Use synchronized normal reboot instead of forced no-sync reboot. |
| U13 | Clear hidden update-view USB callbacks; header Back stops USB; queued view callbacks verify visibility. |
| H1 | Bound Intel HEX records and output buffers, verify hex digits/count/checksum/record shape, require EOF, and preflight the whole MCU file before initiation. Correct the extra transmitted byte and reset per-run state. |
| H2 | Reserve the header terminator and reject marker indexes beyond the fixed header. |
| H3 | Receive exactly the announced bytes across short/EAGAIN/EINTR reads with a timeout; write only received bytes; bound chunks and total length; reset counters per transfer; require length/fsync/close success before completion. |
| H4 | Device-selected staging directory, basename-only image destinations, size limits, no-follow/exclusive temporary creation and checked final rename. Parse numeric sizes without uncaught conversion exceptions. |
| H5 | Retain fragmented UART headers/payloads; use unsigned lengths; validate command payload minimums before dispatch and guard contract helper access. |
| H6 | Return process names by value. |
| H7 | Remove only the temporary unregistered socket entry after logical registration. |
| H8 | Publish JSON with its UTF-8 byte count. |
| H9 (found during repairs) | The audio graph traversed bus buffer vectors concurrently with routing changes that reallocated them. ASan reported a null-slot access in `AudIOBuffer.h:275` from `Bus::process`. Rendering now tries the existing graph mutation lock and clears the block if an editor owns it; preparation uses that lock too. A live 48-program bus-growth stress case covers the path. This can briefly silence a block during edits instead of blocking the audio thread. |

## Additional compiler-confirmed lifetime repairs

- Return asset display names by value instead of a reference to a temporary.
- Return owned audio-clip wrappers instead of pointers to stack objects.
- Fix the bounded-follow-up CV calibration lookup to use note indexes relative to the validated channel root. Physical CV acceptance remains pending.

## Validation limits

Hardware flashing, USB gadget endpoints, UART devices, CV output and Linux multi-process recovery are not exercised on this Windows host. Portable parsers and destructive-file operations are tested with disposable synthetic fixtures. Hardware and ARM timing acceptance remain required; a Windows sanitizer pass is not a Linux/device certification or a leak checker.

The publishing restrictions in AUDIT-HANDOFF.md still apply: push only to `fork` when explicitly requested, no upstream PRs, no ASIO builds/SDK or personal/commercial assets in distribution, and distribution archives only from `Kick/build/dist`.

## Optimization round 2 (2026-10-28)

The original audit repairs were committed as `e5f5ea8c` and published in [v0.1.1](https://github.com/chillbeast/0700-AAM/releases/tag/v0.1.1), a community prerelease matching v0.1.0's release status. The public Windows ZIP was downloaded and its SHA-256 verified as `7a3526902e2f992cc76ed7b71830a1c2e976ddbbab65efa459e71a1bb7589e37`.

The changes below follow that release and are included in community simulator v0.1.2. They are not included in v0.1.1.

| ID | Change and coverage |
|---|---|
| O1 | Remove the mutable silent frame shared by all sample renderers. Each voice clears and returns its own frame; interpolation marks that frame as non-clear again for JUCE buffer operations. Covers writes from downstream voice filters and resumed playback. |
| O2 | Fractional forward/reverse loops interpolate from the last trimmed sample back to the first trimmed sample. One-shot playback continues to hold its last sample. In-range loop advances skip unnecessary `fmod`; arbitrary large steps still wrap. |
| O3 | Mono Modular rendering now runs the stereo effect chain and folds both channels into mono using fixed-size scratch storage. Covers irregular block sizes, note release and delay tails against the stereo sum. Previously mono skipped all effects. |
| O4 | Cache exact oscillator increments/unison gain and filter coefficients while their inputs remain unchanged. Invalidate on pitch/tune, spread, voice count, cutoff/resonance/key tracking and sample-rate changes as applicable. Cache base knob conversions as well; per-voice modulation is still evaluated normally. Dynamic controls/reset/rate changes match forced coefficient recomputation sample-for-sample. No sine/tanh approximation is introduced. |
| O5 | Acquire an owned sample-source snapshot and revision under a short lock when binding a voice. Keep its data alive; stop the old voice after a source replacement and bind the new source on reset. Bound reads and duration to the logical sample count, excluding resampler padding. Covers retention, replacement/restart, oversized trims and 500 concurrent source replacements. |

The pre-change Debug and Release reproductions each failed five assertions covering O1-O3. Source ownership was a separate source-confirmed lifetime gap; the added O5 tests exercise the repaired path. The cleared-buffer flag was caught while validating the new owned silent frames and corrected before completion.

`OptimizationRegressionTests.h` now adds 15 permanent checks, including the inventory follow-up. Set `KICK_TEST_OPTIMIZE_ONLY=1` to run those checks. Add `KICK_TEST_DSP_BENCH=1` for isolated static-control benchmarks: 16 voices, one second at 48 kHz in 16-sample sub-blocks, one warmup plus five measured runs, reporting the median. The oscillator uses seven saw unison copies; the filter is an SVF with fixed cutoff/resonance. These measurements exclude the rest of the application and do not establish an ARM callback deadline.

| Isolated Release workload | Before | After | Elapsed-time reduction |
|---|---|---|---|
| Oscillator, 16 voices with seven saw unison copies | 28.598 ms | 19.279 ms | 32.6% |
| SVF, 16 voices with fixed controls | 5.355 ms | 4.024 ms | 24.9% |

Both static-workload output checksums are unchanged; automation tests also compare every output sample. The timings describe these individual modules on this desktop, not an equivalent improvement to the full application. Logs/results are in `../devtools/wave2-*`.

| Round-2 validation before the inventory follow-up | Result |
|---|---|
| Full Debug engine suite | **257 passed, 0 failed**, exit 0. |
| Full ASan engine suite | **257 passed, 0 failed**, exit 0; no `asan_wave2*` report. |
| Release optimization regressions and benchmarks | **14 passed, 0 failed**, exit 0. |
| Simulator audit | **5 passed, 0 failed**, exit 0. |
| Simulator instrument / Modular | Both exit 0 and reach `SELFTEST done`. |
| Patch whitespace check | Passed. |


## Inventory follow-up: preview ownership (2026-10-28)

The v0.1.2 [functionality inventory](FEATURE-INVENTORY.md) recorded 165 capability groups, including seven community extensions, their implementation/state evidence and remaining acceptance checks. Its indexes cover all 54 button-command IDs, all 34 screen IDs, 29 registered screens and 1,079 UI callback/signal/event candidates. This is a functionality ledger, not a claim that all features are accepted.

The inventory exposed an integration regression in the local O5 changes: ASoundPlayer embeds its ASound as a member, while its base renderer now retains an owning source snapshot. Loading a synthetic WAV and destroying that wrapper caused an ASan invalid free. The renderer must release that snapshot before the embedded source's lifetime ends. The same detachment is needed for purge/reload. The wrapper now detaches before these operations and rebinds on its next load; a permanent load/purge/reload/destruction regression was added. v0.1.1 predates O5 and is unaffected by this regression.

The original failing report is retained outside the repository at ../devtools/inventory-preview-probe/inventory_preview_asan.21760. After the correction, the same standalone probe exits 0 without a new sanitizer report. Full Debug and ASan suites each pass **258/258**, Release optimization checks pass **15/15**, and the simulator rebuild succeeds. Current results are ../devtools/inventory-{debug,asan,release}-results.txt; fixed probe output is fixed-probe.log beside its harness. The earlier round-2 simulator smoke results remain historical coverage.

At the inventory snapshot, concurrent live Browser audition switching remained a separate locking gap (BR-04). The release review below reproduces and fixes that race; neither change closes all rows in the inventory.

## v0.1.2 release review (2026-10-28)

**O6 / BR-04:** a standalone AddressSanitizer stress probe switching live audition files while rendering reproduced a null slice read in `ASliceRenderer::interpolateSample`, called from `AudioPreview::process`. Source ownership alone could not protect the renderer's mutable binding during a simultaneous load. `AudioPreview` now shares a lock across loading, gain/pan edits and preparation; rendering only tries the lock and clears its output during contention. The callback does not wait for file I/O. The same 1,000-switch probe now exits 0 without a sanitizer report.

The review also fixed preview mute being reset by source rebinding, muted one-shots not advancing, absent files replaying the previous sound, and output-meter disabling ignoring its argument. Six permanent O6 regressions exercise concurrent file/gain/mute/rate changes, subsequent stable audio, stop/silence, missing-file handling and retained mute/silent progression. Full Debug and ASan suites each pass **264/264**. Original/fixed probe evidence and release-validation logs are outside the repository in `../devtools/release-v0.1.2-review/`.

The simulator now embeds the vector hardware panel, retains Original behind a saved toolbar choice, and keeps dialogs attached to the live screen through layout changes. The panel test checks real click targets and firmware input packets, retained state, restart preference, environment overrides and switching with an open dialog. Popup checks now use the bundled template instead of a local commercial kit. Clicking the already-selected layout no longer refits the window. The simulator has its own 0.1.2 application version.

The README now starts with the maintainer's unofficial-project disclaimer. The manual, functionality inventory, release notes, artwork notes and repeatable no-ASIO packaging procedure are reconciled for v0.1.2. Release configuration/package results are recorded in [RELEASE-NOTES.md](RELEASE-NOTES.md) and [the release guide](Kick/how-to/release-simulator.md).

## Linux desktop release v0.1.3 (2026-10-28)

The native Linux x86_64 simulator is built with GCC 11 on Ubuntu 22.04/glibc 2.35. `KICK_DESKTOP` separates simulator/test paths and hardware behavior from the embedded ARM build: runtime assets and default projects/preferences live beside the executable, while CV/I2C, USB gadget, firmware flashing, device networking, host power, backlight and system-time commands are unavailable. Native ALSA/JACK audio and system MIDI remain desktop services.

GCC exposed missing thread/chrono includes, a JUCE integer/SIMD overload mismatch and an out-of-line LFO helper incorrectly declared inline. These are corrected. Removing GCC/Clang `-ffast-math -Ofast` preserves the existing non-finite audio guards; normal configuration optimization remains enabled. ARM timing acceptance is still required.

Six platform regressions cover portable runtime/project paths, isolated preferences, unavailable/cleared I2C reads, unavailable device Wi-Fi and retained NaN guards. They extend the full suite from 264 to 270 checks and the focused optimization run from 21 to 27. The Linux suite originally completed its assertions but hung in JUCE's outer event loop: running the suite from an asynchronous callback re-entered JUCE 7's shared Linux FD callback storage during nested test pumps. Calling the suite directly during initialization restores clean exit. The test-kit argument is trimmed before unquoting, handling Windows command-line trailing whitespace.

Linux full engine validation now passes 270/270 with exit 0 using a private PulseAudio null sink. All nine Release UI cases pass on Linux and Windows, including three panel runs with 227 assertions each and the shared UI audit with five. Linux GUI checks run under Xvfb from `/` with isolated preferences; panel screenshots were inspected. These checks do not establish physical interface acceptance or complete the inventory's remaining gaps. Windows suite results and distribution validation are recorded in [RELEASE-NOTES.md](RELEASE-NOTES.md); local evidence is in `../devtools/linux-v0.1.3/`.

The cross-platform package tool preserves tracked assets, documentation, licenses and exact source/executable hashes. Linux archives include an executable launcher, ELF/dependency checks and the Ubuntu baseline. Previous public release tags and archives remain intact. The README disclaimer is retained, and the manual, Linux build guide and inventory distinguish desktop portability from embedded hardware functionality.

## Simulator MIDI learn (current source after v0.1.3)

The simulator adds right-click learning and a toolbar mode for notes/CCs controlling pads, buttons/alternate commands, Q-links, jog/Enter and touch strip. Assignments persist globally in `simulator-midi.json` under isolated preferences and identify the input port, channel and note/CC. The MIDI menu exposes assignments, bypass, clear and held-control release. Absolute CCs, three relative encoder formats, press-only trigger buttons and pad velocity/pressure are supported. Analog references and touchscreen parameter widgets are outside this panel mapping layer.

The existing MIDI-thread queue copies a stable device identifier as well as its display name. Simulator-only interception runs on the message thread before musical routing; mapped events are consumed and unrelated MIDI remains routed normally. Learned note releases are tracked across cancel, bypass, unmap and reassignment. Held controls release on disconnect/All Notes Off; events queued before a disconnect cannot reactivate them afterward. Keyboard note-offs begun before learning and channel pressure for concurrently played keyboard notes retain their musical route. Asynchronous menus use a weak lifetime token, and the simulator removes its filter before tearing down controls.

Validation: **35 MIDI checks plus seven application-restart checks**, each passing in Windows Release, Windows AddressSanitizer and native Linux Release with exit 0; no sanitizer report. Both platforms also pass all nine existing UI cases (227 assertions per panel run; five shared audit checks). Tests exercise input identity/channel isolation, learning without accidental actions, velocity/pressure, relative formats, persistence, source reassignment, trigger buttons, bypass/disconnect/late-event releases, malformed entries, right-click handling, Escape and both layout packet paths. Evidence is in `../devtools/midi-learn-review/`. These are synthetic MIDI tests; real controller/driver acceptance remains separate. Existing firmware capability counts are unchanged, and the extension is recorded as EX-08 in both inventory forms. The desktop shortcut still resolves to the locally rebuilt no-ASIO `Kick/build/dist/KT_KickEM.exe`.

## Controller and startup review wave 3

This review follows the first MIDI learn implementation. The original controller probes produced **10 failing assertions across seven defects** against the unchanged mapper/control behavior. A later deterministic startup probe reproduced an access violation (`0xc0000005`) by loading a project and sending input during window construction, before queued screens existed. The original source snapshots and failing logs are preserved outside the repository in `../devtools/optimization-wave3-review/baseline/`, `baseline.log` and `startup-baseline.log`.

| ID | Reproduction and repair |
|---|---|
| M1 | A note already routed musically is retriggered after learning its source. Its release previously returned to the musical route before releasing the mapped control. Release both destinations, including when unmapping between press and release. |
| M2 | A sustain/switch press passes through while mappings are bypassed, then its release is swallowed after enabling/learning. Track previously forwarded switch presses and preserve their releases. CC64–69 use the specified 0–63 off / 64–127 on boundary, so a nonzero off value cannot teach a button or leave one held. |
| M3 | Arming then cancelling learn enabled mappings in memory while the saved file retained bypass. Enable only when an assignment succeeds; cancellation keeps runtime and saved bypass consistent. |
| M4 | Reset/other channel-mode commands could be learned or restored as ordinary CC bindings. Keep CC120–127 in musical routing and reject such saved assignments. Reset All Controllers releases mapped CC switches without releasing held note controls. |
| M5 | Two's-complement encoder value 64 was decoded as neutral. Decode the signed seven-bit minimum as negative movement, retaining the existing 16-detent cap. |
| M6 | Unmapping a controller before unplugging removed the only identifier checked for stale input. Remember observed/connected devices independently of bindings and reject late presses. In-flight releases already owed to the musical route remain forwarded, including sustain, instead of getting swallowed by the disconnect guard. |
| M7 | The mapped SHIFT used the mouse's one-shot latch and cleared on the first button release. Maintain an independent momentary MIDI modifier; releasing it preserves any mouse latch, and the mouse latch remains one-shot. |
| M9 | Queued startup left `currentScreen` invalid and hardware routing dereferenced missing views; lookups could also insert null screen entries. Select the existing Recent Projects view synchronously, queue deferred creation directly on the UI thread, defer missing destinations until ready, use non-mutating lookups and guard unavailable views. Weak callbacks and a released-state flag prevent queued loads/creation after shutdown; resize is safe after release. |

M2/M4's switch ranges and channel-mode boundary follow the [MIDI Association control-change reference](https://midi.org/midi-1-0-control-change-messages). Physical controller/driver acceptance remains separate from these synthetic input tests.

**M8 optimization:** filtering previously erased each consumed MIDI event from the array, shifting the remaining tail repeatedly. Compact survivors in one pass and trim once, retaining message payloads, source identities, timestamps and order. The checked benchmark compares the old erasing loop and the new filter in the same executable: 4,096 events, half consumed, one warmup and five timed runs per implementation, reporting medians. A survivor count and timestamp checksum validate each measured result. This measures array filtering, excluding controller actions, audio rendering and UI work.

Learn highlights now keep one entry per physical component, avoiding duplicate outlines for alternate button commands/jog Enter and repeated selected-target lookups. Deferred view creation also avoids a worker-pool hop for work that always executes on the message thread.

The follow-up regression suite contains **21 controller/queue checks**, **five early-startup checks** and **three early-shutdown checks**. Together with the original 35 MIDI and seven restart checks, this is **71 focused assertions** across five processes. Startup checks wait for queued callbacks rather than assuming a fixed frame delay. These checks are separate from the historical 270-check engine baseline and do not complete the remaining firmware inventory.

Final validation (2026-10-28):

| Check | Result |
|---|---|
| Windows Debug focused suite | 71 passed, all processes exit 0 |
| Windows no-ASIO Release focused suite | 71 passed, all processes exit 0 |
| Windows AddressSanitizer focused suite | 71 passed, all processes exit 0; no report |
| Native Linux Release focused suite | 71 passed, all processes exit 0 |
| Full UI regression on Windows and Linux | Nine cases pass on each platform; three panel runs at 227 assertions each and shared audit at five |
| Windows distribution preflight | ASIO/ASan disabled; 899 tracked runtime files match source |

The full UI runs preceded the final M6 release-only guard refinement; the final 71-check suite was rerun on all four configurations afterward. Linux GUI/audio validation uses Xvfb and a private PulseAudio null sink. The Windows desktop shortcut still targets the rebuilt `Kick/build/dist/KT_KickEM.exe`.

| Checked Windows Release filtering workload | Legacy erasing loop | One-pass compaction |
|---|---|---|
| 4,096 events, half consumed; median of five runs | 10.690 ms | 0.096 ms |

Both implementations retain the same 2,048 events and checked timestamp sum. The measurement excludes MIDI target actions, rendering and audio callbacks; it is not a claim of an equivalent whole-application speedup. Final logs/builds are under `../devtools/optimization-wave3-review/`, including `final-release-tests`, `release-ownership-debug-tests`, `release-ownership-asan-tests` and `linux-final-*.log`.

## Timing, recording and panel-control gap filling

Unreleased source after v0.1.4 (2026-10-28). This pass addresses the timing/recording and related panel controls selected from the functionality inventory. Existing public releases are preserved.

- **T1: recording ownership and replace semantics:** REC clears note onsets in the initially selected track's active playback loop once, after count-in. OVERDUB preserves them. Later passes add. Other tracks/outside notes survive; one undo restores the take. Recording slots distinguish MIDI channels, pin the track ValueTree, retain elapsed time across loops, finalize held notes on stop, and release their tree references afterward. External MIDI recording obeys input filters and retains its destination across track selection changes. Newly snapped notes suppress an immediate sequencer echo without deferred project-writing callbacks.
- **T2: count-in and click:** attach the previously unbound count-in preference and offer Off/one/two bars. Countdown ticks are excluded from sequence playback and note capture. STOP/cancel before the downbeat preserves existing notes. Normal click enable/rate/mode is connected; a generated, finite click replaces absent factory sounds. The status bar shows beats remaining. Its timer stops before audio shutdown.
- **T3: timing transforms:** implement All/Selected/bar Range and Start/End/Length/same-pitch Legato edits, swing 50-75, tick shift, window and strength. Keep the playback index and sorted event list synchronized, group edits for undo, and save live-recording settings in the sequence. Explicit Apply remains active when live correction is bypassed; Cancel does not change state. BAR follows the current meter.
- **T4: meter and transport:** validate stopped-sequence whole-meter changes, update derived track/bar lengths on meter and sequence-length changes, and keep meter independent of global BPM. Clear all playhead fields on PLAY START, resume paused playback, stop non-looping takes before tick zero repeats, and restart a completed sequence on PLAY. Next-sequence boundaries are detected even when tick notifications coalesce. Per-bar meter editing is not added.
- **T5: panel performance:** implement Tap Tempo with configured tap counts/bounce/timeout handling and external-clock ownership; ordinary-pad Note Repeat/Latch with phase recovery; sixteen velocity levels; Full/Half fixed velocity; TC/ON-OFF; MASTER mixer access; LINK assignment-screen access; and BACK dialog/history navigation. STOP releases repeated/latched notes. Mode changes, project changes and stale track identities release or discard their owned notes. Global automation capture, pitch-based levels and repeated Pad Perform chords remain absent; AUTOMATION provides an editor route and explicit availability feedback.
- **T6: step boundaries and LEDs:** include partial pages and fractional final steps, correct bank/page offsets for every step parameter, clamp selection/edit ranges at the bar boundary, and delete only the selected pitch. Preserve page selection after refresh and repair the 1/32-triplet index. Poll recording/performance LED state without per-frame packet repeats; dual-function simulator buttons combine their two logical LED states.

`TimingRecordingTests.h` exercises the production engine/API with synthetic notes and a frozen external clock. It includes a real synth-voice render of the fallback click, input ownership, count-in cancellation, undo/reload, meters, all Timing Correct transforms, latched release, non-looping completion and playhead restart. `KICKEM_SELFTEST=timing-controls` drives the actual panel, dialogs, LEDs, navigation and partial triplet edits in either layout. Existing MIDI/startup/UI checks remain separate regressions.

Validation (2026-10-28):

| Configuration / check | Result |
|---|---|
| Windows Debug engine | 320 passed, zero failed; exit 0 |
| Windows AddressSanitizer engine | 320 passed, zero failed; exit 0; no sanitizer report |
| Windows no-ASIO Release engine | 320 passed, zero failed; exit 0 |
| Native Linux Release engine | 320 passed, zero failed; exit 0 |
| Final timing/control UI, both layouts | 25 passed per layout on Windows ASan, Windows Release and Linux; all exit 0 |
| Existing MIDI/controller/startup regressions | 71 passed on Windows ASan, Windows Release and Linux; all exit 0 |
| Broader UI regression | All nine cases pass on Windows and Linux; three panel runs at 227 assertions each and shared audit at five |

The engine suite adds 50 checks to the preceding 270. The broader UI runs preceded the final BACK cancel-code correction and test-only bounded waits for queued navigation, modal callbacks and LED delivery. Final timing UI runs cover standard BACK cancellation. The first final Linux run caught two LED assertions that assumed delivery by the next timer turn; an ASan run similarly caught a modal callback assertion before delivery. The bounded waits retain the final state assertions and fail if delivery does not complete. No engine source changed after its 320-check validation. The final 44-file Windows/native Linux source comparison matches after normalizing line endings. Linux GUI/audio uses Xvfb and a private PulseAudio null sink; physical MIDI/audio acceptance remains separate.

Evidence, source hashes, results and build/run helpers are under `../devtools/timing-controls-review/`, including `validation.json`. The desktop shortcut still points to the rebuilt no-ASIO `Kick/build/dist/KT_KickEM.exe`. Changes remain unreleased and public v0.1.4 downloads are preserved.

The capability matrix now records 115 present, 26 partial, 23 missing and two unverified groups (158 base groups plus eight community extensions). Present identifies an implementation path; it does not certify every combination or physical-device behavior. Scheduling remains tick/message-thread based, and recorded durations are capped at the track length. Existing factory-content and hardware limits remain.

## Program and mixer control gap filling

Unreleased source after the timing pass (2026-10-29). No commit, push or public release is made in this pass.

- **G1: MIDI program CCs.** The editor attached its sliders to an unbound ValueTree. Bind only to the selected MIDI program, detach on invalid selection, retain undo and state updates, and address the active bank with Q-links. CC generation accepts canonical CC0-CC127 properties, clamps data bytes and uses the program channel instead of the currently selected track's unrelated channel.
- **G2: keygroup audio and tracking.** The inherited keygroup note-on method returned before starting a voice. Restore mapped sample voices with per-zone tracking (on by default), tuning/gain/pan, inclusive velocity zones and cycle/random selection. Mapping uses the first matching zone in root-key order. Connect KEY TRACK, undo and reload. Correct the swapped Sample Play labels; release-triggered samples retain press velocity when a controller releases with velocity zero, with duplicate-release and All Notes Off cancellation. Keygroup filter/envelope/LFO consumers, simultaneous overlapping zones and voice stealing remain partial; PG-06 is corrected accordingly.
- **G3: solo and mute controls.** Return/output solo callbacks were commented out, and the audio layer excluded output buses. Add additive solo membership within each group, separate explicit mute from solo-derived mute, persist/undo membership, and restore it into the audio layer. Return/output mute/solo Q-links and master mute now edit audio state instead of only their icons.
- **G4: limiter.** Connect the master strip to a saved per-output limiter. Processing follows inserts/gain, links stereo channels, limits sample peaks to 0.98 immediately and releases over 100 ms. It defaults off, adds no lookahead/makeup, remains active with bus-insert bypass, and is not a true-peak limiter.
- **G5: inventory consistency.** Refresh panel rows left stale after the timing implementation, regenerate structural indexes and reconcile Markdown/CSV capability rows. Four controls move to Present while PG-06 is corrected to Partial: 118 present, 26 partial, 20 missing and two unverified groups (110/26/20/2 across 158 base groups, plus eight present extensions).

Validation (2026-10-29): 353 engine checks pass on Windows Debug, AddressSanitizer and no-ASIO Release, and native Linux Release (33 new checks beyond the timing pass). The 16 new program/mixer UI checks and 25 timing/control checks pass per layout on all four configurations. Existing MIDI/controller/startup suites pass 71 checks on Windows ASan/Release and Linux. All nine broader UI cases pass on Windows Release and Linux, including three panel runs at 227 assertions each. All final test processes exit 0; there are no AddressSanitizer reports. Final Windows/native Linux source contents match across 69 C++ files after normalizing line endings. Evidence and helpers are in `../devtools/program-mixer-review/`. Engine tests exercise rendered limiter output, real keygroup voices, solo membership, release triggers, undo and project reload; `KICKEM_SELFTEST=program-mixer` exercises the real editor/mixer components in both simulator layouts. Physical MIDI loopback and embedded hardware acceptance remain separate.

The first diagnostic UI fixture omitted the nonzero bounds that KickGrid needs to attach its controls; the final fixture lays out the real components and uses bounded waits for queued callbacks. Earlier focused engine fixtures also needed asset registration, normal voice cleanup and full bypass-ramp settling. Those diagnostic failures are retained in the evidence folder. The final checks above use the completed fixtures; no unresolved test failures remain. The desktop shortcut still targets the rebuilt no-ASIO `Kick/build/dist/KT_KickEM.exe`. Changes remain uncommitted and unpublished.

## Sample instrument gap filling

The instrument pass connects the actual saved LFO/AMPENV/FILTERENV/PITCHENV siblings to shared DrumVoice consumers for both drums and keygroups. Envelope TYPE and filterType are separate, normalized LFO pan remains floating point, and filter/envelope/LFO defaults bypass processing. Shared controls expose filter selection/cutoff/resonance, three envelopes with independent Release/Depth, LFO enable and zone selection; the existing LFO tabs also expose enable. Undo refreshes the new controls, and live-program validation prevents stale project edits.

All overlapping keygroup zones play, with independent One Shot/Note On/Note Off behavior. Pending release triggers retain press velocity and zone identity, ignore duplicate/stale releases, and clear on All Notes Off even when muted. Per-zone note limits and program limits replace older voices, preferring release tails. Drums retain choke/simultaneous groups with recursion protection. Sparse valid layer slots work in Cycle/Random; inclusive velocity ranges use first-match hard splits and silent gaps. No velocity crossfade is claimed.

The shared voice applies program gain/pan only once and configures the resample rate when a slice is assigned after prepare. LFO frequency updates preserve phase/fade across blocks, and Sample & Hold uses a held value until its next boundary. Enabled amplitude envelopes keep a finite release tail; stage values remain source-duration-relative. The supplied 27 filter models remain a reconstruction of unavailable original DSP. The manual describes exact ranges and semantics.

Validation (2026-10-29): 393 engine checks pass on Windows Debug, AddressSanitizer and no-ASIO Release, and native Linux Release (40 new instrument checks). All four configurations pass both layouts with 15 instrument, 16 program/mixer and 25 timing/control UI checks per layout. The 71-check MIDI/controller/startup suites pass on Windows ASan/Release and Linux. All nine broader UI cases pass on Windows Release and Linux, including three panel runs at 227 assertions each. All final test processes exit 0, with no AddressSanitizer reports. Final Windows/native Linux source contents match across 96 changed C++ files after line-ending normalization.

Evidence is in `../devtools/instrument-review/`. The initial full-device transport test replayed an unrelated drum pattern; the final fixture mutes that track while retaining the original synth silence threshold. Earlier focused fixture failures and logs are retained. PG-03, PG-04 and PG-06 move to Present; remaining firmware work and physical-device acceptance stay explicit in the inventory. No commit, push or publication is part of this pass.

## Audio recording and editing gap filling

- Connect selected audio-input routing, visible ARM/MONITOR, independent track clocks and actual region playback. Add bounded one-pass selected-track recording, count-in gating, checked 24-bit WAV saves, additive overdub and undoable REC replacement that preserves outside audio.
- Connect audio Pencil, Copy/Paste and Split/Cut, plus mute/level/reverse/tuning/fades and all seven automation lanes. Preserve source bounds and fade timing across edits. Clear/erase automation supports undo; send automation restores manual levels on stop.
- Fix duplicate input notifications, exact loop-boundary wrapping, late stopped-track ticks, waveform level scaling, stale audio-editor targets and unscoped cross-component notifications. Replace index-based program solo membership and immediately gate sustained synth/audio output when another program is soloed.
- Keep WARP/BPM stretching, global automation capture, rendered bounce/export and physical-device acceptance explicit as remaining work. Update README, manual and the inventory; six existing groups become Present.

Audio input is copied before output buffers are cleared. The audio callback performs no recording file writes or tree edits; the message thread commits a take only after a checked temporary WAV replacement. Capture is bounded to 256 MiB and the active range end. Earlier regions are cropped only after successful save/registration, and undo retains the WAV. Source snapshots retain decoded audio; per-track clocks stop on removal and ignore late ticks. Monitor defaults off for audio, while MIDI/pad ARM and monitor default on.

Automation is step-held; volume/pan/mute are consumed in region playback, and sends follow the existing tick path. Warp and guaranteed sample accuracy are not claimed. Program solo membership is independent of indexes and restored constructor order, with scoped notifications to ongoing synth renderers. Audio editor notifications use scoped, shared signals rather than per-translation-unit statics.

Validation (2026-10-29): 496 engine checks pass on Windows Debug, AddressSanitizer and no-ASIO Release, and native Linux Release. Both layouts pass 23 audio-workflow, 15 instrument, 16 program/mixer and 25 timing/control UI checks on all four configurations. Separate-process project/startup checks pass all four launch cases, restored autosave, name collisions and visible-project close (21 assertions plus five fixture checks per configuration). The existing 71-check MIDI/controller/startup suites pass on Windows ASan/Release and Linux. All nine broader UI cases pass on Windows Release and Linux, including three panel runs at 227 assertions each. All final test processes exit 0 with no AddressSanitizer reports. Windows/native Linux contents match across 131 changed C++ files, two CMake files and the community version file after line-ending normalization. The final public-load directory guard was checked with the 26-test project suite on all four configurations after the full regression runs; it rejects a directory collision before closing the active project. Evidence is in `../devtools/audio-workflow-review/`. Diagnostic failures are retained: the first fixtures exposed an early deferred-device access, repeated input notifications and a rounding-boundary correction; Debug incremental linking also rejected the large unity object, so Debug targets now request a full link.

Project workflow follow-up applies tempo/meter/bars/counts/key to complete new project documents before loading them. Later programs inherit the saved project key. Autosave configuration is per XML instance and restores from saved preferences at engine creation; disabling project autosave leaves preference persistence independent. Dirty edits made while disabled save after it is enabled. XML output uses a checked temporary replacement, and corrupt candidate projects are rejected before closing the current project. Selector, New Project and Last Used policies are honored; automatic creation avoids existing names, missing last projects fall back to the selector, and creation/load errors stop the loading animation. The legacy Auto Load switch and launch dropdown stay synchronized. Focused tests cover failed-write preservation, real defaults/reload and separate-process preference/startup restoration.

Final platform follow-up: refuse unexpected existing WAV targets and project/XML directory collisions before replacing files or editing regions. Linux exposed JUCE replacement of an empty target directory; new file/directory collision checks preserve unrelated content. Startup close checks also exposed a stale track-data pointer; track rows now detach before deletion and validate meter/solo targets. Main Track ARM/MONITOR use the same saved note/audio state as mixer controls, with undo and scoped program-change refresh. MIDI preference tests explicitly reconnect their synthetic controller before injecting events; production disconnected-device protection is retained.
