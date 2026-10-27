# arma-gimp-plugin

GIMP 3 plug-in for the texture formats of Arma:

- load **PAA** (Real Virtuality) and **EDDS** (Enfusion) files,
- export **PAA** files (DXT1, or DXT5 when the image has transparency, with
  mipmaps).

Written in Rust with [arma-file-formats](https://github.com/arma-tools/arma-file-formats-rs)
and the [gimp-rs](../gimp-rs) bindings.

## Building and installing

Requires Rust, the GIMP 3 development files and a checkout of `gimp-rs` next to
this repository (`../gimp-rs`).

```sh
./install.sh
```

builds the plug-in and installs it with `gimptool` into the user's plug-in
folder. Restart GIMP afterwards.

## Notes

- GIMP 3.2 ships its own PAA loader (`file-paa-load`), which cannot read DXT1
  textures. This plug-in registers its loader with a higher priority.
- Exporting requires power-of-two dimensions. Images are merged and converted
  to RGBA for the export, the image itself is not changed.

## Tests

```sh
tests/run-gimp-tests.sh
```

runs the plug-in in a headless GIMP with a temporary profile and checks loading
and exporting. The first run is slow while GIMP sets up the profile.

## License

GPL-2.0-or-later
