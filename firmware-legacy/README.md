# firmware-legacy — first inference version

The first working version of on-device inference, kept because it documents an earlier
stage of the project and because it contains the OLED variant that the modular rewrite in
`../firmware/` dropped.

Unlike `../firmware/`, the logic here is not split into modules: each variant is a single
`.cpp` file holding configuration, sensor handling, preprocessing, inference and output
together. That is the main difference between the two directories, and the reason both are
kept.

## The three variants

`src/` contains three files, each with its own `setup()` and `loop()`. Only one can be
compiled at a time, selected through `build_src_filter` in `platformio.ini`:

| File | Lines | What it does |
|---|---|---|
| `main.cpp` | 313 | Inference with serial output. Two FreeRTOS tasks, 100 Hz acquisition, automatic I2C recovery. This is the default build. |
| `main_with_web.cpp` | 467 | Adds an SSD1306 OLED display and an asynchronous web server with a captive DNS |
| `mainC.cpp` | 252 | Earlier single-loop attempt, without task separation |

The default is `main.cpp`:

```ini
build_src_filter = +<*> -<main_with_web.cpp> -<mainC.cpp>
```

To build the OLED and web variant instead:

```ini
build_src_filter = +<*> -<main.cpp> -<mainC.cpp>
```

The libraries needed by `main_with_web.cpp` (`ESPAsyncWebServer`, `AsyncTCP`,
`Adafruit GFX`, `Adafruit SSD1306`) are declared in `platformio.ini`; they are unused by
the default build and the linker leaves them out.

## Build and flash

```
pio run -t upload
pio device monitor
```

`board_build.partitions = huge_app.csv` is required — the default partition table does not
leave enough application space for the TFLite Micro runtime.

## Model

This directory carries the **session 1** artefacts (`kInputMean[0] = 0.163085`), which
match `../colectare-date/date_raw_2025_12/`. They are not interchangeable with the ones in
`../firmware/`; see the provenance table in the root README.
