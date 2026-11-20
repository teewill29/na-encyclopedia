#!/usr/bin/env bash
# Builds a macOS .pkg installer for one NA Encyclopedia volume.
#
# Installs for the current user (no admin password):
#   AU   → ~/Library/Audio/Plug-Ins/Components
#   VST3 → ~/Library/Audio/Plug-Ins/VST3
#
# Usage:
#   installer/build-pkg.sh <AU .component> <VST3 .vst3> <version> <bundle id> <title> <output .pkg>
#
# Unsigned for now. Developer ID signing + notarization come in Milestone 8.

set -euo pipefail

if [ "$#" -ne 6 ]; then
    echo "usage: $0 <au.component> <plugin.vst3> <version> <bundle-id> <title> <output.pkg>" >&2
    exit 2
fi

AU="$1"
VST3="$2"
VERSION="$3"
BUNDLE_ID="$4"
TITLE="$5"
OUTPUT="$6"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

build_component_pkg() {
    local bundle="$1" location="$2" id="$3" out="$4"
    local root="$WORK/root-$(basename "$out" .pkg)"
    local plist="$WORK/$(basename "$out" .pkg).plist"

    mkdir -p "$root"
    ditto "$bundle" "$root/$(basename "$bundle")"

    # Stop Installer from "upgrading" a copy of the bundle it finds elsewhere
    # on disk (e.g. in a build folder) instead of installing to our location.
    pkgbuild --analyze --root "$root" "$plist"
    plutil -replace BundleIsRelocatable -bool NO "$plist"

    pkgbuild \
        --root "$root" \
        --component-plist "$plist" \
        --identifier "$id" \
        --version "$VERSION" \
        --install-location "$location" \
        --scripts "$SCRIPT_DIR/scripts" \
        "$out"
}

build_component_pkg "$AU"   "/Library/Audio/Plug-Ins/Components" "$BUNDLE_ID.au"   "$WORK/au.pkg"
build_component_pkg "$VST3" "/Library/Audio/Plug-Ins/VST3"       "$BUNDLE_ID.vst3" "$WORK/vst3.pkg"

cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>$TITLE</title>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_anywhere="false" enable_currentUserHome="true" enable_localSystem="false"/>
    <os-version min="12.0"/>
    <choices-outline>
        <line choice="au"/>
        <line choice="vst3"/>
    </choices-outline>
    <choice id="au" title="Audio Unit (Logic Pro, GarageBand)" start_selected="true">
        <pkg-ref id="$BUNDLE_ID.au"/>
    </choice>
    <choice id="vst3" title="VST3" start_selected="true">
        <pkg-ref id="$BUNDLE_ID.vst3"/>
    </choice>
    <pkg-ref id="$BUNDLE_ID.au" version="$VERSION">au.pkg</pkg-ref>
    <pkg-ref id="$BUNDLE_ID.vst3" version="$VERSION">vst3.pkg</pkg-ref>
</installer-gui-script>
EOF

mkdir -p "$(dirname "$OUTPUT")"
productbuild \
    --distribution "$WORK/distribution.xml" \
    --package-path "$WORK" \
    --identifier "$BUNDLE_ID" \
    --version "$VERSION" \
    "$OUTPUT"

echo "Built $OUTPUT"
