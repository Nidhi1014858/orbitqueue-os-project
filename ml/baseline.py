#!/usr/bin/env python3
"""Hand-written BASELINE rules for comparison with the ML router."""

import argparse
from pathlib import Path

import pandas as pd


def baseline_action(row):
    """Return a deliberately simple threshold-based action."""
    battery = int(row["battery_level"])
    link = int(row["link_quality"])
    urgency = int(row["urgency"])
    size = int(row["data_size"])

    if battery < 20 or link < 25:
        return "REJECT" if urgency <= 2 else "DEFER"
    if battery < 35 or link < 45 or (size > 600000 and urgency <= 3):
        return "DEFER"
    return "TRANSMIT"


def predict_packets(packet_frame):
    return pd.DataFrame({
        "packet_id": packet_frame["packet_id"].astype(int),
        "action": packet_frame.apply(baseline_action, axis=1),
        "source": "BASELINE",
    })


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packets", type=Path, default=Path("data/satellite_data.csv"))
    parser.add_argument("--out", type=Path, default=Path("outputs/decisions_baseline.csv"))
    return parser.parse_args()


def main():
    args = parse_args()
    packets = pd.read_csv(args.packets, comment="#")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    predict_packets(packets).to_csv(args.out, index=False, lineterminator="\n")
    print(f"BASELINE wrote {len(packets)} decisions to {args.out}")


if __name__ == "__main__":
    main()
