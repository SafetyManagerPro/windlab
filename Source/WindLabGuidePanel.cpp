#include "WindLabGuidePanel.h"
#include <utility>

juce::StringArray WindLabGuidePanel::sectionTitles()
{
    return {
        "01  Primo avvio",
        "02  Scegliere uno strumento",
        "03  Suoni e articolazioni",
        "04  Tastiera PC / notebook",
        "05  Tastiere MIDI e controller",
        "06  Logic Pro (Mac)",
        "07  Studio One e altre DAW",
        "08  Controlli del suono",
        "09  Phrase Editor e curve",
        "10  Vista spartito e BPM",
        "11  Risolvere i problemi",
        "12  Limiti e prossimi sviluppi",
        "13  Strumento a video",
        "14  Espressione, preset e MIDI Learn"
    };
}

juce::String WindLabGuidePanel::articleFor(int topic)
{
    switch (topic)
    {
        case start: return R"(PRIMO AVVIO - IN 5 PASSAGGI

1. Apri WindLab come STRUMENTO SOFTWARE nella DAW, oppure esegui WindLab Standalone.
2. Seleziona INSTRUMENT: Trumpet, Tenor Sax, Trombone, Flute, Clarinet o French Horn.
3. Imposta SOUND ENGINE su ACOUSTIC - REAL WAV per ascoltare le registrazioni originali.
4. Prova le note sulla tastiera grafica in basso, con una tastiera MIDI, oppure con i tasti PC.
5. Regola ATTACK, BREATH LENGTH, BRIGHTNESS, ROOM e REVERB in base al brano.

In qualsiasi momento premi il pulsante ? per tornare a questa guida. Dal Phrase Editor puoi aprire direttamente il capitolo relativo alle curve.

La guida e i suoni sono incorporati nel programma e non richiedono una connessione Internet. Il salvataggio dei controlli avviene nel progetto della DAW. Anche le frasi create nel Phrase Editor vengono salvate nello stato del plugin, ma la riproduzione della frase non parte automaticamente quando riapri un progetto.)";

        case instruments: return R"(STRUMENTI DISPONIBILI

TRUMPET - tromba; adatta a linee brillanti, staccati e aperture melodiche.
TENOR SAX - sassofono tenore; usa campioni FreePats di qualità elevata.
TROMBONE - registri medio-gravi, frasi ampie e accenti.
FLUTE - flauto; sonorità leggere e note sostenute.
CLARINET - clarinetto; timbro morbido e dinamiche delicate.
FRENCH HORN - corno francese; pad orchestrali, melodie e crescendi.

Per ogni strumento parti dal motore ACOUSTIC e dal registro naturale. Le note molto distanti dal campione originale vengono trasposte e possono risultare meno credibili.

Se il suono appare troppo artificiale prova un attacco leggermente più lento, riduci gli effetti, varia l'intensità MIDI e disegna un fiato che non rimanga perfettamente costante.)";

        case soundEngine: return R"(MOTORE SONORO E ARTICOLAZIONI

SOUND ENGINE
- ACOUSTIC - REAL WAV: registrazioni autentiche CC0 da VSCO 2 CE / FreePats.
- HYBRID: miscela campioni acustici e modello sintetico.
- SYNTH: solo sintesi; utile per timbri creativi e leggeri.

ARTICULATION
- SUSTAIN: nota tenuta. Gestisci la fine tramite note-off e RELEASE.
- STACCATO: usa registrazioni staccate quando disponibili; sono one-shot.
- SWELL: attacco progressivo.

La VELOCITY MIDI influenza l'intensità iniziale e la scelta dei campioni. Il fiato CC2, l'espressione CC11 e le curve modificano la nota mentre viene sostenuta.

Il legato del Phrase Editor produce una piccola sovrapposizione MIDI: non si tratta ancora di registrazioni specifiche della transizione tra note.)";

        case keyboard: return R"(TASTIERA DEL COMPUTER / NOTEBOOK

Nella schermata principale, sotto INPUT METHOD, scegli PC / NOTEBOOK.

MAPPATURA CROMATICA:
A=C    W=C#    S=D    E=D#    D=E    F=F
T=F#  G=G     Y=G#   H=A     U=A#   J=B    K=C successivo

Premi i tasti per suonare. I pulsanti - e + spostano l'ottava della tastiera PC: non trasportano le note suonate via MIDI esterno.
PC / MOUSE VELOCITY regola l'intensità dei tasti del PC e dei clic sul pianoforte virtuale.

Se i tasti scrivono nel campo di una DAW oppure non suonano, fai clic sulla tastiera virtuale o riseleziona PC / NOTEBOOK per recuperare il focus. Alcuni host intercettano i tasti: in quel caso usa la Standalone oppure una tastiera MIDI.

La tastiera QWERTY non misura la forza fisica della pressione. Per avere una velocity reale occorre un controller MIDI.)";

        case midi: return R"(TASTIERE MIDI, PIANOFORTE PESATO E FIATO

Un pianoforte con uscita USB-MIDI puo' inviare note e velocity direttamente al computer: non serve un'interfaccia audio se la tastiera invia MIDI via USB.
Per strumenti con porta MIDI DIN usa un'interfaccia MIDI compatibile.

STANDALONE: apri Options > Audio/MIDI Settings, scegli dispositivo audio e abilita l'ingresso MIDI.
DAW: abilita il dispositivo nelle preferenze della DAW, seleziona WindLab su una traccia strumento e attiva il monitoraggio.

MIDI DYNAMICS:
- NATURAL: usa la velocity originale.
- LIGHT TOUCH: adatto a tastiere leggere; facilità nell'ottenere intensità superiori.
- WEIGHTED PIANO: più risoluzione sulle pressioni leggere.
- BREATH CONTROLLER: prepara un campione udibile; la pressione continua del fiato viene controllata da CC2.

MESSAGGI UTILI:
CC1 = vibrato / modulation
CC2 = fiato / breath
CC11 = espressione / volume
Pitch Bend = intonazione continua
Aftertouch = pressione, quando inviata dal controller

MIDI LEARN (v0.9.2):
1. Premi MIDI LEARN in alto nella schermata principale.
2. Premi IMPARA accanto a VIBRATO, FIATO o VOLUME.
3. Muovi un pedale, un knob o uno slider hardware: il primo messaggio CC valido viene riconosciuto.
4. Verifica il numero CC e premi CHIUDI per tornare allo strumento.

ANNULLA smette di ascoltare senza modificare la mappatura.
RESET CC ripristina Vibrato CC1, Fiato CC2, Volume CC11.
Le assegnazioni vengono salvate nel progetto Logic/Studio One e ricordate alla riapertura.

Se due espressioni utilizzano lo stesso numero CC, l'assegnazione precedente viene scambiata automaticamente: una sorgente non controlla due funzioni in conflitto. CC64 puo' essere appreso come pedale di espressione, ma in tal caso non svolge contemporaneamente la funzione sustain. I CC interni del Phrase Editor (canale MIDI 16) sono esclusi dal MIDI Learn.

CONTROLLER A FIATO / EWI (MIDI PERFORMANCE):
- TASTIERA / MIDI: comportamento normale.
- STRUMENTO A FIATO: note e soffio dallo stesso controller.
- IBRIDO: note dalla tastiera sul canale NOTE CH, soffio da un controller separato sul canale BREATH CH (1-15).
- SOFFIO DA: CC (MIDI LEARN) oppure AFTERTOUCH (Channel Pressure, per EWI/Aerophone impostati cosi').
- SOGLIA = pressione minima; CURVA = sensibilita' (1 = lineare).
- In MIDI LEARN: profili STANDARD CC2 / EWI / AEROPHONE e ARTINOISE RE.CORDER CC11, piu' il monitor ULTIMA NOTA / ULTIMO CC / ULTIMO AFTERTOUCH per controllare canale e CC del dispositivo.

Il MIDI Learn attuale riguarda tre espressioni: vibrato, fiato e volume. Non e' ancora un MIDI Learn universale di tutte le manopole. L'MPE per singola nota non e' ancora disponibile.)";

        case logic: return R"(UTILIZZO IN LOGIC PRO (APPLE SILICON)

1. Scarica l'artifact macOS AU/VST3/Standalone di WindLab dal workflow GitHub pubblicato.
2. Copia WindLab.component nella cartella:
   ~/Library/Audio/Plug-Ins/Components/
3. Riavvia Logic Pro.
4. Crea una traccia STRUMENTO SOFTWARE, quindi apri WindLab dal menu AU Instruments.
5. Seleziona la traccia e attiva la ricezione MIDI. Suona con controller MIDI o piano virtuale.

Se WindLab non compare, apri Logic Pro > Impostazioni > Gestione plug-in e controlla la convalida Audio Unit.

Il formato AU serve per Logic: non usare il file VST3 all'interno di Logic.
WindLab 0.8+ puo' seguire il BPM letto dalla DAW con HOST BPM, ma non la posizione della timeline. Devi premere PLAY nel Phrase Editor per avviare la frase.

La compilazione AU su macOS viene verificata da GitHub Actions. Il caricamento reale in Logic e la firma/notarizzazione richiedono ancora un collaudo manuale sul Mac.)";

        case studioOne: return R"(UTILIZZO IN STUDIO ONE E ALTRE DAW

1. Installa WindLab.vst3 nella normale cartella VST3 del sistema.
   macOS: ~/Library/Audio/Plug-Ins/VST3/
   Windows: C:\\Program Files\\Common Files\\VST3\\
2. Fai una scansione dei plugin nella DAW.
3. Crea una traccia strumento, carica WindLab e assegna l'ingresso MIDI.
4. Attiva il monitoraggio della traccia e prova le note.
5. Usa l'automazione dei parametri per variare dinamica, EQ e riverbero.

I controlli dei campioni si integrano con il MIDI della DAW.
Il Phrase Editor e' un sequencer INTERNO del plugin: produce suono, ma non genera automaticamente una regione MIDI modificabile in Studio One.

Il pulsante HOST BPM segue il BPM, NON il transport della timeline. Le note dell'editor possono partire in un punto diverso dalla battuta: premi PLAY manualmente.

Se non senti audio, verifica il monitoraggio, il canale MIDI, l'uscita audio e la scelta dello strumento.)";

        case controls: return R"(CONTROLLO DEI FIATI - COSA FA OGNI MANOPOLA

PERFORMANCE / BREATH:
ATTACK = velocita' con cui nasce la nota.
RELEASE = durata della coda dopo il rilascio del tasto.
BREATH LENGTH = limite del fiato; 30 = nota continua finche' tieni premuto.
VIBRATO = profondita' di base del vibrato.
NATURAL = piccole variazioni esecutive / humanize.

TIMBRE / EQ:
BRIGHTNESS = brillantezza del suono.
AIR = rumore del fiato.
BASS dB / MID dB / TREBLE dB = equalizzazione a tre bande.

SPACE / OUTPUT:
ROOM = prime riflessioni acustiche.
REVERB = percentuale dell'ambiente.
REVERB SIZE = dimensione dell'ambiente.
DAMPING = attenuazione delle alte frequenze nella coda.
OUTPUT = livello generale.

CONSIGLIO PRATICO:
Per un fiato credibile usa un po' di air, attacchi non tutti identici, vibrato moderato ed espressione che respiri. Aggiungi riverbero dopo avere stabilizzato il suono diretto.)";

        case phraseEditor: return R"(PHRASE EDITOR - NOTE, LEGATO E AUTOMAZIONE

Clicca PHRASE EDITOR nella schermata principale.

GRIGLIA (menu GRID):
- 1/8 = crome, 16 step. 1/16 = semicrome, 32 step.
- 1/8 TRIPLET = terzine di crome, 24 step. 1/16 TRIPLET = terzine di semicrome, 48 step.
- La frase dura sempre due battute in 4/4: cambia solo la risoluzione. La griglia viene salvata con il progetto.

REGISTRAZIONE:
- REC MIDI (SCHERMATA PRINCIPALE): consigliato quando suoni con la tastiera PC. Premi REC MIDI, suona le note, poi STOP REC oppure lascia terminare automaticamente due battute. Il pulsante diventa VEDI TAKE: apre la registrazione nel Phrase Editor senza togliere il focus ai tasti durante la ripresa.
- REC (NEL PHRASE EDITOR): ideale per dispositivi MIDI esterni. La cattura dura due battute sulla griglia scelta; STOP REC termina prima.
- IMPORTA: soltanto quando la cattura e' pronta, sostituisce le note attualmente nel Phrase Editor con la registrazione e porta con se' la griglia. Non cancella la frase prima della tua conferma.
- Vengono registrate note, velocity, durata, CC2 fiato, CC11 volume, CC1 vibrato e pitch bend. Il risultato e' MIDI modificabile, NON una registrazione audio/WAV.
- ACCORDI: fino a 4 note per step. Una quinta nota suonata sullo stesso step viene ignorata.
- PEDALE DI SUSTAIN (CC64): mentre il pedale e' premuto le note continuano anche se rilasci il tasto; finiscono quando alzi il pedale. Lunghezza massima di una nota: 8 step.

PIANO ROLL CON IL MOUSE:
- Clic su una riga vuota: aggiunge una nota allo step, anche sopra un accordo gia' presente (massimo 4 note).
- Clic e trascinamento: in verticale cambia l'altezza, in orizzontale la durata (da 1 a 8 step).
- Clic su una nota esistente senza trascinare: la toglie. Clic e trascinamento sulla nota: la sposta o la allunga.
- Clic destro su una nota: la cancella.
- Due note dello stesso step non possono avere la stessa altezza: se trascini sopra un'altra nota dell'accordo, l'altezza resta quella precedente.

ALTRI COMANDI:
- PLAY / STOP: avvia o interrompe il loop interno.
- DEMO: carica una frase dimostrativa.
- CLEAR: cancella tutte le note.
- LEGATO: seleziona uno step e attiva LEGATO per sovrapporre brevemente la transizione con il precedente.
- PHRASE BPM: velocita' del loop interno sulle due battute.
- SEGUI PLAY DAW: la frase parte e si ferma con il play della DAW e segue la sua posizione (il giro di due battute e' agganciato alle battute della canzone). Attiva anche il tempo della DAW.
- TRASCINA IN DAW: tieni premuto il pulsante e trascinalo su una traccia della DAW: la frase viene inserita come clip MIDI, senza salvare file.
- EXPORT MIDI: salva la frase come file .mid standard (formato 0, 480 tick per quarto) con tempo, note e accordi, CC2, CC11, CC1 e pitch bend. Trascina il file in Logic Pro, Studio One, Ableton Live, Reaper o in un'altra DAW.

CORSIE GRAFICHE:
BREATH (CC2) = pressione del fiato.
VELOCITY = forza iniziale / timbro della nota.
VOLUME (CC11) = volume continuo.
PITCH BEND = intonazione continua, circa +/-2 semitoni.
VIBRATO (CC1) = profondita' del vibrato.

Seleziona una corsia, poi fai clic e trascina i suoi punti. Fiato, volume, pitch e vibrato vengono interpolati fra gli step e valgono per tutte le note dello step. La velocity si applica all'attacco della prima nota dello step; le altre note dell'accordo mantengono la loro velocity registrata (96 se inserite con il mouse).

Le frasi vengono salvate con lo stato del plugin. Al riavvio del progetto non partono da sole. Il MIDI interno della frase utilizza il canale 16.)";

        case notation: return R"(SPARTITO E TEMPO DI LOGIC / STUDIO ONE

Premi SCORE VIEW nell'editor delle frasi per visualizzare il rigo musicale con note, alterazioni (#), indicazioni grafiche di durata e legature di portamento.

Questa vista e' di sola consultazione: premi PIANO ROLL per modificare le note. Non e' ancora un editor MusicXML o un impaginatore di spartiti professionale.

BPM:
- HOST BPM: legge la velocita' del progetto della DAW se disponibile.
- HOST BPM: OFF: usa il tempo interno impostato con il cursore.

IMPORTANTE: il tempo puo' seguire Logic o Studio One, ma la sequenza non segue automaticamente play, stop o la posizione della testina nella DAW. Per sincronizzarla a orecchio avvia il loop con PLAY.

La notazione rappresenta indicativamente l'altezza delle note; la durata reale e' quella degli step del sequencer e delle gate MIDI, non una trascrizione ritmica completa.)";

        case troubleshooting: return R"(RISOLUZIONE DEI PROBLEMI

NON SENTO AUDIO
- Seleziona uno strumento e controlla OUTPUT.
- Verifica dispositivo audio, uscita e monitoraggio della traccia.
- Prova una nota sulla tastiera virtuale e poi via MIDI.
- Controlla BREATH/CC2 e EXPRESSION/CC11: valori a zero possono silenziare la nota.

WINDLAB NON COMPARE IN LOGIC
- Verifica WindLab.component nella cartella Audio Unit corretta.
- Riavvia Logic, quindi ripeti la convalida nella Gestione plug-in.
- Usa la build macOS AU; un file VST3 non e' caricabile in Logic.

LA TASTIERA PC NON SUONA
- Seleziona PC / NOTEBOOK.
- Clicca i tasti del piano virtuale per ottenere focus.
- Alcune DAW intercettano i tasti: usa un controller MIDI esterno.

LO SPARTITO NON SI MODIFICA
- E' una vista di anteprima; clicca PIANO ROLL.

IL LOOP NON SEGUE L'INIZIO DEL BRANO
- HOST BPM segue soltanto il tempo; il trasporto non e' ancora sincronizzato.

LE NOTE LUNGHE NON SEMBRANO NATURALI
- Riduci intervalli estremi rispetto al registro dello strumento.
- Prova leggera variazione di CC2/CC11, meno riverbero e un'altra articolazione.)";

        case limitations: return R"(COSA E' IMPLEMENTATO E COSA NO

PRESENTE:
- Plugin AU (macOS), VST3 (Mac/Windows) e Standalone.
- Sei strumenti a fiato con campioni CC0 incorporati e motore Hybrid/Synth.
- Tastiera grafica, tastiera PC, ingresso MIDI esterno e profili velocity.
- Expression & Phrase Editor con griglie 1/8, 1/16 e terzine (fino a 48 step), accordi fino a 4 note, pedale di sustain, legato MIDI, fiato, volume, pitch e vibrato.
- Esportazione della frase in file MIDI standard per le DAW.
- STRUMENTO A VIDEO: i 9 strumenti disegnati e suonabili con mouse e tastiera del computer, con diteggiature vere.
- ESPRESSIONE: tempo e modalita' del legato, velocita' del vibrato e vibrato dal fiato, growl, flutter tongue, effetti jazz (scoop, fall, doit, shake), pan.
- MIDI Learn universale (clic destro su ogni manopola) e preset di fabbrica e personali.
- Phrase Editor agganciato al play della DAW e trascinamento della frase nella DAW.
- Vista spartito di lettura e opzione HOST BPM.
- MIDI LEARN per assegnare CC a vibrato, fiato ed espressione, salvato per progetto.
- Loop delle registrazioni sostenute e coda degli staccati perfezionati.

NON ANCORA IMPLEMENTATO:
- Legato campionato con vere transizioni fra note.
- Assegnazione visuale dei controller MIDI ai pistoni e alle chiavi dello strumento a video.
- Fiato dal microfono, modalita' sezione (piu' strumenti insieme) e riverbero a convoluzione.
- MPE avanzato per ogni singola nota.
- Spartito completo modificabile ed esportazione MusicXML.
- Velocity separata per ogni nota dell'accordo dalle corsie grafiche.
- Firma/notarizzazione universale dei pacchetti e collaudo completo in tutte le DAW.

Questa guida descrive i comandi realmente presenti nel programma. I miglioramenti futuri vengono aggiunti solo dopo lo sviluppo e la verifica.)";

        case virtualInstrument: return R"(STRUMENTO A VIDEO - SUONARE LO STRUMENTO DISEGNATO

Premi STRUMENTO A VIDEO in alto: compare il disegno dello strumento scelto in INSTRUMENT (puoi cambiarlo anche dal menu STRUMENTO del pannello). Funziona con tutti i 9 strumenti: sax soprano, contralto, tenore e baritono, clarinetto, flauto, tromba, corno e trombone.

COME SI SUONA:
- Le chiavi, i fori e i pistoni si premono con i tasti del computer (la lettera e' scritta sulla chiave) oppure con il mouse.
- Con il mouse un clic BLOCCA la chiave premuta e un secondo clic la rilascia: cosi' puoi preparare una diteggiatura completa. RILASCIA CHIAVI le libera tutte.
- La nota esce solo quando soffi: tieni premuta la BARRA SPAZIATRICE (FIATO DA: SPAZIO) oppure tieni premuto il mouse sulla barra del FIATO (FIATO DA: MOUSE; piu' in alto = piu' forte).
- FORZA DEL FIATO regola quanto soffia la barra spaziatrice.
- Cambiando diteggiatura mentre soffi le note sono LEGATE, come sullo strumento vero.

DITEGGIATURE:
- SAX: F D S = mano sinistra (Si, La, Sol), J K L = mano destra (Fa, Mi, Re), M e N = Do e Si gravi, A = Sol#, Q W E = chiavi palmari, R e U = chiavi laterali Sib e Do, SHIFT = chiave d'ottava.
- CLARINETTO: V = foro del pollice, F D S e J K L = fori, M e N = chiavi del Fa e del Mi gravi, R = chiave del La, SHIFT = portavoce (sale di una dodicesima).
- FLAUTO: V = chiave del pollice, F D S e J K L, A = Sol#, M = Do grave; FRECCE SU/GIU' o SHIFT = ottava.
- Per tutti i legni H (oppure il pulsante # +1) alza la nota di un semitono, utile per le alterazioni.
- TROMBA e CORNO: J K L = pistoni o cilindri 1, 2, 3; FRECCE SU/GIU' o la colonna ARMONICO scelgono l'armonico, come la tensione delle labbra.
- TROMBONE: trascina la coulisse con il mouse oppure usa i tasti 1-7 per le posizioni; fra una posizione e l'altra la nota scivola (glissando). FRECCE SU/GIU' = armonico.

NOTA SCRITTA E NOTA REALE:
Il riquadro NOTA mostra la nota scritta per lo strumento (sax e clarinetto e tromba sono traspositori) e la nota che suona davvero. Flauto e trombone suonano come scritto.

REGISTRARE:
REC MIDI registra quello che suoni nel Phrase Editor, come con una tastiera MIDI; poi puoi esportarlo con EXPORT MIDI. In modalita' IBRIDO le note escono sul canale NOTE CH.

LIMITI:
- Sono presenti le diteggiature principali; quelle alternative e il registro sovracuto verranno aggiunti dopo.
- Il mouse preme una chiave alla volta: per suonare veloce usa la tastiera del computer.
- Il microfono come sorgente del fiato non e' ancora disponibile.)";

        case expression: return R"(ESPRESSIONE, PRESET E MIDI LEARN UNIVERSALE

Premi ESPRESSIONE (a destra, accanto ai canali MIDI) per aprire il pannello.

LEGATO E VIBRATO:
- MODALITA' LEGATO: AUTO = legato quando suoni una nota alla volta, accordi quando premi piu' tasti insieme; SEMPRE LEGATO = ogni nota sovrapposta e' legata, anche nei passaggi velocissimi; MAI LEGATO = ogni nota ha il suo attacco (per accordi e sezioni).
- LEGATO TIME: quanto dura la scivolata da una nota all'altra (pochi ms = dita veloci, 100-300 ms = glissando, ideale per il trombone). Durante il legato il fiato cala un poco, come sullo strumento vero.
- VIBRATO RATE: velocita' del vibrato (x 1 = naturale dello strumento).
- VIBRATO DAL FIATO: a 0 il vibrato ha sempre la stessa ampiezza; alzandolo il vibrato cresce quando soffi di piu' (controller a fiato, CC2 o strumento a video).

COLORI E EFFETTI JAZZ:
- GROWL: ruvidezza di chi canta dentro lo strumento (sax rock e jazz).
- FLUTTER TONGUE: frullato di lingua (flauto, tromba, sax).
- EFFETTO JAZZ: SCOOP = la nota entra da sotto; FALL = alla fine la nota cade; DOIT = alla fine la nota sale; SHAKE = trillo di labbro sulle note tenute. Il fall e il doit si sentono quando lasci la nota; nel legato fra due note non intervengono.
- PAN: posizione dello strumento da sinistra (L) a destra (R).

PRESET:
- Il menu PRESET contiene suoni pronti (sax, tromba, flauto, clarinetto, trombone, corno). SALVA PRESET registra il tuo suono; i tuoi preset compaiono in fondo al menu. APRI PRESET carica un file .windlabpreset.
- I preset cambiano solo il suono: non toccano canali MIDI, modalita' ibrida, sorgente e calibrazione del fiato.

MIDI LEARN UNIVERSALE:
- Clic destro (oppure ctrl+clic) su qualsiasi manopola, nella finestra principale o in questo pannello, poi "Impara" e muovi un controllo MIDI: la manopola lo seguira'.
- Sotto la manopola compare il CC assegnato, per esempio [CC21]. Dallo stesso menu puoi rimuoverlo; RIMUOVI TUTTI I CC li cancella tutti.
- Le assegnazioni sono salvate nel progetto. Il pedale (CC64) e i messaggi di banco non sono assegnabili.
- Il MIDI LEARN dei tre controlli espressivi (vibrato, fiato, volume) resta nel pannello MIDI LEARN.)";
    }
    return {};
}

WindLabGuidePanel::WindLabGuidePanel(std::function<void()> onClose)
    : closeAction(std::move(onClose))
{
    setOpaque(true);
    const auto titles = sectionTitles();
    for (int i = 0; i < titles.size(); ++i)
        sections.addItem(titles[i], i + 1);
    sections.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff203640));
    sections.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    sections.onChange = [this] { openTopic(sections.getSelectedId() - 1); };
    addAndMakeVisible(sections);

    subtitle.setText("MANUALE INTERATTIVO  |  OFFLINE  |  AU / VST3 / STANDALONE",
                     juce::dontSendNotification);
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xffa3bdc5));
    subtitle.setFont(juce::Font(13.0f));
    addAndMakeVisible(subtitle);

    article.setMultiLine(true, true);
    article.setReadOnly(true);
    article.setCaretVisible(false);
    article.setScrollbarsShown(true);
    article.setFont(juce::Font(16.0f));
    article.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff172c35));
    article.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe5eff0));
    article.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff45616c));
    addAndMakeVisible(article);

    for (auto* button : {&previousButton, &nextButton, &closeButton})
    {
        button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff36525e));
        button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible(*button);
    }
    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffa86e47));
    previousButton.onClick = [this] { openTopic(currentTopic - 1); };
    nextButton.onClick = [this] { openTopic(currentTopic + 1); };
    closeButton.onClick = [this] { if (closeAction) closeAction(); };
    openTopic(start);
}

void WindLabGuidePanel::openTopic(int topic)
{
    currentTopic = juce::jlimit(0, topicCount - 1, topic);
    sections.setSelectedId(currentTopic + 1, juce::dontSendNotification);
    article.setText(articleFor(currentTopic), false);
    article.setCaretPosition(0);
    previousButton.setEnabled(currentTopic != 0);
    nextButton.setEnabled(currentTopic != topicCount - 1);
}

void WindLabGuidePanel::paint(juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff081319));
    // Host-safe floating dialog presentation within the plugin editor.
    // Unlike a separate OS window this also works in restrictive DAW hosts.
    g.setColour(juce::Colour(0xff43606b));
    g.fillRoundedRectangle(5.0f, 5.0f, full.getWidth() - 10.0f,
                           full.getHeight() - 10.0f, 13.0f);
    g.setColour(juce::Colour(0xff122630));
    g.fillRoundedRectangle(8.0f, 8.0f, full.getWidth() - 16.0f,
                           full.getHeight() - 16.0f, 11.0f);
    g.setColour(juce::Colour(0xffeab080));
    g.setFont(juce::Font(30.0f, juce::Font::bold));
    g.drawText("WINDLAB   ?   GUIDA", 30, 18, getWidth() - 210, 42,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff42616c));
    g.drawLine(30.0f, 71.0f, static_cast<float>(getWidth() - 30), 71.0f, 1.0f);
}

void WindLabGuidePanel::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    subtitle.setBounds(32, 65, w - 60, 24);
    sections.setBounds(32, 97, w - 64, 39);
    article.setBounds(32, 149, w - 64, h - 220);
    previousButton.setBounds(32, h - 57, 177, 36);
    nextButton.setBounds(217, h - 57, 172, 36);
    closeButton.setBounds(w - 170, h - 57, 138, 36);
}
