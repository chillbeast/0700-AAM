# Code audit handoff (2026-10-27)

**Review, local repairs and desktop validation completed on 2026-10-27.** See [AUDIT-FIXES.md](AUDIT-FIXES.md) for the implementation, final results, and hardware acceptance limits. The baseline findings below describe the pre-repair revision.

**Community simulator v0.1.2 (2026-10-28):** sample voice/source lifetime fixes, fractional loop seams, mono Modular effects, exact DSP coefficient caching and the release-review live audition race repair are documented at the end of `AUDIT-FIXES.md`. See [RELEASE-NOTES.md](RELEASE-NOTES.md) for the vector panel, validation and remaining limits. The preceding audit build is [v0.1.1](https://github.com/chillbeast/0700-AAM/releases/tag/v0.1.1) at `e5f5ea8c`.

**Original review handoff:** The completed findings, verification results, corrections to this original handoff, and repair priorities are in [AUDIT-REVIEW.md](AUDIT-REVIEW.md). The review was read-only; the subsequent repair pass changes production source and tests and is documented with its commit on `desktop-build`.

The completed pass adds 30 findings to the 32 listed here. Eleven findings were reproduced against unchanged engine objects, including three AddressSanitizer memory errors. The existing Debug and ASan suites each passed 196/196, and both simulator self-tests completed successfully. Those baseline passes do not cover the new failure paths. Use the completed report's reconciliation table before implementing any inherited finding.

**Functionality inventory (2026-10-28):** [FEATURE-INVENTORY.md](FEATURE-INVENTORY.md) is the current capability/gap ledger. It reconciles the older manual, maps every panel command and screen ID, indexes UI handlers, records state/acceptance requirements, and distinguishes regression coverage from feature completion. Its inventory-driven preview regression fix is tracked with O5.

**Simulator panel:** the supplied `kick-flat.png` is reconstructed as an embedded SVG with 57 named layout regions. Hardware is the default layout; the toolbar retains Original as a saved preference. Both layouts reuse the same controls, native 1024 x 600 screen, firmware packets and LED state. See [artwork notes](Kick/apps/kickem/artwork/README.md) for regeneration and tests. Release panel checks pass 227/227 for default layout, preference restart and Original override, including switching with an open dialog. Popup, Audio, instrument and Modular checks also pass. MASTER/REC remain analog visual references. The source and downloadable Windows build are associated with [community v0.1.2](https://github.com/chillbeast/0700-AAM/releases/tag/v0.1.2); follow the release guide's public asset/hash verification when publishing.

## Project and rules

- **Repository:** the community fork of the Behringer KICK firmware, at `E:\BehringerKick\0700-AAM`, branch `desktop-build`.
- **Stack:** C++17, JUCE 7.0.12, CMake with CPM, MSVC VS2022 on Windows. The device itself is an RK3588 running ARM Linux; the Windows simulator `KT_KickEM` is built from the same code.
- **Review baseline:** `1a6f1769`, tagged `v0.1.0`, was already pushed to the fork. The working tree was clean before the audit; subsequent repairs are recorded in branch history.

**Publishing rules (must follow):**
- Push only to the remote `fork` (https://github.com/chillbeast/0700-AAM), and only when the user asks.
- Never push to `origin`, which is upstream MusicTribeOpenSource. Never open upstream PRs.
- Commit email: `58664879+chillbeast@users.noreply.github.com` (already set in the repo config).
- Don't push the branch `backup/desktop-build-before-email`.
- Never commit or distribute the ASIO SDK (`C:/Users/chill/Documents/asio-sdk`) or ASIO builds. Build release zips only from `Kick/build/dist`, which has no ASIO.
- No personal prefs and no commercial sample kits (ESW, LUN, DILLA...) in releases.
- Don't kill the user's running simulator.

## Build and test

Run the builds from PowerShell; Git Bash mangles MSBuild `/flags`.

| Folder | Config | What it builds |
|---|---|---|
| `Kick/build/win` | Debug | Targets `Kick` and `KickEngineTest`. |
| `Kick/build/sim` | Debug | Simulator `KT_KickEM`. |
| `Kick/build/release-sim` | Release | Release simulator, built **with ASIO**. Never distribute it. |
| `Kick/build/asan` | RelWithDebInfo | Build with `/fsanitize=address`. |
| `Kick/build/dist` | Release | Release simulator without ASIO. Release zips come from here. |

- **Engine tests:** run `cmake --build Kick/build/win --config Debug --target KickEngineTest --parallel`, then run the exe.
  - It writes `engine_test_results.txt` next to the exe, and the exit code is the number of failures.
  - The v0.1.2 suite passes 264/264 in Debug and ASan (243 audit-repair checks plus 21 optimization/release regressions). The original 196-check baseline and pre-repair memory-error evidence are preserved in the review; use the permanent regressions for the current tree.
- **Simulator self-tests:**
  - Set `KICKEM_SELFTEST=1|audit|instrument|audio|tour|modular|panel|panel-preference|popup`.
  - Add `KICKEM_LOG=<file>` to write a log.
  - Self-tests use a temp prefs copy, so they never touch the user's prefs.
- **ASan:**
  - Add `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64` to PATH.
  - Set `ASAN_OPTIONS=log_path=asan_report:halt_on_error=1` (colon-separated options; use a relative log path to avoid an unquoted Windows drive colon being parsed as an option).
  - Run from the build folder as the current directory.

## Audit status

The code was split into six areas. The risk-based source review is now complete across all six; production code was kept read-only. Hardware-only behavior still needs device acceptance testing, as documented in the completed report.

| Area | Status |
|---|---|
| `engine/audio_program` (programs, synth, instrument, Modular engine) | A1–A14 rechecked; A1/A2 reproduced |
| `kick_ui` (all screens, swupdate, file browser, sampler, recent projects) | U1–U18 rechecked; U19 added |
| `engine/audio_engine` (transport, sequence, track, note, recorder, sample player, bus, inserts, looper) | Reviewed: E1–E11 in completed report |
| `engine/{audio_file_manager,audio_grouping,audio_midi,interface,objects,project}` and `_playback` | Reviewed: F1–F5 in completed report |
| `hardware`, `ipc`, `input_events`, `contracts`, `common`, `compat`, `kick_leds`, `audio_interface`, `apps/*` | Reviewed: H1–H8 and scoped observations in completed report |
| `program_edit_ui` (incl. Modular editor), `kick_ui_channel_mixer/common/components/controls/header/selectors/settings/systemResources` | Reviewed: P1–P5 in completed report |

Notes on the findings:
- **Verification:** the original ✅ marks identify the earlier hand checks. Every inherited ID has now been reconciled in `AUDIT-REVIEW.md`; that table distinguishes reproduced bugs, source-confirmed issues, unmeasured performance candidates, gaps, and narrowed claims.
- **Paths:** all are relative to `Kick/modules/`.
- **Fixes:** implemented; see `AUDIT-FIXES.md`. Original standalone evidence remains outside the repository in `../devtools/audit-2026-10-27/`; repaired results and logs use `../devtools/audit-fixes-*`.

### A. Programs and the Modular engine

Modular files live in `engine/audio_program/instrument/modular/`, abbreviated `modular/` below.

**A1 ✅ HIGH: notes held when the arp is turned on never release.**
- Where: `modular/mt_modular_engine.cpp:240-254` (`startNote` / `releaseNote`), `:413-432` (`arpNoteOff` / `arpStop`), `:1024-1039` (carry-over in `setPatch`).
- What happens: once the arp is on, `releaseNote` sends every note-off to `arpNoteOff`. Voices started before the arp was on are never stopped.
- Same problem after a preset change: carried-over notes go through `playNote`, which skips the arp.
- Fix: in `releaseNote`, also `stopNote` any gated voice the arp doesn't own. In the carry-over, route notes through `arpNoteOn` when the new graph's arp is on.

**A2 ✅ HIGH: changing play mode while notes are held leaves stuck or phantom notes.**
- Where: `modular/mt_modular_engine.cpp:317-363`. `structureKey` (`modular/mt_modular_patch.cpp:53-72`) excludes `playMode`, so a mode change doesn't rebuild the graph.
- Poly → Mono: `stopNote` only releases `voices[0]`.
- Mono → Poly → Mono: the `held` stack keeps stale notes, which can sound with no key down.
- Fix: on a play-mode change, release all gated voices and set `numHeld = 0`.

**A3 MED: re-preparing the device while notes are held leaves stuck voices or a runaway arp.**
- Where: `instrument/mt_instrument_object.cpp:71-76`; `modular/mt_modular_engine.cpp:177-188` and `:880-887`.
- What happens: `prepareToPlay` clears `sounding` but never calls `engine->allNotesOff()`. `Graph::prepare` keeps `held`, `arpHeld` and `arpSounding`.
- Fix: reset the engine in `prepareToPlay`, and reset the arp and mono stack in `Graph::prepare`.

**A4 MED: runaway feedback patches pass huge finite values downstream.**
- Where: `modular/mt_modular_engine.cpp:620-625`.
- What happens: the guard only catches non-finite values, and only checks the last sample.
- Fix: kill the voice when the sub-block peak goes above about 32, or soft-clip the voice mix.

**A5 MED (realtime/perf): `freeVoice()` → `Delay::reset()` clears the whole 1-second buffer on the audio thread.**
- Where: `modular/mt_modular_modules.cpp:557-567`; `modular/mt_modular_engine.cpp:559-567` and `:650-651`.
- Cost: with 16 voices, about 3 MB per Delay module at 48 kHz.
- Fix: track how much was written since the last clear and clear only that, or clear lazily.

**A6 MED: drum voice filter-modulation formula looks wrong** (probably inherited from upstream; check against the original before changing it).
- Where: `engine/audio_program/synth/drum_voice.cpp:184`, which reads `fMod = filtAdsr*depth*((lfoToFilter*lfoOut)+(lfoToPitch*lfoOut))`.
- What happens: the filter envelope does nothing without an LFO, and the pitch-LFO amount also moves the cutoff.
- Also: filter coefficients (`tan`) are recomputed every sample (`mt_voice_dsp.h:84`).

**A7 LOW (gap): structural edits during playback click.**
- What happens: releasing voices are cut, and effect tails (reverb, delay) stop dead because the effects are new instances.
- Fix: crossfade, or move effect instances over when type and slot match.

**A8 LOW: arp drifts against the tempo.**
- Where: `modular/mt_modular_engine.cpp:493-531`.
- What happens: `arpCountdown = step` throws away the overshoot.
- Fix: use `+=`.

**A9 LOW: a deferred note-off hits a retriggered note in the same block.**
- Where: `instrument/mt_instrument_object.cpp:100-127`.
- Fix: on NoteOn, drop that note from `deferredOffs`.

**A10 LOW: arp sub-blocks shorter than 16 samples leave stale tail samples in one-block-late cable outputs.**
- Where: `modular/mt_modular_engine.cpp:603-604` and `:672-675`.

**A11 LOW (realtime): if `prepare` ran during a build, `setPatch` re-prepares (allocates) while holding `graphLock`, and the audio thread spins on that lock.**
- Where: `modular/mt_modular_engine.cpp:1020-1023`.

**A12 LOW (perf): transcendental functions every sample.**
- Where in `modular/mt_modular_modules.cpp`:
  - osc `std::sin` for each unison copy, `:155`;
  - chorus `sin`, `:763`;
  - drive `tanh` + `sin`, `:546`;
  - ladder `tanh`, `:253`.
- Fix: tables or polynomial approximations.

**A13 LOW (gap): no sustain pedal (CC64) for instrument programs.**
- Where: `engine/audio_program/source/mt_audio_instrument_program.cpp:59-79` and `mt_audio_iprogram.cpp:312-320`.

**A14 LOW: user preset file-name collisions.**
- Where: `modular/mt_modular_patch.cpp:1015-1037`.
- What happens: "Bass?" and "Bass" map to the same file, and `remove()` doesn't check the tag name.

### U. Main UI (`kick_ui`)

**U1 ✅ CRITICAL: MIDI-learn callbacks capture function locals by reference (`[&]`).**
- Where: `kick_ui/modes/midicontrol/kick_MidiControlTabs.cpp:2556, 2568, 2603, 2688, 2716, 2730, 2759` (in `parameterMapping`, from line 2520).
- What happens: the `callAsync` lambdas run after the function returns, so they read dangling locals (`program`, `value`, `seqNum`, ...). Turning a learned CC can crash.
- Fix: capture by value, and look programs and buses up again inside the callback.

**U2 CRITICAL: MCU firmware-update acks are never routed, so the update loops forever at 100% CPU.**
- Where: `kick_ui/dialogs/kick_AppHome.cpp:355-388` and `kick_ui/swupdate/kick_SWUpdateScreen.cpp:229-258`.
- What happens: `KickHome::handleCommand` drops `EN_SYS_SW_UPDATE` SYSTEM packets.
- Not confirmed on hardware.

**U3 HIGH: swupdate status callbacks touch the UI from worker threads.**
- Where: `kick_ui/swupdate/kick_SWUpdateScreen.cpp:103-227`.
- What happens: popups, `closeScreen` and the label string are all changed off the message thread, and `delete warningPopup` deletes a local of a stack frame that has already returned.
- Fix: `callAsync` with values copied in.

**U4 HIGH: the SWUpdateView device list is cleared right after it's filled, so tapping Update uses freed data.**
- Where: `kick_ui/swupdate/kick_SWUpdateView.cpp:110-175` and `:232-252`.
- Also: `onThreadEnd` is set after `startThread()`.

**U5 HIGH: hardware buttons can navigate away mid-flash, and a second update can then be started.**
- Where: `kick_ui/dialogs/kick_AppHome.cpp:389-470` and `kick_ui/swupdate/kick_SWUpdateScreen.cpp:56-78`.
- Fix: block navigation while updating, and add an in-progress guard.

**U6 MED: the post-update reboot uses `reboot -nf`, so the filesystem is never synced.**
- Where: `kick_ui/swupdate/kick_SWUpdateScreen.cpp:125`.

**U7 ✅ HIGH: a new project from a template overwrites an existing project with the same name.**
- Where: `kick_ui/recent_project/kick_RecentProject.cpp:157-169`.
- What happens: there is no duplicate, empty-name or path-separator check (`createProject` has the duplicate check).

**U8 HIGH: track list rows are never trimmed.**
- Where: `kick_ui/track_view/source/kick_AppTrack.cpp:189-198` and `kick_AppTrackComponent.cpp:273-284`.
- What happens: `refreshSoloMute` writes through stale `pTrack` pointers after the track count shrinks.

**U9 MED: the Recent Projects encoder press can index past the end of the list after a remove.**
- Where: `kick_ui/recent_project/kick_RecentProject.cpp:375-380` and `:437`.

**U10 MED: `onProjectLoaded` runs on the loader thread and edits the settings ValueTree.**
- Where: `kick_ui/recent_project/kick_RecentProject.cpp:134-140` and `kick_RecentProject_Utils.cpp:19-38`.
- Also: `isProjectLoading` is a non-atomic bool.

**U11 MED (perf): the file browser computes folder sizes recursively, synchronously and per row, on the message thread.**
- Where: `kick_ui/modes/filebrowser/kick_fileBrowser.cpp:512-577` and `:634-641`; `kick_fileUtils.cpp:42-62`.
- Also: the size is an int32, so folders over 2 GB overflow.

**U12 MED (perf): the sampler recorder rebuilds its whole thumbnail at 30 Hz while recording, and passes the wrong sample rate.**
- Where: `kick_ui/audio_components/sampler/kick_SamplerRecorder.cpp:407-431`.

**U13 MED: the USB callback stays bound after the Firmware Update view is left through the header back button.**
- Where: `kick_ui/swupdate/kick_SWUpdateView.cpp:12, 225-226, 286-295`.

**U14 MED: the file browser's delete guard compares names only, so deleting a parent folder removes the open project.**
- Where: `kick_ui/modes/filebrowser/kick_fileBrowser.cpp:1071-1157`.

**U15 LOW: the looper reads `values[1]` for a mono meter, and line 350 sets `playB` where it should set `reverseB`.**
- Where: `kick_ui/audio_components/looper/kick_LoopRecorder.cpp:350` and `:363`.

**U16 LOW (gap): Sampler Export/Clear and Song Export are empty handlers.**
- Where: `kick_ui/audio_components/sampler/kick_Sampler.cpp:56-66` and `kick_ui/modes/kick_song.cpp:411-421`.

**U17 LOW: null dereference of `getProgram(getSelectedProgram())`.**
- Where: `kick_ui/audio_components/sampler/kick_Sampler.cpp:232`.

**U18 LOW: dialog content leaks on Cancel, and a status condition is always true.**
- Leaks: `kick_AppTrackOptions.cpp:236`, `kick_AppGrid.cpp:953`, `kick_fileBrowser.cpp:818-825`.
- Always true: `kick_SWUpdateScreen.cpp:177` (`|| ENUM` is always true).

## Current follow-up

The repair implementation and desktop checks are tracked in [AUDIT-FIXES.md](AUDIT-FIXES.md), including H9 and optimization round 2 (O1-O5). Debug and ASan each pass 258 checks; Release passes 15 optimization checks. The inventory-discovered preview lifecycle regression is fixed and its separate ASan probe exits 0. The simulator rebuild succeeds; the prior round-2 audit/instrument/Modular smoke results remain historical coverage.

Remaining acceptance requires the physical device: MCU acknowledgement/timeout flow, firmware transfer through USB, UART fragmentation on the real transport, CV calibration output, and ARM audio deadlines. A12 remains a profiling candidate; accurate DSP functions were retained. Linux multiprocess recovery stress was outside this desktop pass.

The portable audit subset (59 checks including optimization regressions) is available with `KICK_TEST_AUDIT_ONLY=1`; `KICK_TEST_OPTIMIZE_ONLY=1` runs the 15 new checks, and `KICK_TEST_DSP_BENCH=1` adds warmed module timing measurements. Ordinary `KickEngineTest` runs the complete suite. Simulator modes include `audit`, `instrument`, and `modular`. Use isolated preferences and run suites serially. Preserve the publishing restrictions above: only push to `fork` when requested and package distributable builds from `Kick/build/dist` without ASIO or personal/commercial content.
