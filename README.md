# Perspective für Pebble Time 2

[Deutsch](#deutsch) · [English](#english)

![Perspective auf der Pebble Time 2 / Perspective on Pebble Time 2](docs/screenshot.png)

## Deutsch

Eine Anpassung des ursprünglichen **Perspective**-Ziffernblatts von Jnm
für die Pebble Time 2.

### Funktionen

- Dreidimensionale Punktwolken-Anzeige der Uhrzeit
- Gefaltete Datumsebene oberhalb der Uhrzeit
- Gefaltete Akkuanzeige unterhalb der Uhrzeit
- Bewegungsgesteuerte Perspektive mit zweifacher visueller Verstärkung
- Sichere Begrenzung der visuellen Neigung auf 77 Grad
- Schüttelgeste zum Umschalten zwischen schwarzem und weißem Design
- Dauerhafte Speicherung des gewählten Designs
- Ausschließlich für die Pebble Time 2 (`emery`) gebaut

### Bedienung

Bewege oder neige die Uhr, um die Perspektive zu verändern. Durch Schütteln
der Uhr wird zwischen schwarzem und weißem Design umgeschaltet.

### Bauen und installieren

```bash
pebble clean
pebble build
pebble install --phone PHONE_IP
```

### Danksagung

Ursprüngliches Ziffernblatt: **Jnm**

Anpassung für die Pebble Time 2 und zusätzliche Funktionen: **Maru**

---

## English

A Pebble Time 2 adaptation of the original **Perspective** watchface by Jnm.

### Features

- Three-dimensional point-cloud time display
- Folded date plane above the time
- Folded battery percentage below the time
- Motion-controlled perspective with a 2× visual response
- Safe 77-degree visual tilt limit
- Shake gesture to switch between black and white themes
- Persistent theme selection
- Built exclusively for Pebble Time 2 (`emery`)

### Controls

Move or tilt the watch to change the perspective. Shake the watch to invert
the black-and-white theme.

### Build and install

```bash
pebble clean
pebble build
pebble install --phone PHONE_IP
```

### Credits

Original watchface: **Jnm**

Pebble Time 2 adaptation and additional features: **Maru**
