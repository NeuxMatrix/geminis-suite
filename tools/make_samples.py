#!/usr/bin/env python3
"""Prepara las muestras gratuitas (CC0) que lleva Geminis Instrument.

Fuentes (dominio público, CC0):
  - VSCO 2 Community Edition  https://github.com/sgossner/VSCO-2-CE
  - Versilian Community Sample Library (VCSL)  https://github.com/sgossner/VCSL

Uso:
  python3 tools/make_samples.py <ruta VSCO-2-CE> <ruta VCSL>

Escribe Assets/samples/*.ogg (mono, 44.1 kHz) y Source/inst/SampleManifest.inc.
Para los instrumentos sostenidos se prepara un bucle con fundido para que la nota
pueda mantenerse todo lo que se quiera.
"""
import os, re, subprocess, sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'Assets', 'samples')
MANIFEST = os.path.join(ROOT, 'Source', 'inst', 'SampleManifest.inc')
SR = 44100
NOTES = {'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'D#': 3, 'Eb': 3, 'E': 4, 'F': 5, 'F#': 6, 'Gb': 6,
         'G': 7, 'G#': 8, 'Ab': 8, 'A': 9, 'A#': 10, 'Bb': 10, 'B': 11}


def note_to_midi(name, octave):
    # VSCO y VCSL nombran el Do central como C3 (convención de Yamaha)
    return (int(octave) + 2) * 12 + NOTES[name]


def git_files(repo, folder):
    out = subprocess.run(['git', '-C', repo, 'ls-tree', '-r', 'HEAD', '--name-only', '--', folder],
                         capture_output=True, text=True, check=True).stdout
    return [l for l in out.splitlines() if l.lower().endswith('.wav')]


def decode(repo, path):
    blob = subprocess.run(['git', '-C', repo, 'cat-file', 'blob', 'HEAD:' + path], capture_output=True, check=True).stdout
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', 'pipe:0', '-ac', '1', '-ar', str(SR), '-f', 'f32le', 'pipe:1'],
                         input=blob, capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32).copy()


def encode(x, dest, quality):
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 'f32le', '-ar', str(SR), '-ac', '1', '-i', 'pipe:0',
                    '-c:a', 'libvorbis', '-q:a', str(quality), dest], input=x.astype(np.float32).tobytes(), check=True)


def estimate_hz(x):
    """Frecuencia fundamental aproximada (autocorrelación) para comprobar la nota raíz."""
    seg = x[int(0.15 * SR): int(0.65 * SR)]
    if len(seg) < 4096:
        return 0.0
    seg = seg - seg.mean()
    n = 1 << int(np.ceil(np.log2(len(seg) * 2)))
    f = np.fft.rfft(seg, n)
    ac = np.fft.irfft(f * np.conj(f))[: len(seg)]
    lo, hi = int(SR / 2000), int(SR / 25)
    if ac[0] <= 0:
        return 0.0
    ac = ac / ac[0]
    lag = lo + int(np.argmax(ac[lo:hi]))
    # Preferir el primer pico alto (evita octava abajo)
    for k in range(lo, hi):
        if ac[k] > 0.85 * ac[lag] and ac[k] >= ac[k - 1] and ac[k] >= ac[k + 1]:
            lag = k
            break
    return SR / lag


def trim_start(x, threshold=0.003):
    idx = np.argmax(np.abs(x) > threshold * max(1e-9, np.max(np.abs(x))))
    return x[max(0, idx - 32):]


def prepare(x, max_sec, loop, fade_ms=60):
    x = trim_start(x)
    x = x[: int(max_sec * SR)]
    loop_start = -1
    if loop:
        # Bucle en la parte estable de la nota con fundido de 120 ms
        end = len(x)
        loop_start = int(end * 0.45)
        xf = min(int(0.12 * SR), loop_start // 2)
        w = np.linspace(0.0, 1.0, xf, dtype=np.float32)
        x[end - xf: end] = x[end - xf: end] * (1.0 - w) + x[loop_start - xf: loop_start] * w
    else:
        fade = min(len(x) // 4, int(fade_ms * 0.001 * SR))
        x[-fade:] *= np.linspace(1.0, 0.0, fade, dtype=np.float32) ** 2
    peak = np.max(np.abs(x))
    if peak > 0:
        x = x * (0.89 / peak)
    return x, loop_start


# --- Instrumentos melódicos -------------------------------------------------------------
# (id, [(repo, carpeta, regex con grupos n=nota o=octava, o i=índice)], paso mínimo en semitonos,
#  segundos máximos, bucle, calidad ogg, función opcional índice->midi)
def piano_index(i):
    return 21 + 2 * int(i)


MELODIC = [
    ('piano', [('vsco', 'Keys/Upright Piano', r'Player_dyn2_rr1_(?P<i>\d{3})\.wav$')], 4, 5.0, False, 3, piano_index),
    ('pianofm', [('vcsl', 'Electrophones/TX81Z/FM Piano', r'FMPiano_(?P<n>[A-G]#?)(?P<o>-?\d)_vl2\.wav$')], 3, 3.5, False, 3, None, -12),
    ('organo', [('vsco', 'Keys/Organ/Loud', r'Rode_Man3Open_(?P<i>\d\d)\.wav$')], 5, 3.0, True, 3, lambda i: 35 + int(i)),
    ('cuerdas', [('vsco', 'Strings/Cello Section/susvib', r'susvib_(?P<n>[A-G]#?)(?P<o>\d)_v3_1\.wav$'),
                 ('vsco', 'Strings/Violin Section/susVib', r'VlnEns_susVib_(?P<n>[A-G]#?)(?P<o>\d)_v2\.wav$')], 4, 3.2, True, 3, None),
    ('pizzicato', [('vsco', 'Strings/Cello Section/pizzT', r'pizzT_(?P<n>[A-G]#?)(?P<o>\d)_v2_RR1\.wav$'),
                   ('vsco', 'Strings/Violin Section/Pizz', r'VlnEns_Pizz_(?P<n>[A-G]#?)(?P<o>\d)_v2_rr1\.wav$')], 4, 1.6, False, 3, None),
    ('contrabajo', [('vsco', 'Strings/Solo Contrabass/Pizz', r'BKCtbss_Pizz_(?P<n>[A-G]#?)(?P<o>\d)_v1_rr1\.wav$')], 3, 2.2, False, 3, None),
    ('arpa', [('vsco', 'Strings/Harp', r'KSHarp_(?P<n>[A-G]#?)(?P<o>\d)_(mf|f)\.wav$')], 4, 3.5, False, 3, None, -12),
    ('metales', [('vsco', 'Brass/Tenor Trombone/sus', r'tenortbn_sus_(?P<n>[A-G]#?)(?P<o>\d)_v3_1\.wav$'),
                 ('vsco', 'Brass/Trumpet/sus', r'Sum_SHTrumpet_sus_(?P<n>[A-G]#?)(?P<o>\d)_v3_rr1\.wav$')], 4, 2.6, True, 3, None),
    ('flauta', [('vsco', 'Woodwinds/Flute/susvib', r'LDFlute_susvib_(?P<n>[A-G]#?)(?P<o>\d)_v1_1\.wav$')], 3, 2.6, True, 3, None),
    ('saxo', [('vcsl', 'Aerophones/Reed Aerophones/Tenor Saxophone/Non-Vibrato', r'BrettTenor_NV_Main_(?P<n>[A-G]#?)(?P<o>\d)_vl3_rr1\.wav$')], 4, 2.4, True, 3, None),
    ('marimba', [('vsco', 'Percussion/Marimba', r'Marimba_hit_Outrigger_(?P<n>[A-G]#?)(?P<o>\d)_loud_01\.wav$')], 3, 2.0, False, 3, None),
    ('vibrafono', [('vcsl', 'Idiophones/Struck Idiophones/Vibraphone/Soft Mallets', r'Vibes_soft_(?P<n>[A-G]#?)(?P<o>\d)_v2_rr1_Main\.wav$')], 3, 3.5, False, 3, None),
    ('celesta', [('vsco', 'Percussion/Glock', r'glock_medium_(?P<n>[A-G]#?)(?P<o>\d)\.wav$')], 3, 2.5, False, 3, None),
    ('kalimba', [('vcsl', 'Idiophones/Plucked Idiophones/Kalimba, Tanzania', r'MBira3_pluck_Main_(?P<n>[A-G]#?)(?P<o>\d)_k\d+(alt)?_50_100_rr2\.wav$')], 3, 2.2, False, 3, None),
]

# --- Batería y percusión (un golpe cada una) ---------------------------------------------
DRUMS = [
    ('caja_acustica', 'vcsl', 'Membranophones/Struck Membranophones/Snare Drum, Modern 1/Snare2_HitSN_v7_rr1_Mid.wav', 1.0),
    ('caja_suave', 'vcsl', 'Membranophones/Struck Membranophones/Snare Drum, Modern 1/Snare2_HitSN_v5_rr1_Mid.wav', 0.9),
    ('caja_aro', 'vcsl', 'Membranophones/Struck Membranophones/Snare Drum, Modern 1/Snare2_stick_v1_rr1_Mid.wav', 0.5),
    ('caja_vintage', 'vsco', 'VSCO 1 Percussion/drums/snare/OldSnare/snare_f.wav', 1.0),
    ('bombo_concierto', 'vsco', 'VSCO 1 Percussion/drums/bass/bdrum_f_1.wav', 1.6),
    ('charles_cerrado', 'vcsl', 'Idiophones/Struck Idiophones/Hi-Hat Cymbal/HiHat_HitC_v3_rr1_Mid.wav', 0.5),
    ('charles_abierto', 'vcsl', 'Idiophones/Struck Idiophones/Hi-Hat Cymbal/HiHat_HitO_rr1_Mid.wav', 1.6),
    ('charles_pedal', 'vcsl', 'Idiophones/Struck Idiophones/Hi-Hat Cymbal/HiHat_Close_rr1_Mid.wav', 0.5),
    ('platillo_crash', 'vcsl', 'Idiophones/Struck Idiophones/Suspended Cymbal 1/susCymb1_hit_stick_f1.wav', 3.0),
    ('platillo_ride', 'vcsl', 'Idiophones/Struck Idiophones/Suspended Cymbal 1/susCymb1_hit_stick_mp1.wav', 2.5),
    ('platillo_campana', 'vcsl', 'Idiophones/Struck Idiophones/Suspended Cymbal 1/susCymb1_hit_bell_mf1.wav', 2.0),
    ('tom_agudo', 'vcsl', 'Membranophones/Struck Membranophones/Tom 1/Stick/TomH_HitS_v4_rr1_Mid.wav', 1.2),
    ('tom_grave', 'vcsl', 'Membranophones/Struck Membranophones/Tom 2/Stick/TomL_HitS_v4_rr1_Mid.wav', 1.4),
    ('palmas', 'vcsl', 'Idiophones/Struck Idiophones/Claps/Clap_rr1.wav', 0.7),
    ('cencerro', 'vcsl', 'Idiophones/Struck Idiophones/Cowbells/Cowbell1_Normal_v3_rr1_Mid.wav', 0.8),
    ('pandereta', 'vcsl', 'Idiophones/Struck Idiophones/Tambourine 1/Tamb1_Hit_v2_rr1_Mid.wav', 0.8),
    ('maraca', 'vcsl', 'Idiophones/Struck Idiophones/Shaker, Small/Mid_Shaker_Slap_rr1.wav', 0.4),
    ('claves', 'vcsl', 'Idiophones/Struck Idiophones/Claves/Claves1_Hit_v3_rr1_Mid.wav', 0.4),
    ('bloque_madera', 'vcsl', 'Idiophones/Struck Idiophones/Woodblock/wood_click_f_rr1.wav', 0.4),
    ('conga', 'vcsl', 'Membranophones/Struck Membranophones/Conga/Conga_HitN_v2_rr1_Sum.wav', 1.0),
    ('conga_aguda', 'vcsl', 'Membranophones/Struck Membranophones/Conga/Quinto_HitN_v2_rr1_Sum.wav', 0.9),
    ('tumbadora', 'vcsl', 'Membranophones/Struck Membranophones/Conga/Tumba_HitN_v3_rr1_Sum.wav', 1.2),
    ('bongo_agudo', 'vcsl', 'Membranophones/Struck Membranophones/Bongos/BongoH_Hit1_v3_rr1_Mid.wav', 0.6),
    ('bongo_grave', 'vcsl', 'Membranophones/Struck Membranophones/Bongos/BongoL_Hit1_v3_rr1_Mid.wav', 0.7),
    ('cajon_grave', 'vcsl', 'Idiophones/Struck Idiophones/Cajon/Cajon_hit1_f_rr1.wav', 0.8),
    ('cajon_golpe', 'vcsl', 'Idiophones/Struck Idiophones/Cajon/Cajon_hit3_f_rr1.wav', 0.6),
    ('triangulo', 'vcsl', 'Idiophones/Struck Idiophones/Triangles/', 2.0),
    ('yunque', 'vcsl', 'Idiophones/Struck Idiophones/Anvil/Anvil_Hit1_v3_rr1_Mid.wav', 1.2),
    ('tambor_freno', 'vcsl', 'Idiophones/Struck Idiophones/Brake Drum/BrakeDrum1_Hammer_v3_rr1_Mid.wav', 1.0),
]


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    repos = {'vsco': sys.argv[1], 'vcsl': sys.argv[2]}
    os.makedirs(OUT, exist_ok=True)
    lines = ['// Generado por tools/make_samples.py: no editar a mano.',
             '// { instrumento, recurso, nota raíz MIDI, inicio del bucle (-1 = sin bucle) }']
    total = 0

    for entry in MELODIC:
        inst, sources, step, max_sec, loop, quality, index_fn = entry[:7]
        shift = entry[7] if len(entry) > 7 else 0   # el piano FM y el arpa nombran el Do central como C4
        found = {}
        for repo, folder, pattern in sources:
            rx = re.compile(pattern)
            for path in git_files(repos[repo], folder):
                m = rx.search(path.split('/')[-1])
                if not m:
                    continue
                gd = m.groupdict()
                midi = (index_fn(gd['i']) if gd.get('i') is not None else note_to_midi(gd['n'], gd['o'])) + shift
                found.setdefault(midi, (repo, path))
        chosen, last = [], -100
        for midi in sorted(found):
            if midi - last >= step:
                chosen.append(midi)
                last = midi
        print(f'{inst}: {len(chosen)} muestras {chosen}')
        for midi in chosen:
            repo, path = found[midi]
            x = decode(repos[repo], path)
            hz = estimate_hz(x)
            if hz > 0:
                detected = 69 + 12 * np.log2(hz / 440.0)
                off = (detected - midi + 6) % 12 - 6
                if abs(off) > 0.6:
                    print(f'   aviso {inst} {midi}: suena {detected:.1f} ({off:+.1f} st)  {path}')
            y, loop_start = prepare(x, max_sec, loop)
            name = f'{inst}_{midi:03d}'
            dest = os.path.join(OUT, name + '.ogg')
            encode(y, dest, quality)
            total += os.path.getsize(dest)
            lines.append(f'{{ "{inst}", "{name}_ogg", {midi}, {loop_start} }},')

    for name, repo, path, max_sec in DRUMS:
        if path.endswith('/'):
            files = git_files(repos[repo], path.rstrip('/'))
            files = [f for f in files if re.search(r'(_f|_mf|hit|Hit)', f.split('/')[-1])] or files
            path = sorted(files)[0]
        x = decode(repos[repo], path)
        y, _ = prepare(x, max_sec, False, fade_ms=40 if max_sec < 1 else 120)
        dest = os.path.join(OUT, name + '.ogg')
        encode(y, dest, 4)
        total += os.path.getsize(dest)
        lines.append(f'{{ "drum", "{name}_ogg", 60, -1 }},')
        print(f'drum {name}: {path}')

    with open(MANIFEST, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'Total: {total / 1e6:.2f} MB')


if __name__ == '__main__':
    main()
