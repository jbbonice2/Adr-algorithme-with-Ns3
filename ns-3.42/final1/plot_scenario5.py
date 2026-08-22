#!/usr/bin/env python3
"""
Plot PDR and Energy per algorithm for Scenario 5 (densite_mobilite).

For each algorithm:
  - X axis: Number of Devices (density)
  - Y axis: PDR (%) or Average Energy (mJ)
  - Legend: 3 mobility speeds (0, 30, 60 km/h)

Generates 10 graphs: 2 (PDR + Energy) × 5 algorithms.
"""

import os
import pandas as pd
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

# ─── Configuration ─────────────────────────────────────────────────────────────
CSV_PATH = os.path.join(os.path.dirname(__file__), "summaries", "summary_scenario5.csv")
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "plots_scenario5")

ALGO_ORDER = ["No-ADR", "ADR-MAX", "ADR-AVG", "ADR-MIN", "ADR-Lite"]

# One style per mobility speed
MOBILITY_STYLES = {
    0.0:  {"color": "#2ecc71", "marker": "o", "linestyle": "-",  "label": "Mobility = 0 km/h"},
    30.0: {"color": "#3498db", "marker": "s", "linestyle": "--", "label": "Mobility = 30 km/h"},
    60.0: {"color": "#e74c3c", "marker": "^", "linestyle": "-.", "label": "Mobility = 60 km/h"},
}


def main():
    # Read only the expected number of columns (first 13) to avoid shifted rows
    df = pd.read_csv(CSV_PATH, usecols=range(13), engine='python', index_col=False)
    # Normalize column names and values
    df.columns = df.columns.str.strip()
    df['Algorithm'] = df['Algorithm'].astype(str).str.strip()
    df['MobilitySpeed'] = pd.to_numeric(df['MobilitySpeed'], errors='coerce')
    df['NumDevices'] = pd.to_numeric(df['NumDevices'], errors='coerce')
    # Drop rows missing critical values
    df = df.dropna(subset=['Algorithm', 'MobilitySpeed', 'NumDevices'])
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    algorithms = [a for a in ALGO_ORDER if a in df["Algorithm"].unique()]
    mobility_speeds = sorted(df["MobilitySpeed"].unique())

    print(f"Algorithms: {algorithms}")
    print(f"Mobility speeds: {mobility_speeds}")
    print(f"Device counts: {sorted(df['NumDevices'].unique())}")

    plot_count = 0

    for algo in algorithms:
        algo_df = df[df["Algorithm"] == algo]

        # ── PDR Plot ──────────────────────────────────────────────────────
        fig, ax = plt.subplots(figsize=(10, 6))

        for speed in mobility_speeds:
            style = MOBILITY_STYLES.get(speed, {"color": "gray", "marker": "x", "linestyle": "-", "label": f"{speed} km/h"})
            subset = algo_df[algo_df["MobilitySpeed"] == speed].sort_values("NumDevices")
            if subset.empty:
                continue
            ax.plot(subset["NumDevices"], subset["PDR_Percent"],
                    label=style["label"],
                    color=style["color"],
                    marker=style["marker"],
                    linestyle=style["linestyle"],
                    linewidth=2, markersize=8)

        ax.set_xlabel("Number of Devices (# devices)", fontsize=12)
        ax.set_ylabel("Packet Delivery Rate (PDR) (%)", fontsize=12)
        ax.legend(fontsize=10, loc='best')
        ax.grid(True, alpha=0.3)
        ax.set_ylim(0, 105)
        ax.set_yticks(np.arange(0, 110, 10))
        x_ticks = sorted(algo_df["NumDevices"].unique())
        ax.set_xticks(x_ticks)
        ax.set_xlim(min(x_ticks) - 50, max(x_ticks) + 50)

        plt.tight_layout()
        fname = f"scenario5_{algo.replace('-','_')}_PDR.png"
        fpath = os.path.join(OUTPUT_DIR, fname)
        fig.savefig(fpath, dpi=150, bbox_inches='tight')
        # also save PDF and EPS
        fpath_pdf = os.path.splitext(fpath)[0] + ".pdf"
        fpath_eps = os.path.splitext(fpath)[0] + ".eps"
        fig.savefig(fpath_pdf, dpi=300, bbox_inches='tight')
        fig.savefig(fpath_eps, dpi=300, bbox_inches='tight')
        plt.close(fig)
        plot_count += 1
        print(f"  [{plot_count}] {os.path.basename(fpath)}, {os.path.basename(fpath_pdf)}, {os.path.basename(fpath_eps)}")

        # ── Energy Plot ───────────────────────────────────────────────────
        fig, ax = plt.subplots(figsize=(10, 6))

        for speed in mobility_speeds:
            style = MOBILITY_STYLES.get(speed, {"color": "gray", "marker": "x", "linestyle": "-", "label": f"{speed} km/h"})
            subset = algo_df[algo_df["MobilitySpeed"] == speed].sort_values("NumDevices")
            if subset.empty:
                continue
            ax.plot(subset["NumDevices"], subset["AvgEnergy_mJ"],
                    label=style["label"],
                    color=style["color"],
                    marker=style["marker"],
                    linestyle=style["linestyle"],
                    linewidth=2, markersize=8)

        ax.set_xlabel("Number of Devices (# devices)", fontsize=12)
        ax.set_ylabel("Average Energy (mJ)", fontsize=12)
        ax.legend(fontsize=10, loc='best')
        ax.grid(True, alpha=0.3)
        ax.set_ylim(0, 120)
        ax.set_yticks(np.arange(0, 121, 10))
        x_ticks = sorted(algo_df["NumDevices"].unique())
        ax.set_xticks(x_ticks)
        ax.set_xlim(min(x_ticks) - 50, max(x_ticks) + 50)

        plt.tight_layout()
        fname = f"scenario5_{algo.replace('-','_')}_Energy.png"
        fpath = os.path.join(OUTPUT_DIR, fname)
        fig.savefig(fpath, dpi=150, bbox_inches='tight')
        # also save PDF and EPS
        fpath_pdf = os.path.splitext(fpath)[0] + ".pdf"
        fpath_eps = os.path.splitext(fpath)[0] + ".eps"
        fig.savefig(fpath_pdf, dpi=300, bbox_inches='tight')
        fig.savefig(fpath_eps, dpi=300, bbox_inches='tight')
        plt.close(fig)
        plot_count += 1
        print(f"  [{plot_count}] {os.path.basename(fpath)}, {os.path.basename(fpath_pdf)}, {os.path.basename(fpath_eps)}")

    print(f"\nTotal: {plot_count} plots generated in {OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
