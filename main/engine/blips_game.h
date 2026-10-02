/*
 * blips_game.h - one level being played: the per-frame order of the original game loop.
 *
 * Derived from "Blips" by Willems Davy (joyrider3774), MIT License (Game.cpp).
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "blips_world.h"

namespace blips {

struct Controls {
    bool left = false, right = false, up = false, down = false;
    bool look = false;               // held: the d-pad scrolls the view instead of moving the player
};

class Game {
public:
    World world;

    bool look_reset = false;

    // One frame (30 Hz): input, then drawing (which also removes dead parts), then movement.
    void tick(const Controls& c, const Painter* painter = nullptr);

    bool stage_done() const { return world.StageDone(); }
    // Somebody died and the explosion is over: the level has to be restarted.
    bool lost() const { return world.AnyPlayerDead() && !world.ExplosionsActive(); }
    // Nothing is moving any more and no explosion is running.
    bool settled() const;
};

}  // namespace blips
