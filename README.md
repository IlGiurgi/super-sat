# SUPER·SAT

Saturatore multibanda (3 bande, 5 stili, oversampling 4x) con Goku che si carica più saturi.

- `index.html` — versione web (Web Audio), si apre nel browser.
- `vst3/` — plugin VST3 (JUCE 8). Compilato da GitHub Actions (`.github/workflows/build.yml`).

## Installare su Mac
1. Scarica l'artifact `SuperSat-mac` dalla tab **Actions** del repo e decomprimilo.
2. Apri `SuperSat-Mac-Installer.pkg` (tasto destro > Apri, perché non è notarizzato). Installa:
   VST3 in `/Library/Audio/Plug-Ins/VST3`, AU in `/Library/Audio/Plug-Ins/Components` (per Logic) e l'app Standalone in Applicazioni.
3. Riavvia la DAW e fai una nuova scansione dei plugin.
(In alternativa i file `.zip` contengono solo il VST3 o solo l'AU.)

Il plugin è firmato "ad-hoc" (non notarizzato): per distribuirlo ad altri serve un Apple Developer ID.
