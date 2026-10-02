# Blips pour Gamebuino AKA (et PC / Linux / Windows)

Portage de **[Blips](https://github.com/joyrider3774/blips)** de Willems Davy (joyrider3774, licence MIT),
lui-même remake de *Bips*, *Bips Gold* et *Bips Platinum* de Bryant Brownell, sur la console
**Gamebuino AKA**, avec une version **SDL2** qui fait tourner **exactement le même code** sur PC.

Ramasse toutes les pièces d'un niveau en poussant des caisses (jamais deux à la fois). La dynamite
tue : pousse une caisse dessus pour la faire sauter. La caisse-bombe détruit les murs fissurés et les
caisses qu'elle touche, deux caisses-murs côte à côte forment un mur. Certains niveaux se jouent à deux
joueurs (on change de joueur avec A).

- 69 niveaux d'origine en 4 packs (Bips 26, Gold 9, Gold 2 joueurs 9, Platinum 25) ; un niveau se
  débloque en réussissant le précédent (option « tous les niveaux » pour tout ouvrir).
- **Éditeur de niveaux** (Titre → Éditeur) : crée un pack (saisie du nom), dessine sur une zone de
  50 × 50 cases, teste, enregistre dans `BLIPS/mylevels/<NOM>/levelN.lev` (même format que le jeu d'origine).
- Écran « Comment jouer », regard libre autour du niveau, aperçu du niveau, sauvegarde de la progression.
- Musique et bruitages, 5 langues (FR, EN, DE, ES, IT), capture d'écran.
- Les règles reprennent le moteur d'origine : les niveaux se jouent à l'identique.

## Commandes

| Touche console | Clavier PC | Action |
|---|---|---|
| Croix / joystick | flèches | se déplacer / naviguer |
| A | Entrée, Espace, Z | valider ; en jeu : **changer de joueur** (niveaux à 2 joueurs) |
| B | Retour arrière, X | retour ; en jeu : retour au choix des niveaux (avec confirmation) |
| C | C | en jeu : recommencer le niveau (avec confirmation) |
| D (maintenu) + croix | D + flèches | en jeu : **regarder autour** du niveau |
| L1 / R1 | A, R (ou PgSuiv / PgPréc) | ±5 dans la liste des niveaux ; piste de musique |
| MENU | Échap, M | menu pause. Maintenu 1 s : capture d'écran (`SHOTxxxx.BMP`) |
| RUN + MENU (0,5 s) | Maj + Échap | retour au loader (PC : quitte) |
| | F11 / F12 / Ctrl+Q | plein écran / capture / quitter (PC) |

### Éditeur

| Touche | Action |
|---|---|
| Croix | déplacer le curseur (la vue défile) |
| A (maintenu + croix : peindre) | poser la pièce choisie |
| C | effacer la case |
| L1 / R1 | pièce précédente / suivante (gomme, joueur 1, caisse, sol, dynamite, mur, pièce, joueur 2, caisses 1 et 2, caisse-bombe, caisse-mur, mur fissuré) |
| D | tester le niveau |
| B | menu : reprendre, tester, centrer, enregistrer, quitter sans enregistrer, tout effacer |

Dans la liste des niveaux d'un pack perso : A édite (ou crée avec `+`), D deux fois supprime. Un niveau doit avoir
au moins un joueur et une pièce pour être enregistré.

## Installer sur la console

1. Récupère `blips-aka-sdcard-vX.Y.Z.zip` dans les [Releases](../../releases).
2. Copie le dossier `BLIPS` à la racine de la carte SD (il contient `firmware.bin`, `meta.json`,
   `Picture.png`, `screen.bmp`, `levelpacks/`, `sound/`, `music/`).
3. Lance-le depuis le loader de la AKA. Les réglages et la progression sont écrits dans ce même
   dossier (`CFG.DAT`, `PROGRESS.DAT`).

## Jouer sur PC

- **Windows** : dézipper `blips-pc-windows-x64-…zip`, lancer `blips.exe`.
- **Linux** : installer SDL2 (`sudo apt install libsdl2-2.0-0`), dézipper `blips-pc-linux-x64-…zip`,
  lancer `./blips`.
- Options : `--scale N` (taille de la fenêtre), `--fullscreen`, `--data DIR`, `--save DIR`, `--help`.
  Le dossier `BLIPS` est cherché à côté de l'exécutable.

## Compiler

```bash
# PC (Linux / MSYS2) -- nécessite SDL2 et CMake
cmake -S pc -B build-pc -DCMAKE_BUILD_TYPE=Release
cmake --build build-pc
python3 tools/make_sd_audio.py music        # génère la musique WAV (ffmpeg requis)
build-pc/blips --data SD_files/BLIPS

# tests (moteur + parcours scripté de l'interface + éditeur)
bash tests/run_tests.sh build-pc/blips

# Console -- ESP-IDF 5.5.1
python3 tools/fix_gamebuino_case.py components/gamebuino   # une fois
idf.py set-target esp32s3 && idf.py build                  # donne build/blips.bin = firmware.bin
```

Le workflow `.github/workflows/release.yml` fait tout cela à chaque mise à jour de la branche
principale (firmware ESP-IDF, versions Linux et Windows, tests) puis publie une Release en
incrémentant la version mineure.

Les sprites et les sons sont régénérables : `python3 tools/gen_tiles.py` (Pillow),
`python3 tools/make_sd_audio.py sounds` (numpy), `python3 tools/gen_i18n.py` (textes).

## Organisation

```
main/engine/   règles du jeu (pièces, déplacements, explosions, caméra, fichiers .lev) sans matériel, testées
main/          jeu : écrans, entrées, packs, audio, sauvegarde, textes (5 langues)
pc/            couche SDL2 qui remplace uniquement gb_ll_* (écran, clavier, son) -> même rendu au pixel près
components/gamebuino/   bibliothèque Gamebuino-AKA (LGPL)
assets/        sprites et son d'origine réutilisables, musique source (.mod) ; tools/ les convertit
SD_files/BLIPS/         ce qui va sur la carte SD
tests/         tests du moteur, de l'interface (scriptée), de l'éditeur, du backend PC
```

## Licences et remerciements

Jeu original © Willems Davy (MIT). Les graphismes, la musique et les niveaux appartiennent à leurs
auteurs et gardent leurs licences (CC BY-SA, CC0…). Tout est détaillé dans **[CREDITS.md](CREDITS.md)**,
y compris ce qui **n'est pas redistribué** (dynamite et son de fin de niveau : assets payants de
l'original, remplacés par des créations originales) et les points sans licence documentée
(musique `title.mod`, niveaux). Les textes de licences sont dans `LICENSE` et `THIRD_PARTY/`.
