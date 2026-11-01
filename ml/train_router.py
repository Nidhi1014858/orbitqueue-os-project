#!/usr/bin/env python3
"""Train an explainable decision tree and create packet decisions."""

import argparse
from pathlib import Path

import pandas as pd
from sklearn.compose import ColumnTransformer
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix
from sklearn.model_selection import train_test_split
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import OneHotEncoder
from sklearn.tree import DecisionTreeClassifier, export_text

from baseline import baseline_action, predict_packets


FEATURES = ["type", "urgency", "link_quality", "battery_level", "data_size"]
LABEL_ORDER = ["TRANSMIT", "DEFER", "REJECT"]


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--train", type=Path, default=Path("data/training_scenarios.csv"))
    parser.add_argument("--packets", type=Path, default=Path("data/satellite_data.csv"))
    parser.add_argument("--out", type=Path, default=Path("outputs/decisions.csv"))
    parser.add_argument("--baseline-out", type=Path, default=Path("outputs/decisions_baseline.csv"))
    parser.add_argument("--metrics", type=Path, default=Path("outputs/ml_metrics.txt"))
    parser.add_argument("--rules", type=Path, default=Path("outputs/tree_rules.txt"))
    parser.add_argument("--seed", type=int, default=42)
    return parser.parse_args()


def main():
    args = parse_args()
    training = pd.read_csv(args.train, comment="#")
    packets = pd.read_csv(args.packets, comment="#")

    missing_train = set(FEATURES + ["label"]) - set(training.columns)
    missing_packets = set(FEATURES + ["packet_id"]) - set(packets.columns)
    if missing_train or missing_packets:
        raise ValueError(f"Missing columns: training={sorted(missing_train)}, packets={sorted(missing_packets)}")

    x_train, x_test, y_train, y_test = train_test_split(
        training[FEATURES], training["label"], test_size=0.20,
        random_state=args.seed, stratify=training["label"]
    )

    preprocess = ColumnTransformer([
        ("type", OneHotEncoder(handle_unknown="ignore", sparse_output=False), ["type"]),
        ("numbers", "passthrough", ["urgency", "link_quality", "battery_level", "data_size"]),
    ])
    tree = DecisionTreeClassifier(max_depth=5, random_state=args.seed, class_weight="balanced")
    model = Pipeline([("preprocess", preprocess), ("tree", tree)])
    model.fit(x_train, y_train)

    ml_predictions = model.predict(x_test)
    baseline_predictions = x_test.apply(baseline_action, axis=1)
    feature_names = model.named_steps["preprocess"].get_feature_names_out()
    importances = model.named_steps["tree"].feature_importances_

    metrics_text = [
        "Synthetic policy evaluation (not real satellite performance)",
        f"Seed: {args.seed}",
        f"Training rows: {len(x_train)}; test rows: {len(x_test)}",
        f"Decision tree accuracy: {accuracy_score(y_test, ml_predictions):.4f}",
        f"BASELINE accuracy: {accuracy_score(y_test, baseline_predictions):.4f}",
        "",
        "Decision tree per-class metrics:",
        classification_report(y_test, ml_predictions, labels=LABEL_ORDER, zero_division=0),
        "Confusion matrix (rows=true, columns=predicted; TRANSMIT, DEFER, REJECT):",
        str(confusion_matrix(y_test, ml_predictions, labels=LABEL_ORDER)),
        "",
        "Feature importance:",
    ]
    for name, importance in sorted(zip(feature_names, importances), key=lambda item: item[1], reverse=True):
        metrics_text.append(f"{name}: {importance:.6f}")

    for path in (args.out, args.baseline_out, args.metrics, args.rules):
        path.parent.mkdir(parents=True, exist_ok=True)
    args.metrics.write_text("\n".join(metrics_text) + "\n", encoding="utf-8", newline="\n")
    args.rules.write_text(
        export_text(model.named_steps["tree"], feature_names=list(feature_names)) + "\n",
        encoding="utf-8", newline="\n"
    )

    packet_predictions = model.predict(packets[FEATURES])
    pd.DataFrame({
        "packet_id": packets["packet_id"].astype(int),
        "action": packet_predictions,
        "source": "ML",
    }).to_csv(args.out, index=False, lineterminator="\n")
    predict_packets(packets).to_csv(args.baseline_out, index=False, lineterminator="\n")

    print(f"Trained on {len(x_train)} rows; evaluated on {len(x_test)} rows.")
    print(f"Wrote ML decisions to {args.out} and BASELINE decisions to {args.baseline_out}.")
    print("NOTICE: Results measure learning of a synthetic policy, not real satellite performance.")


if __name__ == "__main__":
    main()
