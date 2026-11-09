#!/bin/bash
# WindLab developer installer for macOS Sonoma 14+ (Apple Silicon).
# Installs only WindLab bundles inside the current user's home directory.
set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"

finish() {
  status=$?
  if [[ $status -eq 0 ]]; then
    printf '\nInstallazione WindLab completata.\n'
  else
    printf '\nInstallazione interrotta (codice %s). Nessun controllo di sicurezza globale e stato disattivato.\n' "$status" >&2
  fi
  if [[ -t 0 ]]; then
    printf '\nPremi INVIO per chiudere questa finestra... '
    read -r _ || true
  fi
}
trap finish EXIT

fail() { echo "ERRORE: $*" >&2; exit 1; }
[[ "$(uname -s)" == Darwin ]] || fail "Questo installer richiede macOS."
[[ "$(uname -m)" == arm64 ]] || fail "Questa build e solo Apple Silicon (M1/M2/M3/M4)."

OS_VERSION="$(sw_vers -productVersion)"
OS_MAJOR="$(printf '%s' "$OS_VERSION" | cut -d. -f1)"
[[ "$OS_MAJOR" =~ ^[0-9]+$ ]] || fail "Versione macOS sconosciuta: $OS_VERSION"
(( OS_MAJOR >= 14 )) || fail "Serve macOS 14 Sonoma o successivo (rilevato $OS_VERSION)."

for path in "$ROOT/VST3/WindLab.vst3" "$ROOT/AU/WindLab.component" "$ROOT/Standalone/WindLab.app"; do
  [[ -d "$path" ]] || fail "File mancante nello ZIP: $path. Estrai lo ZIP completo prima di installare."
done

printf 'WindLab - installazione per %s (arm64)\n' "$OS_VERSION"
echo "Installa AU e VST3 solo per l'utente corrente, senza password amministratore."
echo "Chiudi Logic Pro e Studio One prima di continuare."
if [[ -t 0 ]]; then
  printf 'Premi INVIO per procedere (Ctrl-C per annullare)... '
  read -r _ || true
fi

install_bundle() {
  src="$1"
  target_dir="$2"
  name="$(basename "$src")"
  mkdir -p "$target_dir"
  # Stage a full bundle before replacing it, to avoid mixing old/new files.
  staging="$(mktemp -d "$target_dir/.windlab-install.XXXXXXXX")"
  if ! /usr/bin/ditto "$src" "$staging/$name"; then
    /bin/rm -rf "$staging"
    fail "Impossibile copiare $name."
  fi
  target="$target_dir/$name"
  /bin/rm -rf "$target"
  /bin/mv "$staging/$name" "$target"
  /bin/rmdir "$staging"
  # Clear Gatekeeper quarantine ONLY for our own locally installed WindLab.
  /usr/bin/xattr -dr com.apple.quarantine "$target" >/dev/null 2>&1 || true
  if ! /usr/bin/codesign --verify --deep --strict "$target" >/dev/null 2>&1; then
    echo "AVVISO: verifica firma locale non superata per $name. Non disabilitiamo Gatekeeper."
  fi
  echo "Installato: $target"
}

install_bundle "$ROOT/VST3/WindLab.vst3" "$HOME/Library/Audio/Plug-Ins/VST3"
install_bundle "$ROOT/AU/WindLab.component" "$HOME/Library/Audio/Plug-Ins/Components"
install_bundle "$ROOT/Standalone/WindLab.app" "$HOME/Applications"

# Warn about older system-wide copies which could take precedence in a DAW.
for old in /Library/Audio/Plug-Ins/VST3/WindLab.vst3 /Library/Audio/Plug-Ins/Components/WindLab.component /Applications/WindLab.app; do
  if [[ -e "$old" ]]; then
    echo "ATTENZIONE: esiste anche una copia di sistema: $old"
    echo "Potrebbe causare duplicati o far caricare una versione diversa. Questa procedura non la elimina."
  fi
done

echo "Riapri la DAW e, se necessario, esegui una nuova scansione dei plugin."
echo "Logic Pro: Gestione Plug-in > Ripristina e riesegui scansione di WindLab (AU)."
