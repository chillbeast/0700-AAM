# Community simulator release

Run these commands from the repository root on Windows with Visual Studio 2022 and CMake. Publish only to the `fork` remote (`chillbeast/0700-AAM`), never upstream `origin`. Keep the ASIO SDK and SDK-enabled builds out of commits and distribution.

1. Review the pending source changes and the [functionality inventory](../../FEATURE-INVENTORY.md). Fix new regressions and document existing incomplete features. Update the README, manual, release notes, audit handoff, inventory and `Kick/apps/kickem/version.txt`. The simulator's community version is separate from `Kick/apps/kick/version.txt`, the upstream device firmware version.
2. Run the complete Debug and AddressSanitizer engine suites with isolated `KICK_PREFS_DIR` directories. Save `engine_test_results.txt` from each. Investigate every failure and sanitizer report.
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
