# model — training and export

Notebooks that train the classifier, compare it against classical baselines, and export a
quantised model the ESP32 can run.

## Contents

| File | Role |
|---|---|
| `CNN1_2_3D.ipynb` | Main notebook: CNN 1D/2D/3D, classical baselines, INT8 export |
| `tran_model.ipynb` | Earlier CNN 1D pipeline that first produced a working TFLite model |
| `tran_model_v1.ipynb` | The previous iteration of the same notebook, kept for history |
| `modelTrain_Clean.ipynb` | First exploration: lag features and logistic regression |
| `analiza_vibratii.py` | Exploratory analysis: vibration score per class and FFT spectrum |
| `data_view.py` | Class balance and raw signal comparison |
| `run_2026_06/` | Session 2 artefacts (dataset and model) |

Artefacts in this directory belong to **session 1**. Files in `run_2026_06/` belong to
session 2 and are the ones currently deployed on `../firmware/`.

## Main notebook

`CNN1_2_3D.ipynb` runs end to end:

1. **Loading and splitting.** `load_and_split_merged` splits 80/20 **sequentially inside
   each class × speed group** rather than shuffling rows. Shuffling first would put
   neighbouring samples from the same recording on both sides of the split and inflate the
   score.
2. **Standardisation.** `StandardScaler` is fitted on the training set only; the resulting
   means and scales are what end up in `preprocessing.h`.
3. **Windowing.** 128 samples, stride 64. The window label is the majority label inside it.
4. **Models.** CNN 1D, then the same data reshaped for CNN 2D and CNN 3D, then the
   classical baselines: fixed amplitude threshold, K-Means, SVM and Random Forest on
   extracted features.
5. **Quantisation.** INT8 conversion with a representative dataset, exported as a C array
   in `model_cnn1d_int8.h` (11,560 bytes) together with the scaler in `preprocessing.h`.

The dataset path is set once, in the first cell:

```python
DATASET = 'dataset_final.csv'
# DATASET = 'run_2026_06/dataset_final.csv'
```

`dataset_final.csv` is not tracked in git. Regenerate it from the raw recordings first —
see `../colectare-date/README.md`.

## Results

Session 1, 6,727 training windows and 1,681 test windows:

| Method | Accuracy |
|---|---|
| Fixed amplitude threshold | 0.28 |
| K-Means on extracted features | 0.85 |
| SVM | 1.00 |
| Random Forest | 1.00 |
| CNN 1D / 2D / 3D | 1.00 |

CNN 1D: 3,093 parameters, 11,560 bytes after INT8 quantisation.

## Exporting to the board

The notebook writes `model_cnn1d_int8.h` and `preprocessing.h`. Copy **both** into
`../firmware/include/` — they are a matched pair. A model from one session combined with a
scaler from the other produces confident but wrong predictions, with nothing in the logs
to indicate it. The `DIAGNOSTIC_MODE` output in the firmware exists to catch exactly this.

## Known issue

`modelTrain_Clean.ipynb` still loads its data from an absolute Windows path on another
machine, with `;` as the delimiter. It is kept as a record of the first attempt and is not
part of the working pipeline; it will not run as-is.
