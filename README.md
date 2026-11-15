# Geminis Suite

Suite de audio propia: al abrirse pregunta si vas a tocar la guitarra, trabajar una voz o tocar un instrumento MIDI.
Este repositorio contiene el MVP: el plugin **Geminis FX** con la pantalla de inicio y el modo Guitarra.

## Qué incluye el MVP

- Pantalla de inicio con los tres modos (Voz e Instrumentos MIDI aparecen como próximos).
- Modo Guitarra: puerta de ruido, compresor, pedal de saturación Impulso, tres amplificadores
  (Cristal, Britannia, Titán), cuatro pantallas y carga de respuestas a impulso propias, trémolo, delay y reverb.
- Seis presets de fábrica, selector de entrada, medidores y limitador de seguridad a la salida.
- Formatos: VST3 y aplicación independiente en Windows, macOS y Linux, y AU en macOS.

Todos los modelos son diseños originales con nombres propios. No se usan marcas de terceros.

## Compilar

Hace falta CMake 3.22 o superior y un compilador de C++20 (Visual Studio 2022 en Windows, Xcode en macOS).
JUCE se descarga solo la primera vez.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

El resultado queda en `build/GeminisFX_artefacts/Release/`. En Windows, copia `Geminis FX.vst3` a
`C:\Program Files\Common Files\VST3\` y vuelve a escanear los plugins en tu DAW.

En GitHub, el flujo `.github/workflows/build.yml` compila Windows y macOS en cada cambio y deja los plugins
como descarga en la pestaña Actions.

## Prueba de audio

```
cmake -B build -DGEMINIS_BUILD_TESTS=ON
cmake --build build --target GeminisDspTest
```

`GeminisDspTest` pasa notas de prueba por cada amplificador, comprueba que la salida es válida y que no
supera 0 dBFS, y escribe un WAV por caso.

## Estructura

- `Source/dsp/` contiene el audio: `GuitarChain.h` es la cadena de guitarra y `Biquad.h` los filtros.
- `Source/PluginProcessor.*` conecta parámetros, presets y estado con el DAW.
- `Source/PluginEditor.*` es la interfaz: pantalla de inicio, módulos y medidores.
