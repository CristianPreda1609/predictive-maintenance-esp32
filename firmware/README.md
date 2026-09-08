# firmware — on-device inference (final version)

Real-time classification of the motor state on the ESP32, plus a web dashboard served by
the board over its own WiFi access point. This is the version the thesis describes as
final; the earlier monolithic one is kept in `../firmware-legacy/`.

## Architecture

`main.cpp` is wiring only — 41 lines, no logic. Everything else lives in modules that do
not share globals:

```
main.cpp        setup(): realtime_init() -> web_init() -> start tasks
  |
  +-- config.{h,cpp}   compile-time constants; the single definition of
  |                    class_names, feature_names and the allowed PWM list
  |
  +-- realtime.{h,cpp} MPU6050 + scaler + CNN1D + prediction state   (Core 1)
  |
  +-- web.{h,cpp}      SoftAP + HTTP server                          (Core 0)
        +-- web_page.h   the dashboard HTML, served from PROGMEM
```

Generated artefacts (`model_cnn1d_int8.h`, `preprocessing.h`) are included only by
`realtime.cpp`, so there is exactly one copy of the weights and of the scaler in the
binary.

### Concurrency

Two FreeRTOS tasks are pinned to Core 1 and one to Core 0:

| Task | Core | Priority | Role |
|---|---|---|---|
| `SensorTask` | 1 | 3 | Reads the MPU6050 at exactly 100 Hz using `vTaskDelayUntil` |
| `AITask` | 1 | 2 | Fills the 128-sample window, runs the CNN, publishes the result |
| `WebTask` | 0 | 1 | Polls the HTTP server, yields 2 ms per iteration |

Sampling is decoupled from inference through a 256-slot queue (about 2.5 s of buffer), so
a slow `Invoke()` cannot distort the sampling rate. `xQueueSend` is non-blocking: under
saturation one sample is dropped rather than stalling acquisition.

Two mutexes guard the shared state. `i2c_mutex` serialises access to the I2C bus.
`state_mutex` protects the published prediction and the scope ring buffer. All internal
state sits in an anonymous namespace inside `realtime.cpp`, so the only way the web layer
can reach it is through the public API:

```c
bool realtime_init();
void realtime_start_tasks();
bool realtime_snapshot(StateSnapshot* out, TickType_t wait);  // atomic copy
bool realtime_set_pwm(int v);                                 // whitelist-checked
int  realtime_get_pwm();
```

`realtime_snapshot` copies every field under one mutex hold, so the dashboard can never
render a new class next to the previous probabilities. It also reorders the scope ring
buffer chronologically, so the web layer only serialises and does not reimplement storage
logic.

### Signal path

1. `SensorTask` reads six channels and pushes them on the queue, also feeding the scope
   ring buffer used by the dashboard.
2. `AITask` accumulates 128 samples, then applies a gyroscope idle filter: if all three
   gyro axes stay within 0.04 over the whole window, the class is reported as `Repaus`
   without invoking the network. This is more stable than letting the CNN decide on a
   near-flat signal.
3. Otherwise each value is standardised with `(x - kInputMean[f]) / kInputScale[f]`, then
   quantised to INT8 with the scale and zero point read from the model at boot.
4. After each prediction the last 64 samples are shifted to the front, giving the 50%
   overlap used during training.

### Safety details worth knowing

- **PWM whitelist.** `realtime_set_pwm` accepts only the speeds present in the training
  distribution (0, 160, 180, 200, 220, 240, 255) and rejects everything else with HTTP 422.
  Any other speed would put the motor in a regime the model has never seen.
- **I2C recovery.** The motor's electrical noise can freeze the bus. If a reading fails, or
  if `ax` repeats exactly for more than 20 cycles, the firmware reinitialises I2C and the
  MPU by itself instead of requiring a manual reset.
- **Sensor settings are not free parameters.** ±16 G, ±500 °/s and the 21 Hz filter must
  match the acquisition firmware exactly; any change shifts the input away from the scaler.

## Build and flash

```
pio run -t upload
pio device monitor
```

The board starts an access point `Mentenanta-ESP32` (password `vibratii2026`). The
dashboard is at `http://192.168.4.1` and shows the predicted class, per-class
probabilities, inference latency and a six-channel scope.

Endpoints: `GET /` (page), `GET /data` (JSON state), `GET /pwm?v=<speed>` (set speed).

`DIAGNOSTIC_MODE` at the top of `realtime.cpp` is set to `1`. It prints, for every window,
the observed mean per channel against the training mean. Use it to catch a sensor that was
remounted differently from acquisition; set it to `0` for a clean demo.

## Updating the model

Copy `model_cnn1d_int8.h` and `preprocessing.h` from the training run into `include/`.
Both files must come from the **same** run — see the provenance table in the root README.
This directory currently carries the session 2 artefacts (`kInputMean[0] = 0.002488`).
