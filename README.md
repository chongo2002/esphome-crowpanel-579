# ESPHome: Elecrow CrowPanel 5.79" E-Paper Display Driver

A custom ESPHome external component for the **Elecrow CrowPanel 5.79" e-paper display** (model DIS08792E). Supports the ESPHome lambda drawing API, LVGL, and **partial refresh**.

## Demo

Three-page interactive demo cycled with the MENU button (GPIO2). Full source in [`demo.yaml`](demo.yaml).

**Page 1 — Boot screen** (ESPHome logo drawn in primitives):

![Demo page 1 boot screen](docs/images/demo_page1_boot.jpg)

**Page 2 — Graphics demo** (circles, rectangles, lines):

![Demo page 2 graphics](docs/images/demo_page2_graphics.jpg)

**Page 3 — Typography showcase** (Roboto font sizes):

![Demo page 3 typography](docs/images/demo_page3_typography.jpg)

## Hardware

| Spec | Value |
|------|-------|
| Display | Elecrow CrowPanel 5.79" E-Paper (DIS08792E) |
| Resolution | 792 × 272 pixels, black & white |
| Driver ICs | Dual SSD1683 (one per half of the panel) |
| MCU | ESP32-S3 (tested on ESP32-S3-WROOM-1-N8R8, 8MB Flash, 8MB PSRAM) |
| Framework | ESP-IDF (not Arduino) |

### Pin Connections (CrowPanel default wiring)

| Signal | GPIO |
|--------|------|
| SPI CLK | GPIO12 |
| SPI MOSI | GPIO11 |
| CS | GPIO45 |
| DC | GPIO46 |
| RST | GPIO47 |
| BUSY | GPIO48 |
| PWR (optional) | GPIO7 |

### Optional Buttons / Rotary Encoder

The CrowPanel board also exposes:

| Input | GPIO |
|-------|------|
| MENU button | GPIO2 |
| EXIT button | GPIO1 |
| Rotary encoder UP | GPIO6 |
| Rotary encoder DOWN | GPIO4 |
| Rotary encoder CLICK | GPIO5 |

---

## Installation

### Option A — GitHub source (recommended)

```yaml
external_components:
  - source: github://samperk1/esphome-crowpanel-579
    components: [crowpanel_579]
```

### Option B — Local copy

Copy the `components/crowpanel_579` folder into your ESPHome `config/components/` directory:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [crowpanel_579]
```

---

## Basic YAML (lambda mode)

```yaml
spi:
  clk_pin: GPIO12
  mosi_pin: GPIO11

display:
  - platform: crowpanel_579
    id: my_display
    cs_pin: GPIO45
    dc_pin: GPIO46
    reset_pin: GPIO47
    busy_pin:
      number: GPIO48
      mode:
        input: true
        pulldown: true
    update_interval: never
    lambda: |-
      it.fill(Color::BLACK);  // clears screen to white (see color note below)
      it.print(10, 10, id(my_font), Color::WHITE, "Hello World");
      id(my_display).display();
```

> **Color convention:** This display uses an inverted convention in lambda mode.
> `Color::WHITE` draws **black ink**. `Color::BLACK` clears to **white paper**.
> Use `it.fill(Color::BLACK)` for a white background, then draw with `Color::WHITE`.

---

## LVGL

```yaml
spi:
  clk_pin: GPIO12
  mosi_pin: GPIO11

display:
  - platform: crowpanel_579
    id: my_display
    cs_pin: GPIO45
    dc_pin: GPIO46
    reset_pin: GPIO47
    busy_pin:
      number: GPIO48
      mode:
        input: true
        pulldown: true
    rotation: 90
    auto_clear_enabled: false

lvgl:
  displays:
    - my_display
  color_depth: 16
  bg_color: 0xFFFFFF
  on_draw_end:
    - lambda: "id(my_display).display();"
  pages:
    - id: main_page
      widgets:
        - label:
            text: "Hello from LVGL"
            align: CENTER
            text_color: 0x000000
```

> **LVGL notes:**
> - `color_depth: 16` is required — 1-bit mode is not supported.
> - `auto_clear_enabled: false` is required.
> - `rotation: 90` is required for correct LVGL orientation.
> - Call `display()` from `on_draw_end`, not from `update()`.
> - Use `bg_color: 0xFFFFFF` (white paper) and `text_color: 0x000000` (black ink).

---

## Partial Refresh

The driver supports windowed partial refresh via `partial_refresh(x, y, w, h)`. Only the specified region is updated, which is significantly faster than a full refresh for small changes.

```yaml
display:
  - platform: crowpanel_579
    id: my_display
    ...
    auto_clear_enabled: false
    update_interval: never
    lambda: |-
      // Draw your updated content into the buffer
      it.filled_rectangle(10, 10, 200, 60, COLOR_OFF);  // white background
      it.print(20, 20, id(my_font), COLOR_ON, "Updated!");
      // Then partial refresh just that region
      id(my_display).partial_refresh(10, 10, 200, 60);
```

**Important notes:**
- Coordinates are **physical** (no rotation applied) — 792 wide × 272 tall.
- `partial_refresh` uses physical coordinates even if your display has `rotation: 90` set.
- A full `display()` call before the first partial refresh establishes the baseline for the ghosting waveform. Ghosting builds up over many partial refreshes; do a full `display()` periodically (e.g. once per hour) to reset it.
- The refresh takes ~1–3 seconds and blocks the main loop.

### Partial refresh test photos

These photos show the 4-step partial refresh test (`partial_refresh_test.yaml`):

**Step 0 — Full refresh baseline** (seam line + LEFT/RIGHT labels):

![Step 0 baseline](docs/images/step0_baseline.jpg)

**Step 1 — Seam crossing** (partial refresh spanning both chips):

![Step 1 seam crossing](docs/images/step1_seam_crossing.jpg)

**Step 2 — Slave chip only** (physical right half):

![Step 2 slave only](docs/images/step2_slave_only.jpg)

**Step 3 — Master chip only** (physical left half):

![Step 3 master only](docs/images/step3_master_only.jpg)

---

## Optional: Power Pin

Add `power_pin: GPIO7` to force a hard power cycle of the display chip on boot. This clears any stuck BUSY state that a normal RST pulse can't recover from — useful after repeated OTA flashes or safe-mode cycles. Also required if your board controls display power for battery management.

```yaml
display:
  - platform: crowpanel_579
    ...
    power_pin: GPIO7
```

---

## Mirroring a Home Assistant Dashboard

You can have the panel display a screenshot of a Home Assistant Lovelace view, refreshed on a timer. Two pieces are involved:

1. **A render service** (runs on your HA host) — screenshots the dashboard with a headless browser, dithers it to a 1-bit 792×272 PNG, and writes it to `/config/www/dashboard.png` (HA serves anything in `/config/www/` at `http://<ha-host>:8123/local/...`, no extra web server needed).
2. **The ESPHome device** — fetches that PNG over HTTP with `online_image` and draws it with `it.image()` + `display()`.

A ready-made render service is included in [`ha_addon_dashboard_render/`](ha_addon_dashboard_render) (Home Assistant local add-on: Playwright + Pillow). The ESPHome side is just two blocks added to your existing device YAML (step 4 below) — there's no separate example file, since it needs to merge into whatever config you're already using to flash the device.

> **Why dithering happens on the HA side, not the ESP32 side:** ESPHome's `online_image` component with `type: BINARY` only does a hard per-pixel luminance threshold at runtime — it does not dither. Feeding it a plain grayscale screenshot produces visible banding on text and UI chrome. The render service pre-dithers with Floyd–Steinberg before saving, so the threshold ESPHome applies afterward is a no-op (every pixel is already pure black or white).

### 1. Build a Lovelace view sized for the panel

Create a dashboard view sized to 792×272 (or your desired aspect ratio — `online_image`'s `resize:` will letterbox rather than stretch a mismatched aspect ratio, so getting the render close to 792×272 up front looks best). A `type: panel` view removes Lovelace's default padding. If you want the header/sidebar hidden entirely for a clean render, the community [kiosk-mode](https://github.com/NemesisRE/kiosk-mode) integration (via HACS) is the standard way to do that per-dashboard.

### 2. Allow the render service to load the dashboard without a login prompt

The render service needs to open the dashboard URL without hitting HA's login page. The simplest supported way is a `trusted_networks` auth provider scoped to the add-on's internal Docker subnet (typically `172.30.32.0/23`), in your HA `configuration.yaml`:

```yaml
homeassistant:
  auth_providers:
    - type: trusted_networks
      trusted_networks:
        - 172.30.32.0/23
      allow_bypass_login: true
    - type: homeassistant   # keep normal login working for everyone else
```

### 3. Install the render add-on

Copy [`ha_addon_dashboard_render/`](ha_addon_dashboard_render) into the `addons` share on your HA OS host (e.g. via the Samba or SSH add-on), then in HA: **Settings → Add-ons → Add-on Store → ⋮ → Check for updates**, find it under **Local add-ons**, install, and configure:

| Option | Meaning |
|--------|---------|
| `dashboard_url` | Full URL to the Lovelace view, e.g. `http://homeassistant.local:8123/lovelace-epaper/0` |
| `width` / `height` | Screenshot viewport size — match your panel: `792` × `272` |
| `interval_seconds` | How often to re-render, e.g. `300` for every 5 minutes |
| `output_path` | Where to write the PNG — leave as `/config/www/dashboard.png` unless you have a reason to change it |

> **Architecture note:** the add-on's Playwright base image supports `amd64` and `aarch64`. Chromium under Playwright is not well supported on 32-bit ARM (e.g. older Raspberry Pi images) — run this on an amd64 or 64-bit ARM HA host.

### 4. Flash the ESPHome device

Add `http_request:` and `online_image:` blocks to your existing device YAML, and point your `display:` entry's redraw at the fetched image instead of (or in addition to) its normal lambda:

```yaml
http_request:
  verify_ssl: false   # set true if your HA instance has a valid TLS certificate

online_image:
  - id: dashboard_image
    url: "http://homeassistant.local:8123/local/dashboard.png"
    format: PNG
    type: BINARY
    update_interval: 5min
    on_download_finished:
      - then:
          - lambda: |-
              // This driver's lambda-mode color convention is inverted (see the
              // color note above): Color::WHITE draws black ink, Color::BLACK
              // draws white paper. Swap color_on/color_off here so bright
              // dashboard pixels come out as white paper instead of black ink.
              id(my_display).image(0, 0, id(dashboard_image), Color::BLACK, Color::WHITE);
              id(my_display).display();
    on_error:
      - logger.log: "Dashboard image download failed"
```

Set the `display:` entry's `update_interval:` to `never` — redraws are now driven by `on_download_finished` above, not a timer on the display itself. The panel does a full refresh every `online_image.update_interval` (5 minutes here).

> **Colors inverted (dashboard looks like a photo negative)?** You forgot the `Color::BLACK, Color::WHITE` arguments above — `it.image()` defaults to the normal ESPHome sense (bright = white), which this driver's `Color::WHITE`-means-black-ink convention flips.

Because every refresh is a full `display()` call rather than a `partial_refresh()`, there's no ghosting to manage — each refresh redraws the whole panel from a clean waveform.

---

## How It Works

The 5.79" panel uses **two SSD1683 driver chips** wired to the same SPI bus — one chip drives the left half, the other drives the right half. Both chips share CS, DC, RST, and BUSY lines. They are differentiated by their command sets:

- **Slave** (physical right half, columns 0–399): commands `0x91`, `0xA4`, `0xA6`, `0xC4/C5/CE/CF`
- **Master** (physical left half, columns 392–791): commands `0x11`, `0x24`, `0x26`, `0x44/45/4E/4F`

The 8-pixel overlap at columns 392–399 (byte 49 in the buffer) is shared between both chips and provides seam alignment.

The single framebuffer is 99 bytes × 272 rows = 26,928 bytes. On each `display()` call, bytes 0–49 per row go to the slave and bytes 49–98 per row (reversed) go to the master. A full refresh takes approximately 3 seconds.

### Buffer / color convention

| Buffer bit | Display |
|-----------|---------|
| `1` | White paper |
| `0` | Black ink |

In **lambda mode** the convention is inverted at the API level:
- `Color::WHITE` → black ink (bit cleared to 0)
- `Color::BLACK` → white paper (bit set to 1)

In **LVGL mode** (`draw_pixels_at`) the luminance threshold applies:
- RGB luminance ≥ 382 → white paper
- RGB luminance < 382 → black ink

---

## Known Limitations

- Coordinates in `partial_refresh()` are always physical (landscape) regardless of `rotation` setting.
- The `display()` and `partial_refresh()` calls block the main loop (~1–3 seconds) while the e-paper panel refreshes.
- Ghosting accumulates with repeated partial refreshes — periodically call `display()` for a full refresh to clear it.
- Tested and confirmed working on ESPHome **2026.3.x and 2026.4.x** with ESP-IDF framework only (not Arduino).

---

## Reference Documentation

- [SSD1683 Datasheet](docs/SSD1683_Datasheet.pdf) — driver IC used in this panel
- [CrowPanel 5.79" Hardware Reference](docs/CrowPanel_579_Hardware.pdf) — pin mapping, schematic
