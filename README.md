# SUPER·SAT

Saturatore multibanda (3 bande, 5 stili, oversampling 4x) con Goku che si carica più saturi.

- `index.html` — versione web (Web Audio), si apre nel browser.
- `vst3/` — plugin VST3 (JUCE 8). Compilato da GitHub Actions (`.github/workflows/build.yml`).

## Installare su Mac
1. Scarica l'artifact `SuperSat-mac-vst3` dalla tab **Actions** del repo e decomprimilo.
2. Copia `Super Sat.vst3` in `~/Library/Audio/Plug-Ins/VST3/` (o `/Library/Audio/Plug-Ins/VST3/`).
3. Se macOS lo blocca: `xattr -cr "~/Library/Audio/Plug-Ins/VST3/Super Sat.vst3"`
4. Riavvia la DAW e fai una nuova scansione dei plugin.

Il plugin è firmato "ad-hoc" (non notarizzato): per distribuirlo ad altri serve un Apple Developer ID.
