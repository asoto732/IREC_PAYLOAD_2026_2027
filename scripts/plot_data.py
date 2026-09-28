#!/usr/bin/env python3
"""Post-flight data visualization for Payload Software.

Opens a saved data CSV (see docs/DATA_FORMAT.md) and produces basic plots
of every logged stream over time -- pressure, temperature, vibration,
and humidity -- one subplot each, stacked so timing across sensors is
easy to compare. Satisfies requirement PST-1 / PST-2 in
docs/REQUIREMENTS.md.

A MATLAB equivalent could read the same CSV with readtable() and would
be a reasonable alternative per the assignment; this script is the
Python reference implementation (PST-3).

Usage:
    python3 scripts/plot_data.py data/sample_payload_log.csv
    python3 scripts/plot_data.py data/sample_payload_log.csv --out plot.png
    python3 scripts/plot_data.py data/sample_payload_log.csv --show
    python3 scripts/plot_data.py data/sample_payload_log.csv --only PRESSURE VIBRATION
"""

import argparse
import csv
import sys
from collections import defaultdict

# Plot order. Anything not listed here (a sensor added after this script
# was last touched) still gets plotted, just at the end.
SENSOR_ORDER = (
    "PRESSURE",
    "TEMPERATURE",
    "VIBRATION",
    "HUMIDITY",
)

# Statuses where the row still carries a real measurement: an out-of-range
# reading is logged with the offending value. Every other fault leaves no
# value, so plotting it as a data point would drag the axis to zero.
STATUS_WITH_VALUE = ("OUT_OF_RANGE",)

UNITS = {
    "PRESSURE": "kPa",
    "TEMPERATURE": "deg C",
    "VIBRATION": "g",
    "HUMIDITY": "%RH",
}


def time_column(fieldnames):
    """Return (column name, seconds-per-unit) for the file's time column.

    Logs written before the 800 Hz vibration rate was adopted use
    `time_ms`; current logs use `time_us` because millisecond resolution
    cannot separate 1.25 ms vibration samples. Both are readable here so
    old data does not become unplottable.
    """
    names = fieldnames or []
    if "time_us" in names:
        return "time_us", 1e-6
    if "time_ms" in names:
        return "time_ms", 1e-3
    return None, None


def load_data(path):
    """Read the CSV and group (time_s, value, status) points by sensor."""
    series = defaultdict(lambda: {"time_s": [], "value": [], "status": []})

    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        time_name, time_scale = time_column(reader.fieldnames)
        required = {"sensor", "value", "status"}
        missing = required - set(reader.fieldnames or [])
        if time_name is None:
            missing.add("time_us")
        if missing:
            raise ValueError(
                f"{path} is missing expected column(s) {sorted(missing)}; "
                f"found {reader.fieldnames}"
            )

        for row in reader:
            sensor = row["sensor"]
            status = row["status"]
            try:
                t_s = float(row[time_name]) * time_scale
                value = float(row["value"])
            except (TypeError, ValueError):
                # A malformed row (e.g. truncated last line from an abrupt
                # power loss) is skipped rather than crashing the whole plot.
                continue
            series[sensor]["time_s"].append(t_s)
            series[sensor]["value"].append(value)
            series[sensor]["status"].append(status)

    return series


def summarize(series):
    for sensor in sorted(series, key=lambda s: (SENSOR_ORDER.index(s) if s in SENSOR_ORDER else len(SENSOR_ORDER))):
        data = series[sensor]
        n = len(data["value"])
        n_bad = sum(1 for s in data["status"] if s != "OK")
        span = (max(data["time_s"]) - min(data["time_s"])) if n > 1 else 0.0
        rate = (n - 1) / span if span > 0 else 0.0
        print(f"  {sensor:14s} samples={n:6d}  measured rate={rate:7.1f} Hz  non-OK status={n_bad}")


def make_plot(series, title):
    import matplotlib

    matplotlib.use("Agg")  # safe default for headless/CI use; overridden by --show
    import matplotlib.pyplot as plt

    order = [s for s in SENSOR_ORDER if s in series]
    order += [s for s in series if s not in order]  # any future sensor, still plotted
    if not order:
        raise ValueError("no recognized sensor rows found in the data file")

    fig, axes = plt.subplots(len(order), 1, figsize=(10, 2.4 * len(order)), sharex=True)
    if len(order) == 1:
        axes = [axes]

    for ax, sensor in zip(axes, order):
        data = series[sensor]
        rows = list(zip(data["time_s"], data["value"], data["status"]))

        ok_t = [t for t, _, s in rows if s == "OK"]
        ok_v = [v for _, v, s in rows if s == "OK"]
        bad_t = [t for t, _, s in rows if s != "OK" and s in STATUS_WITH_VALUE]
        bad_v = [v for _, v, s in rows if s != "OK" and s in STATUS_WITH_VALUE]
        # Faults that produced no reading at all: marked along the bottom of
        # the panel instead of plotted as a value, which would be zero and
        # would drag the axis off the real data.
        novalue_t = [t for t, _, s in rows if s != "OK" and s not in STATUS_WITH_VALUE]

        ax.plot(ok_t, ok_v, "-", linewidth=1, label="OK")
        if bad_t:
            ax.scatter(bad_t, bad_v, color="red", s=15, zorder=3, label="faulted reading")
        if novalue_t:
            # Pin the limits to the real data before adding the marks.
            low, high = ax.get_ylim()
            ax.set_ylim(low, high)
            ax.vlines(novalue_t, low, low + 0.08 * (high - low),
                      color="red", linewidth=1.2, zorder=3, label="no reading")
        label = sensor.replace("_", " ").title()
        ax.set_ylabel(f"{label}\n({UNITS.get(sensor, '')})")
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
    parser.add_argument(
        "--only",
        nargs="+",
        metavar="SENSOR",
        default=None,
        help="plot only these sensors, e.g. --only PRESSURE VIBRATION",
    )
    args = parser.parse_args()

    try:
        series = load_data(args.csv_path)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.only:
        wanted = {s.upper() for s in args.only}
        unknown = wanted - set(series)
        if unknown:
            print(f"error: no rows for sensor(s) {sorted(unknown)}", file=sys.stderr)
            return 1
        series = {k: v for k, v in series.items() if k in wanted}

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
