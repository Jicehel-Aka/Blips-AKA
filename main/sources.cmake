# Sources of the game, shared by the ESP-IDF build (main/CMakeLists.txt) and the PC build (pc/CMakeLists.txt).
set(BLIPS_GAME_SRCS
  app_main.cpp
  app.cpp
  audio.cpp
  gfx.cpp
  i18n.cpp
  platform.cpp
  save.cpp
  assets/font_accents.cpp
  assets/tiles_data.cpp
  packs.cpp
  engine/blips_world.cpp
  engine/blips_game.cpp
)
