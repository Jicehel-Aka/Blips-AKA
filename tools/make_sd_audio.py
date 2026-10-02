#!/usr/bin/env python3
"""Builds the audio files of the SD package in the exact format gb_audio_track_wav needs:
canonical 44 byte header, PCM, mono, 16 bit, 44100 Hz.

  tools/make_sd_audio.py sounds   [DEST]   effects -> DEST/sound/*.wav
  tools/make_sd_audio.py music    [DEST]   assets/music/title.mod -> DEST/music/title.wav  (needs ffmpeg)
  tools/make_sd_audio.py upstream DIR [DEST]
        optional: converts the six sounds of the upstream game found in DIR (its "sound" folder) over the
        synthesized ones. For your own use only - their licence is not documented upstream.

DEST defaults to SD_files/BLIPS.

Effects
  move.wav      Willems Davy (upstream, "feel free to use") - copied from assets/original/sound
  all others    synthesized here with numpy (original sounds of this port, same licence as the port: MIT)
The upstream stageend.wav is a paid asset and is never used; the upstream menu/select/menuback/error/
collect/explode sounds have no documented licence, they are replaced by the synthesized ones.
"""
import os, subprocess, sys, wave

np = None          # numpy is only needed to synthesize the effects (not for the music conversion in CI)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RATE = 44100


def write_wav(path, samples):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    s = np.asarray(samples)
    if s.dtype != np.int16:
        s = np.clip(s, -1.0, 1.0)
        s = (s * 32767).astype(np.int16)
    with wave.open(path, "wb") as w:        # the wave module writes the plain 44 byte header
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(s.tobytes())


def mono_from_wav(path):
    with wave.open(path, "rb") as w:
        ch, width, rate = w.getnchannels(), w.getsampwidth(), w.getframerate()
        raw = w.readframes(w.getnframes())
    assert width == 2, "%s: expected 16 bit" % path
    a = np.frombuffer(raw, dtype="<i2").astype(np.float64)
    if ch > 1:
        a = a.reshape(-1, ch).mean(axis=1)
    if rate != RATE:                         # simple linear resampling
        n = int(len(a) * RATE / rate)
        a = np.interp(np.linspace(0, len(a) - 1, n), np.arange(len(a)), a)
    return a.astype(np.int16)


def t(sec):
    return np.arange(int(sec * RATE)) / RATE


def env(n, attack=0.005, decay=6.0):
    x = np.arange(n) / RATE
    e = np.exp(-decay * x)
    a = int(attack * RATE)
    if a:
        e[:a] *= np.linspace(0, 1, a)
    return e


def square(f, x, duty=0.5):
    return np.where((f * x) % 1.0 < duty, 1.0, -1.0)


def tone(freq, sec, kind="sine", decay=6.0, vol=0.6):
    x = t(sec)
    ph = freq * x
    if kind == "square":
        w = square(freq, x, 0.5) * 0.7
    elif kind == "tri":
        w = 2 * np.abs(2 * (ph % 1.0) - 1) - 1
    else:
        w = np.sin(2 * np.pi * ph)
    return w * env(len(x), decay=decay) * vol


def seq(notes, kind="sine", gap=0.0, decay=7.0, vol=0.55):
    parts = []
    for f, d in notes:
        parts.append(tone(f, d, kind, decay, vol) if f else np.zeros(int(d * RATE)))
        if gap:
            parts.append(np.zeros(int(gap * RATE)))
    return np.concatenate(parts)


def noise_burst(sec, decay, lp=0.5, seed=1):
    rng = np.random.default_rng(seed)
    n = rng.uniform(-1, 1, int(sec * RATE))
    out = np.zeros_like(n)
    acc = 0.0
    for i, v in enumerate(n):                # one pole low-pass: lp near 0 = dark
        acc += (v - acc) * lp
        out[i] = acc
    return out * env(len(out), attack=0.002, decay=decay)


def synth_np():
    global np
    if np is None:
        import numpy
        np = numpy


def synth():
    global np
    import numpy
    np = numpy
    s = {}
    s["menu"] = seq([(660, 0.05)], "square", decay=30, vol=0.35)
    s["select"] = seq([(523, 0.06), (784, 0.10)], "square", decay=14, vol=0.35)
    s["menuback"] = seq([(784, 0.06), (523, 0.10)], "square", decay=14, vol=0.35)
    s["error"] = seq([(196, 0.10), (147, 0.18)], "square", decay=9, vol=0.4)
    # coin: two rising notes, bright
    s["collect"] = seq([(988, 0.05), (1319, 0.16)], "tri", decay=10, vol=0.6)
    # explosion: low rumble + noise burst, falling pitch
    x = t(0.7)
    rumble = np.sin(2 * np.pi * (90 * np.exp(-3 * x) + 30) * x) * env(len(x), 0.003, 4.5) * 0.7
    s["explode"] = np.clip(rumble + noise_burst(0.7, 5.0, 0.35, 3) * 1.6 + noise_burst(0.7, 18, 0.9, 5) * 0.5, -1, 1) * 0.9
    # level finished: little fanfare, C major arpeggio and a held chord
    arp = seq([(523, 0.11), (659, 0.11), (784, 0.11), (1047, 0.11)], "tri", decay=5, vol=0.5)
    ch = sum(tone(f, 0.7, "tri", 3.5, 0.28) for f in (523, 659, 784, 1047))
    s["stageend"] = np.concatenate([arp, ch])
    return s


def sounds(dest):
    synth_np()
    for name, data in synth().items():
        write_wav(os.path.join(dest, "sound", name + ".wav"), data)
        print("sound", name)
    src = os.path.join(ROOT, "assets", "original", "sound", "move.wav")
    write_wav(os.path.join(dest, "sound", "move.wav"), mono_from_wav(src))
    print("sound move (Willems Davy)")


def upstream(updir, dest):
    synth_np()
    for name in ("menu", "select", "menuback", "error", "collect", "explode"):
        p = os.path.join(updir, name + ".wav")
        if os.path.exists(p):
            write_wav(os.path.join(dest, "sound", name + ".wav"), mono_from_wav(p))
            print("upstream", name)


def music(dest):
    src = os.path.join(ROOT, "assets", "music", "title.mod")
    out = os.path.join(dest, "music", "title.wav")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    subprocess.check_call(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-t", "180", "-ac", "1", "-ar", str(RATE),
                           "-c:a", "pcm_s16le", "-map_metadata", "-1", "-bitexact", out])
    # ffmpeg may write extra chunks: rewrite as the plain 44 byte header file gb_audio_track_wav expects
    with wave.open(out, "rb") as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2 and w.getframerate() == RATE
        raw = w.readframes(w.getnframes())
    with wave.open(out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(raw)
    print("music title")


def main():
    a = sys.argv[1:]
    if not a:
        print(__doc__)
        return 1
    cmd = a[0]
    if cmd == "upstream":
        updir = a[1]
        dest = a[2] if len(a) > 2 else os.path.join(ROOT, "SD_files", "BLIPS")
        upstream(updir, dest)
        return 0
    dest = a[1] if len(a) > 1 else os.path.join(ROOT, "SD_files", "BLIPS")
    {"sounds": sounds, "music": music}[cmd](dest)
    return 0


if __name__ == "__main__":
    sys.exit(main())
