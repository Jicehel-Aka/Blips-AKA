/*
 * blips_game.cpp - see blips_game.h.
 *
 * Derived from "Blips" by Willems Davy (joyrider3774), MIT License (Game.cpp).
 * SPDX-License-Identifier: MIT
 */
#include "blips_game.h"

namespace blips {

void Game::tick(const Controls& c, const Painter* painter)
{
    if (c.look) {
        if (c.left) world.VP->Move(-2, 0);
        if (c.right) world.VP->Move(2, 0);
        if (c.up) world.VP->Move(0, -2);
        if (c.down) world.VP->Move(0, 2);
        look_reset = true;
    } else if (look_reset) {
        world.LimitVPLevel();
        look_reset = false;
    }

    Part* pl = world.Player;
    if (pl && !pl->IsMoving && ((world.Player1 && !world.Player1->IsDeath) || (world.Player2 && !world.Player2->IsDeath)) &&
        !c.look) {
        // same order as the original: down, up, left, right (the first one that works wins)
        if (c.down) pl->MoveTo(pl->GetPlayFieldX(), pl->GetPlayFieldY() + 1, false);
        if (c.up) pl->MoveTo(pl->GetPlayFieldX(), pl->GetPlayFieldY() - 1, false);
        if (c.left) pl->MoveTo(pl->GetPlayFieldX() - 1, pl->GetPlayFieldY(), false);
        if (c.right) pl->MoveTo(pl->GetPlayFieldX() + 1, pl->GetPlayFieldY(), false);
    }

    world.Draw(painter);
    world.Move();
}

bool Game::settled() const
{
    for (int i = 0; i < world.ItemCount; i++)
        if (world.Items[i]->IsMoving || world.Items[i]->NeedToKill()) return false;
    return !world.ExplosionsActive();
}

}  // namespace blips
