# Géminis Suite

Suite de audio propia, con diseño y sonidos originales. Son dos plugins (VST3 en Windows, macOS y Linux,
y AU en macOS) que también funcionan como programa independiente. Valen para cualquier DAW: Cubase, Logic,
Ableton, Reaper, FL Studio o Maschine.

Al abrirlos, una pantalla de inicio animada con el emblema de Géminis pregunta qué vas a hacer.

| Plugin | Dónde se pone | Modos |
|---|---|---|
| **Geminis FX** | Pista de audio (guitarra, voz) | Guitarra · Voz |
| **Geminis Instrument** | Pista de instrumento o MIDI (o un Sound de Maschine) | Sintetizador · Batería · Instrumentos |

Todos los modelos son diseños propios con nombres propios. No se usan marcas ni aspectos de terceros.

## Geminis FX: guitarra y voz

- Rack de 10 huecos en el orden que quieras, con 27 módulos. Puedes poner varios delays y varias
  reverbs a la vez, cada una distinta.
- **Guitarra:** puerta de ruido, compresor, limitador, cuatro pedales de saturación (Impulso, Furia, Nébula, Chispa),
  octavador, wah, ecualizador, seis amplificadores (del limpio al bajo), pantallas con respuestas a impulso propias
  o tuyas, chorus, flanger, phaser, vibrato, trémolo, altavoz rotativo, delays (digital, cinta, analógico,
  ping-pong, multitap, inverso), reverbs (habitación, sala, placa, muelle, catedral, shimmer) y lo-fi.
  Afinador en la barra superior.
- **Voz:** ecualizador vocal, de-esser, compresor, saturación de cinta, doblador, afinación, modulación,
  delays y reverbs.
- Presets de fábrica por estilos para guitarra y voz, y los tuyos se guardan en `Documentos/Geminis/Presets FX`.
- Sirve en directo con monitorización o sobre pistas ya grabadas. Selector de entrada mono o estéreo.

## Geminis Instrument: sintetizador, batería e instrumentos

### Sintetizador modular
- Dos osciladores con **ondas que se curvan**: el mando CURVA dobla la onda y el sonido cambia según la doblas.
  Cinco maneras de curvar (curvar, plegar, sincro, formante y bits) sobre once ondas de partida.
- **Dibuja tu propia onda** con el ratón directamente sobre la pantalla del oscilador, o parte de una de las formas incluidas.
- Unísono de hasta 7 voces, sub oscilador, ruido, FM y modulación en anillo.
- Filtro de cinco tipos con saturación, tres envolventes, dos LFO sincronizables y una matriz de 6 modulaciones.
- Efectos: saturación, compresor, chorus/phaser/flanger, delay y reverb.
- **66 presets** (bajos, leads, pads, teclas, secuencias, industrial, futuristas, clásicos analógicos y ondas dibujadas).
  En muchos, la rueda de modulación curva la onda. Tus sonidos se guardan en `Documentos/Geminis/Sonidos`.

### Batería por estilos
- 16 kits con 4 patrones cada uno: rock, metal, funk, pop, electrónica, hip hop, trap, techno, house,
  industrial, lo-fi, reguetón, drum and bass, jazz, latina y synthwave.
- 16 pads con el pad 1 abajo a la izquierda, como en Maschine. **Mapa MIDI de Maschine** (pad 1 = C1, nota 36)
  o **General MIDI** (bombo 36, caja 38, charles 42...).
- 18 sonidos sintetizados y 29 acústicos grabados. Cada pad tiene afinación, caída, tono, golpe, saturación,
  nivel, panorama y grupo de corte.
- Secuenciador de 16 o 32 pasos con acentos y swing que sigue al DAW o toca con su propio tempo.
  **Arrastra el patrón como MIDI** a tu DAW o a Maschine.

### Instrumentos
- Con muestras libres: piano vertical, piano eléctrico FM, órgano de tubos, cuerdas, pizzicato, contrabajo,
  arpa, metales, flauta, saxo tenor, marimba, vibráfono, glockenspiel y kalimba.
- Modelados con cuerda física: bajo eléctrico con dedos o con púa, guitarra acústica, de nylon, eléctrica limpia y muteada.

### En todos los modos
- Teclado en pantalla y teclado del ordenador (A S D F G H J K; Z y X cambian de octava).
- Ventana escalable del 60 % al 140 %; en pantallas pequeñas empieza más pequeña.
- Limitador suave a la salida para no saturar nunca el DAW.

## Descargar

GitHub compila Windows y macOS en cada cambio. En la pestaña **Actions** del repositorio, abre la última
ejecución y descarga `Geminis-Windows` o `Geminis-macOS`. En Windows, copia las carpetas `.vst3` a
`C:\Program Files\Common Files\VST3\` y vuelve a buscar plugins en tu DAW. En Maschine:
Preferencias > Plug-ins > Rescan.

## Compilar

Hace falta CMake 3.22 o superior y un compilador de C++20 (Visual Studio 2022 en Windows, Xcode en macOS).
JUCE se descarga solo la primera vez.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Los plugins quedan en `build/GeminisFX_artefacts/Release/` y `build/GeminisInstrument_artefacts/Release/`.

## Pruebas de audio

```
cmake -B build -DGEMINIS_BUILD_TESTS=ON
cmake --build build --target GeminisDspTest GeminisInstTest
```

- `GeminisDspTest` pasa guitarra y voz de prueba por los 27 módulos y todos los presets de Geminis FX.
- `GeminisInstTest` toca los 66 presets del sinte, los 16 kits y los 20 instrumentos, y mide sus niveles.
  También comprueba que la rueda de modulación curva el timbre, que responden los 16 pads con los dos mapas
  MIDI y que el proyecto del DAW guarda y recupera todo. Con una carpeta como argumento escribe un WAV por caso.

## Estructura

- `Source/common/`: aspecto visual, pantalla de inicio, tipografías, filtros y reverb compartidos.
- `Source/fx/`: Geminis FX (módulos, rack, presets, procesador e interfaz).
- `Source/inst/`: Geminis Instrument (sintetizador y tablas de onda, batería y kits, instrumentos con muestras,
  presets, efectos, procesador e interfaz).
- `Assets/`: logotipo, tipografías libres y muestras CC0 (ver `Assets/samples/LICENCIAS.md`).
- `tools/make_samples.py`: prepara las muestras a partir de las librerías originales.
- `tests/`: pruebas de audio sin interfaz.
