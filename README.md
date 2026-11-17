# Géminis Suite

Suite de audio propia, con diseño y sonidos originales. Tiene dos partes:

- **Géminis**, el programa. Al abrirlo, una pantalla de inicio animada con el emblema de Géminis pregunta
  qué vas a hacer: Guitarra, Voz, Sintetizador, Batería o Instrumentos. Lo que elijas se abre en la misma
  ventana, y el botón Inicio vuelve a esa pantalla para cambiar de modo.
- **Dos plugins** para cualquier DAW (VST3 en Windows, macOS y Linux, y AU en macOS): Cubase, Logic,
  Ableton, Reaper, FL Studio o Maschine. Son dos porque los DAW separan los efectos de los instrumentos.

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
- **Afinador** a pantalla completa con el botón Afinador: nota grande, aguja de cents, la cuerda más cercana
  (afinación estándar) y el nivel de entrada. Silencia la salida mientras afinas, como un pedal afinador.
- **Voz:** ecualizador vocal, de-esser, compresor, saturación de cinta, doblador, afinación, modulación,
  delays y reverbs. La **afinación** lleva la voz a la nota de la escala elegida sin cambiar su timbre: con
  velocidad 1 suena el efecto robótico y con valores medios corrige de forma natural. Añade unos 25 ms de
  latencia, que el DAW compensa. El **doblador** suma dos tomas, una a cada lado, con su propio destiempo,
  afinación y timbre.
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
- 16 pads con el pad 1 abajo a la izquierda, como en Maschine, y los 16 kits con la misma distribución:

  | Fila | Pads | Qué lleva |
  |---|---|---|
  | 4 (arriba) | 13-16 | Melodía (nota pulsada), acorde, voz y efecto |
  | 3 | 9-12 | Toms, cencerros, bloques o percusión secundaria (crash y ride en los kits acústicos) |
  | 2 | 5-8 | Bombo secundario (o 808), caja secundaria (aro), otro charles (de pedal) y percusión ligera (palmas) |
  | 1 (abajo) | 1-4 | 1 bombo · 2 caja · 3 charles cerrado · 4 charles abierto |

- **Mapa MIDI de Maschine** (pad 1 = C1, nota 36) o **General MIDI** (bombo 36, caja 38, charles 42 y 46...).
  El mapa de Maschine sigue la octava que llegue: la Maschine en modo MIDI, sin su software, manda los pads
  desde otra octava, y basta con tocar cualquiera de los pads 1 a 12 para que el programa la siga. El piloto
  **MIDI** de la batería se enciende con cada nota que llega.
- **Tocar con la Maschine MK3 en el programa Géminis:** cierra el software de Maschine y pulsa **SHIFT + CHANNEL
  (MIDI)** en la Maschine para ponerla en modo MIDI. Sus entradas MIDI se activan solas.
- 18 sonidos de batería sintetizados, 29 acústicos grabados y 3 melódicos (nota pulsada, acorde menor con
  séptima y voz cantando «aa»), en do y transportables con la afinación. Cada pad tiene afinación, caída, tono,
  golpe, saturación, nivel, panorama y grupo de corte.
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
ejecución y baja hasta las descargas.

- **Windows:** descarga `Geminis-Instalador-Windows`, descomprímelo y abre `Geminis-Suite-…-Instalador.exe`.
  Instala el programa Géminis con su acceso en el menú Inicio (y en el escritorio), copia los plugins a
  `C:\Program Files\Common Files\VST3\` y aparece en «Aplicaciones instaladas» para desinstalarlo.
  Como el instalador no está firmado, Windows puede avisar con «Windows protegió su PC»: pulsa
  «Más información» y luego «Ejecutar de todas formas».
- **Sin instalador:** `Geminis-Windows` trae los mismos archivos sueltos. Copia las carpetas `.vst3` a
  `C:\Program Files\Common Files\VST3\`; `Geminis.exe` funciona desde cualquier carpeta.
- **macOS:** `Geminis-macOS`.

Después, vuelve a buscar plugins en tu DAW. En Maschine: Preferencias > Plug-ins > Rescan.

### El programa Géminis

- Los ajustes de audio están en el botón **Ajustes de audio**, arriba a la izquierda: tarjeta de sonido,
  frecuencia, búfer y entradas MIDI. Los controladores MIDI, como Maschine, se activan solos al conectarlos.
- La entrada de audio empieza silenciada para que no se acople. En Guitarra y Voz aparece un aviso con el
  botón «Activar entrada». Usa auriculares si tocas con el micro y los altavoces del portátil, porque se
  acoplan enseguida. El programa recuerda lo que elijas y también tus sonidos de cada modo.
- Empieza a **44100 Hz**. «Windows Audio» ofrece 44100, 48000, 88200 y 96000 Hz aunque Windows
  trabaje a otra frecuencia, porque el programa convierte la frecuencia al abrir la tarjeta.
- **Tarjetas de sonido con driver ASIO** (Maschine MK3, Focusrite, Audient...): en «Tipo de dispositivo» elige
  **ASIO** y en «Dispositivo» tu tarjeta. Es la opción con menos latencia. Si no sale, instala el driver del
  fabricante (en Maschine, desde Native Access). Muchos drivers ASIO solo dejan usar la tarjeta a un programa
  a la vez: si no se abre, cierra el software de Maschine u otro programa de audio.
- Con ASIO, Windows no toca el volumen del programa. Por eso Géminis sigue él mismo el **volumen y el silencio de
  Windows** (las teclas de volumen del teclado y el icono del altavoz) de la salida por defecto. Se puede quitar en
  Ajustes de audio con la casilla «Con ASIO, seguir el volumen y el silencio de Windows».
- Si abres una batería guardada con una versión anterior, los pads vuelven a la distribución de Maschine.

## Compilar

Hace falta CMake 3.22 o superior y un compilador de C++20 (Visual Studio 2022 en Windows, Xcode en macOS).
JUCE se descarga solo la primera vez.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Los plugins quedan en `build/GeminisFX_artefacts/Release/` y `build/GeminisInstrument_artefacts/Release/`,
y el programa en `build/GeminisApp_artefacts/Release/`.

## Pruebas de audio

```
cmake -B build -DGEMINIS_BUILD_TESTS=ON
cmake --build build --target GeminisDspTest GeminisInstTest
```

- `GeminisDspTest` pasa guitarra y voz de prueba por los 27 módulos y todos los presets de Geminis FX, y comprueba
  que cada efecto hace lo que dice: el afinador acierta a menos de 1 cent, la afinación corrige una voz
  desafinada, el doblador abre el estéreo y las modulaciones mueven el sonido de forma clara.
- `GeminisInstTest` toca los 66 presets del sinte, los 16 kits y los 20 instrumentos, y mide sus niveles.
  También comprueba que la rueda de modulación curva el timbre, que responden los 16 pads con los dos mapas
  MIDI, que los 16 kits siguen la distribución de Maschine, que «Windows Audio» se abre a 44100 Hz y que el
  proyecto del DAW guarda y recupera todo. Con una carpeta como argumento escribe un WAV por caso.

## Estructura

- `Source/common/`: aspecto visual, pantalla de inicio, tipografías, filtros y reverb compartidos.
- `Source/fx/`: Geminis FX (módulos, rack, presets, procesador e interfaz).
- `Source/inst/`: Geminis Instrument (sintetizador y tablas de onda, batería y kits, instrumentos con muestras,
  presets, efectos, procesador e interfaz).
- `Source/app/`: el programa Géminis, que reúne los dos plugins tras la pantalla de inicio común.
- `installer/`: instalador de Windows (Inno Setup).
- `Assets/`: logotipo, tipografías libres y muestras CC0 (ver `Assets/samples/LICENCIAS.md`).
- `tools/make_samples.py`: prepara las muestras a partir de las librerías originales.
- `tests/`: pruebas de audio sin interfaz.
