#!/usr/bin/env bash
# Lo ejecuta GitHub Actions en el VPS, como usuario deploy, después de copiar
# esta carpeta a /opt/neux/apps/<app>/.
#
#   Uso: arrancar.sh <app> <tag-de-imagen> <usuario-github>
#   El token para leer la imagen de ghcr.io entra por stdin.
set -euo pipefail

APP="$1"
TAG="$2"
USUARIO="$3"
DIR="/opt/neux/apps/$APP"

# Solo un despliegue a la vez en el VPS. Cada app se despliega desde su propio
# repositorio, y si dos coinciden Docker falla al descargar las imágenes
# ("lease does not exist") y un logout pisa el login del otro.
exec 9>/opt/neux/despliegue.lock
flock -w 600 9

cd "$DIR"

echo "==> Descargando la imagen $TAG"
docker login ghcr.io -u "$USUARIO" --password-stdin
export TAG
for intento in 1 2 3; do
  docker compose pull -q && break
  if [ "$intento" -eq 3 ]; then
    echo "No se pudo descargar la imagen tras 3 intentos" >&2
    docker logout ghcr.io >/dev/null
    exit 1
  fi
  echo "    fallo al descargar, reintento $intento en 15 s"
  sleep 15
done
docker logout ghcr.io >/dev/null

echo "==> Arrancando"
docker compose up -d --remove-orphans

echo "==> Actualizando el dominio en Caddy"
cp "$APP.caddy" /opt/neux/caddy/sites/
docker exec caddy caddy reload --config /etc/caddy/Caddyfile

# Imágenes antiguas fuera, que el disco es de 40 GB
docker image prune -f >/dev/null

docker compose ps
