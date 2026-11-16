# Web de Géminis Suite

Página estática en `web/`, publicada en https://geminis.neuxmatrix.es en el VPS de NeuxMatrix,
igual que el resto de webs del grupo (Caddy del VPS delante, contenedor con Caddy que sirve los ficheros).

- `index.html`: toda la página, con estilos y scripts dentro.
- `img/`: capturas (WebP) y emblema. `audio/`: demos en MP3.
- `Dockerfile` y `deploy/`: imagen, compose, arranque y sitio de Caddy.

El flujo `.github/workflows/desplegar-web.yml` la despliega en cada push a `main` que toque `web/`.
Necesita el secret `VPS_SSH_KEY` (la misma clave del usuario `deploy` que usan los otros repos) y un
registro DNS A `geminis` → `132.243.166.66` en Hostinger.
