# Muestras incluidas en Geminis Instrument

Todas las muestras de esta carpeta son de dominio público (CC0 1.0 Universal). Se pueden usar,
modificar y distribuir sin pedir permiso ni pagar, también en música comercial. Aun así, damos el crédito:

- **VSCO 2 Community Edition** (Versilian Studios, Sam Gossner y colaboradores), CC0 1.0.
  https://github.com/sgossner/vsco-2-ce
  De aquí salen el piano vertical, el órgano, las cuerdas (arco y pizzicato), el contrabajo, el arpa,
  los metales, la flauta, el saxo tenor, la marimba, el vibráfono, el glockenspiel y parte de la batería acústica.

- **Versilian Community Sample Library (VCSL)** (Versilian Studios y colaboradores), CC0 1.0.
  https://github.com/sgossner/vcsl
  De aquí salen el piano eléctrico FM, la kalimba y la percusión de mano y latina (congas, bongós, cajón,
  pandereta, maracas, claves, cencerro, triángulo, yunque, tambor de freno...).

Las muestras se han recortado, pasado a mono, normalizado y convertido a OGG con `tools/make_samples.py`.
Los instrumentos sostenidos llevan un bucle con fundido de 120 ms hecho por ese mismo script.

Los sonidos sintetizados de la batería, el sintetizador y los bajos y guitarras modelados (cuerda física)
no usan muestras: se generan en tiempo real con código propio.
