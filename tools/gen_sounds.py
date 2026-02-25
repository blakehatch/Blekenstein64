#!/usr/bin/env python3
"""
Generate retro SFX and convert MP3 music → WAV64 for Blekenstein 64.

WAV64 raw format (all integers big-endian):
  char[4]  id          "WV64"
  int8     version     2
  int8     format      0  (RAW)
  int8     channels    1  (mono)
  int8     nbits       16
  int32_be freq        sample rate
  int32_be len         number of samples
  int32_be loop_len    0 = no loop
  int32_be start_off   24 (= sizeof header)
  int16_be[len]        PCM samples
"""

import struct, math, os, subprocess, sys, wave

SAMPLE_RATE = 22050
OUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'filesystem', 'sounds')
os.makedirs(OUT_DIR, exist_ok=True)


def clamp16(v):
    return max(-32768, min(32767, int(v)))


def write_wav64(filename, samples, sample_rate=SAMPLE_RATE, loop=False):
    n = len(samples)
    loop_len = n if loop else 0
    with open(filename, 'wb') as f:
        f.write(b'WV64')
        f.write(struct.pack('bbbb', 2, 0, 1, 16))           # ver, fmt, ch, bits
        f.write(struct.pack('>iiii', sample_rate, n, loop_len, 24))  # freq,len,loop,off
        f.write(struct.pack(f'>{n}h', *samples))


# ── Button press: short 880 Hz square blip with envelope ────────────────────
dur = int(SAMPLE_RATE * 0.08)
phase = 0.0
btn = []
for i in range(dur):
    env = (1.0 - i / dur) ** 2
    phase += 880 / SAMPLE_RATE
    v = 18000 if (phase % 1.0) < 0.5 else -18000
    btn.append(clamp16(v * env))
write_wav64(os.path.join(OUT_DIR, 'button_press.wav64'), btn)
print("  [WAV64] button_press.wav64")


# ── Shoot: falling square-wave sweep 1400 → 60 Hz, 220 ms ──────────────────
dur = int(SAMPLE_RATE * 0.22)
phase = 0.0
sht = []
for i in range(dur):
    t = i / dur
    freq = 1400 * math.exp(-t * 3.2)   # exponential sweep feels punchier
    env = (1.0 - t) ** 0.6
    phase += freq / SAMPLE_RATE
    v = 24000 if (phase % 1.0) < 0.45 else -24000   # slightly asymmetric
    sht.append(clamp16(v * env))
write_wav64(os.path.join(OUT_DIR, 'shoot.wav64'), sht)
print("  [WAV64] shoot.wav64")


# ── Death: descending wah with vibrato, 650 ms ──────────────────────────────
dur = int(SAMPLE_RATE * 0.65)
phase = 0.0
dth = []
for i in range(dur):
    t = i / dur
    base  = 720 * (1 - 0.72 * t)
    vibr  = 1.0 + 0.10 * math.sin(t * 2 * math.pi * 7)
    freq  = max(30, base * vibr)
    env   = (1.0 - t) ** 0.5
    phase += freq / SAMPLE_RATE
    v = 21000 if (phase % 1.0) < 0.5 else -21000
    dth.append(clamp16(v * env))
write_wav64(os.path.join(OUT_DIR, 'death.wav64'), dth)
print("  [WAV64] death.wav64")


# ── Music: convert MP3 → WAV → WAV64 using macOS afconvert ──────────────────
mp3_path = os.path.join(os.path.dirname(__file__), '..', 'assets', 'sounds',
                        'NESquik Beat 95 BPM.mp3')
wav_tmp   = '/tmp/blekenstein_music.wav'
out_music = os.path.join(OUT_DIR, 'music.wav64')

if not os.path.exists(mp3_path):
    print(f"  [SKIP] music.wav64 (source not found: {mp3_path})")
    sys.exit(0)

# afconvert decodes MP3 → 16-bit little-endian stereo WAV at 22050 Hz
result = subprocess.run(
    ['afconvert', '-f', 'WAVE', '-d', 'LEI16@22050', '-c', '2',
     mp3_path, wav_tmp],
    capture_output=True)
if result.returncode != 0:
    print(f"  [FAIL] afconvert: {result.stderr.decode()}")
    sys.exit(1)

# Read WAV then convert to big-endian WAV64 (stereo → mono by averaging)
with wave.open(wav_tmp, 'rb') as wf:
    nch    = wf.getnchannels()
    rate   = wf.getframerate()
    nframe = wf.getnframes()
    raw    = wf.readframes(nframe)

nsamples = nframe * nch
little_endian = struct.unpack(f'<{nsamples}h', raw)

if nch == 2:
    # Mix stereo down to mono
    mono = [clamp16((little_endian[i*2] + little_endian[i*2+1]) // 2)
            for i in range(nframe)]
else:
    mono = list(little_endian)

write_wav64(out_music, mono, sample_rate=rate, loop=True)
print(f"  [WAV64] music.wav64  ({len(mono)} samples @ {rate} Hz, looping)")
