#!/usr/bin/env python3
"""
Script de tracé des courbes pour chaque scénario de simulation LoRaWAN ADR.

Pour chaque scénario, on identifie le paramètre variable principal (axe X)
et on trace une courbe PDR + Énergie pour chaque combinaison des autres paramètres.

Usage:
    python3 plot_scenarios.py <dossier_summaries>

Exemple:
    python3 plot_scenarios.py summaries
"""

import sys
import os
import glob
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')  # Backend non-interactif pour sauvegarder les figures

# ─── Configuration des scénarios ───────────────────────────────────────────────
# Pour chaque scénario : paramètre principal (axe X) + label d'axe
SCENARIO_CONFIG = {
    "density": {
        "x_param": "NumDevices",
        "x_label": "Nombre de dispositifs",
        "combo_params": ["MobilitySpeed", "TrafficInterval", "MaxRandomLoss"],
        "title": "Scénario Densité",
    },
    "mobilite": {
        "x_param": "MobilitySpeed",
        "x_label": "Vitesse de mobilité (km/h)",
        "combo_params": ["NumDevices", "TrafficInterval", "MaxRandomLoss"],
        "title": "Scénario Mobilité",
    },
    "sigma": {
        "x_param": "MaxRandomLoss",
        "x_label": "Perte aléatoire maximale (dB)",
        "combo_params": ["NumDevices", "MobilitySpeed", "TrafficInterval"],
        "title": "Scénario Sigma",
    },
    "intervalle_d_envoie": {
        "x_param": "TrafficInterval",
        "x_label": "Intervalle d'envoi (s)",
        "combo_params": ["NumDevices", "MobilitySpeed", "MaxRandomLoss"],
        "title": "Scénario Intervalle d'envoi",
    },
}

# Styles pour chaque algorithme
ALGO_STYLES = {
    "No-ADR":   {"color": "#e74c3c", "marker": "o", "linestyle": "-"},
    "ADR-MAX":  {"color": "#2ecc71", "marker": "s", "linestyle": "--"},
    "ADR-AVG":  {"color": "#3498db", "marker": "^", "linestyle": "-."},
    "ADR-Lite": {"color": "#9b59b6", "marker": "D", "linestyle": ":"},
}

# Labels des paramètres pour les titres
PARAM_LABELS = {
    "NumDevices": "N",
    "MobilitySpeed": "Vit",
    "TrafficInterval": "Intv",
    "MaxRandomLoss": "Loss",
}

PARAM_UNITS = {
    "NumDevices": "",
    "MobilitySpeed": " km/h",
    "TrafficInterval": " s",
    "MaxRandomLoss": " dB",
}


def format_combo_label(combo_params, combo_values):
    """Génère un label lisible pour une combinaison de paramètres."""
    parts = []
    for p, v in zip(combo_params, combo_values):
        label = PARAM_LABELS.get(p, p)
        unit = PARAM_UNITS.get(p, "")
        # Formater la valeur
        if isinstance(v, float) and v == int(v):
            parts.append(f"{label}={int(v)}{unit}")
        else:
            parts.append(f"{label}={v}{unit}")
    return ", ".join(parts)


def plot_scenario(df, scenario_name, config, output_dir):
    """
    Trace les courbes pour un scénario donné.
    Pour chaque combinaison des paramètres non-principaux,
    crée une figure avec 2 sous-graphiques (PDR et Énergie).
    """
    x_param = config["x_param"]
    x_label = config["x_label"]
    combo_params = config["combo_params"]
    scenario_title = config["title"]

    # Créer les dossiers de sortie
    scenario_out = os.path.join(output_dir, scenario_name)
    os.makedirs(scenario_out, exist_ok=True)

    # Trouver les combinaisons uniques
    combos = df[combo_params].drop_duplicates().sort_values(combo_params)

    print(f"\n{'='*60}")
    print(f"  {scenario_title}")
    print(f"  Paramètre variable (axe X) : {x_param}")
    print(f"  Combinaisons trouvées : {len(combos)}")
    print(f"{'='*60}")

    plot_count = 0
    for _, combo_row in combos.iterrows():
        combo_values = [combo_row[p] for p in combo_params]
        combo_label = format_combo_label(combo_params, combo_values)

        # Filtrer les données pour cette combinaison
        mask = pd.Series(True, index=df.index)
        for p, v in zip(combo_params, combo_values):
            mask &= (df[p] == v)
        subset = df[mask].copy()

        if subset.empty:
            continue

        # Trier par le paramètre X
        subset = subset.sort_values(x_param)

        # Créer la figure avec 2 sous-graphiques
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
        fig.suptitle(f"{scenario_title}\n{combo_label}", fontsize=13, fontweight='bold')

        has_data = False

        # Tracer une courbe par algorithme
        algorithms = sorted(subset["Algorithm"].unique(),
                           key=lambda a: list(ALGO_STYLES.keys()).index(a)
                           if a in ALGO_STYLES else 99)

        for algo in algorithms:
            algo_data = subset[subset["Algorithm"] == algo].sort_values(x_param)
            if algo_data.empty:
                continue

            style = ALGO_STYLES.get(algo, {"color": "gray", "marker": "x", "linestyle": "-"})
            x_vals = algo_data[x_param].values
            pdr_vals = algo_data["PDR_Percent"].values
            energy_vals = algo_data["AvgEnergy_mJ"].values

            # Courbe PDR
            ax1.plot(x_vals, pdr_vals,
                    label=algo,
                    color=style["color"],
                    marker=style["marker"],
                    linestyle=style["linestyle"],
                    linewidth=2, markersize=7)

            # Courbe Énergie
            ax2.plot(x_vals, energy_vals,
                    label=algo,
                    color=style["color"],
                    marker=style["marker"],
                    linestyle=style["linestyle"],
                    linewidth=2, markersize=7)

            has_data = True

        if not has_data:
            plt.close(fig)
            continue

        # Configuration axe PDR
        ax1.set_xlabel(x_label, fontsize=11)
        ax1.set_ylabel("PDR (%)", fontsize=11)
        ax1.set_title("Taux de livraison des paquets (PDR)", fontsize=11)
        ax1.legend(fontsize=9, loc='best')
        ax1.grid(True, alpha=0.3)
        ax1.set_ylim(bottom=max(0, ax1.get_ylim()[0] - 2), top=min(102, ax1.get_ylim()[1] + 1))

        # Configuration axe Énergie
        ax2.set_xlabel(x_label, fontsize=11)
        ax2.set_ylabel("Énergie moyenne par paquet (mJ)", fontsize=11)
        ax2.set_title("Consommation énergétique", fontsize=11)
        ax2.legend(fontsize=9, loc='best')
        ax2.grid(True, alpha=0.3)

        # Graduation spécifique pour la densité : 0 à 1000 par pas de 100
        if x_param == "NumDevices":
            ax1.set_xticks(range(0, 1100, 100))
            ax1.set_xlim(0, 1000)
            ax2.set_xticks(range(0, 1100, 100))
            ax2.set_xlim(0, 1000)

        plt.tight_layout()

        # Nom de fichier
        combo_str = "_".join([f"{PARAM_LABELS[p]}{v}" for p, v in zip(combo_params, combo_values)])
        combo_str = combo_str.replace(" ", "").replace("/", "-")
        filename = f"{scenario_name}_{combo_str}.png"
        filepath = os.path.join(scenario_out, filename)
        fig.savefig(filepath, dpi=150, bbox_inches='tight')
        plt.close(fig)

        plot_count += 1
        print(f"  [{plot_count:3d}] {combo_label}  -> {filename}")

    print(f"\n  Total : {plot_count} graphiques sauvegardés dans {scenario_out}/")
    return plot_count


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 plot_scenarios.py <dossier_summaries>")
        print("Exemple: python3 plot_scenarios.py summaries")
        sys.exit(1)

    summary_dir = sys.argv[1]

    if not os.path.isdir(summary_dir):
        print(f"Erreur: Le dossier '{summary_dir}' n'existe pas.")
        sys.exit(1)

    # Dossier de sortie pour les graphiques
    output_dir = os.path.join(os.path.dirname(summary_dir) or ".", "plots")
    os.makedirs(output_dir, exist_ok=True)

    # Chercher tous les fichiers summary_scenario*.csv
    csv_files = sorted(glob.glob(os.path.join(summary_dir, "summary_scenario*.csv")))

    if not csv_files:
        print(f"Erreur: Aucun fichier summary_scenario*.csv trouvé dans '{summary_dir}'.")
        sys.exit(1)

    print(f"Fichiers trouvés : {len(csv_files)}")
    for f in csv_files:
        print(f"  - {os.path.basename(f)}")

    total_plots = 0

    for csv_file in csv_files:
        # Lire le CSV
        df = pd.read_csv(csv_file)

        # Identifier le scénario à partir de la colonne Scenario
        scenario_name = df["Scenario"].iloc[0].strip()

        if scenario_name not in SCENARIO_CONFIG:
            print(f"\nScénario inconnu '{scenario_name}' dans {csv_file}, ignoré.")
            continue

        config = SCENARIO_CONFIG[scenario_name]
        total_plots += plot_scenario(df, scenario_name, config, output_dir)

    print(f"\n{'='*60}")
    print(f"  TERMINÉ : {total_plots} graphiques générés au total")
    print(f"  Dossier de sortie : {output_dir}/")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
