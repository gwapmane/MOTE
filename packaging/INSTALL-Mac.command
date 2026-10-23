#!/bin/bash
# Double-click this file. It copies MOTE into the folders your Mac's music programs look in.
cd "$(dirname "$0")" || exit 1
echo "Installing MOTE..."
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/Components"
rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/MOTE.vst3" "$HOME/Library/Audio/Plug-Ins/Components/MOTE.component"
cp -R MOTE.vst3 "$HOME/Library/Audio/Plug-Ins/VST3/"
cp -R MOTE.component "$HOME/Library/Audio/Plug-Ins/Components/"
# the download is marked "from the internet"; clear that mark so macOS will load the plugin
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/MOTE.vst3" "$HOME/Library/Audio/Plug-Ins/Components/MOTE.component" 2>/dev/null
xattr -dr com.apple.quarantine MOTE.app 2>/dev/null
echo
echo "Done. MOTE is installed for your user (VST3 and AU)."
echo "Now open FL Studio:  Options > Manage plugins > Find installed plugins."
echo "MOTE is an instrument, so look for it under Generators."
echo
read -n 1 -s -r -p "Press any key to close this window."
