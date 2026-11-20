#!/usr/bin/env bash
# Builds the .pkg installer for one NA Encyclopedia volume.
#
#   installer/build-pkg.sh <AU .component> <VST3 .vst3> <bundle id> <title> <output dir>
#
# Installs for every user on the Mac (same as the other Noveltee Audio plugins):
#   AU   → /Library/Audio/Plug-Ins/Components
#   VST3 → /Library/Audio/Plug-Ins/VST3
# The version is read from the built AU, so the package always matches the plugin.
#
# Unsigned for now. Developer ID signing + notarization come in Milestone 8.

set -euo pipefail

if [ "$#" -ne 5 ]; then
    echo "usage: $0 <au.component> <plugin.vst3> <bundle-id> <title> <output-dir>" >&2
    exit 2
fi

AU="$1"; VST3="$2"; BUNDLE_ID="$3"; TITLE="$4"; OUT_DIR="$5"
for b in "$AU" "$VST3"; do
    [ -d "$b" ] || { echo "error: missing $b (build first)" >&2; exit 1; }
done

VERSION="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$AU/Contents/Info.plist")"
[ -n "$VERSION" ] || { echo "error: could not read the version from $AU" >&2; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/au" "$WORK/vst3" "$WORK/pkgs" "$OUT_DIR"
ditto "$AU"   "$WORK/au/$(basename "$AU")"
ditto "$VST3" "$WORK/vst3/$(basename "$VST3")"

# pkgbuild marks bundles "relocatable" by default: if Installer finds another
# copy anywhere on disk it updates THAT copy instead of installing where Logic
# looks. Turn relocation off (set it if present, add it if newer pkgbuild left it out).
component_pkg() {
    local root="$1" id="$2" dest="$3" out="$4"
    local plist="$WORK/$(basename "$out" .pkg).plist"
    pkgbuild --analyze --root "$root" "$plist" >/dev/null
    local i=0
    while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" >/dev/null 2>&1; do
        /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$plist" 2>/dev/null \
            || /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$plist"
        i=$((i + 1))
    done
    pkgbuild --root "$root" \
             --component-plist "$plist" \
             --identifier "$id" \
             --version "$VERSION" \
             --install-location "$dest" \
             --scripts "$SCRIPT_DIR/scripts" \
             "$out"
}

component_pkg "$WORK/au"   "$BUNDLE_ID.au"   "/Library/Audio/Plug-Ins/Components" "$WORK/pkgs/AU.pkg"
component_pkg "$WORK/vst3" "$BUNDLE_ID.vst3" "/Library/Audio/Plug-Ins/VST3"       "$WORK/pkgs/VST3.pkg"

cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>$TITLE $VERSION</title>
    <!-- hostArchitectures stops Apple Silicon Macs asking to install Rosetta. -->
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true" enable_anywhere="false" enable_currentUserHome="false"/>
    <allowed-os-versions><os-version min="12.0"/></allowed-os-versions>
    <choices-outline>
        <line choice="au"/>
        <line choice="vst3"/>
    </choices-outline>
    <choice id="au" title="Audio Unit (Logic Pro)" start_selected="true">
        <pkg-ref id="$BUNDLE_ID.au"/>
    </choice>
    <choice id="vst3" title="VST3" start_selected="true">
        <pkg-ref id="$BUNDLE_ID.vst3"/>
    </choice>
    <pkg-ref id="$BUNDLE_ID.au"   version="$VERSION" onConclusion="none">AU.pkg</pkg-ref>
    <pkg-ref id="$BUNDLE_ID.vst3" version="$VERSION" onConclusion="none">VST3.pkg</pkg-ref>
</installer-gui-script>
XML

FINAL="$OUT_DIR/$(echo "$TITLE" | tr -d ' ')-$VERSION.pkg"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" "$FINAL"
echo "Built $FINAL"
