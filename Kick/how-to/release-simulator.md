# Community simulator release

Windows commands below run from the repository root with Visual Studio 2022 and CMake. The Linux procedure follows. Publish only to the `fork` remote (`chillbeast/0700-AAM`), never upstream `origin`. Keep the ASIO SDK and SDK-enabled builds out of commits and distribution.

1. Review the pending source changes and the [functionality inventory](../../FEATURE-INVENTORY.md). Fix new regressions and document existing incomplete features. Update the README, manual, release notes, audit handoff, inventory and `Kick/apps/kickem/version.txt`. The simulator's community version is separate from `Kick/apps/kick/version.txt`, the upstream device firmware version.
2. Run the complete Debug and AddressSanitizer engine suites sequentially, with isolated `KICK_PREFS_DIR` directories. Their temporary project names overlap on a shared host. Save `engine_test_results.txt` from each. Investigate every failure and sanitizer report.
3. Configure and build the distribution directory with ASIO and ASan disabled:

   ```powershell
   cmake -S Kick -B Kick/build/dist -G "Visual Studio 17 2022" -A x64 -DKICK_SIMULATOR=ON -DKICK_ENGINE_TEST=ON -DKICK_ASIO_SDK_DIR:PATH= -DUSE_ASAN_KICK=OFF -DTIMESTAMP=OFF
   cmake --build Kick/build/dist --config Release --target KT_KickEM KickEngineTest --parallel 3
   python Kick/tools/package_simulator.py --check-only
   ```

4. Run Release optimization regressions (`KICK_TEST_OPTIMIZE_ONLY=1`) and simulator checks against that build: `audit`, `audio`, `panel`, `panel-preference`, `popup` in both layouts, `instrument` and `modular`. Use fresh isolated preferences for each independent case; `panel-preference` shares the preceding panel run's preferences and has no `KICKEM_LAYOUT` override. Re-run `panel` with `KICKEM_LAYOUT=original` to verify the environment override. Set `KICKEM_LOG` and `KICKEM_SHOT` to directories outside the repository. Check both exit status and the expected completion/assertion lines. Inspect full-panel screenshots. Popup and panel tests use a bundled template; instrument/Modular tests also work without commercial kits.
5. Commit the reviewed files. Build once more to confirm the committed tree is current, then package the clean tree:

   ```powershell
   python Kick/tools/package_simulator.py
   ```

   The script reads the simulator version and checks the executable's Windows version resource, the ASIO/ASan configuration and every tracked runtime file's hash. It packages only the executable, tracked assets/templates/demos, documentation and licenses. Documentation keeps its repository-relative paths. `BUILD-INFO.json` records the exact source commit and executable hash; `SHA256SUMS.txt` covers the package contents, with a separate ZIP checksum. Archives are created only under `Kick/build/dist/releases/`, and existing release directories are never overwritten.

6. Extract the ZIP to a new directory outside the build tree. Check every manifest hash and run the extracted executable with an isolated preferences directory. At minimum repeat panel/preference and popup checks and inspect Audio's device-type list to confirm no ASIO backend is present. Check that the archive has no `prefs`, commercial `samples/projects`, SDK, PDB or import-library content.
7. Push `desktop-build` and the exact release tag to `fork`. Create a draft GitHub release for that tag, attach the Windows ZIP and its `.sha256`, verify both uploads, then publish. Keep the project's community prerelease status unless deliberately changing release policy. Do not replace an existing public release or delete old tags to correct a mistake.
8. Verify the tag resolves to the committed source, the release is public, its assets can be downloaded anonymously, and the downloaded ZIP matches the local SHA-256. Report the direct release link and exact commit to the user. A branch push alone does not create a downloadable release.

These checks establish desktop regression coverage for this release. They do not complete every inventory row or establish physical CV/MIDI/USB/UART/update behavior, Linux recovery, or ARM audio timing.

## Linux desktop

The Linux package targets **x86_64**, built on **Ubuntu 22.04** with GCC 11. It requires **glibc 2.35+**, an X11/XWayland desktop and system audio/graphics libraries. Extract the `.tar.gz` to a writable folder and run `./launch.sh`. The archive preserves executable permissions. `./launch.sh --version` prints the community version. Assets and default projects/preferences resolve beside the executable; `KICK_PREFS_DIR` overrides preferences.

On Ubuntu, install runtime libraries if the desktop does not already provide them:

```sh
sudo apt-get install libasound2 libasound2-plugins libjack-jackd2-0 libfreetype6 libfontconfig1 \
  libx11-6 libxcomposite1 libxcursor1 libxext6 libxi6 libxinerama1 libxrandr2 libxrender1 libgl1 libatomic1
```

For a source build, use a Linux filesystem and install build dependencies. The patched JUCE multitouch code requires `libxi-dev`:

```sh
sudo apt-get install build-essential cmake ninja-build pkg-config git python3 \
  libasound2-dev libjack-jackd2-dev libfreetype6-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxi-dev libxinerama-dev \
  libxrandr-dev libxrender-dev libgl1-mesa-dev libglu1-mesa-dev libcurl4-openssl-dev xvfb xauth
cmake -S Kick -B Kick/build/dist -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DKICK_SIMULATOR=ON -DKICK_ENGINE_TEST=ON -DTIMESTAMP=OFF -DUSE_ASAN_KICK=OFF
cmake --build Kick/build/dist --target KT_KickEM KickEngineTest --parallel 4
python3 Kick/tools/make_test_kit.py Kick/build/test-kit
KICK_PREFS_DIR="$PWD/Kick/build/test-prefs" xvfb-run -a Kick/build/dist/KickEngineTest "$PWD/Kick/build/test-kit"
python3 Kick/tools/package_simulator.py --check-only
```

Engine tests need a working audio callback. Headless validation can use ALSA's PulseAudio plugin connected to a private null sink; an Xvfb display supplies GUI rendering. This validates the Linux application and audio callback path without certifying physical interfaces. Run the same panel/preference, popup, audio, instrument and Modular checks as on Windows, with isolated preferences. Also run from a different working directory to catch accidental `/app` or current-directory resource dependencies.

After committing a clean source tree, `python3 Kick/tools/package_simulator.py` creates `KICK-Simulator-v<version>-linux-x86_64.tar.gz` and its `.sha256` under `Kick/build/dist/releases/`. It checks ELF architecture, the executable's `--version`, Release/desktop/ASIO flags, resolved shared-library dependencies and tracked runtime hashes. The archive includes `launch.sh`, the same documentation/licenses and source-commit manifest as the Windows package. Test a fresh extraction, inspect `ldd` for missing libraries, verify all manifest hashes and preserve both executable bits before uploading.

Desktop builds explicitly disable embedded I2C/CV, USB gadget, network configuration, firmware flashing and host power commands. Native system audio and MIDI remain available. Embedded firmware retains its device paths and platform implementation. GCC/Clang builds use normal configuration optimizations so non-finite audio guards remain effective.

Publish Linux and Windows assets under a new version tag when portability fixes change the source. Preserve earlier public tags and archives.
