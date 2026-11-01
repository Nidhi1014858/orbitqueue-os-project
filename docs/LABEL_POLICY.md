# OrbitQueue Synthetic Label Policy

## Purpose and disclaimer

The labels in `data/training_scenarios.csv` are synthetic teaching labels. They are not observations, commands, or telemetry from SomaiyaSat or any other spacecraft. The model is evaluated on how well it learns this policy, not on operational satellite performance.

## Inputs

Each scenario uses packet type, urgency (1 to 5), link quality (0 to 100), battery level (0 to 100), and data size (1 to 1,000,000 bytes). The generator uses a fixed seed so the dataset can be reproduced.

## Policy before noise

Rules are applied in this order:

1. Battery below 18 gives `REJECT` for urgency 1–2 and `DEFER` otherwise.
2. Link quality below 25 gives `REJECT` for urgency 1–2 and `DEFER` otherwise.
3. Battery below 32 or link quality below 42 gives `DEFER`.
4. A payload above 650,000 bytes with urgency at most 3 gives `DEFER`.
5. An SSTV payload above 450,000 bytes with link quality below 65 gives `DEFER`.
6. Urgency 4–5 with battery at least 35 and link quality at least 45 gives `TRANSMIT`.
7. Urgency 1–2 with battery below 45 or link quality below 50 gives `REJECT`.
8. Remaining scenarios give `TRANSMIT`.

About 35% of generated rows are sampled close to battery, link, and size boundaries. This overlap makes neighbouring scenarios less artificial than three completely separated groups.

## Label noise

After applying the policy, the generator independently changes about 5% of labels to one of the other two actions. With a fixed seed the changed rows are reproducible. Noise represents inconsistent or uncertain historical decisions and prevents the classifier from obtaining a misleading perfect score by simply copying thresholds.

## Model versus safety policy

The decision tree recommends `TRANSMIT`, `DEFER`, or `REJECT`. It is not the final safety authority. Nidhi's integrated router applies the hard override: TT&C and HOUSEKEEPING packets can never be deferred or rejected. Keeping the override outside the learned model makes the safety rule explicit, reviewable, and independent of prediction errors.

## Baseline

`ml/baseline.py` is deliberately labelled `BASELINE`. It uses fewer hand-written thresholds than the label policy. Both the baseline and decision tree are evaluated on the same stratified test rows, making their accuracy comparison reproducible and fair within this synthetic experiment.
