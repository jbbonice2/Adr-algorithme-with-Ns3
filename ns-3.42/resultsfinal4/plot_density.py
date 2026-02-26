#!/usr/bin/env python3
"""
plot_density.py

Lit le fichier de résumé CSV et trace des courbes (PDR et énergie) pour
chaque combinaison de `NumDevices`, `MobilitySpeed`, `TrafficInterval`.

Usage:
  python3 plot_density.py --csv path/to/summary_scenario1.csv --outdir resultsfinal4/plots

Dépendances: pandas, matplotlib
"""
import argparse
import os
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def ensure_outdir(path):
    os.makedirs(path, exist_ok=True)


def plot_group(df_group, outdir, metrics):
    # df_group: subset for one (NumDevices, MobilitySpeed, TrafficInterval)
    # pivot on MaxRandomLoss, columns Algorithm
    group_keys = {
        'NumDevices': int(df_group['NumDevices'].iloc[0]),
        'MobilitySpeed': float(df_group['MobilitySpeed'].iloc[0]),
        'TrafficInterval': int(df_group['TrafficInterval'].iloc[0]),
    }

    # Ensure numeric sorting by MaxRandomLoss
    df_group = df_group.copy()
    df_group['MaxRandomLoss'] = pd.to_numeric(df_group['MaxRandomLoss'], errors='coerce')
    df_group = df_group.sort_values('MaxRandomLoss')

    for metric in metrics:
        pivot = df_group.pivot_table(index='MaxRandomLoss', columns='Algorithm', values=metric)
        if pivot.shape[0] < 2:
            # Not enough x points to draw a curve
            continue

        plt.figure(figsize=(8, 5))
        for col in pivot.columns:
            plt.plot(pivot.index, pivot[col], marker='o', label=col)

        plt.xlabel('MaxRandomLoss')
        plt.ylabel(metric)
        title = f"{metric} — N={group_keys['NumDevices']} v={group_keys['MobilitySpeed']} int={group_keys['TrafficInterval']}"
        plt.title(title)
        plt.grid(True, linestyle='--', alpha=0.5)
        plt.legend()

        # filename safe
        fname = f"metric_{metric.replace(' ', '_')}_N{group_keys['NumDevices']}_v{group_keys['MobilitySpeed']}_int{group_keys['TrafficInterval']}.png"
        outpath = Path(outdir) / fname
        plt.tight_layout()
        plt.savefig(outpath, dpi=200)
        plt.close()


def main():
    parser = argparse.ArgumentParser(description='Tracer les courbes pour le scénario density')
    parser.add_argument('--csv', default='resultsfinal4/summaries/summary_scenario1.csv', help='Chemin vers le CSV de résumé')
    parser.add_argument('--outdir', default='resultsfinal4/plots', help='Dossier de sortie pour les figures')
    parser.add_argument('--metrics', default='PDR_Percent,AvgEnergy_mJ', help='Liste de métriques séparées par une virgule')
    parser.add_argument('--scenario', default='density', help='Filtrer sur Scenario (par défaut: density)')
    args = parser.parse_args()

    csv_path = Path(args.csv)
    if not csv_path.exists():
        raise SystemExit(f"Fichier CSV introuvable: {csv_path}")

    df = pd.read_csv(csv_path)
    # Filtrer par scenario
    df = df[df['Scenario'] == args.scenario]

    metrics = [m.strip() for m in args.metrics.split(',') if m.strip()]

    ensure_outdir(args.outdir)

    group_cols = ['NumDevices', 'MobilitySpeed', 'TrafficInterval']
    groups = df.groupby(group_cols)

    total = len(groups)
    print(f"Found {total} group(s). Génération des figures dans {args.outdir}...")

    for keys, group in groups:
        plot_group(group, args.outdir, metrics)

    print('Terminé.')


if __name__ == '__main__':
    main()
#!/usr/bin/env python3
"""
plot_density.py

Lit le fichier de résumé CSV et trace des courbes (PDR et énergie) pour
chaque combinaison de `NumDevices`, `MobilitySpeed`, `TrafficInterval`.

Usage:
  python3 plot_density.py --csv path/to/summary_scenario1.csv --outdir resultsfinal4/plots

Dépendances: pandas, matplotlib
"""
import argparse
import os
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def ensure_outdir(path):
    os.makedirs(path, exist_ok=True)


def plot_group(df_group, outdir, metrics):
    # df_group: subset for one (NumDevices, MobilitySpeed, TrafficInterval)
    # pivot on MaxRandomLoss, columns Algorithm
    group_keys = {
        'NumDevices': int(df_group['NumDevices'].iloc[0]),
        'MobilitySpeed': float(df_group['MobilitySpeed'].iloc[0]),
        'TrafficInterval': int(df_group['TrafficInterval'].iloc[0]),
    }

    # Ensure numeric sorting by MaxRandomLoss
    df_group = df_group.copy()
    df_group['MaxRandomLoss'] = pd.to_numeric(df_group['MaxRandomLoss'], errors='coerce')
    df_group = df_group.sort_values('MaxRandomLoss')

    for metric in metrics:
        pivot = df_group.pivot_table(index='MaxRandomLoss', columns='Algorithm', values=metric)
        if pivot.shape[0] < 2:
            # Not enough x points to draw a curve
            continue

        plt.figure(figsize=(8, 5))
        for col in pivot.columns:
            plt.plot(pivot.index, pivot[col], marker='o', label=col)

        plt.xlabel('MaxRandomLoss')
        plt.ylabel(metric)
        title = f"{metric} — N={group_keys['NumDevices']} v={group_keys['MobilitySpeed']} int={group_keys['TrafficInterval']}"
        plt.title(title)
        plt.grid(True, linestyle='--', alpha=0.5)
        plt.legend()

        # filename safe
        fname = f"metric_{metric.replace(' ', '_')}_N{group_keys['NumDevices']}_v{group_keys['MobilitySpeed']}_int{group_keys['TrafficInterval']}.png"
        outpath = Path(outdir) / fname
        plt.tight_layout()
        plt.savefig(outpath, dpi=200)
        plt.close()


def main():
    parser = argparse.ArgumentParser(description='Tracer les courbes pour le scénario density')
    parser.add_argument('--csv', default='resultsfinal4/summaries/summary_scenario1.csv', help='Chemin vers le CSV de résumé')
    parser.add_argument('--outdir', default='resultsfinal4/plots', help='Dossier de sortie pour les figures')
    parser.add_argument('--metrics', default='PDR_Percent,AvgEnergy_mJ', help='Liste de métriques séparées par une virgule')
    parser.add_argument('--scenario', default='density', help='Filtrer sur Scenario (par défaut: density)')
    args = parser.parse_args()

    csv_path = Path(args.csv)
    if not csv_path.exists():
        raise SystemExit(f"Fichier CSV introuvable: {csv_path}")

    df = pd.read_csv(csv_path)
    # Filtrer par scenario
    df = df[df['Scenario'] == args.scenario]

    metrics = [m.strip() for m in args.metrics.split(',') if m.strip()]

    ensure_outdir(args.outdir)

    group_cols = ['NumDevices', 'MobilitySpeed', 'TrafficInterval']
    groups = df.groupby(group_cols)

    total = len(groups)
    print(f"Found {total} group(s). Génération des figures dans {args.outdir}...")

    for keys, group in groups:
        plot_group(group, args.outdir, metrics)

    print('Terminé.')


if __name__ == '__main__':
    main()
