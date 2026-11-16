# Cabeceras del SDK de ASIO

`asio.h`, `asiosys.h` e `iasiodrv.h` son del SDK de ASIO 2.3 de Steinberg, copiadas tal cual de JUCE 8.0.15
(`modules/juce_audio_devices/native/asio`). Steinberg las publica con doble licencia: la suya propia o la
GPL versión 3 (ver `LICENSE.txt`). Géminis las usa bajo la GPLv3.

Solo las usa el programa Géminis en Windows (`JUCE_ASIO=1`), para que aparezcan las tarjetas de sonido con
driver ASIO, como la Maschine MK3. Los plugins no las necesitan: el audio lo pone el DAW.
