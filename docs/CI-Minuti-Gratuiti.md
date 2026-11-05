# WindLab — risparmio minuti GitHub Actions

Per non consumare continuamente i minuti gratuiti di GitHub Actions
(macOS in particolare), la build completa AU/VST3/Standalone usa
una procedura **a gruppi di circa tre commit**.

## Workflow di sviluppo

1. Aprire una **Draft Pull Request** sul ramo dell'upgrade.
2. Realizzare e controllare localmente o via review i primi **tre commit**
   coerenti (es. motore → GUI → test/manuale), senza compilazione automatica
   per ogni commit.
3. Quando il gruppo e' pronto, usare **Ready for review** nella PR:
   l'evento `pull_request.ready_for_review` attiva **un'unica build completa**
   Mac Apple Silicon (AU, VST3, Standalone) e Windows x64 (VST3, Standalone).
4. Se i test falliscono, riportare la PR a **Draft**, accumulare le correzioni
   (preferibilmente tre commit) e ripetere **Ready for review** una volta sola.
5. Eseguire il merge su `main` solo con il check verde e senza regressioni.
   Ogni merge su `main` mantiene una **build di produzione automatica**.
6. Per casi urgenti, `workflow_dispatch` permette la build manuale dalla
   sezione GitHub Actions usando il pulsante **Run workflow**.

**Importante:** GitHub non conta automaticamente i commit con il workflow
attuale. Il raggruppamento per tre e' una regola operativa, non un timer
o una condizione matematica. La scelta elimina le build di ogni singolo
`push` sulla PR; preserva i due gate espliciti (PR pronta + main).

## Note per chi sviluppa

- Non marcare subito **Ready for review** una PR con soli uno o due commit.
- Prima della build completa, aggiornare codice, test, README e la guida `?`
  nella stessa tranche.
- Evitare modifiche alla PR durante una build: ogni modifica successiva
  richiede un nuovo giro di verifica per essere considerata validata.
- Una compilazione riuscita non sostituisce il collaudo manuale di tasti,
  registrazione MIDI, popup e strumenti a fiato in Windows Standalone o
  Logic Pro AU.
- Non eliminare i job macOS per risparmiare minuti: l'AU per Logic e la
  Standalone Mac fanno parte dei requisiti di WindLab.
