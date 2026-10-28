# KICK panel artwork

`kick-panel.svg` is a vector reconstruction of the supplied `kick-flat.png` front panel. It contains paths, text, shapes and gradients; no bitmap is embedded. The Behringer wordmark reuses the existing outlined repository asset.

Edit the geometry and labels in `generate_panel.py`, then run:

```powershell
python Kick/apps/kickem/artwork/generate_panel.py
```

Commit both the generator and generated SVG. The simulator embeds the SVG at build time, so it needs no external artwork file at runtime. Python is only needed to regenerate the artwork.

The transparent `hit-regions` group supplies 57 named rectangles, including the screen aperture, control groups, and individual controls. Simulator coordinates are twice the SVG viewBox coordinates. Keep the artwork and hit regions together when changing positions. The touchscreen keeps its native 1024 x 600 resolution and is transformed into its aperture; the whole board is then scaled to fit the window.

The toolbar switches between **Hardware** and **Original** layouts and saves the choice in the simulator preferences. Both layouts use the same live controls and firmware screen. Hover a control for its mouse interaction or Shift function. MASTER and REC are visual references for the hardware's analog controls, as in the original interface.

For automated checks, `KICKEM_LAYOUT=hardware|original` overrides the startup preference. `KICKEM_SELFTEST=panel` checks startup preference/override handling, both layouts, click targets, firmware packets, state preservation and switching layouts with a dialog open; it finishes by saving Original. Run `KICKEM_SELFTEST=panel-preference` with the same isolated `KICK_PREFS_DIR` and no layout override to verify that choice survives a restart. A fresh preferences directory defaults to Hardware. Tests use a bundled template and need no commercial sample kit. `KICKEM_SHOT` names a directory for full-panel PNG previews. Existing popup, instrument and modular self-tests can also run with either layout override.
