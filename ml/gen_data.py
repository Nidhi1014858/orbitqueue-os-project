#!/usr/bin/env python3
"""Generate deterministic synthetic packet and training CSV files."""

import argparse
import csv
import random
from pathlib import Path


DISCLAIMER = "Synthetic educational data; not SomaiyaSat telemetry."
TYPES = ("TT&C", "HOUSEKEEPING", "SSTV", "VOICE")
TYPE_WEIGHTS = (0.18, 0.27, 0.20, 0.35)
LABELS = ("TRANSMIT", "DEFER", "REJECT")


def policy_label(row):
    """Return the synthetic ground-truth policy label before noise."""
    battery = row["battery_level"]
    link = row["link_quality"]
    urgency = row["urgency"]
    size = row["data_size"]
    packet_type = row["type"]

    if battery < 18:
        return "REJECT" if urgency <= 2 else "DEFER"
    if link < 25:
        return "REJECT" if urgency <= 2 else "DEFER"
    if battery < 32 or link < 42:
        return "DEFER"
    if size > 650000 and urgency <= 3:
        return "DEFER"
    if packet_type == "SSTV" and size > 450000 and link < 65:
        return "DEFER"
    if urgency >= 4 and battery >= 35 and link >= 45:
        return "TRANSMIT"
    if urgency <= 2 and (battery < 45 or link < 50):
        return "REJECT"
    return "TRANSMIT"


def random_row(rng):
    """Create one scenario, with extra samples near decision boundaries."""
    near_boundary = rng.random() < 0.35
    if near_boundary:
        battery = max(0, min(100, rng.choice((18, 32, 35, 45)) + rng.randint(-4, 4)))
        link = max(0, min(100, rng.choice((25, 42, 45, 50, 65)) + rng.randint(-5, 5)))
        size = max(1, min(1000000, rng.choice((450000, 650000)) + rng.randint(-40000, 40000)))
    else:
        battery = rng.randint(8, 100)
        link = rng.randint(10, 100)
        size = rng.randint(500, 900000)
    return {
        "type": rng.choices(TYPES, TYPE_WEIGHTS, k=1)[0],
        "urgency": rng.randint(1, 5),
        "link_quality": link,
        "battery_level": battery,
        "data_size": size,
    }


def add_label_noise(row, rng, noise_rate=0.05):
    label = policy_label(row)
    if rng.random() < noise_rate:
        label = rng.choice([candidate for candidate in LABELS if candidate != label])
    return label


def write_csv(path, fieldnames, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        handle.write(f"# {DISCLAIMER}\n")
        writer = csv.DictWriter(handle, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def make_packets(count, rng):
    rows = []
    edge_cases = [
        ("TT&C", 5, 45, 35, 1200),
        ("HOUSEKEEPING", 3, 42, 32, 8000),
        ("SSTV", 2, 64, 44, 650000),
        ("VOICE", 4, 25, 31, 16000),
        ("SSTV", 3, 65, 45, 450000),
        ("VOICE", 2, 50, 18, 24000),
        ("TT&C", 5, 24, 60, 900),
        ("HOUSEKEEPING", 4, 70, 17, 6000),
    ]
    for packet_id in range(1, count + 1):
        if packet_id <= len(edge_cases):
            packet_type, urgency, link, battery, size = edge_cases[packet_id - 1]
            row = {
                "type": packet_type,
                "urgency": urgency,
                "link_quality": link,
                "battery_level": battery,
                "data_size": size,
            }
        else:
            row = random_row(rng)
        rows.append({"packet_id": packet_id, **row})
    return rows


def make_training(count, rng):
    rows = []
    for scenario_id in range(1, count + 1):
        row = random_row(rng)
        rows.append({"scenario_id": scenario_id, **row, "label": add_label_noise(row, rng)})
    return rows


def make_bad_rows():
    return [
        {"packet_id": 1, "type": "TT&C", "urgency": 5, "link_quality": 80, "battery_level": 70, "data_size": 900},
        {"packet_id": 2, "type": "XYZ", "urgency": 3, "link_quality": 70, "battery_level": 60, "data_size": 2000},
        {"packet_id": 3, "type": "VOICE", "urgency": 4, "link_quality": 75, "battery_level": 140, "data_size": 12000},
        {"packet_id": 4, "type": "SSTV", "urgency": 2, "link_quality": 65, "battery_level": 55, "data_size": -10},
        {"packet_id": 5, "type": "HOUSEKEEPING", "urgency": 3, "link_quality": "", "battery_level": 50, "data_size": 3000},
        {"packet_id": 1, "type": "VOICE", "urgency": 2, "link_quality": 60, "battery_level": 45, "data_size": 8000},
        {"packet_id": 7, "type": "VOICE", "urgency": "urgent", "link_quality": 60, "battery_level": 45, "data_size": 8000},
    ]


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--packets", type=int, default=60)
    parser.add_argument("--train", type=int, default=600)
    parser.add_argument("--data-dir", type=Path, default=Path("data"))
    args = parser.parse_args()
    if args.packets < 8 or args.train < 30:
        parser.error("--packets must be at least 8 and --train at least 30")
    return args


def main():
    args = parse_args()
    rng = random.Random(args.seed)
    packet_fields = ["packet_id", "type", "urgency", "link_quality", "battery_level", "data_size"]
    training_fields = ["scenario_id", "type", "urgency", "link_quality", "battery_level", "data_size", "label"]

    write_csv(args.data_dir / "satellite_data.csv", packet_fields, make_packets(args.packets, rng))
    write_csv(args.data_dir / "training_scenarios.csv", training_fields, make_training(args.train, rng))
    write_csv(args.data_dir / "satellite_data_bad.csv", packet_fields, make_bad_rows())
    print(f"NOTICE: {DISCLAIMER}")
    print(f"Wrote {args.packets} packets and {args.train} labelled training scenarios using seed {args.seed}.")


if __name__ == "__main__":
    main()
