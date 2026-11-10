# Firma Apple e notarizzazione di WindLab (macOS)

Senza firma Apple, macOS blocca WindLab al primo avvio e bisogna togliere il blocco a mano
(Terminale `xattr -dr com.apple.quarantine …` oppure **Impostazioni di Sistema → Privacy e
sicurezza → Apri comunque**). Con la firma **Developer ID** e la **notarizzazione** il pacchetto
si installa con un doppio clic, senza avvisi.

La build è già pronta: basta inserire i dati dell'account Apple come **secret** del repository.
Finché i secret mancano, la build continua a funzionare come oggi (firma locale "ad-hoc").

## 1. Cosa serve

- Iscrizione all'**Apple Developer Program** (99 € l'anno) — <https://developer.apple.com/programs/>.
- Due certificati creati su <https://developer.apple.com/account/resources/certificates>:
  - **Developer ID Application** (firma app e plugin);
  - **Developer ID Installer** (firma il pacchetto `.pkg`).
- Una **password specifica per app** del tuo Apple ID — <https://account.apple.com> → Accesso e
  sicurezza → Password specifiche per le app.
- Il tuo **Team ID** (10 caratteri), visibile in <https://developer.apple.com/account> → Membership.

## 2. Esportare i certificati (sul Mac)

1. Apri **Accesso Portachiavi**, categoria **I miei certificati**.
2. Seleziona entrambi i certificati *Developer ID Application* e *Developer ID Installer*
   (con le loro chiavi private), tasto destro → **Esporta 2 elementi…** → formato `.p12`,
   imposta una password.
3. Nel Terminale converti il file in testo:
   `base64 -i Certificati.p12 | pbcopy` (il testo finisce negli appunti).

## 3. Inserire i secret su GitHub

Repository → **Settings → Secrets and variables → Actions → New repository secret**:

| Nome | Valore |
|---|---|
| `MACOS_CERT_P12` | il testo base64 copiato al punto 2 |
| `MACOS_CERT_PASSWORD` | la password del file `.p12` |
| `MACOS_SIGN_IDENTITY` | `Developer ID Application: Nome Cognome (TEAMID)` |
| `MACOS_INSTALLER_IDENTITY` | `Developer ID Installer: Nome Cognome (TEAMID)` |
| `APPLE_ID` | l'email del tuo Apple ID |
| `APPLE_TEAM_ID` | il Team ID |
| `APPLE_APP_PASSWORD` | la password specifica per app |

I nomi esatti delle identità si leggono nel Terminale con
`security find-identity -v` (sul Mac dove sono installati i certificati).

## 4. Cosa fa la build

1. Importa i certificati in un portachiavi temporaneo del runner.
2. Firma app, AU e VST3 con **hardened runtime** e il permesso del microfono
   (`installers/WindLab.entitlements`, serve al fiato dal microfono).
3. Firma il `.pkg` con il certificato Installer.
4. Invia il `.pkg` ad Apple per la **notarizzazione** (di solito 2–10 minuti) e "pinza"
   (staple) il risultato al pacchetto e ai plugin.
5. Verifica con `spctl` che macOS accetti il pacchetto.

Se uno dei secret manca, i passi 1, 3, 4 e 5 vengono saltati e la firma resta ad-hoc.
