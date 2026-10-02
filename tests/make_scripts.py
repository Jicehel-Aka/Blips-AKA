#!/usr/bin/env python3
"""Writes the scripts driven by the scripted PC build (AKA_PC_SCRIPT) and the progress file they need.

  make_scripts.py play   OUT.script SAVE_DIR   solves level 2 of the pack "bips" through the real UI
  make_scripts.py editor OUT.script            draws a small level in the editor, tests it and saves it

The solution below was found by the breadth first solver of tests/engine_test.cpp (119 moves) and is
replayed by that test with the real engine, so a change of the rules shows up there first.
"""
import struct, sys, os

SOLUTION = "rddrrrrllllddrrrrllllddrrrdddlruuulldldduuuuuuuuurrrrrrdddddddddrrrrrrrrrruuuuuuuuulllllllldddddddrrrrrruuuuulllldddrru"


def play(out, save_dir):
    # progress: pack "bips", 2 levels unlocked -> the level select opens on level 2
    os.makedirs(save_dir, exist_ok=True)
    entry = struct.pack("<40sHH", b"bips", 2, 0)
    data = struct.pack("<II", 0x42505231, 1) + entry + b"\0" * (44 * 63)
    open(os.path.join(save_dir, "PROGRESS.DAT"), "wb").write(data)
    l, f = [], 5
    l.append("%d tap a" % f); f += 8          # title: Play
    l.append("%d tap a" % f); f += 8          # pack list: bips
    l.append("%d shot levels.bmp" % f); f += 2
    l.append("%d tap a" % f); f += 20         # start level 2
    l.append("%d shot level2.bmp" % f); f += 2
    key = {"l": "left", "r": "right", "u": "up", "d": "down"}
    for m in SOLUTION:
        l.append("%d press %s" % (f, key[m])); f += 1
        l.append("%d release %s" % (f, key[m])); f += 9      # a tile takes 8 frames
    f += 30
    l.append("%d shot solved.bmp" % f); f += 2
    l.append("%d tap a" % f); f += 20         # next level
    l.append("%d shot level3.bmp" % f); f += 2
    l.append("%d quit" % f)
    open(out, "w").write("\n".join(l) + "\n")


def editor(out):
    l, f = [], 5

    def tap(k, gap=5):
        nonlocal f
        l.append("%d tap %s" % (f, k)); f += gap

    tap("down"); tap("a"); tap("a"); tap("a", 8); tap("a", 8)      # Editor > new pack > name ok > new level
    for i in range(5):                                              # five floor tiles
        tap("a")
        if i < 4: tap("right")
    tap("l1"); tap("l1")                                            # part: player 1
    for i in range(4): tap("left")
    tap("a")
    for i in range(5): tap("r1")                                    # part: coin
    for i in range(4): tap("right")
    tap("a", 8)
    l.append("%d shot ed_drawn.bmp" % f); f += 2
    tap("d", 10)                                                    # test play
    l.append("%d press right" % f); f += 120
    l.append("%d release right" % f); f += 3
    l.append("%d shot ed_tested.bmp" % f); f += 4
    tap("a", 10)                                                    # back to the editor
    tap("b"); tap("down"); tap("down"); tap("down"); tap("a", 10)   # menu > save
    l.append("%d shot ed_saved.bmp" % f); f += 2
    l.append("%d quit" % f)
    open(out, "w").write("\n".join(l) + "\n")


if __name__ == "__main__":
    if sys.argv[1] == "play":
        play(sys.argv[2], sys.argv[3])
    else:
        editor(sys.argv[2])
