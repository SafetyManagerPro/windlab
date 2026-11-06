# Test WindLab 0.9.3

## Test automatici senza JUCE (Linux, macOS, Windows)

```bash
./tests/run-smoke.sh
```

Lo script compila e lancia in sequenza quattro suite DSP:

- `smoke.cpp`: motore dei fiati, durate, espressione, EQ, controller, tre sample rate.
- `phrase-smoke.cpp`: frasi a 16 step, note, curve e stop.
- `midi-learn-smoke.cpp`: assegnazioni MIDI Learn, scambi di CC e isolamento del canale 16.
- `phrase-record-smoke.cpp`: registrazione di 2 battute, quantizzazione, curve CC e import non distruttivo.

Con CMake, `-DBUILD_TESTING=ON` e `ctest --test-dir build` eseguono le stesse suite.

## Test di build e CI

- `python tests/validate-recorded-bank.py`: verifica i campioni registrati e i livelli di velocity.
- `python tests/validate-guide.py`: verifica la guida offline e i pulsanti "?".
- GitHub Actions compila VST3 e Standalone su Windows, e AU, VST3 e Standalone su macOS, poi esegue `ctest`. Le build complete partono solo quando la PR passa a "Ready for review" o quando si aggiorna `main`.

## Non ancora verificato

- Il plugin JUCE non è compilato in questo ambiente di test: serve JUCE 9.0.5 installato o `-DJUCE_SOURCE_DIR`.
- Nessuna prova su host reali (Logic Pro AU, Ableton, Reaper e altri): va fatta a mano, con registrazione, guida e ritorno alla schermata principale.
- `auval` e `plugval` vanno lanciati su macOS dopo aver verificato i codici finali di tipo, sottotipo e produttore.
- Profilazione CPU e verifica degli alias con diverse impostazioni del sistema armonico.
