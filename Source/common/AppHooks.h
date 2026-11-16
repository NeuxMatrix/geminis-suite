#pragma once
#include <functional>

namespace geminis
{
/** En el programa Géminis, el botón Inicio de cada módulo vuelve a la pantalla
    de inicio común, donde se elige entre los cinco modos. Dentro de un DAW nadie
    lo asigna y cada plugin muestra su propia pantalla de inicio. */
inline std::function<void()>& homeHandler()
{
    static std::function<void()> handler;
    return handler;
}
} // namespace geminis
