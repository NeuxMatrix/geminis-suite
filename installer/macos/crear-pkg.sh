#!/bin/bash
# Crea el instalador .pkg de macOS: copia los plugins a las carpetas que leen
# Maschine, Logic, Ableton, etc. y el programa Géminis a Aplicaciones.
#   /Library/Audio/Plug-Ins/VST3        -> Geminis FX.vst3, Geminis Instrument.vst3
#   /Library/Audio/Plug-Ins/Components  -> Geminis FX.component, Geminis Instrument.component
#   /Applications                       -> Geminis.app
# Uso: installer/macos/crear-pkg.sh <carpeta build> <versión> <salida.pkg>
set -euo pipefail
BUILD=${1:-build}
VERSION=${2:?falta la versión}
SALIDA=${3:-installer/Output/Geminis-Suite-$VERSION-macOS.pkg}
AQUI=$(cd "$(dirname "$0")" && pwd)
TMP=$(mktemp -d)
RAIZ="$TMP/raiz"

mkdir -p "$RAIZ/Library/Audio/Plug-Ins/VST3" "$RAIZ/Library/Audio/Plug-Ins/Components" "$RAIZ/Applications"
cp -R "$BUILD"/Geminis*_artefacts/Release/VST3/*.vst3 "$RAIZ/Library/Audio/Plug-Ins/VST3/"
cp -R "$BUILD"/Geminis*_artefacts/Release/AU/*.component "$RAIZ/Library/Audio/Plug-Ins/Components/"
cp -R "$BUILD"/GeminisApp_artefacts/Release/Geminis.app "$RAIZ/Applications/"

# Firma ad-hoc: sin ella macOS puede negarse a cargar los plugins en algunos Macs.
for b in "$RAIZ"/Library/Audio/Plug-Ins/*/*.* "$RAIZ"/Applications/Geminis.app; do
  codesign --force --deep --sign - "$b"
done

# Que el instalador no "reubique" los paquetes si encuentra otra copia en el disco:
# siempre deben quedar en las carpetas de arriba.
pkgbuild --analyze --root "$RAIZ" "$TMP/componentes.plist"
plutil -convert xml1 "$TMP/componentes.plist"
i=0
while /usr/libexec/PlistBuddy -c "Print :$i" "$TMP/componentes.plist" > /dev/null 2>&1; do
  # No todas las entradas traen la clave: se borra si está y se añade siempre
  /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$TMP/componentes.plist" > /dev/null 2>&1 || true
  /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$TMP/componentes.plist"
  i=$((i + 1))
done

pkgbuild --root "$RAIZ" --component-plist "$TMP/componentes.plist" \
  --identifier com.geminisaudio.suite --version "$VERSION" \
  --install-location / --scripts "$AQUI/scripts" "$TMP/geminis.pkg"

mkdir -p "$(dirname "$SALIDA")"
productbuild --distribution "$AQUI/distribution.xml" --package-path "$TMP" \
  --resources "$AQUI/recursos" --version "$VERSION" "$SALIDA"
rm -rf "$TMP"
pkgutil --payload-files "$SALIDA" | grep -E '\.(vst3|component|app)$'
echo "Instalador creado: $SALIDA"
