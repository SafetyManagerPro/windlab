#!/usr/bin/env bash
# macOS runner packaging: checks Sonoma compatibility and builds an installable ZIP.
set -Eeuo pipefail
cd "$(dirname "$0")/.."
ROOT="${1:-build/WindLab_artefacts/Release}"
OUT_DIR="${2:-.}"
mkdir -p "$OUT_DIR"
ROOT="$(cd "$ROOT" && pwd)"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
VERSION="$(sed -n 's/^project(WindLab VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt | head -1)"
[[ -n "$VERSION" ]] || { echo "Cannot determine WindLab version" >&2; exit 1; }

VST3="$ROOT/VST3/WindLab.vst3"
AU="$ROOT/AU/WindLab.component"
APP="$ROOT/Standalone/WindLab.app"
for bundle in "$VST3" "$AU" "$APP"; do
  [[ -d "$bundle" ]] || { echo "Missing macOS bundle: $bundle" >&2; exit 1; }
  bin="$bundle/Contents/MacOS/WindLab"
  [[ -f "$bin" ]] || { echo "Missing binary: $bin" >&2; exit 1; }
  /usr/bin/lipo -archs "$bin" | /usr/bin/grep -qw arm64 || { echo "arm64 missing: $bin" >&2; exit 1; }
  build_info="$(xcrun vtool -show-build "$bin")"
  printf 'Deployment: %s\n%s\n' "$bin" "$build_info"
  # A minimum greater than 14.4 would prevent loading on Sonoma 14.4.1.
  printf '%s\n' "$build_info" | /usr/bin/grep -Eq 'minos 14\.0([[:space:]]|$)' || {
    echo "Expected macOS 14.0 deployment target in $bin" >&2; exit 1;
  }
  # Ad-hoc signatures support local development but are NOT Apple notarization.
  /usr/bin/codesign --force --deep --sign - "$bundle"
  /usr/bin/codesign --verify --deep --strict --verbose=2 "$bundle"
done

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
PAYLOAD="$WORK/WindLab-macOS-Sonoma-14-arm64"
PKGROOT="$WORK/pkgroot"
SCRIPTS="$WORK/pkgscripts"
mkdir -p "$PAYLOAD/VST3" "$PAYLOAD/AU" "$PAYLOAD/Standalone" \
  "$PKGROOT/Library/Audio/Plug-Ins/VST3" \
  "$PKGROOT/Library/Audio/Plug-Ins/Components" \
  "$PKGROOT/Applications" "$SCRIPTS"

/usr/bin/ditto "$VST3" "$PAYLOAD/VST3/WindLab.vst3"
/usr/bin/ditto "$AU" "$PAYLOAD/AU/WindLab.component"
/usr/bin/ditto "$APP" "$PAYLOAD/Standalone/WindLab.app"
/usr/bin/ditto "$VST3" "$PKGROOT/Library/Audio/Plug-Ins/VST3/WindLab.vst3"
/usr/bin/ditto "$AU" "$PKGROOT/Library/Audio/Plug-Ins/Components/WindLab.component"
/usr/bin/ditto "$APP" "$PKGROOT/Applications/WindLab.app"

/bin/cp "installers/Installa WindLab.command" "$PAYLOAD/Installa WindLab.command"
/bin/chmod 755 "$PAYLOAD/Installa WindLab.command"
/bin/cp "installers/LEGGIMI-macOS.txt" "$PAYLOAD/LEGGIMI.txt"
/bin/cat > "$SCRIPTS/postinstall" <<'POSTINSTALL'
#!/bin/bash
# Only clear quarantine on installed WindLab bundles, never globally.
for target in \
  "/Library/Audio/Plug-Ins/VST3/WindLab.vst3" \
  "/Library/Audio/Plug-Ins/Components/WindLab.component" \
  "/Applications/WindLab.app"
do
  if [[ -d "$target" ]]; then
    /usr/bin/xattr -dr com.apple.quarantine "$target" >/dev/null 2>&1 || true
  fi
done
exit 0
POSTINSTALL
/bin/chmod 755 "$SCRIPTS/postinstall"

PKG="$PAYLOAD/WindLab-$VERSION-macOS-Sonoma-arm64.pkg"
/usr/bin/pkgbuild --root "$PKGROOT" --install-location / \
  --identifier "com.windlab.prototype.installer" --version "$VERSION" \
  --scripts "$SCRIPTS" "$PKG"
/usr/bin/xar -tf "$PKG" | /usr/bin/grep -qx PackageInfo

ZIP="$OUT_DIR/WindLab-macOS-Sonoma-14-arm64.zip"
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$PAYLOAD" "$ZIP"
/usr/bin/unzip -tq "$ZIP"
echo "Package built and verified: $ZIP"
