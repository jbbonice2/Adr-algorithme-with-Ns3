#!/usr/bin/env python3
"""
Plotting script for each LoRaWAN ADR simulation scenario.

For each scenario, identifies the main variable parameter (X axis)
and plots separate PDR and Energy curves for each combination of the other parameters.

Usage:
    python3 plot_scenarios.py <summaries_folder>

Example:
    python3 plot_scenarios.py summaries
"""

import sys
import os
import glob
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')  # Non-interactive backend for saving figures

# ─── Scenario configuration ───────────────────────────────────────────────────
# For each scenario: main parameter (X axis) + axis label
SCENARIO_CONFIG = {
    "density": {
        "x_param": "NumDevices",
        "x_label": "Number of Devices",
        "combo_params": ["MobilitySpeed", "TrafficInterval", "MaxRandomLoss"],
        "title": "Density Scenario",
    },
    "mobilite": {
        "x_param": "MobilitySpeed",
        "x_label": "Mobility Speed (km/h)",
        "combo_params": ["NumDevices", "TrafficInterval", "MaxRandomLoss"],
        "title": "Mobility Scenario",
    },
    "sigma": {
        "x_param": "MaxRandomLoss",
        "x_label": "Maximum Random Loss (dB)",
        "combo_params": ["NumDevices", "MobilitySpeed", "TrafficInterval"],
        "title": "Sigma Scenario",
    },
    "intervalle_d_envoie": {
        "x_param": "TrafficInterval",
        "x_label": "Messages per Hour (msg/h)",
        "combo_params": ["NumDevices", "MobilitySpeed", "MaxRandomLoss"],
        "title": "Sending Interval Scenario",
    },
}

# Styles for each algorithm
ALGO_STYLES = {
    "No-ADR":   {"color": "#e74c3c", "marker": "o", "linestyle": "-"},
    "ADR-MAX":  {"color": "#2ecc71", "marker": "s", "linestyle": "--"},
    "ADR-AVG":  {"color": "#3498db", "marker": "^", "linestyle": "-."},
    "ADR-Lite": {"color": "#9b59b6", "marker": "D", "linestyle": ":"},
    "ADR-MIN":  {"color": "#f39c12", "marker": "P", "linestyle": "-"},
}

# Parameter labels for titles
PARAM_LABELS = {
    "NumDevices": "NumberDevices",
    "MobilitySpeed": "MobilitySpeed",
    "TrafficInterval": "SendingInterval",
    "MaxRandomLoss": "RandomLoss",
}

PARAM_UNITS = {
    "NumDevices": "",
    "MobilitySpeed": " km/h",
    "TrafficInterval": " s",
    "MaxRandomLoss": " dB",
}


def format_combo_label(combo_params, combo_values):
    """Generate a readable label for a combination of parameters."""
    parts = []
    for p, v in zip(combo_params, combo_values):
        label = PARAM_LABELS.get(p, p)
        unit = PARAM_UNITS.get(p, "")
        if isinstance(v, float) and v == int(v):
            parts.append(f"{label}={int(v)}{unit}")
        else:
            parts.append(f"{label}={v}{unit}")
    return ", ".join(parts)


def plot_scenario(df, scenario_name, config, output_dir):
    """
    Plot curves for a given scenario.
    For each combination of non-main parameters,
    creates TWO separate figures: one for PDR and one for Energy.
    """
    x_param = config["x_param"]
    x_label = config["x_label"]
    combo_params = config["combo_params"]
    scenario_title = config["title"]

    # For the sending interval scenario, convert TrafficInterval to Messages per Hour
    use_msg_per_hour = (scenario_name == "intervalle_d_envoie")
    if use_msg_per_hour:
        df = df.copy()
        df["MessagesPerHour"] = 3600.0 / df["TrafficInterval"]
        plot_x_param = "MessagesPerHour"
    else:
        plot_x_param = x_param

    # Create output directories
    scenario_out = os.path.join(output_dir, scenario_name)
    os.makedirs(scenario_out, exist_ok=True)

    # Find unique combinations
    combos = df[combo_params].drop_duplicates().sort_values(combo_params)

    print(f"\n{'='*60}")
    print(f"  {scenario_title}")
    print(f"  Variable parameter (X axis): {x_param}")
    print(f"  Combinations found: {len(combos)}")
    print(f"{'='*60}")

    plot_count = 0
    for _, combo_row in combos.iterrows():
        combo_values = [combo_row[p] for p in combo_params]
        combo_label = format_combo_label(combo_params, combo_values)

        # Filter data for this combination
        mask = pd.Series(True, index=df.index)
        for p, v in zip(combo_params, combo_values):
            mask &= (df[p] == v)
        subset = df[mask].copy()

        if subset.empty:
            continue

        # Sort by X parameter
        subset = subset.sort_values(plot_x_param)

        # Sort algorithms in defined order
        algorithms = sorted(subset["Algorithm"].unique(),
                           key=lambda a: list(ALGO_STYLES.keys()).index(a)
                           if a in ALGO_STYLES else 99)

        # ── Collect data per algorithm ──
        algo_plot_data = []
        for algo in algorithms:
            algo_data = subset[subset["Algorithm"] == algo].sort_values(plot_x_param)
            if algo_data.empty:
                continue
            style = ALGO_STYLES.get(algo, {"color": "gray", "marker": "x", "linestyle": "-"})
            algo_plot_data.append({
                "algo": algo,
                "x_vals": algo_data[plot_x_param].values,
                "pdr_vals": algo_data["PDR_Percent"].values,
                "energy_vals": algo_data["AvgEnergy_mJ"].values,
                "style": style,
            })

        if not algo_plot_data:
            continue

        # Helper to configure x-axis ticks
        def configure_x_ticks(ax):
            if x_param == "NumDevices":
                ax.set_xticks(range(100, 1100, 100))
                ax.set_xlim(50, 1050)
            elif use_msg_per_hour:
                unique_x = np.array(sorted(subset[plot_x_param].unique()))
                ax.set_xticks(unique_x)
                ax.set_xticklabels(
                    [f'{v:.0f}' if v == int(v) else f'{v:.1f}' for v in unique_x],
                    rotation=45, ha='right')

        # File name base
        combo_str = "_".join([f"{PARAM_LABELS[p]}{v}" for p, v in zip(combo_params, combo_values)])
        combo_str = combo_str.replace(" ", "").replace("/", "-")

        # ── Figure 1: PDR ──
        fig_pdr, ax_pdr = plt.subplots(figsize=(10, 6))
        fig_pdr.suptitle(f"{scenario_title} — Packet Delivery Rate (PDR)\n{combo_label}",
                         fontsize=13, fontweight='bold')

        for d in algo_plot_data:
            ax_pdr.plot(d["x_vals"], d["pdr_vals"],
                        label=d["algo"],
                        color=d["style"]["color"],
                        marker=d["style"]["marker"],
                        linestyle=d["style"]["linestyle"],
                        linewidth=2, markersize=7)

        ax_pdr.set_xlabel(x_label, fontsize=11)
        ax_pdr.set_ylabel("Packet Delivery Rate (PDR) (%)", fontsize=11)
        ax_pdr.set_title("Packet Delivery Rate (PDR)", fontsize=11)
        ax_pdr.legend(fontsize=9, loc='best')
        ax_pdr.grid(True, alpha=0.3)
        ax_pdr.set_ylim(0, 100)
        ax_pdr.set_yticks(np.arange(0, 101, 10))
        configure_x_ticks(ax_pdr)

        plt.tight_layout()
        pdr_filename = f"{scenario_name}_PDR_{combo_str}.png"
        pdr_filepath = os.path.join(scenario_out, pdr_filename)
        fig_pdr.savefig(pdr_filepath, dpi=150, bbox_inches='tight')
        plt.close(fig_pdr)

        # ── Figure 2: Energy ──
        fig_energy, ax_energy = plt.subplots(figsize=(10, 6))
        fig_energy.suptitle(f"{scenario_title} — Energy Consumption\n{combo_label}",
                            fontsize=13, fontweight='bold')

        for d in algo_plot_data:
            ax_energy.plot(d["x_vals"], d["energy_vals"],
                           label=d["algo"],
                           color=d["style"]["color"],
                           marker=d["style"]["marker"],
                           linestyle=d["style"]["linestyle"],
                           linewidth=2, markersize=7)

        ax_energy.set_xlabel(x_label, fontsize=11)
        ax_energy.set_ylabel("Average Energy per Packet (mJ)", fontsize=11)
        ax_energy.set_title("Energy Consumption", fontsize=11)
        ax_energy.legend(fontsize=9, loc='best')
        ax_energy.grid(True, alpha=0.3)
        configure_x_ticks(ax_energy)

        plt.tight_layout()
        energy_filename = f"{scenario_name}_Energy_{combo_str}.png"
        energy_filepath = os.path.join(scenario_out, energy_filename)
        fig_energy.savefig(energy_filepath, dpi=150, bbox_inches='tight')
        plt.close(fig_energy)

        plot_count += 2
        print(f"  [{plot_count-1:3d}] {combo_label}  -> {pdr_filename}")
        print(f"  [{plot_count:3d}] {combo_label}  -> {energy_filename}")

    print(f"\n  Total: {plot_count} plots saved in {scenario_out}/")
    return plot_count


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 plot_scenarios.py <summaries_folder>")
        print("Example: python3 plot_scenarios.py summaries")
        sys.exit(1)

    summary_dir = sys.argv[1]

    if not os.path.isdir(summary_dir):
        print(f"Error: Folder '{summary_dir}' does not exist.")
        sys.exit(1)

    # Output folder for plots
    output_dir = os.path.join(os.path.dirname(summary_dir) or ".", "plots")
    os.makedirs(output_dir, exist_ok=True)

    # Find all summary_scenario*.csv files
    csv_files = sorted(glob.glob(os.path.join(summary_dir, "summary_scenario*.csv")))

    if not csv_files:
        print(f"Error: No summary_scenario*.csv files found in '{summary_dir}'.")
        sys.exit(1)

    print(f"Files found: {len(csv_files)}")
    for f in csv_files:
        print(f"  - {os.path.basename(f)}")

    total_plots = 0

    for csv_file in csv_files:
        # Read CSV
        df = pd.read_csv(csv_file)

        # Identify scenario from the Scenario column
        scenario_name = df["Scenario"].iloc[0].strip()

        if scenario_name not in SCENARIO_CONFIG:
            print(f"\nUnknown scenario '{scenario_name}' in {csv_file}, skipped.")
            continue

        config = SCENARIO_CONFIG[scenario_name]
        total_plots += plot_scenario(df, scenario_name, config, output_dir)

    print(f"\n{'='*60}")
    print(f"  DONE: {total_plots} plots generated in total")
    print(f"  Output folder: {output_dir}/")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
