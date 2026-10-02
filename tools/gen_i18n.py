#!/usr/bin/env python3
"""Regenerates main/i18n.cpp from the table below and checks it against the enum in main/i18n.h.
Edit the table (one row per string: key, FR, EN, DE, ES, IT), keep i18n.h's enum in the same order, run:
    python3 tools/gen_i18n.py
UTF-8; only lowercase accents (the 8x8 font has no uppercase accented glyph); lines of <= 38 characters."""
import os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROWS = [
 ("S_PLAY",        "Jouer", "Play", "Spielen", "Jugar", "Gioca"),
 ("S_HOWTO",       "Comment jouer", "How to play", "Spielanleitung", "Cómo jugar", "Come si gioca"),
 ("S_OPTIONS",     "Options", "Options", "Optionen", "Opciones", "Opzioni"),
 ("S_CREDITS",     "Crédits", "Credits", "Mitwirkende", "Créditos", "Crediti"),
 ("S_QUIT",        "Quitter", "Quit", "Beenden", "Salir", "Esci"),
 ("S_CHOOSE_PACK", "Choisis un pack", "Choose a pack", "Wähle ein Paket", "Elige un pack", "Scegli un pacchetto"),
 ("S_LEVELS_FMT",  "%d niveaux", "%d levels", "%d Level", "%d niveles", "%d livelli"),
 ("S_LEVEL_FMT",   "Niveau %d", "Level %d", "Level %d", "Nivel %d", "Livello %d"),
 ("S_BY_FMT",      "par %s", "by %s", "von %s", "por %s", "di %s"),
 ("S_COINS",       "Pièces", "Coins", "Münzen", "Monedas", "Monete"),
 ("S_SOLVED",      "Niveau terminé !", "Level complete!", "Level geschafft!", "¡Nivel completado!", "Livello completato!"),
 ("S_SOLVED_NEXT", "A : niveau suivant", "A: next level", "A: nächstes Level", "A: nivel siguiente", "A: livello successivo"),
 ("S_PACK_DONE",   "Pack terminé, bravo !", "Pack finished, well done!", "Paket gelöst, bravo!", "¡Pack terminado, bravo!", "Pacchetto finito, bravo!"),
 ("S_DIED",        "Dommage, c'est raté !", "Too bad, you died!", "Schade, gestorben!", "¡Qué pena, has muerto!", "Peccato, sei morto!"),
 ("S_DIED_HELP",   "A:réessayer  B:niveaux", "A:try again  B:levels", "A:nochmal  B:Level", "A:reintentar  B:niveles", "A:riprova  B:livelli"),
 ("S_CONFIRM_LEVELS","Retour au choix des niveaux ?", "Back to the level select?", "Zurück zur Levelwahl?", "¿Volver a elegir nivel?", "Tornare alla scelta livello?"),
 ("S_CONFIRM_RELOAD","Recommencer le niveau ?", "Restart the level?", "Level neu starten?", "¿Reiniciar el nivel?", "Ricominciare il livello?"),
 ("S_CONFIRM_HELP","A:oui  B:non", "A:yes  B:no", "A:ja  B:nein", "A:sí  B:no", "A:sì  B:no"),
 ("S_PAUSE",       "Pause", "Paused", "Pause", "Pausa", "Pausa"),
 ("S_RESUME",      "Reprendre", "Resume", "Weiter", "Continuar", "Riprendi"),
 ("S_RESTART",     "Recommencer", "Restart", "Neustart", "Reiniciar", "Ricomincia"),
 ("S_LEVEL_SELECT","Choix du niveau", "Level select", "Levelauswahl", "Elegir nivel", "Scelta livello"),
 ("S_TO_TITLE",    "Ecran titre", "Title screen", "Titelbild", "Pantalla de título", "Schermata titolo"),
 ("S_OPT_LANGUAGE","Langue", "Language", "Sprache", "Idioma", "Lingua"),
 ("S_OPT_MUSIC",   "Musique", "Music", "Musik", "Música", "Musica"),
 ("S_OPT_SFX",     "Bruitages", "Sound fx", "Effekte", "Efectos", "Effetti"),
 ("S_OPT_UNLOCK",  "Tous les niveaux", "All levels open", "Alle Level offen", "Todos los niveles", "Tutti i livelli"),
 ("S_ON",          "oui", "on", "an", "sí", "sì"),
 ("S_OFF",         "non", "off", "aus", "no", "no"),
 ("S_BACK",        "Retour", "Back", "Zurück", "Volver", "Indietro"),
 ("S_NO_PACKS",    "Aucun pack de niveaux", "No level pack found", "Keine Levelpakete", "Ningún pack de niveles", "Nessun pacchetto"),
 ("S_LOAD_ERROR",  "Niveau illisible", "Level unreadable", "Level unlesbar", "Nivel ilegible", "Livello illeggibile"),
 ("S_SHOT_SAVED",  "Capture enregistrée", "Screenshot saved", "Bildschirmfoto gespeichert", "Captura guardada", "Schermata salvata"),
 ("S_SHOT_FAILED", "Capture impossible", "Screenshot failed", "Bildschirmfoto fehlgeschlagen", "Captura imposible", "Schermata non riuscita"),
 ("S_HELP_MENU",   "A:valider B:retour", "A:select B:back", "A:wählen B:zurück", "A:aceptar B:volver", "A:ok B:indietro"),
 ("S_HELP_LEVELS", "A:jouer B:retour L/R:+-5", "A:play B:back L/R:+-5", "A:los B:zurück L/R:+-5", "A:jugar B:volver L/R:+-5", "A:gioca B:indietro L/R:+-5"),
 ("S_HOW1",        "Ramasse toutes les pièces.", "Collect all the coins.", "Sammle alle Münzen.", "Recoge todas las monedas.", "Raccogli tutte le monete."),
 ("S_HOW2",        "Pousse les caisses (jamais deux).", "Push boxes (never two at once).", "Schiebe Kisten (nie zwei).", "Empuja cajas (nunca dos).", "Spingi le casse (mai due)."),
 ("S_HOW3",        "La dynamite tue : pousse une", "Dynamite kills: push a box", "Dynamit tötet: schiebe eine", "La dinamita mata: empuja una", "La dinamite uccide: spingi una"),
 ("S_HOW4",        "caisse dessus pour la faire sauter.", "onto it to blow it up.", "Kiste darauf, um ihn zu sprengen.", "caja encima para hacerla estallar.", "cassa sopra per farla esplodere."),
 ("S_HOW5",        "Caisse-bombe : détruit murs fissurés", "Bomb box: destroys cracked walls", "Bombenkiste: zerstört rissige", "Caja bomba: destruye muros", "Cassa bomba: distrugge muri"),
 ("S_HOW6",        "et caisses. Deux caisses-mur = un mur.", "and boxes. Two wall boxes = a wall.", "Wände. Zwei Wandkisten = Wand.", "agrietados. Dos cajas muro = muro.", "crepati. Due casse muro = muro."),
 ("S_HOW7",        "A : changer de joueur (2 joueurs)", "A: switch player (2 players)", "A: Spieler wechseln (2 Spieler)", "A: cambiar de jugador (2)", "A: cambia giocatore (2)"),
 ("S_HOW8",        "D + croix : regarder autour", "D + d-pad: look around", "D + Kreuz: umsehen", "D + cruz: mirar alrededor", "D + croce: guarda attorno"),
 ("S_HOW9",        "B : niveaux   C : recommencer", "B: levels   C: restart", "B: Level   C: Neustart", "B: niveles   C: reiniciar", "B: livelli   C: ricomincia"),
 ("S_CR_TITLE_GAME","Jeu d'origine", "Original game", "Originalspiel", "Juego original", "Gioco originale"),
 ("S_CR_TITLE_ART","Graphismes", "Graphics", "Grafik", "Gráficos", "Grafica"),
 ("S_CR_TITLE_SOUND","Musique et sons", "Music and sound", "Musik und Klang", "Música y sonido", "Musica e suoni"),
 ("S_CR_TITLE_LEVELS","Niveaux", "Levels", "Level", "Niveles", "Livelli"),
 ("S_CR_TITLE_PORT","Portage AKA", "AKA port", "AKA-Portierung", "Versión AKA", "Porting AKA"),
 ("S_PAGE_FMT",    "%d/%d", "%d/%d", "%d/%d", "%d/%d", "%d/%d"),
 # ---- level editor
 ("S_EDITOR",      "Editeur de niveaux", "Level editor", "Leveleditor", "Editor de niveles", "Editor di livelli"),
 ("S_ED_PACKS_TITLE","Mes packs (éditeur)", "My packs (editor)", "Meine Pakete (Editor)", "Mis packs (editor)", "I miei pacchetti (editor)"),
 ("S_ED_NEW_PACK", "Nouveau pack", "New pack", "Neues Paket", "Nuevo pack", "Nuovo pacchetto"),
 ("S_ED_NEW_LEVEL","Nouveau niveau", "New level", "Neues Level", "Nuevo nivel", "Nuovo livello"),
 ("S_ED_NAME_TITLE","Nom du nouveau pack", "Name of the new pack", "Name des neuen Pakets", "Nombre del nuevo pack", "Nome del nuovo pacchetto"),
 ("S_ED_NAME_HELP","haut/bas:lettre gauche/droite:place", "up/down:letter left/right:place", "hoch/runter:Buchst. links/rechts", "arr/abj:letra izq/der:lugar", "su/giu:lettera sx/dx:posto"),
 ("S_ED_NAME_HELP2","D:effacer  A:ok  B:annuler", "D:delete  A:ok  B:cancel", "D:löschen  A:ok  B:abbrechen", "D:borrar  A:ok  B:cancelar", "D:cancella  A:ok  B:annulla"),
 ("S_ED_EXISTS",   "Ce nom existe déjà", "This name already exists", "Name existiert schon", "Ese nombre ya existe", "Questo nome esiste già"),
 ("S_ED_TEST",     "Tester le niveau", "Test the level", "Level testen", "Probar el nivel", "Prova il livello"),
 ("S_ED_SAVE",     "Enregistrer et quitter", "Save and exit", "Speichern und beenden", "Guardar y salir", "Salva ed esci"),
 ("S_ED_DISCARD",  "Quitter sans enregistrer", "Exit without saving", "Ohne Speichern beenden", "Salir sin guardar", "Esci senza salvare"),
 ("S_ED_CLEAR",    "Tout effacer", "Clear all", "Alles löschen", "Borrar todo", "Cancella tutto"),
 ("S_ED_CENTER",   "Centrer le niveau", "Center the level", "Level zentrieren", "Centrar el nivel", "Centra il livello"),
 ("S_ED_SAVED",    "Niveau enregistré", "Level saved", "Level gespeichert", "Nivel guardado", "Livello salvato"),
 ("S_ED_SAVE_FAILED","Ecriture impossible", "Cannot write the file", "Schreiben fehlgeschlagen", "No se pudo escribir", "Scrittura impossibile"),
 ("S_ED_DELETE_CONFIRM","D encore : supprimer", "Press D again to delete", "D nochmal: löschen", "D otra vez: borrar", "D ancora: elimina"),
 ("S_ED_TEST_OK",  "Test réussi !", "Test passed!", "Test bestanden!", "¡Prueba superada!", "Test superato!"),
 ("S_ED_HELP",     "A:pose C:efface L/R:pièce D:test B:menu", "A:put C:erase L/R:part D:test B:menu", "A:setzen C:weg L/R:Teil D:Test B:Menü", "A:poner C:borra L/R:pieza D:test B:menú", "A:metti C:canc L/R:pezzo D:test B:menu"),
 ("S_ED_LIST_HELP","A:éditer D x2:supprimer B:retour","A:edit D x2:delete B:back","A:ändern D x2:löschen B:zurück","A:editar D x2:borrar B:volver","A:modifica D x2:elimina B:indietro"),
 ("S_ED_NO_PLAYER","Il manque un joueur", "A player is missing", "Spieler fehlt", "Falta un jugador", "Manca un giocatore"),
 ("S_ED_NO_COIN",  "Il manque des pièces", "Coins are missing", "Münzen fehlen", "Faltan monedas", "Mancano le monete"),
 ("S_ED_TOO_MANY", "Trop de pièces", "Too many parts", "Zu viele Teile", "Demasiadas piezas", "Troppi pezzi"),
 ("S_P_ERASE",     "Gomme", "Eraser", "Radierer", "Goma", "Gomma"),
 ("S_P_PLAYER",    "Joueur 1", "Player 1", "Spieler 1", "Jugador 1", "Giocatore 1"),
 ("S_P_BOX",       "Caisse", "Box", "Kiste", "Caja", "Cassa"),
 ("S_P_FLOOR",     "Sol", "Floor", "Boden", "Suelo", "Pavimento"),
 ("S_P_DYNAMITE",  "Dynamite", "Dynamite", "Dynamit", "Dinamita", "Dinamite"),
 ("S_P_WALL",      "Mur", "Wall", "Wand", "Muro", "Muro"),
 ("S_P_COIN",      "Pièce", "Coin", "Münze", "Moneda", "Moneta"),
 ("S_P_PLAYER2",   "Joueur 2", "Player 2", "Spieler 2", "Jugador 2", "Giocatore 2"),
 ("S_P_BOX1",      "Caisse joueur 1", "Box for player 1", "Kiste Spieler 1", "Caja jugador 1", "Cassa giocatore 1"),
 ("S_P_BOX2",      "Caisse joueur 2", "Box for player 2", "Kiste Spieler 2", "Caja jugador 2", "Cassa giocatore 2"),
 ("S_P_BOXBOMB",   "Caisse-bombe", "Bomb box", "Bombenkiste", "Caja bomba", "Cassa bomba"),
 ("S_P_BOXWALL",   "Caisse-mur", "Wall box", "Wandkiste", "Caja muro", "Cassa muro"),
 ("S_P_BREAKABLE", "Mur fissuré", "Cracked wall", "Rissige Wand", "Muro agrietado", "Muro crepato"),
]

def c(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'

def main():
    hp = os.path.join(ROOT, "main", "i18n.h")
    hdr = open(hp).read()
    keys = [r[0] for r in ROWS]
    enum = "enum Str {\n" + "".join("    %s,\n" % k for k in keys) + "    S_COUNT\n};"
    hdr = re.sub(r"enum Str \{.*?\n\};", lambda m: enum, hdr, flags=re.S)
    open(hp, "w").write(hdr)
    for r in ROWS:
        assert len(r) == 6, r
        for s in r[1:]:
            assert "\n" not in s
    out = ['// GENERATED by tools/gen_i18n.py - edit the table there, not this file.',
           '// UTF-8, lowercase accents only (the 8x8 font has no uppercase accented glyph).',
           '#include "i18n.h"', '', 'namespace i18n {', '', 'static int s_lang = FR;', '',
           'static const char* const T[LANG_COUNT][S_COUNT] = {']
    for li, lang in enumerate(['FR', 'EN', 'DE', 'ES', 'IT']):
        out.append('  {  // ' + lang)
        for r in ROWS:
            out.append('    ' + c(r[1 + li]) + ',  // ' + r[0])
        out.append('  },')
    out += ['};', '',
            'static const char* const NAMES[LANG_COUNT] = { "Français", "English", "Deutsch", "Español", "Italiano" };', '',
            'void set_lang(int l) { s_lang = (l >= 0 && l < LANG_COUNT) ? l : FR; }',
            'int lang() { return s_lang; }',
            'const char* lang_name(int l) { return (l >= 0 && l < LANG_COUNT) ? NAMES[l] : NAMES[EN]; }',
            'const char* tr(Str s) { return (s >= 0 && s < S_COUNT) ? T[s_lang][s] : ""; }', '',
            '}  // namespace i18n', '']
    open(os.path.join(ROOT, "main", "i18n.cpp"), "w").write("\n".join(out))
    print(len(ROWS), "strings x 5 languages")

main()
