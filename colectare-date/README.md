# colectare-date — data acquisition

Everything used to produce the training data: the firmware that streams sensor readings,
the scripts that record and merge them, and the raw recordings themselves.

## Contents

| Path | Role |
|---|---|
| `firmware/` | ESP32 sketch that streams the six channels over serial at 100 Hz |
| `date_logger.py` | Records one CSV from the serial port (session 1) |
| `date_logger_v2.py` | Same, as used for session 2 |
| `date_master.py` | Merges the raw CSVs into a single labelled dataset |
| `date_raw_2025_12/` | Session 1 recordings, 25 files |
| `date_raw_2026_06/` | Session 2 recordings, 25 files |

The two sessions are **different recordings**, not copies of each other — the sensor was
remounted between them, so every file differs and each session has its own scaler. See the
provenance table in the root README before mixing them.

## Acquisition firmware

`firmware/src/main.cpp` is deliberately minimal, 64 lines. It configures the MPU6050 at
±16 G, ±500 °/s with the 21 Hz filter, drives the motor at a fixed PWM, and prints one
comma-separated line every 10 ms:

```
ax,ay,az,gx,gy,gz,temp
```

The `delay(10)` in `loop()` is what sets the 100 Hz rate that the whole pipeline assumes.
The sensor settings here are the reference the inference firmware must match.

```
cd firmware
pio run -t upload
```

Close the serial monitor before running the logger — the port cannot be opened twice.

## Recording

```
python date_logger.py
```

The configuration sits at the top of the script and is edited before each recording:

```python
SERIAL_PORT = "COM6"
FILENAME = "fixare_slaba_255.csv"
CURRENT_LABEL = 4
DURATION_SECONDS = 303
```

The script writes into `date/`, appends the label column, and stops on its own after the
configured duration.

**The file name carries the metadata.** `date_master.py` parses the condition and the
motor speed straight out of it, so `fixare_slaba_255.csv` means condition `fixare` at
PWM 255. Renaming a file changes how it is labelled.

Full protocol: five faults × six speeds (160, 180, 200, 220, 240, 255), roughly five
minutes each, plus one `zgomot.csv` recorded with the motor stopped.

## Merging into a dataset

```
python date_master.py
```

Reads every CSV in `MASTER_DIRECTORY`, derives `label` and `pwm_speed` from each file name,
concatenates everything and writes `dataset_final.csv`:

| Label | Name in file | Meaning |
|---|---|---|
| 0 | `zgomot` | Motor stopped, noise floor |
| 1 | `normal` | Healthy |
| 2 | `dezechilibru` | Imbalance |
| 3 | `frecare` | Friction |
| 4 | `fixare` | Loose mounting |

The default is session 1. For session 2, change one line:

```python
MASTER_DIRECTORY = "date_raw_2026_06"
```

Expected output — this doubles as a check that nothing was lost:

| Session | Rows | MD5 of `dataset_final.csv` |
|---|---|---|
| `date_raw_2025_12` | 538,327 | `1f63d437a6c433832742a149f19dad0e` |
| `date_raw_2026_06` | 324,064 | `9b6dd13596c961b6dffb4c00d4c689b2` |

The generated `dataset_final.csv` is not tracked in git; it is reproduced from the raw
recordings. Copy it next to the notebook that will use it — `../model/` for session 1,
`../model/run_2026_06/` for session 2.
