# Kick

Application software for the BE 0700-AAM, written in C++ with [JUCE](https://juce.com).

## License

Kick is licensed under the GNU Affero General Public License v3.0 or later, see [LICENSE](LICENSE).
Bundled third-party code keeps its own license (see below).

## Building

The community `desktop-build` branch supports Windows x64 and Linux x86_64 simulators. Use the [release/build guide](how-to/release-simulator.md) for configuration, dependencies, validation and packaging, and the [manual](docs/manual/USER_MANUAL.md) for operation. Public archives are on [GitHub Releases](https://github.com/chillbeast/0700-AAM/releases). Desktop builds use the reconstructed `AudIO` and `ComProtocol` sources included at the repository root and disable embedded device controls.

For an embedded ARM firmware build, additional environment and hardware acceptance are still required:

- **Original private dependencies are not included.** The in-repository reconstructions replace them for community builds. If those directories are absent, `3rdParty.cmake` falls back to the private `MT_PLATFORM_GIT_PATH` repositories; that fallback requires your own replacements/access.
- **No cross-compilation SDK is distributed.** Build one from the companion Buildroot tree
  (`make KTKick_defconfig && make sdk`) and pass its `share/buildroot/toolchainfile.cmake`
  as `CMAKE_TOOLCHAIN_FILE`. See `how-to/Kick_Compilation.txt`.
- **JUCE** 7.0.12 is downloaded by CPM and patched with `patches/juce_multitouch_7.0.12.patch`.

## Removed from this release

Material that could not be redistributed was removed. The code may still refer to it by path:

- Audio samples (factory content and metronome / percussion sounds).
- The Myriad Pro font. The default theme now uses Source Sans 3 (SIL OFL 1.1) instead; to use another font, drop it in `assets/Default` and change `<FONT _0=...>` in `theme.xml`.
- The prebuilt aarch64 SDK tarball.
- `modules/3rdParty/juce_serialport`, which has no license.

## Third-party code included

| Component | Location | License |
|---|---|---|
| CPM.cmake | `get_cpm.cmake` | MIT |
| cmake_timestamp (Kalle Raiskila) | `timestamp/` | see `timestamp/license.txt` |
| dywapitchtrack (Antoine Schmitt) | `modules/3rdParty/dywapitchtrack/src` | MIT |
| fpm | `modules/3rdParty/fpm` | MIT |
| juce-toys, JUCE natvis (Jim Credland) | `modules/3rdParty/juce-toys`, `juce.natvis` | MIT |
| magic_enum | `modules/3rdParty/magic_enum` | MIT |
| melatonin_parameters | `modules/3rdParty/melatonin_parameters` | MIT |
| moodycamel ConcurrentQueue | `modules/3rdParty/moodycamel` | see its `LICENSE.md` |
| NanoLog | `modules/3rdParty/NanoLog` | MIT |
| signal | `modules/3rdParty/signal` | MIT |
| dRowAudio waveform | `modules/3rdParty/waveform` | MIT |
| smbPitchShift (Stephan M. Bernsee) | `modules/engine/objects/sampler/smbPitchShift` | Wide Open License |
| stftPitchShift (Juergen Hock) | `modules/engine/objects/sampler/stftPitchShift` | MIT |
| Gaussian blur (Roland Rabien) | `modules/kick_ui_controls/source/kick_utils_blur.h` | BSD 3-Clause |
| ttymidi (Thiago Teixeira) | `apps/midiPort/src/ttymidi.c` | GPL-3.0-or-later |
| Source Sans 3 font (Adobe) | `assets/Default/SourceSans3-Regular.ttf` | SIL OFL 1.1, see `assets/Default/SourceSans3-OFL.md` |
