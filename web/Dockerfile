# La web es estática: Caddy sirve los ficheros tal cual.
FROM caddy:2-alpine
COPY deploy/Caddyfile /etc/caddy/Caddyfile
COPY index.html /srv/
COPY img /srv/img
COPY audio /srv/audio
EXPOSE 8080
