# WindLab 0.9.17 — Realismo avanzato: chiusura acustica delle frasi

## Novita' 0.9.17 — Codine originali delle registrazioni e release consapevole

- I campioni **Sustain/Swell** vengono controllati al caricamento: se il tratto finale originale contiene un decadimento RMS marcato, WindLab conserva fino a 260 ms della registrazione successiva al loop, anziche' scartarla automaticamente.
- Al **note-off** o allo scadere della durata del fiato, il cursore passa dal sustain in loop al tratto finale registrato con una dissolvenza morbida di circa 23 ms. La fine della sorgente e' sfumata sugli ultimi 12 ms per evitare click.
- Se la registrazione non offre una coda affidabile (o e' uno staccato one-shot), resta il **rilascio precedente**, senza inventare campioni o applicare correzioni rischiose. Le code originali sono conservate solo quando l'analisi le riconosce: lieve costo RAM addizionale limitato ai take idonei.
- Un passaggio **legato** tra note non attiva la coda di fine frase: la voce precedente usa il rilascio rapido sincronizzato con il nuovo ingresso. Nessun cambio alle automazioni, ai parametri o ai salvataggi dei preset.
- **Respirazione di fine frase:** sugli strumenti con durata del fiato limitata, volume e vibrato si riducono insieme con una curva morbida dipendente dalla famiglia strumentale. Il livello ridotto rimane stabile durante il note-off automatico, senza il rimbalzo di volume possibile con il precedente inviluppo. Con Breath Length a 30 s il comportamento preesistente resta invariato.
- Analisi RMS eseguita solo all'apertura della banca; nel thread audio solo due letture di campioni gia' residenti e un crossfade a costo costante. Tutte le elaborazioni sono prive di allocazione/I/O/lock.
- I test verificano segmenti che decadono, campioni senza release, silenzio, casi limite, note-off, auto-release del fiato, reset delle voci, legato, taper delle frasi e assenza di aumento improvviso al rilascio, a 44,1/48/96 kHz.

**Limite musicale:** la coda esiste solo se presente nel take originale e identificabile; non e' una libreria dedicata di release ne' un vero legato intervallare registrato. Serve collaudo all'ascolto per ogni strumento. L'interfaccia e' gia' stata verificata dall'utente e non viene modificata. macOS e Windows rimangono gli unici target. Non si introduce firma o notarizzazione Apple. **GitHub Actions solo dopo il merge su `main`.**

## WindLab 0.9.16 — Espressività specifica dei nove fiati

## Novità 0.9.16 — Vibrato, timbro e attacco differenziati

- Il filtro acustico dei veri campioni ha **un profilo separato per ciascuno dei nove strumenti**: tromba, sax tenore, trombone, flauto, clarinetto, corno francese, sax soprano/contralto/baritono. L'apertura armonica al fiato (CC2), la risposta della Brightness e i limiti spettrali sono calibrati in modo diverso: legni tendenzialmente piu' morbidi, flauto piu' aperto e ottoni piu' brillanti durante il forte.
- Gli attacchi di registrazioni non legate conservano per pochi millisecondi una frazione controllata del **vero transiente WAV**. Le note legate, che entrano nel corpo del campione, non ricevono falsi accenti.
- Il **ritardo e lo sviluppo del vibrato** sono differenziati per famiglia di strumenti, oltre ai profili di frequenza e profondita' gia' presenti. Le variazioni di fiato, velocity, MPE e mod wheel esistenti rimangono controllabili.
- I profili sono costanti C++17 e non creano allocazioni, lock, processi di lettura o analisi supplementari nell'audio callback. Il filtro ricalcola i coefficienti a inizio nota, non ad ogni sample.
- Nessun nuovo ID parametro, nessun cambiamento alla struttura dei preset o ai file campionati. Sono preservati i formati VST3 / AU / Standalone su Windows e macOS.
- Test DSP: caratterizzazione timbrica tra clarinetto e flauto, risposta piano/forte, attacco non legato rispetto a legato, limiti di tutti i profili e output finito del motore dei nove strumenti alle frequenze 44.1/48/96 kHz.

**Limite:** i profili sono euristiche musicali da verificare all'ascolto; non sostituiscono la registrazione di veri intervalli legato o di altri livelli dinamici. Non e' richiesta alcuna firma o notarizzazione Apple. **GitHub Actions solo su `main`**, dopo il merge.

## WindLab 0.9.15 — Attacchi staccati e ripetizioni espressive

## Novità 0.9.15 — Articolazioni acustiche più affidabili

- **Round robin indipendente per nota:** quando la banca contiene più registrazioni realmente intercambiabili della stessa nota campionata, intensità e articolazione, WindLab le alterna con un contatore specifico per strumento, tasto MIDI e Sustain/Staccato. Le note diverse non avanzano più il contatore delle ripetizioni altrui; le voci di unisono possono scegliere take diversi.
- **Staccato più pronto:** all'apertura della banca il plugin cerca un eventuale silenzio tecnico iniziale nei campioni one-shot e lo salta mantenendo circa 1,5 ms prima del transiente naturale. Limite conservativo: non oltre 80 ms; se il segnale è troppo debole o ambiguo, non lo modifica.
- **Suono originale preservato:** niente compressione del transiente, niente nuovi campioni inventati, nessun taglio della coda one-shot e nessuna alterazione dei loop Sustain. Quando esiste un solo take, viene sempre usato quello.
- Nessun nuovo parametro pubblico, modifica dei preset o modifica a firma/notarizzazione. AU, VST3 e applicazioni Standalone rimangono i target per macOS e Windows.
- I nuovi test verificano contatori isolati tra note/articolazioni e strumenti, assenza di take alternativi, attacco iniziale, silenzio, click isolato e frequenze 44,1/48/96 kHz.

Il controllo dell'attacco e la banca dei take alternativi si realizzano nel caricamento o con operazioni a costo costante nel callback audio. La qualità timbrica va verificata all'ascolto per tromba, sax, trombone e legni.

**Build:** GitHub Actions esclusivamente dopo la pubblicazione su `main`; nessun trigger di branch o PR.

## WindLab 0.9.14 — Allineamento di fase tra dinamiche campionate

## Novità 0.9.14 — Transizioni piano/forte più coerenti

- Durante il caricamento della banca acustica, il plugin analizza le coppie di take *piano/forte* della stessa nota e articolazione. Cerca entro un periodo musicale lo scorrimento temporale che migliora la correlazione fra le due forme d'onda, anche quando la frequenza di campionamento è diversa.
- Applica l'allineamento solo quando il confronto è affidabile e mostra un miglioramento sostanziale. In caso contrario conserva il campione originale, senza correzioni speculative.
- La fase del campione secondario viene anticipata mediante il cursore già presente. Il calcolo della correlazione è **esclusivamente in fase di caricamento**: nessun FFT, allocazione, ricerca o ritardo aggiuntivo nel thread audio.
- Preset, parametri pubblici, staccati one-shot, note senza una seconda registrazione e formati AU/VST3/Standalone restano invariati.
- Nuovo test C++ per registrazioni simulate con fase diversa e livello diverso, sample rate 44,1/48/96 kHz, combinazioni 44,1/48 kHz e condizioni non valide.
- **Limite da ascoltare:** è una correzione approssimata di fase iniziale; il vibrato e l'intonazione di esecuzioni indipendenti possono comunque divergere nel tempo. Non è un algoritmo di time-stretch e non sostituisce il legato registrato autentico.

**Build Actions:** sempre solo dopo l'aggiornamento di `main`; niente build delle PR. La firma e notarizzazione Apple rimangono fuori scope.

## WindLab 0.9.13 — Loop acustici più continui

## Novità 0.9.13 — Chiusura dei campioni sostenuti con confronto della forma d'onda

- Per i campioni Sustain/Swell la ricerca del punto finale del loop considera, oltre al passaggio per lo zero, **quanto si somigliano i due segmenti WAV/FLAC sovrapposti** durante la dissolvenza.
- La scelta viene eseguita solo quando si caricano i campioni; il thread audio mantiene invariata la riproduzione in tempo reale, senza nuove allocazioni o scansioni.
- L'algoritmo conserva il precedente punto finale se nessuna alternativa fornisce un miglioramento netto: cosi' si minimizzano regressioni su registrazioni rumorose o poco periodiche.
- Staccati one-shot, campioni originali, MIDI, parametri, preset e formati AU/VST3/Standalone restano invariati.
- Nuovo test DSP con sustain quasi-periodico e deriva d'intonazione a 44,1/48/96 kHz; verifica che la giunzione non peggiori rispetto alla scelta precedente.
- **Build CI solo su main:** niente trigger Actions su PR o branch di sviluppo; un avvio manuale e' consentito solo se il riferimento selezionato e' main.
- **Da validare all'ascolto:** questa misura attenua differenze di forma d'onda sul loop; non corregge automaticamente la fase fra i differenti take dinamici e non sostituisce registrazioni di vero legato.

## WindLab 0.9.12 — Legato dei campioni registrati (prima fase)

## Novità 0.9.12 — Passaggio fra note senza nuovo attacco

- Il passaggio legato fra registrazioni mantiene lo slur e l'ingresso nella parte sostenuta del campione, ma adesso usa una dissolvenza in ingresso per voce sincronizzata con il rilascio veloce della voce uscente.
- Il tempo della transizione dipende dall'intervallo (circa 35–74 ms): gli intervalli piccoli passano più rapidamente, quelli grandi sono un poco più morbidi. L'inviluppo monotono evita cambi di livello istantanei del nuovo campione.
- Chord mode, note indipendenti, staccato e sorgente Synthetic mantengono il loro comportamento precedente. La dissolvenza aggiuntiva si applica solo ai campioni registrati in legato.
- Nessun nuovo parametro pubblico, modifica ai file audio, variazione dei codici AU/VST3 o riconfigurazione hardware. Nuovo test unitario per 44,1/48/96 kHz.
- **Limite dichiarato:** non sono transizioni intervallari autenticamente registrate; l'ascolto su strumenti reali è obbligatorio, soprattutto per gli intervalli ampi e per un eventuale calo di livello nella sovrapposizione.

## WindLab 0.9.11 — Prima evoluzione del realismo acustico

## Novità 0.9.11 — Il fiato cambia anche il timbro dei campioni

- Il motore dei campioni acustici ora applica un filtraggio spettrale per voce, controllato dal fiato **CC2** (anche via controller, microfono o aftertouch instradato) e dalla manopola **Brightness**.
- Un soffio più leggero rende la registrazione più scura; aumentando il fiato il suono recupera naturalmente contenuto armonico, non soltanto volume. I cambiamenti sono smussati per evitare salti bruschi.
- Funziona anche in modalità Hybrid; non modifica la modalità Synthetic, i campioni WAV/FLAC, gli ID dei parametri, le mappature MIDI o i progetti esistenti.
- DSP per voce, nessuna allocazione o I/O nella callback audio. Nuovo test sulle frequenze acute e gravi, cambi di fiato e sample rate 44,1/48/96 kHz.
- **Limite esplicito:** è una modellazione timbrica del campione, non un crossfade tra vere registrazioni forti/piano o un legato registrato. Questi rimangono obiettivi del successivo upgrade acustico.

### Fase 2 — Campioni dinamici reali sotto controllo del fiato

- Per note Sustain o Swell, quando sono disponibili **due registrazioni genuine con identico strumento, nota campionata e articolazione**, WindLab le prepara entrambe, mantenendo la prima selezionata dalla velocity MIDI.
- Dopo i primi 45 ms di attacco, un fiato esplicito (CC2, controller ibrido, microfono o sorgente configurata) muove una dissolvenza smussata di circa 75 ms tra la registrazione morbida e quella forte, senza ricaricare i campioni o cambiare la frequenza della nota. Se il fiato non e' attivo, resta il vecchio livello scelto dalla velocity MIDI.
- Se esiste un solo livello autentico per la nota, oppure la nota usa uno staccato one-shot, non si inventa un secondo livello: la riproduzione originale resta invariata.
- Non sono stati introdotti nuovi parametri pubblici, cambi di salvataggio o dipendenze; test DSP separati controllano pesi, range, limiti e cambi rapidi ai sample rate 44.1/48/96 kHz.
- **Limite da verificare con ascolto:** registrazioni separate possono avere differenze di fase e timbro nei punti intermedi. La dissolvenza riduce il rischio di click, ma non equivale a phase alignment, legato autentico o vero morphing spettrale. Il collaudo su sax, tromba e controller e' necessario prima del merge.

## WindLab 0.9.10 — Strumento a video, espressione, sezioni, MPE e sale registrate

Plugin AU / VST3 / Standalone di strumenti a fiato (9 strumenti, campioni reali + motore espressivo).

## Novità 0.9.5 – 0.9.10

- **Sale registrate** (OpenAIR, CC BY 4.0, scaricate in CI) e **sala personalizzata** da file WAV;
  **MPE** (canali 2–15: bend, pressione = fiato, CC74 = timbro per nota);
  **frasi di 2, 4 o 8 battute** nel Phrase Editor; **sovracuto del sax** nello strumento a video.

- **MOSTRA DITEGGIATURA**: lo strumento a video mostra le chiavi delle note suonate (didattica);
  **unisono** a 2 o 3 esecutori; **velocity di ogni nota dell'accordo** con la rotella del mouse;
  diteggiatura alternativa Sib "1+1".

- **Fiato dal microfono** (SOFFIO DA: MICROFONO), **modalità sezione** (sax, ottoni, legni, big band)
  e **riverbero a convoluzione** (studio, club jazz, sala da concerto, chiesa).
- **Firma Apple**: Developer ID e notarizzazione pronte nella build, si attivano con i secret
  dell'account Apple — vedi [docs/Firma-Apple-Notarizzazione.md](docs/Firma-Apple-Notarizzazione.md).

- **STRUMENTO A VIDEO**: sax soprano, contralto, tenore e baritono, clarinetto, flauto,
  tromba, corno e trombone disegnati e suonabili con mouse e tastiera del computer,
  con diteggiature vere, fiato dalla barra spaziatrice o dal mouse e coulisse con glissando.
- **ESPRESSIONE**: modalità e tempo del legato (con calo di fiato fra le note),
  velocità del vibrato e vibrato dal fiato, growl, flutter tongue, effetti jazz
  (scoop, fall, doit, shake), pan.
- **Preset** di fabbrica e personali (`.windlabpreset`).
- **MIDI Learn universale**: clic destro su qualsiasi manopola, assegnazioni salvate nel progetto.
- **Controller a fiato**: modalità tastiera / fiato / ibrida, soffio da CC o aftertouch,
  calibrazione SOGLIA e CURVA, profili EWI / Aerophone / Artinoise, monitor MIDI.
- **Phrase Editor**: griglie 1/8, 1/16 e terzine, accordi fino a 4 note, pedale di sustain,
  esportazione MIDI, **SEGUI PLAY DAW** (agganciato al play e alle battute della DAW)
  e **TRASCINA IN DAW** (la frase diventa una clip MIDI).

Guida completa: [docs/Guida-Utente-WindLab-IT.md](docs/Guida-Utente-WindLab-IT.md) e pulsante **?** dentro il plugin.

## Versione 0.9.3 — REC MIDI → Phrase Editor + popup in-plugin

**CI a basso consumo:** il progetto accumula circa tre commit per PR in
modalita' Draft e compila tutti i formati solo quando la PR passa a
**Ready for review**. La build automatica finale su `main` e quella manuale
`Run workflow` restano disponibili.
[Procedura per risparmiare minuti GitHub Actions](docs/CI-Minuti-Gratuiti.md).


**REC MIDI / STOP REC / VEDI TAKE** nella schermata principale, e
**REC / STOP REC / IMPORTA** nello **Expression & Phrase Editor**.
Per usare la tastiera PC premi REC MIDI nella schermata principale: il focus
rimane sui tasti; quando la registrazione è pronta premi VEDI TAKE e infine
IMPORTA nell'editor. Con una periferica MIDI esterna puoi registrare anche
dentro il Phrase Editor.
Suona una tastiera PC, il piano grafico, un controller USB MIDI, un
pianoforte pesato o uno strumento MIDI a fiato. WindLab acquisisce fino a
**due battute / 16 step di crome** in un buffer separato (nessuna cancellazione
automatica della vecchia frase), poi con **IMPORTA** copia le note e le
curve registrate nel piano roll modificabile.

- Note-on/off, velocity, durate (gate 1..8), legato in presenza di sovrapposizione.
- Curve di fiato **CC2**, espressione **CC11**, vibrato **CC1**, pitch bend,
  anche con i CC fisici rimappati tramite MIDI Learn.
- Input canali **1–15**. Il playback interno del Phrase Editor sul
  canale **16** non viene mai registrato di nuovo.
- Sequencer limitato a **una nota per step**: un accordo sullo stesso step
  scarta le note in più. Registrazione quantizzata, non una traccia MIDI
  arbitraria e **non audio WAV**.
- Solo il risultato **importato** viene salvato nel progetto host.
  La cattura temporanea non viene ripristinata se si chiude il plugin prima
  di importarla.
- Parser di registrazione a dimensione fissa: nessuna allocazione,
  lock o I/O nell'audio callback. Test C++ dedicati per registrazione e import.
- **Phrase Editor e ?** mostrati come popup host-safe *interni* alla
  finestra WindLab, non come finestre native separate (confermato
  esplicitamente nella guida). Questo preserva la compatibilità AU,
  VST3 e Standalone.

La compatibilità delle build non prova da sola che tutti i comandi
siano cliccabili nei diversi host: sono necessari test manuali
di registrazione/guida/ritorno alla schermata principale in Windows
Standalone, Mac Standalone e Logic Pro AU.

**[Guida utente in italiano](docs/Guida-Utente-WindLab-IT.md)**

## Cronologia versioni

Le versioni precedenti alla 0.9.3 sono in [CHANGELOG.md](CHANGELOG.md).

## What's in the ZIP

- `Source/` — JUCE plugin wrapper, MIDI synth, JUCE-independent DSP and GUI.
- `CMakeLists.txt` — JUCE 9.0.5 as a pinned CMake dependency (fetched when configuring).
- `build-mac.sh` — build AU, VST3, Standalone via Xcode generator.
- `tests/run-smoke.sh` — JUCE-free C++ functional tests.
- `tests/render-demo.sh` — render the six synthesized timbres to a WAV file.
- `Preview/WindLab_0.2_demo.wav` — a small mono preview: trumpet, tenor sax, trombone, flute, clarinet, horn, **in that order**. Do not interpret this as a realistic audio library.
- `TESTING.md` — validation results and untested paths.

## Build on an Apple Silicon Mac

Prerequisites: macOS, Xcode and Xcode Command Line Tools, Git, CMake >= 3.22, internet to fetch JUCE. Ensure the licensing terms of JUCE are compatible with intended distribution.

```bash
xcode-select --install
# If CMake isn't installed already: brew install cmake
cd /path/to/WindLab_Mac_v0.2
chmod +x build-mac.sh
./build-mac.sh
```

For a locally checked-out JUCE source tree, configure without fetching JUCE:

```bash
cmake -S . -B build -G Xcode -DJUCE_SOURCE_DIR=/absolute/path/to/JUCE -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release --parallel 4
ctest --test-dir build --build-config Release --output-on-failure
```

Find generated bundles:

```bash
find build -type d \( -name 'WindLab.vst3' -o -name 'WindLab.component' -o -name 'WindLab.app' \)
```

Install local AU/VST3 builds:

```bash
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/Components"
cp -R build/WindLab_artefacts/Release/VST3/WindLab.vst3 "$HOME/Library/Audio/Plug-Ins/VST3/"
cp -R build/WindLab_artefacts/Release/AU/WindLab.component "$HOME/Library/Audio/Plug-Ins/Components/"
```

If output paths differ, locate the bundles using `find` as above. Restart/rescan the relevant DAW if needed. For wider distribution, sign and notarize with Apple Developer ID and arrange manufacturer/plugin identity before a public release. Pro Tools additionally needs AAX, not included.

## How to play

1. Insert WindLab as an instrument plugin in your DAW, or open the Standalone build.
2. Play MIDI notes or click the 61-note on-screen keyboard (`C2` through `C7`).
3. Choose one of six instruments and an articulation.
4. Shape `Attack`, `Release`, `Breath Length`, `Vibrato`, `Natural`, `Brightness`, `Air`, `Bass`, `Mid`, `Treble`, `Room`, `Reverb`, `Reverb Size`, `Damping`, and `Output` live, or automate these from the DAW.
5. Send MIDI CC1 = vibrato, CC2 = breath gain, CC11 = expression, aftertouch = brightness/dynamics, pitch bend ±2 semitones. If no CC2 has been sent, breath gain defaults to 100%; if CC2 is sent with value 0 it deliberately mutes the current channel until raised again.
6. Change articulation or instrument, then trigger a new note; these two selections are sampled at note-on rather than morphing sustained voices mid-note.
7. Test a 12-note chord, MIDI note-off, sustain pedal, host automation, save/reopen session, sample rates and CPU load inside each DAW.

## Known limitations

- **Not authentic woodwind/brass samples.** Realism requires recorded multi-dynamic and multi-articulation libraries (or advanced physical modelling), real legato transition mapping, round robins, tunings, sample layers, and controllable noise/transients.
- `Room` is a two-tap early-reflection sketch. The new `Reverb` is an algorithmic stereo reverb, not an impulse response of a sampled space.
- Staccato and Swell are envelope behaviours rather than complete musically captured articulations.
- Voice count is fixed at 12, no advanced legato/divisi or ensemble arranging. No native preset browser yet (the host saves automatable parameters).
- No MPE per-note pitch, microtuning, sample drag/drop, or mic positions. Basic per-note aftertouch is supported by JUCE voice callback.
- Changing attack/release or articulation/instrument affects new notes; other controls update held voices.
- CI previously compiled WindLab 0.2 AU/VST3 on GitHub's macOS and Windows runners. The WindLab 0.3 plugin must pass the new CI and still requires host validation, performance testing and manual listening on an actual Mac/Windows DAW.
- IDs are provisional; finalize bundle ID and manufacturer/product codes before shipping to prevent session compatibility breakage.

## Roadmap (recommended)

**0.4 — Authentic trumpet sample engine.** Record or license 3–4 velocity layers, notes at sensible intervals over trumpet's playable range, note starts/sustain/release, staccato, marcato. Implement resampling, SFZ or custom note/velocity mapping, round robins and audio asset manifest with legal metadata.

**0.5 — Performance realism.** Connect legato transitions between intervals, breath control crossfades, vibrato style/lag, ensemble unison detuning, instrument-specific attack and release noise.

**0.6 — Brass section builder.** Independent trumpet 1/2, tenor sax, trombone parts, divisi/voice-leading, panorama, timing offsets and preset management.

**0.7 — Production.** Profiling and anti-denormal monitoring, unit/host QA on Apple Silicon and Intel, accurate plugin IDs, binary signing/notarization, installer, license and redistribution review.

## Licensing

JUCE is distributed under its own licensing terms, potentially including AGPLv3/commercial options depending on the edition and mode of use; consult https://juce.com/get-juce/ and https://juce.com/legal/. Redistribution of third-party audio samples requires separate permission, even when the sound library can otherwise be used to produce music.

## API links

- https://docs.juce.com/master/classjuce_1_1Synthesiser.html
- https://docs.juce.com/master/classjuce_1_1SynthesiserVoice.html
- https://github.com/juce-framework/JUCE/blob/master/examples/CMake/AudioPlugin/CMakeLists.txt
