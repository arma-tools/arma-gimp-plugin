#!/bin/sh
# Builds the plug-in and installs it into the user's GIMP plug-in folder
# (e.g. ~/.config/GIMP/3.2/plug-ins/arma-gimp-plugin/).
set -e
cd "$(dirname "$0")"

cargo build --release
GIMPTOOL=$(command -v gimptool-3 || command -v gimptool)
"$GIMPTOOL" --install-bin target/release/arma-gimp-plugin
