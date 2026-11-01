# Section 3: Synthetic Data and Machine Learning Router

## 3.1 Dataset source and disclaimer

All values are synthetic and are not SomaiyaSat telemetry. The files exist only for an educational simulation of downlink scheduling. No personal information or operational spacecraft information is used.

`ml/gen_data.py` uses Python's seeded pseudo-random generator. The standard command is:

```bash
python3 ml/gen_data.py --seed 42 --packets 60 --train 600
```

It creates 60 input packets, 600 labelled training scenarios, and a deliberately invalid validation file. The packet mix includes TT&C, HOUSEKEEPING, SSTV and VOICE. Several rows lie close to battery, link-quality and payload-size thresholds so the examples include boundary cases. Packet identifiers are unique in the valid input.

## 3.2 Validation and loaders

The C++ loader checks the exact header, field count, integer conversion, value ranges, known packet type, positive unique packet identifier, and non-empty fields. Invalid rows are skipped and written with their physical line number and reason to `logs/rejected_rows.log`. A missing file or invalid header is a clear fatal input error.

Linux evidence commands:

```bash
./build/csv_check data/satellite_data.csv
./build/csv_check data/satellite_data_bad.csv
./build/csv_check data/does_not_exist.csv
```

The first command should report zero rejected rows. The invalid file contains one valid row and six deliberate faults: unknown type, battery 140, negative size, empty field, duplicate identifier, and non-numeric urgency. Actual Linux output must be saved in `outputs/data_validation.txt`; it is not reproduced here until the team runs it.

## 3.3 Label creation

The full ordered policy is documented in `docs/LABEL_POLICY.md`. In summary, very low battery or a weak link leads to rejection or deferral, large low-urgency payloads are deferred, and urgent packets with sufficient power and link quality are transmitted. About 35% of scenarios are sampled around policy boundaries, and about 5% receive a reproducible alternative label. This overlap and noise prevent a perfectly separable toy dataset.

## 3.4 Decision tree model

The ML component uses a scikit-learn `DecisionTreeClassifier` with maximum depth 5, a fixed seed, and an 80/20 stratified split. Packet type is one-hot encoded; urgency, link quality, battery level and data size remain numeric. A decision tree was chosen because it is small, explainable in a viva, fast on ordinary hardware, and exportable as readable rules.

Training writes:

- `outputs/ml_metrics.txt`: accuracy, per-class precision and recall, confusion matrix, baseline accuracy, and feature importance.
- `outputs/tree_rules.txt`: readable decision rules.
- `outputs/decisions.csv`: packet recommendations with source `ML`.
- `outputs/decisions_baseline.csv`: threshold recommendations with source `BASELINE`.

Linux commands:

```bash
python3 -m pip install -r ml/requirements.txt
python3 ml/train_router.py --train data/training_scenarios.csv \
  --packets data/satellite_data.csv --out outputs/decisions.csv --seed 42
```

## 3.5 Baseline comparison and interpretation

The baseline uses a small hand-written set of battery, link, urgency and size thresholds. The training program evaluates it on exactly the same held-out rows as the tree. Real measurements from `outputs/ml_metrics.txt` must be inserted after the Linux run; no accuracy is claimed in this draft.

Even if the decision tree outperforms the baseline, the labels come from a synthetic policy. Accuracy therefore shows only that the tree can approximate that policy. It does not establish reliability on real satellite traffic.

## 3.6 Connection to the router

The model provides a recommendation through `outputs/decisions.csv`. Nidhi's router reads that file, then applies a non-ML safety override so TT&C and HOUSEKEEPING are never deferred or rejected. It then asks the resource manager whether CPU slots, buffer units, and transmitter slots can be granted safely. The model is not complete until this end-to-end connection is tested on Linux.
