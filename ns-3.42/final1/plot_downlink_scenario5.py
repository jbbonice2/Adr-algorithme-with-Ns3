#!/usr/bin/env python3
"""
Trace les DownlinkPackets en fonction de la densité (NumDevices)
pour chaque algorithme ADR — Scénario 5 (densité + mobilité)
Un graphique par vitesse de mobilité.
"""

import matplotlib
matplotlib.use("Agg")
import pandas as pd
import matplotlib.pyplot as plt
import os

# Charger les données (limiter au nombre de colonnes attendues pour éviter le décalage)
script_dir = os.path.dirname(os.path.abspath(__file__))
csv_path = os.path.join(script_dir, "summaries", "summary_scenario5.csv")
df = pd.read_csv(csv_path, usecols=range(13), engine='python', index_col=False)
df.columns = df.columns.str.strip()
df['Algorithm'] = df['Algorithm'].astype(str).str.strip()
df['MobilitySpeed'] = pd.to_numeric(df['MobilitySpeed'], errors='coerce')
df['NumDevices'] = pd.to_numeric(df['NumDevices'], errors='coerce')
df = df.dropna(subset=['Algorithm', 'MobilitySpeed', 'NumDevices'])

# Couleurs et marqueurs par algorithme
styles = {
    "No-ADR":   {"color": "#2ca02c", "marker": "s",  "linestyle": "-"},
    "ADR-MAX":  {"color": "#1f77b4", "marker": "o",  "linestyle": "-"},
    "ADR-AVG":  {"color": "#ff7f0e", "marker": "^",  "linestyle": "-"},
    "ADR-MIN":  {"color": "#d62728", "marker": "D",  "linestyle": "-"},
    "ADR-Lite": {"color": "#9467bd", "marker": "v",  "linestyle": "--"},
}

algorithms = ["No-ADR", "ADR-MAX", "ADR-AVG", "ADR-MIN", "ADR-Lite"]
speeds = sorted(df["MobilitySpeed"].unique())

# Échelle Y globale : min de min et max de max des DownlinkPackets
y_min = df["DownlinkPackets"].min()
y_max = df["DownlinkPackets"].max()
y_margin = (y_max - y_min) * 0.05  # 5% de marge

# Créer un graphique par vitesse
fig, axes = plt.subplots(1, len(speeds), figsize=(7 * len(speeds), 6), sharey=True)
if len(speeds) == 1:
    axes = [axes]

for ax, speed in zip(axes, speeds):
    subset = df[df["MobilitySpeed"] == speed]
    for algo in algorithms:
        data = subset[subset["Algorithm"] == algo].sort_values("NumDevices")
        if data.empty:
            continue
        s = styles.get(algo, {"color": "gray", "marker": "x", "linestyle": "-"})
        ax.plot(data["NumDevices"], data["DownlinkPackets"],
                label=algo, color=s["color"], marker=s["marker"],
                linestyle=s["linestyle"], linewidth=2, markersize=7)

    ax.set_title(f"Vitesse = {speed} m/s", fontsize=13, fontweight="bold")
    ax.set_xlabel("Nombre de devices", fontsize=12)
    ax.set_ylabel("Paquets Downlink", fontsize=12)
    ax.set_ylim(y_min - y_margin, y_max + y_margin)
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.tick_params(labelsize=10)

fig.suptitle("Scénario 5 — Downlink Packets vs Densité\n(par vitesse de mobilité)",
             fontsize=15, fontweight="bold", y=1.02)
plt.tight_layout()

out_path = os.path.join(script_dir, "downlink_vs_densite_scenario5.png")
plt.savefig(out_path, dpi=200, bbox_inches="tight")
print(f"Graphique sauvegardé : {out_path}")

# --- Graphique combiné (toutes vitesses superposées) ---
fig2, ax2 = plt.subplots(figsize=(10, 7))

line_styles_speed = {0.0: "-", 30.0: "--", 60.0: ":"}

for algo in algorithms:
    for speed in speeds:
        data = df[(df["Algorithm"] == algo) & (df["MobilitySpeed"] == speed)].sort_values("NumDevices")
        if data.empty:
            continue
        s = styles.get(algo, {"color": "gray", "marker": "x", "linestyle": "-"})
        ls = line_styles_speed.get(speed, "-.")
        ax2.plot(data["NumDevices"], data["DownlinkPackets"],
                 label=f"{algo} (v={speed})",
                 color=s["color"], marker=s["marker"],
                 linestyle=ls, linewidth=1.8, markersize=6, alpha=0.85)

ax2.set_title("Scénario 5 — Downlink Packets vs Densité (toutes vitesses)",
              fontsize=14, fontweight="bold")
ax2.set_xlabel("Nombre de devices", fontsize=12)
ax2.set_ylabel("Paquets Downlink", fontsize=12)
ax2.set_ylim(y_min - y_margin, y_max + y_margin)
ax2.legend(fontsize=8, ncol=3, loc="upper left")
ax2.grid(True, alpha=0.3)

out_path2 = os.path.join(script_dir, "downlink_vs_densite_scenario5_combined.png")
plt.savefig(out_path2, dpi=200, bbox_inches="tight")
print(f"Graphique combiné sauvegardé : {out_path2}")

plt.show()
