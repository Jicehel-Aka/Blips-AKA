/*
 * i18n.h - texts in five languages (FR, EN, DE, ES, IT). UTF-8, lowercase accents only
 * (the 8x8 font has no uppercase accented glyph).
 * SPDX-License-Identifier: MIT
 */
#pragma once

namespace i18n {

enum Lang { FR, EN, DE, ES, IT, LANG_COUNT };

enum Str {
    S_PLAY,
    S_HOWTO,
    S_OPTIONS,
    S_CREDITS,
    S_QUIT,
    S_CHOOSE_PACK,
    S_LEVELS_FMT,
    S_LEVEL_FMT,
    S_BY_FMT,
    S_COINS,
    S_SOLVED,
    S_SOLVED_NEXT,
    S_PACK_DONE,
    S_DIED,
    S_DIED_HELP,
    S_CONFIRM_LEVELS,
    S_CONFIRM_RELOAD,
    S_CONFIRM_HELP,
    S_PAUSE,
    S_RESUME,
    S_RESTART,
    S_LEVEL_SELECT,
    S_TO_TITLE,
    S_OPT_LANGUAGE,
    S_OPT_MUSIC,
    S_OPT_SFX,
    S_OPT_UNLOCK,
    S_ON,
    S_OFF,
    S_BACK,
    S_NO_PACKS,
    S_LOAD_ERROR,
    S_SHOT_SAVED,
    S_SHOT_FAILED,
    S_HELP_MENU,
    S_HELP_LEVELS,
    S_HOW1,
    S_HOW2,
    S_HOW3,
    S_HOW4,
    S_HOW5,
    S_HOW6,
    S_HOW7,
    S_HOW8,
    S_HOW9,
    S_CR_TITLE_GAME,
    S_CR_TITLE_ART,
    S_CR_TITLE_SOUND,
    S_CR_TITLE_LEVELS,
    S_CR_TITLE_PORT,
    S_PAGE_FMT,
    S_EDITOR,
    S_ED_PACKS_TITLE,
    S_ED_NEW_PACK,
    S_ED_NEW_LEVEL,
    S_ED_NAME_TITLE,
    S_ED_NAME_HELP,
    S_ED_NAME_HELP2,
    S_ED_EXISTS,
    S_ED_TEST,
    S_ED_SAVE,
    S_ED_DISCARD,
    S_ED_CLEAR,
    S_ED_CENTER,
    S_ED_SAVED,
    S_ED_SAVE_FAILED,
    S_ED_DELETE_CONFIRM,
    S_ED_TEST_OK,
    S_ED_HELP,
    S_ED_LIST_HELP,
    S_ED_NO_PLAYER,
    S_ED_NO_COIN,
    S_ED_TOO_MANY,
    S_P_ERASE,
    S_P_PLAYER,
    S_P_BOX,
    S_P_FLOOR,
    S_P_DYNAMITE,
    S_P_WALL,
    S_P_COIN,
    S_P_PLAYER2,
    S_P_BOX1,
    S_P_BOX2,
    S_P_BOXBOMB,
    S_P_BOXWALL,
    S_P_BREAKABLE,
    S_COUNT
};

void set_lang(int lang);
int  lang();
const char* lang_name(int lang);          // native name
const char* tr(Str s);

}  // namespace i18n
