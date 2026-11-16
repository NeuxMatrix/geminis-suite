"""Genera el logo de Geminis (emblema zodiacal) en SVG.

Uso: python3 tools/make_logo.py Assets/logo
Crea logo-emblema.svg (cuadrado) y logo-completo.svg (emblema + nombre).
"""
import math, random, sys, os

out = sys.argv[1] if len(sys.argv) > 1 else "."
os.makedirs(out, exist_ok=True)
rng = random.Random(7)

C = 512  # centro

def defs():
    return """
  <defs>
    <radialGradient id="space" cx="50%" cy="42%" r="60%">
      <stop offset="0" stop-color="#2a1b5e"/>
      <stop offset="0.55" stop-color="#110d2e"/>
      <stop offset="1" stop-color="#05060f"/>
    </radialGradient>
    <radialGradient id="nebula" cx="50%" cy="50%" r="50%">
      <stop offset="0" stop-color="#7b5cff" stop-opacity="0.38"/>
      <stop offset="0.6" stop-color="#3b2a8f" stop-opacity="0.12"/>
      <stop offset="1" stop-color="#000" stop-opacity="0"/>
    </radialGradient>
    <linearGradient id="gold" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#fff6d8"/>
      <stop offset="0.35" stop-color="#f3c96a"/>
      <stop offset="0.72" stop-color="#c88a28"/>
      <stop offset="1" stop-color="#7e4f12"/>
    </linearGradient>
    <linearGradient id="ring" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#8fe9ff"/>
      <stop offset="0.5" stop-color="#f6d488"/>
      <stop offset="1" stop-color="#c88a28"/>
    </linearGradient>
    <linearGradient id="cyan" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#f2fdff"/>
      <stop offset="0.4" stop-color="#8fe9ff"/>
      <stop offset="1" stop-color="#2a8fb8"/>
    </linearGradient>
    <linearGradient id="twin" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#9eeeff"/>
      <stop offset="0.5" stop-color="#fff3d1"/>
      <stop offset="1" stop-color="#f3c96a"/>
    </linearGradient>
    <linearGradient id="shine" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#ffffff" stop-opacity="0.55"/>
      <stop offset="0.45" stop-color="#ffffff" stop-opacity="0"/>
    </linearGradient>
    <linearGradient id="orbit" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#8fe9ff" stop-opacity="0"/>
      <stop offset="0.3" stop-color="#8fe9ff" stop-opacity="0.8"/>
      <stop offset="0.7" stop-color="#f3c96a" stop-opacity="0.8"/>
      <stop offset="1" stop-color="#f3c96a" stop-opacity="0"/>
    </linearGradient>
    <filter id="glow" x="-50%" y="-50%" width="200%" height="200%">
      <feGaussianBlur stdDeviation="14" result="b"/>
      <feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
    </filter>
    <filter id="softglow" x="-50%" y="-50%" width="200%" height="200%">
      <feGaussianBlur stdDeviation="5" result="b"/>
      <feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
    </filter>
    <filter id="starglow" x="-200%" y="-200%" width="500%" height="500%">
      <feGaussianBlur stdDeviation="9" result="b"/>
      <feMerge><feMergeNode in="b"/><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
    </filter>
    <clipPath id="disc"><circle cx="512" cy="512" r="436"/></clipPath>
  </defs>"""

def sparkle(x, y, r, color):
    # Estrella de cuatro puntas con núcleo brillante
    k = r * 0.16
    d = (f"M{x},{y-r} Q{x+k},{y-k} {x+r},{y} Q{x+k},{y+k} {x},{y+r} "
         f"Q{x-k},{y+k} {x-r},{y} Q{x-k},{y-k} {x},{y-r} Z")
    return (f'<g filter="url(#starglow)"><path d="{d}" fill="{color}"/>'
            f'<circle cx="{x}" cy="{y}" r="{r*0.18:.1f}" fill="#ffffff"/></g>')

def emblem():
    s = []
    # Anillo zodiacal exterior
    s.append('<circle cx="512" cy="512" r="488" fill="none" stroke="url(#ring)" stroke-width="5"/>')
    s.append('<circle cx="512" cy="512" r="452" fill="none" stroke="url(#ring)" stroke-width="2" opacity="0.8"/>')
    for i in range(72):
        a = math.radians(i * 5 - 90)
        r1, r2 = (458, 474) if i % 6 == 0 else (460, 467)
        w = 2.2 if i % 6 == 0 else 1.2
        s.append(f'<line x1="{C+r1*math.cos(a):.1f}" y1="{C+r1*math.sin(a):.1f}" '
                 f'x2="{C+r2*math.cos(a):.1f}" y2="{C+r2*math.sin(a):.1f}" '
                 f'stroke="#f3d48c" stroke-width="{w}" opacity="{0.9 if i % 6 == 0 else 0.55}"/>')
    for i in range(12):
        a = math.radians(i * 30 - 75)
        x, y = C + 470 * math.cos(a), C + 470 * math.sin(a)
        big = i in (0, 6)
        rr = 7 if big else 4.5
        s.append(f'<rect x="{x-rr:.1f}" y="{y-rr:.1f}" width="{2*rr}" height="{2*rr}" '
                 f'transform="rotate(45 {x:.1f} {y:.1f})" fill="{"#fff1c4" if big else "#e9c372"}"/>')

    # Disco de espacio profundo
    s.append('<circle cx="512" cy="512" r="438" fill="url(#space)"/>')
    s.append('<g clip-path="url(#disc)">')
    s.append('<ellipse cx="470" cy="470" rx="380" ry="300" fill="url(#nebula)" transform="rotate(-25 470 470)"/>')
    for _ in range(170):
        while True:
            x, y = rng.uniform(80, 944), rng.uniform(80, 944)
            if (x - C) ** 2 + (y - C) ** 2 < 430 ** 2:
                break
        r = rng.choice([0.7, 0.9, 1.1, 1.4, 1.8])
        o = rng.uniform(0.25, 0.9)
        col = rng.choice(["#ffffff", "#cfe9ff", "#fff1d0"])
        s.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{col}" opacity="{o:.2f}"/>')

    # Constelación de Géminis (forma simplificada): dos cuerpos que bajan desde Cástor y Pólux
    castor, pollux = (408, 236), (616, 236)
    left = [castor, (384, 330), (352, 470), (318, 600), (262, 720), (330, 760)]
    right = [pollux, (640, 340), (676, 470), (716, 590), (784, 700), (706, 772)]
    arms = [((352, 470), (250, 452)), ((676, 470), (780, 446)), ((384, 330), (640, 340))]
    def poly(pts):
        return " ".join(f"{x},{y}" for x, y in pts)
    s.append(f'<polyline points="{poly(left)}" fill="none" stroke="#9fdcff" stroke-width="1.6" opacity="0.45"/>')
    s.append(f'<polyline points="{poly(right)}" fill="none" stroke="#ffd98f" stroke-width="1.6" opacity="0.45"/>')
    for a, b in arms:
        s.append(f'<line x1="{a[0]}" y1="{a[1]}" x2="{b[0]}" y2="{b[1]}" stroke="#d8d0ff" stroke-width="1.3" opacity="0.32"/>')
    for x, y in left[1:] + right[1:] + [arms[0][1], arms[1][1]]:
        s.append(f'<circle cx="{x}" cy="{y}" r="4" fill="#ffffff" filter="url(#softglow)" opacity="0.9"/>')

    # Órbita que cruza el glifo
    s.append('<ellipse cx="512" cy="522" rx="360" ry="104" fill="none" stroke="url(#orbit)" '
             'stroke-width="3" transform="rotate(-16 512 522)" opacity="0.85"/>')
    s.append('</g>')

    # Glifo de Géminis: dos columnas (los gemelos) unidas por dos arcos
    pillarL = "M386,346 L438,346 C430,440 430,584 438,678 L386,678 C394,584 394,440 386,346 Z"
    pillarR = "M586,346 L638,346 C630,440 630,584 638,678 L586,678 C594,584 594,440 586,346 Z"
    top = "M276,300 Q512,384 748,300 L740,350 Q512,430 284,350 Z"
    bottom = "M284,674 Q512,594 740,674 L748,724 Q512,640 276,724 Z"
    pillars = f'<path d="{pillarL}" fill="url(#cyan)"/><path d="{pillarR}" fill="url(#gold)"/>'
    bars = f'<path d="{top}" fill="url(#twin)"/><path d="{bottom}" fill="url(#twin)"/>'
    s.append(f'<g filter="url(#glow)" opacity="0.55">{pillars}{bars}</g>')
    # Columnas con su brillo y su filo, y encima los arcos con los suyos
    s.append(pillars)
    s.append(f'<g opacity="0.9"><path d="{pillarL}" fill="url(#shine)"/><path d="{pillarR}" fill="url(#shine)"/></g>')
    s.append(f'<g fill="none" stroke="#fff8e6" stroke-width="1.2" opacity="0.5"><path d="{pillarL}"/><path d="{pillarR}"/></g>')
    s.append(bars)
    s.append(f'<g opacity="0.9"><path d="{top}" fill="url(#shine)"/><path d="{bottom}" fill="url(#shine)"/></g>')
    s.append(f'<g fill="none" stroke="#fff8e6" stroke-width="1.2" opacity="0.6"><path d="{top}"/><path d="{bottom}"/></g>')

    # Cástor y Pólux
    s.append(sparkle(*castor, 34, "#dff8ff"))
    s.append(sparkle(*pollux, 40, "#ffe7a8"))
    return "\n  ".join(s)

FONT = ("<style>@import url('https://fonts.googleapis.com/css2?family=Cinzel:wght@500;700&amp;display=swap');"
        "</style>")

def write(name, w, h, body):
    with open(os.path.join(out, name), "w", encoding="utf-8") as f:
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}">'
                f'{FONT}{defs()}\n  {body}\n</svg>\n')

write("logo-emblema.svg", 1024, 1024, emblem())

full = (f'<g>{emblem()}</g>'
        '<text x="512" y="1180" text-anchor="middle" font-family="Cinzel, serif" font-weight="700" '
        'font-size="150" letter-spacing="26" fill="url(#gold)" filter="url(#softglow)">GÉMINIS</text>'
        '<text x="512" y="1250" text-anchor="middle" font-family="Cinzel, serif" font-weight="500" '
        'font-size="40" letter-spacing="22" fill="#bfc6e0">SUITE DE AUDIO</text>')
write("logo-completo.svg", 1024, 1300, full)
print("ok")
