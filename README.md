> There is no official hardware, and the manufacturer's repository is incomplete, so I've had to fill in the gaps. Nothing here is official; I'm doing this for fun. — lc

<p align="center">
  <img src="docs/images/social-preview.png" alt="Behringer KICK (BE 0700-AAM)" width="100%">
</p>

# Behringer KICK (BE 0700-AAM)

Source code for the Behringer KICK.

> [!NOTE]
> **About this fork: a community build, not an official Behringer / MUSIC Tribe release.**
>
> The `desktop-build` branch builds and runs the KICK firmware on Windows and Linux. It adds a hardware simulator (`KT_KickEM`), so you can play it with no KICK at all. On top of the original code it adds:
> - **Instrument programs**, with a **Modular synth engine**:
>   - a grid patch editor, cables, effects and an arpeggiator;
>   - Mono/Legato with glide, and pad pressure;
>   - 22 presets, and saving your own.
> - **Recording pads** into sequences, and REC on the Sampler.
> - **v0.1.5 timing/recording:** distinct REC/OVERDUB takes, one/two-bar count-in with a generated click, swing and undoable Timing Correct, editable meters, Tap Tempo, Note Repeat/Latch and velocity-based 16 Levels. See the [recording guide](Kick/docs/manual/USER_MANUAL.md#8a-recording-pads-into-a-sequence-).
> - **v0.1.5 program/mixer controls:** saved MIDI CC sliders, audible keygroup zones with KEY TRACK, additive return/output solo and a selected-output master limiter. See the [manual](Kick/docs/manual/USER_MANUAL.md).
> - **v0.1.5 sample instruments:** drum/keygroup filters, envelopes and LFOs now drive audio; overlapping keygroup zones layer, voice limits steal older voices, and Layer Mix exposes velocity ranges. See [sample voice controls](Kick/docs/manual/USER_MANUAL.md#sample-voice-controls-current-source).
> - **v0.1.5 project workflows:** saved defaults create complete projects, startup can show the selector/create a project/reopen the last project, and autosave restores its enable/interval settings. Project saves preserve the previous file if writing fails.
> - **v0.1.5 audio workflows:** selected-input recording with ARM/MONITOR, playable audio regions, pencil/copy/paste/split, fades/tuning and all seven automation lanes. See [audio-track recording](Kick/docs/manual/USER_MANUAL.md#audio-track-recording-current-source).
> - **Real CPU and RAM meters**, plus many crash and UI fixes.
> - **A vector hardware panel**, with live controls and LEDs, plus a saved **Hardware / Original** layout toggle. See the [simulator preview](docs/images/simulator-panel-v0.1.2.png) and [editable SVG](Kick/apps/kickem/artwork/kick-panel.svg).
> - **Sample playback and DSP fixes**, including loop-boundary interpolation, source lifetime handling and exact coefficient caching.
> - **Linux x86_64 desktop support**, with portable runtime paths and device-only controls disabled.
> - **MIDI controller learn:** right-click panel controls or use **MIDI LEARN** to map notes/CCs. Saved mappings work in both layouts, including relative encoders, held modifiers and preserved note/pedal releases. See [controller setup](Kick/docs/manual/USER_MANUAL.md#mapping-a-midi-controller).
>
> - **Try it:** download [community v0.1.5](https://github.com/chillbeast/0700-AAM/releases/tag/v0.1.5) for Windows x64 or Linux x86_64. Extract the entire archive, then run `KT_KickEM.exe` on Windows or `./launch.sh` on Linux. Keep assets, templates and demos alongside the executable. Packages exclude ASIO, personal preferences and commercial sample libraries. Linux requires glibc 2.35+ and an X11/XWayland desktop; see [Linux setup](Kick/how-to/release-simulator.md#linux-desktop). Older builds remain under [Releases](https://github.com/chillbeast/0700-AAM/releases).
> - **Manual:** [Kick/docs/manual/USER_MANUAL.md](Kick/docs/manual/USER_MANUAL.md) covers the panel, the screens and the Modular engine.
> - **Known gaps:** [FEATURE-INVENTORY.md](FEATURE-INVENTORY.md) records implemented, partial, missing and unverified capabilities. Opening a screen or lighting a panel button does not mean the underlying feature is complete. Hardware update, CV, USB/UART and ARM timing acceptance remain pending.
> - **Release changes and checks:** [RELEASE-NOTES.md](RELEASE-NOTES.md).
> - **Build it:** with Visual Studio 2022 and CMake, from `Kick/`:
>   `cmake -S . -B build/sim -G "Visual Studio 17 2022" -A x64 -DKICK_SIMULATOR=ON -DTIMESTAMP=OFF`, then
>   `cmake --build build/sim --config Release --target KT_KickEM`
> - **Licence:** AGPL-3.0, the same as upstream. The original code lives at [MusicTribeOpenSource/0700-AAM](https://github.com/MusicTribeOpenSource/0700-AAM).

<p align="center">
  <img src="docs/images/kick-front.webp" alt="Behringer KICK" width="720">
</p>

## Original product specification

The renders and specification below describe the intended hardware. They are not a list of completed or hardware-verified features in this community build; use the manual and inventory for implementation status.

- AI camera for gesture control
- Touchscreen
- 16 RGB pads, 8 banks
- Sampler with sample editing, step sequencer and looper
- Over 300 built-in effects
- 4 assignable knobs and a jog wheel
- MIDI in/out, USB and 4 CV / Gate jacks

## Connections

<p align="center">
  <img src="docs/images/kick-rear.webp" alt="Behringer KICK rear panel" width="100%">
</p>

| Rear | Front |
|---|---|
| 12 V DC power input, power switch | SD card slot |
| LINK (RJ45) | Headphones output |
| USB-C, USB-A | |
| MIDI IN, MIDI OUT (5-pin DIN) | |
| Stereo output, stereo input | |
| CV / Gate (4 jacks) | |

## Repository

| Folder | Contents | License |
|---|---|---|
| [`Kick/`](Kick) | Application | AGPL-3.0-or-later |
| [`AudIO/`](AudIO) | Audio engine library | AGPL-3.0-or-later |
| [`ComProtocol/`](ComProtocol) | Hardware communication protocol | AGPL-3.0-or-later |
| [`Linux/`](Linux) | Buildroot Linux image | GPL-2.0 (Buildroot) |

The root [`LICENSE`](LICENSE) is the AGPL-3.0 text that covers the Music Tribe code (Kick, AudIO, ComProtocol).
Third-party components keep their original licenses. See the README in each folder.

## Community

- **Chat with us:** [Discord](https://discord.gg/gS6qtuShvw)
- **Contribute:** fork the repository and open a pull request. No membership needed. See the
  [contributing guide](https://github.com/MusicTribeOpenSource/.github/blob/main/CONTRIBUTING.md).
- **Report a security problem** privately: [Report a vulnerability](https://github.com/MusicTribeOpenSource/0700-AAM/security/advisories/new).

<details>
<summary>Product renders</summary>

<p align="center">
  <img src="docs/images/views.png" alt="KICK top, front, side and rear views" width="100%">
</p>
</details>

---

Behringer is a brand of MUSIC Tribe. The Behringer name and logo, and the KICK name and logo, are
trademarks and are not covered by the software licenses in this repository.

Copyright (c) MUSIC Tribe.
