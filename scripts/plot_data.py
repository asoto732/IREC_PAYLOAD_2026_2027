#!/usr/bin/env python3
"""Post-flight data visualization for Payload Software.

Opens a saved data CSV (see docs/DATA_FORMAT.md) and produces basic plots
of pressure, temperature, and vibration over time, one subplot each,
stacked so timing across sensors is easy to compare. Satisfies
requirement PST-1 / PST-2 in docs/REQUIREMENTS.md.

A MATLAB equivalent could read the same CSV with readtable() and would
be a reasonable alternative per the assignment; this script is the
Python reference implementation (PST-3).

Usage:
    python3 scripts/plot_data.py data/sample_payload_log.csv
    python3 scripts/plot_data.py data/sample_payload_log.csv --out plot.png
    python3 scripts/plot_data.py data/sample_payload_log.csv --show
"""

import argparse
import csv
import sys
from collections import defaultdict


def load_data(path):
    """Read the CSV and group (time_ms, value, status) points by sensor."""
    series = defaultdict(lambda: {"time_s": [], "value": [], "status": []})

    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        required = {"time_ms", "sensor", "value", "status"}
        if not required.issubset(reader.fieldnames or []):
            missing = required - set(reader.fieldnames or [])
            raise ValueError(
                f"{path} is missing expected column(s) {sorted(missing)}; "
                f"found {reader.fieldnames}"
            )

        for row in reader:
            sensor = row["sensor"]
            status = row["status"]
            try:
                t_s = float(row["time_ms"]) / 1000.0
                value = float(row["value"])
            except ValueError:
                # A malformed row (e.g. truncated last line from an abrupt
                # power loss) is skipped rather than crashing the whole plot.
                continue
            series[sensor]["time_s"].append(t_s)
            series[sensor]["value"].append(value)
            series[sensor]["status"].append(status)

    return series


def summarize(series):
    for sensor, data in series.items():
        n = len(data["value"])
        n_bad = sum(1 for s in data["status"] if s != "OK")
        print(f"  {sensor:12s} samples={n:6d}  non-OK status={n_bad}")


def make_plot(series, title):
    import matplotlib

    matplotlib.use("Agg")  # safe default for headless/CI use; overridden by --show
    import matplotlib.pyplot as plt

    order = [s for s in ("PRESSURE", "TEMPERATURE", "VIBRATION") if s in series]
    order += [s for s in series if s not in order]  # any future sensor, still plotted
    if not order:
        raise ValueError("no recognized sensor rows found in the data file")

    fig, axes = plt.subplots(len(order), 1, figsize=(10, 3 * len(order)), sharex=True)
    if len(order) == 1:
        axes = [axes]

    units = {"PRESSURE": "kPa", "TEMPERATURE": "deg C", "VIBRATION": "g"}

    for ax, sensor in zip(axes, order):
        data = series[sensor]
        ok_t = [t for t, s in zip(data["time_s"], data["status"]) if s == "OK"]
        ok_v = [v for v, s in zip(data["value"], data["status"]) if s == "OK"]
        bad_t = [t for t, s in zip(data["time_s"], data["status"]) if s != "OK"]
        bad_v = [v for v, s in zip(data["value"], data["status"]) if s != "OK"]

        ax.plot(ok_t, ok_v, "-", linewidth=1, label="OK")
        if bad_t:
            ax.scatter(bad_t, bad_v, color="red", s=15, zorder=3, label="fault/error")
        ax.set_ylabel(f"{sensor.title()} ({units.get(sensor, '')})")
        ax.grid(True, alpha=0.3)
        ax.legend(loc="upper right", fontsize="small")

    axes[-1].set_xlabel("Time since recording start (s)")
    fig.suptitle(title)
    fig.tight_layout()
    return fig


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", help="path to a saved payload data CSV")
    parser.add_argument("--out", default=None, help="save the figure to this path (e.g. plot.png)")
    parser.add_argument("--show", action="store_true", help="open an interactive window instead of/in addition to saving")
    args = parser.parse_args()

    try:
        series = load_data(args.csv_path)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"Loaded {args.csv_path}")
    summarize(series)

    fig = make_plot(series, title=f"Payload sensor data -- {args.csv_path}")

    out_path = args.out
    if out_path is None and not args.show:
        out_path = "plot.png"

    if out_path:
        fig.savefig(out_path, dpi=150)
        print(f"Saved plot to {out_path}")

    if args.show:
        import matplotlib.pyplot as plt

        plt.show()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
