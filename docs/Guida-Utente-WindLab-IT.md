# WindLab — Guida utente (italiano)

> Aggiornata per WindLab **0.9.1**. La stessa guida è consultabile **senza Internet** aprendo il pulsante **?** nella schermata principale del plugin o della Standalone. Nel Phrase Editor, il **?** apre direttamente la sezione dedicata alla creazione e modifica delle frasi.

## 1. Avvio e formati

WindLab è uno strumento virtuale (software instrument). Le build generate da GitHub Actions contengono:

- **macOS Apple Silicon**: `WindLab.component` (Audio Unit per Logic Pro), `WindLab.vst3` (VST3 per Studio One e DAW compatibili), `WindLab.app` (Standalone).
- **Windows x64**: `WindLab.vst3` e `WindLab.exe` (Standalone).
- I WAV CC0 sono incorporati nella build: non occorre installare una libreria di campioni separata.

### Installazione macOS
Estrarre il pacchetto macOS negli **Artifacts** del workflow GitHub:

```text
~/Library/Audio/Plug-Ins/Components/WindLab.component
~/Library/Audio/Plug-Ins/VST3/WindLab.vst3
```

Per Logic Pro serve il file **AU** (il componente `.component`), **non il VST3**. Riavviare Logic Pro dopo la copia. Se non appare tra gli strumenti, verificare in **Logic Pro → Impostazioni → Gestione plug-in**. Trattandosi di un prototipo non firmato/notarizzato, eventuali avvisi di sistema o problemi di validazione richiedono verifica manuale.

### Installazione Windows
Copiare `WindLab.vst3` nella cartella VST3 standard:

```text
C:\Program Files\Common Files\VST3\
```

Oppure estrarre la Standalone e aprire `WindLab.exe`.

## 2. Primo suono: percorso rapido

1. Avviare WindLab in una traccia **Strumento software** o nella Standalone.
2. Scegliere **INSTRUMENT**: Trumpet, Tenor Sax, Trombone, Flute, Clarinet, French Horn.
3. Selezionare **SOUND ENGINE → ACOUSTIC - REAL WAV** per le registrazioni originali.
4. Suonare sulla tastiera grafica in basso o su una tastiera MIDI esterna.
5. Regolare **ATTACK**, **BRIGHTNESS**, **BREATH LENGTH**, **ROOM** e **REVERB**.

**Sound engine**: Acoustic usa i sample reali; Hybrid combina registrazioni e sintesi; Synth usa il modello sintetico.

**Articolazioni**: Sustain per note tenute, Staccato per attacchi brevi con registrazioni dedicate dove disponibili, Swell per ingressi graduali.

## 3. Tastiera PC e tastiere MIDI esterne

In **INPUT METHOD** scegliere **PC / NOTEBOOK**; usare la mappatura cromatica:

| Tasto | A | W | S | E | D | F | T | G | Y | H | U | J | K |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Nota | C | C# | D | D# | E | F | F# | G | G# | A | A# | B | C |

Usare **− / +** per trasporre l'ottava dei tasti PC. Il cursore **PC / MOUSE VELOCITY** modifica l'intensità di note suonate con tastiera PC o mouse, non la velocity ricevuta dalle tastiere MIDI.

Se la tastiera PC non risponde, cliccare sulla tastiera virtuale per riportarvi il focus. Alcune DAW intercettano la tastiera del computer: preferire una periferica MIDI.

Per un **pianoforte a tasti pesati**, collegare la porta USB-MIDI o MIDI DIN con apposita interfaccia. In Standalone selezionare il dispositivo nelle impostazioni Audio/MIDI; in Logic o Studio One abilitare il controller nelle preferenze e attivare il monitoraggio della traccia strumento.

### Profili MIDI DYNAMICS

| Profilo | Effetto |
|---|---|
| Natural | Mantiene la risposta originale della velocity |
| Light Touch | Agevola il raggiungimento dei campioni più intensi |
| Weighted Piano | Aumenta il controllo sulle pressioni leggere |
| Breath Controller | Imposta una zona iniziale udibile; il fiato modula la nota via CC2 |

**Controller**: CC1 vibrato, CC2 fiato, CC11 espressione/volume, pitch bend intonazione, aftertouch pressione. Il controller deve essere configurato per emettere questi messaggi.

## 4. Manopole del suono

| Gruppo | Comandi |
|---|---|
| Performance | Attack, Release, Breath Length (30 = continuo), Vibrato, Natural |
| Timbro / EQ | Brightness, Air, Bass dB, Mid dB, Treble dB |
| Ambiente | Room, Reverb, Reverb Size, Damping, Output |

Per un risultato più realistico, iniziare da timbro pulito e poco riverbero; aggiungere variazioni graduali di fiato ed espressione invece di tenere una dinamica perfettamente piatta.

## 5. Phrase Editor e legato

Aprire **PHRASE EDITOR**. È un editor per **16 step** con piano roll e riproduzione interna.

- Clic sinistro nel piano roll: inserire una nota; trascinare in verticale per cambiare altezza e in orizzontale per estendere la durata.
- Clic destro: cancellare la nota.
- **LEGATO**: applica una breve sovrapposizione di note MIDI, non una transizione registrata fra due note.
- **DEMO**: carica una frase esempio.
- **CLEAR**: cancella la sequenza.
- **PLAY / STOP**: riproduce o ferma la frase.
- **BPM**: imposta il tempo del loop a due battute.

Le **corsie espressive** sono: Breath (CC2), Velocity, Volume/Expression (CC11), Pitch Bend, Vibrato (CC1). Selezionare la corsia e trascinare i punti in basso. Breath, volume, pitch e vibrato vengono interpolati durante la nota; la velocity agisce sull'attacco.

Le frasi sono salvate **nel progetto della DAW** tramite lo stato del plugin. Quando il progetto viene riaperto, la sequenza parte **ferma**. L'editor genera MIDI interno sul **canale 16**.

## 6. Spartito e BPM della DAW

**SCORE VIEW** mostra una versione semplificata dello spartito con altezza delle note, diesis, indicazioni grafiche della durata e legature. È **soltanto un'anteprima**: per spostare le note tornare a **PIANO ROLL**.

**HOST BPM** attiva l'uso del tempo letto da Logic Pro/Studio One, se disponibile. Non segue ancora automaticamente il play/stop o la posizione temporale della testina: premere manualmente PLAY nel Phrase Editor. Disattivare HOST BPM per tornare al tempo interno.

## 7. Risoluzione dei problemi

| Problema | Verifica |
|---|---|
| Nessun suono | Dispositivo audio, monitoraggio traccia, Output, CC2/CC11 non a zero |
| Il plugin non appare in Logic | Usare AU `.component`; controllare Gestione plug-in; riavviare |
| Il PC non suona le note | Selezionare PC/NOTEBOOK e cliccare sul piano per recuperare il focus |
| Il controller MIDI non risponde | Abilitare il dispositivo nel sistema/DAW e assegnare l'ingresso alla traccia |
| Lo spartito non si modifica | Passare a PIANO ROLL |
| La frase è fuori tempo | HOST BPM segue il tempo, non la timeline; avviare PLAY in modo appropriato |
| Le note sostenute risultano artificiali | Provare un registro più naturale, diminuire la trasposizione e modulare CC2/CC11 |

## 8. Limiti attuali (non confondere con funzioni già disponibili)

WindLab è ancora un prototipo in sviluppo. **Non** sono disponibili: legato con registrazioni autentiche di transizione, MIDI Learn universale, sincronizzazione automatica della frase con la posizione della DAW, MPE per-nota completo, spartito completamente modificabile/MusicXML, esportazione MIDI dalla frase e pacchetti universalmente firmati/notarizzati.

La compilazione su GitHub di AU/VST3/Standalone e i test automatici non sostituiscono un **test d'ascolto e caricamento manuale** in Logic Pro e Studio One.

## 9. Dove trovare aiuto dentro WindLab

- Pulsante **?** in alto nella schermata principale: apre la guida completa.
- Pulsante **MIDI ?**: apre direttamente il capitolo MIDI e controller.
- Pulsante **?** nel Phrase Editor: apre direttamente il capitolo sulle frasi e sulle curve.
- Il selettore superiore della guida cambia capitolo; i pulsanti **PRECEDENTE / SUCCESSIVO** scorrono il manuale; **CHIUDI** ritorna esattamente alla schermata precedente.

La guida è **offline**, integra nelle tre edizioni del programma e non richiede una connessione o un browser esterno.
