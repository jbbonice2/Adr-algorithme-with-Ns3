#!/usr/bin/env python3
"""
Script pour tracer les graphiques du scénario de densité.
Compare les algorithmes ADR en fonction du nombre de devices pour une combinaison
spécifique de MobilitySpeed, TrafficInterval et MaxRandomLoss.
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import argparse
import os

def load_data(csv_path):
    """Charge les données du fichier CSV."""
    df = pd.read_csv(csv_path)
    return df

def filter_data(df, mobility_speed, traffic_interval, max_random_loss):
    """Filtre les données selon les paramètres spécifiés."""
    filtered = df[
        (df['MobilitySpeed'] == mobility_speed) &
        (df['TrafficInterval'] == traffic_interval) &
        (df['MaxRandomLoss'] == max_random_loss)
    ]
    return filtered

def get_unique_values(df):
    """Affiche les valeurs uniques disponibles pour chaque paramètre."""
    print("\n=== Valeurs disponibles dans le fichier ===")
    print(f"MobilitySpeed: {sorted(df['MobilitySpeed'].unique())}")
    print(f"TrafficInterval: {sorted(df['TrafficInterval'].unique())}")
    print(f"MaxRandomLoss: {sorted(df['MaxRandomLoss'].unique())}")
    print(f"NumDevices: {sorted(df['NumDevices'].unique())}")
    print(f"Algorithms: {df['Algorithm'].unique().tolist()}")
    print("=" * 45)

def plot_comparison(df, mobility_speed, traffic_interval, max_random_loss, output_dir=None, x_var='NumDevices'):
    """
    Trace les graphiques de comparaison des algorithmes ADR.
    
    Args:
        df: DataFrame filtré
        mobility_speed: Vitesse de mobilité (km/h)
        traffic_interval: Intervalle de trafic (s)
        max_random_loss: Perte aléatoire maximale (dB)
        output_dir: Répertoire de sortie pour les images (optionnel)
    """
    
    algorithms = ['No-ADR', 'ADR-MAX', 'ADR-AVG', 'ADR-Lite']
    colors = {'No-ADR': '#2196F3', 'ADR-MAX': '#4CAF50', 'ADR-AVG': '#FF9800', 'ADR-Lite': '#E91E63'}
    markers = {'No-ADR': 'o', 'ADR-MAX': 's', 'ADR-AVG': '^', 'ADR-Lite': 'D'}
    
    # Créer la figure avec 2 sous-graphiques
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    
    title_suffix = f"(Mobilité={mobility_speed} km/h, Intervalle={traffic_interval}s, Perte={max_random_loss} dB)"
    
    # --- Graphique 1: PDR vs x_var ---
    ax1 = axes[0]
    for algo in algorithms:
        algo_data = df[df['Algorithm'] == algo].sort_values(x_var)
        if not algo_data.empty:
            ax1.plot(algo_data[x_var], algo_data['PDR_Percent'], 
                    marker=markers[algo], color=colors[algo], 
                    label=algo, linewidth=2, markersize=8)
    
    # X label depends on chosen x variable
    xlabel_map = {
        'NumDevices': 'Nombre de Devices',
        'MobilitySpeed': 'Mobilité (km/h)',
        'TrafficInterval': 'Intervalle de trafic (s)',
        'MessagesParHeure': 'Messages par heure (msg/h)',
        'MaxRandomLoss': 'Sigma / Perte aléatoire (dB)'
    }
    ax1.set_xlabel(xlabel_map.get(x_var, x_var), fontsize=12)
    ax1.set_ylabel('PDR (%)', fontsize=12)
    ax1.set_title(f'Taux de Livraison de Paquets (PDR)\n{title_suffix}', fontsize=11)
    ax1.legend(loc='best', fontsize=10)
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim([0, 100])
    ax1.set_yticks(np.arange(0, 101, 10))
    # Set sensible x-ticks based on x_var
    try:
        unique_x = np.array(sorted(df[x_var].unique()))
        if x_var == 'NumDevices':
            ax1.set_xticks(np.arange(100, 1001, 100))
        elif x_var == 'MessagesParHeure':
            ax1.set_xticks(unique_x)
            ax1.set_xticklabels([f'{v:.0f}' if v == int(v) else f'{v:.1f}' for v in unique_x], rotation=45, ha='right')
        elif x_var == 'TrafficInterval':
            ax1.set_xticks(unique_x)
        else:
            ax1.set_xticks(unique_x)
    except Exception:
        pass
    
    # --- Graphique 2: Énergie vs x_var ---
    ax2 = axes[1]
    for algo in algorithms:
        algo_data = df[df['Algorithm'] == algo].sort_values(x_var)
        if not algo_data.empty:
            ax2.plot(algo_data[x_var], algo_data['AvgEnergy_mJ'], 
                    marker=markers[algo], color=colors[algo], 
                    label=algo, linewidth=2, markersize=8)
    
    ax2.set_xlabel(xlabel_map.get(x_var, x_var), fontsize=12)
    ax2.set_ylabel('Énergie Moyenne (mJ)', fontsize=12)
    ax2.set_title(f'Consommation Énergétique\n{title_suffix}', fontsize=11)
    ax2.legend(loc='best', fontsize=10)
    ax2.grid(True, alpha=0.3)
    try:
        if x_var == 'NumDevices':
            ax2.set_xticks(np.arange(100, 1001, 100))
        elif x_var == 'MessagesParHeure':
            ax2.set_xticks(unique_x)
            ax2.set_xticklabels([f'{v:.0f}' if v == int(v) else f'{v:.1f}' for v in unique_x], rotation=45, ha='right')
        elif x_var == 'TrafficInterval':
            ax2.set_xticks(unique_x)
        else:
            ax2.set_xticks(unique_x)
    except Exception:
        pass
    
    plt.tight_layout()
    
    # Sauvegarder si répertoire spécifié
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
        filename = f"density_mob{mobility_speed}_traf{traffic_interval}_loss{max_random_loss}.png"
        filepath = os.path.join(output_dir, filename)
        plt.savefig(filepath, dpi=150, bbox_inches='tight')
        print(f"Graphique sauvegardé: {filepath}")
    
    plt.show()

def plot_bar_comparison(df, mobility_speed, traffic_interval, max_random_loss, output_dir=None, x_var='NumDevices'):
    """
    Trace des graphiques en courbes (remplace les diagrammes en barres).
    """
    algorithms = ['No-ADR', 'ADR-MAX', 'ADR-AVG', 'ADR-Lite']
    colors = {'No-ADR': '#2196F3', 'ADR-MAX': '#4CAF50', 'ADR-AVG': '#FF9800', 'ADR-Lite': '#E91E63'}
    markers = {'No-ADR': 'o', 'ADR-MAX': 's', 'ADR-AVG': '^', 'ADR-Lite': 'D'}

    num_devices_list = sorted(df[x_var].unique())

    fig, axes = plt.subplots(1, 2, figsize=(16, 6))
    title_suffix = f"(Mobilité={mobility_speed} km/h, Intervalle={traffic_interval}s, Perte={max_random_loss} dB)"

    # --- Graphique 1: PDR en courbes ---
    ax1 = axes[0]
    for algo in algorithms:
        algo_data = df[df['Algorithm'] == algo].sort_values(x_var)
        if not algo_data.empty:
            ax1.plot(algo_data[x_var], algo_data['PDR_Percent'],
                     marker=markers[algo], color=colors[algo],
                     label=algo, linewidth=2, markersize=8)

    xlabel_map = {
        'NumDevices': 'Nombre de Devices',
        'MobilitySpeed': 'Mobilité (km/h)',
        'TrafficInterval': 'Intervalle de trafic (s)',
        'MessagesParHeure': 'Messages par heure (msg/h)',
        'MaxRandomLoss': 'Sigma / Perte aléatoire (dB)'
    }
    ax1.set_xlabel(xlabel_map.get(x_var, x_var), fontsize=12)
    ax1.set_ylabel('PDR (%)', fontsize=12)
    ax1.set_title(f'Taux de Livraison de Paquets (PDR)\n{title_suffix}', fontsize=11)
    if x_var == 'MessagesParHeure':
        ax1.set_xticks(num_devices_list)
        ax1.set_xticklabels([f'{v:.0f}' if v == int(v) else f'{v:.1f}' for v in num_devices_list], rotation=45, ha='right')
    elif x_var == 'TrafficInterval':
        ax1.set_xticks(num_devices_list)
        ax1.set_xticklabels([str(x) for x in num_devices_list])
    else:
        ax1.set_xticks(num_devices_list)
        ax1.set_xticklabels([str(x) for x in num_devices_list])
    ax1.legend(loc='best', fontsize=10)
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim([max(0, df['PDR_Percent'].min() - 5), 102])

    # --- Graphique 2: Énergie en courbes ---
    ax2 = axes[1]
    for algo in algorithms:
        algo_data = df[df['Algorithm'] == algo].sort_values(x_var)
        if not algo_data.empty:
            ax2.plot(algo_data[x_var], algo_data['AvgEnergy_mJ'],
                     marker=markers[algo], color=colors[algo],
                     label=algo, linewidth=2, markersize=8)

    ax2.set_xlabel(xlabel_map.get(x_var, x_var), fontsize=12)
    ax2.set_ylabel('Énergie Moyenne (mJ)', fontsize=12)
    ax2.set_title(f'Consommation Énergétique\n{title_suffix}', fontsize=11)
    if x_var == 'MessagesParHeure':
        ax2.set_xticks(num_devices_list)
        ax2.set_xticklabels([f'{v:.0f}' if v == int(v) else f'{v:.1f}' for v in num_devices_list], rotation=45, ha='right')
    elif x_var == 'TrafficInterval':
        ax2.set_xticks(num_devices_list)
        ax2.set_xticklabels([str(x) for x in num_devices_list])
    else:
        ax2.set_xticks(num_devices_list)
        ax2.set_xticklabels([str(x) for x in num_devices_list])
    ax2.legend(loc='best', fontsize=10)
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()

    # Sauvegarder si répertoire spécifié
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
        filename = f"density_curve_mob{mobility_speed}_traf{traffic_interval}_loss{max_random_loss}.png"
        filepath = os.path.join(output_dir, filename)
        plt.savefig(filepath, dpi=150, bbox_inches='tight')
        print(f"Graphique (courbes) sauvegardé: {filepath}")

    plt.show()

def print_summary_table(df):
    """Affiche un tableau récapitulatif des résultats."""
    print("\n=== Tableau Récapitulatif ===")
    pivot_pdr = df.pivot_table(values='PDR_Percent', index='NumDevices', columns='Algorithm', aggfunc='mean')
    pivot_energy = df.pivot_table(values='AvgEnergy_mJ', index='NumDevices', columns='Algorithm', aggfunc='mean')
    
    print("\nPDR (%) par algorithme et nombre de devices:")
    print(pivot_pdr.round(2).to_string())
    
    print("\nÉnergie moyenne (mJ) par algorithme et nombre de devices:")
    print(pivot_energy.round(4).to_string())
    print()

def generate_all_plots(df, mobility_speeds, traffic_intervals, max_random_losses, output_dir, include_bar=False, x_var='NumDevices'):
    """
    Génère tous les graphiques pour toutes les combinaisons de paramètres.
    """
    import matplotlib
    matplotlib.use('Agg')  # Backend non-interactif pour sauvegarder sans afficher
    
    total_combinations = len(mobility_speeds) * len(traffic_intervals) * len(max_random_losses)
    generated = 0
    skipped = 0
    
    print(f"\n=== Génération de {total_combinations} combinaisons ===\n")
    
    for mobility in mobility_speeds:
        for interval in traffic_intervals:
            for loss in max_random_losses:
                filtered_df = filter_data(df, mobility, interval, loss)
                
                if filtered_df.empty:
                    print(f"[SKIP] Aucune donnée pour: mob={mobility}, interval={interval}, loss={loss}")
                    skipped += 1
                    continue
                
                print(f"[OK] Génération: mob={mobility}, interval={interval}, loss={loss}")
                
                # Graphique en lignes
                plot_comparison(filtered_df, mobility, interval, loss, output_dir, x_var=x_var)
                plt.close('all')
                
                # Graphique en barres si demandé
                if include_bar:
                    plot_bar_comparison(filtered_df, mobility, interval, loss, output_dir, x_var=x_var)
                    plt.close('all')
                
                generated += 1
    
    print(f"\n=== Résumé ===")
    print(f"Graphiques générés: {generated}")
    print(f"Combinaisons ignorées (pas de données): {skipped}")
    print(f"Répertoire de sortie: {output_dir}")


def plot_single_scenario(csv_path, output_base, include_bar=False):
    """
    Génère un seul graphique agrégé par scénario (moyennes par abscisse et algorithme).
    """
    import matplotlib
    matplotlib.use('Agg')

    if not os.path.exists(csv_path):
        print(f"[SKIP] Fichier absent: {csv_path}")
        return

    df = load_data(csv_path)
    if df.empty:
        print(f"[SKIP] Aucun enregistrement dans: {csv_path}")
        return

    # Determine scenario name
    if 'Scenario' in df.columns:
        unique = df['Scenario'].unique()
        scenario_name = unique[0] if len(unique) > 0 else os.path.splitext(os.path.basename(csv_path))[0]
    else:
        scenario_name = os.path.splitext(os.path.basename(csv_path))[0]

    scen_lower = scenario_name.lower()
    if 'density' in scen_lower:
        x_var = 'NumDevices'
    elif 'mobil' in scen_lower or 'mobilite' in scen_lower:
        x_var = 'MobilitySpeed'
    elif 'intervalle' in scen_lower or 'traf' in scen_lower or 'interval' in scen_lower:
        # Convertir TrafficInterval (secondes) en messages/heure
        df['MessagesParHeure'] = 3600.0 / df['TrafficInterval']
        x_var = 'MessagesParHeure'
    elif 'sigma' in scen_lower:
        x_var = 'MaxRandomLoss'
    else:
        x_var = 'NumDevices'

    # Agrégation: moyenne par (x_var, Algorithm)
    try:
        agg = df.groupby([x_var, 'Algorithm']).agg({'PDR_Percent': 'mean', 'AvgEnergy_mJ': 'mean'}).reset_index()
    except Exception as e:
        print(f"[ERROR] Impossible d'agréger les données pour {csv_path}: {e}")
        return

    out_dir = os.path.join(output_base, scenario_name)
    os.makedirs(out_dir, exist_ok=True)
    print(f"[OK] Génération agrégée: {scenario_name} (abscisse={x_var}) -> {out_dir}")

    # Use 'all' markers for title fields (they are only used for the subtitle)
    plot_comparison(agg, mobility_speed='all', traffic_interval='all', max_random_loss='all', output_dir=out_dir, x_var=x_var)
    plt.close('all')

    if include_bar:
        plot_bar_comparison(agg, mobility_speed='all', traffic_interval='all', max_random_loss='all', output_dir=out_dir, x_var=x_var)
        plt.close('all')


def generate_all_scenarios(csv_base_path, output_base, include_bar=False):
    """
    Parcourt les fichiers `summary_scenario1.csv` .. `summary_scenario4.csv` présents
    dans le même dossier que `csv_base_path` et génère tous les graphiques pour
    chaque fichier en utilisant les valeurs présentes dans chaque CSV.
    """
    base_dir = os.path.dirname(csv_base_path)
    for i in range(1, 5):
        csv_path = os.path.join(base_dir, f"summary_scenario{i}.csv")
        if not os.path.exists(csv_path):
            print(f"[SKIP] Fichier absent: {csv_path}")
            continue

        print(f"\n[SCENARIO] Chargement: {csv_path}")
        # For each scenario file we produce a single aggregated plot
        plot_single_scenario(csv_path, output_base, include_bar=include_bar)

def main():
    parser = argparse.ArgumentParser(description='Tracer les graphiques du scénario de densité')
    parser.add_argument('--csv', type=str, default='summaries/summary_scenario1.csv',
                        help='Chemin vers le fichier CSV')
    parser.add_argument('--mobility', type=float, default=0.0,
                        help='Vitesse de mobilité (km/h)')
    parser.add_argument('--interval', type=float, default=3600,
                        help='Intervalle de trafic (s)')
    parser.add_argument('--loss', type=float, default=3.96,
                        help='Perte aléatoire maximale (dB)')
    parser.add_argument('--output', type=str, default='plots',
                        help='Répertoire de sortie pour les graphiques')
    parser.add_argument('--list', action='store_true',
                        help='Afficher les valeurs disponibles et quitter')
    parser.add_argument('--bar', action='store_true',
                        help='Afficher également les graphiques en barres')
    parser.add_argument('--no-show', action='store_true',
                        help='Ne pas afficher les graphiques (seulement sauvegarder)')
    parser.add_argument('--all', action='store_true',
                        help='Générer tous les graphiques pour toutes les combinaisons')
    parser.add_argument('--all-scenarios', action='store_true',
                        help='Générer les graphiques pour tous les fichiers summary_scenario1..4.csv')
    
    args = parser.parse_args()
    
    # Charger les données
    print(f"Chargement des données depuis: {args.csv}")
    df = load_data(args.csv)
    
    # Afficher les valeurs disponibles si demandé
    if args.list:
        get_unique_values(df)
        return
    
    # Afficher les valeurs disponibles
    get_unique_values(df)
    
    # Mode génération de tous les graphiques
    if args.all:
        mobility_speeds = [0.0, 33.3, 60.0]
        traffic_intervals = [72, 145, 3600]
        max_random_losses = [0.0, 3.96, 7.92]
        
        generate_all_plots(df, mobility_speeds, traffic_intervals, max_random_losses, 
                          args.output, include_bar=args.bar)
        return

    # Mode génération pour tous les scenarios (summary_scenario1..4.csv)
    if args.all_scenarios:
        generate_all_scenarios(args.csv, args.output, include_bar=args.bar)
        return
    
    # Filtrer les données
    print(f"\nFiltrage: MobilitySpeed={args.mobility}, TrafficInterval={args.interval}, MaxRandomLoss={args.loss}")
    filtered_df = filter_data(df, args.mobility, args.interval, args.loss)
    
    if filtered_df.empty:
        print("ERREUR: Aucune donnée trouvée pour cette combinaison de paramètres!")
        print("Utilisez --list pour voir les valeurs disponibles.")
        return
    
    print(f"Nombre d'enregistrements trouvés: {len(filtered_df)}")
    
    # Afficher le tableau récapitulatif
    print_summary_table(filtered_df)
    
    # Tracer les graphiques
    if not args.no_show:
        plot_comparison(filtered_df, args.mobility, args.interval, args.loss, args.output)
        
        if args.bar:
            plot_bar_comparison(filtered_df, args.mobility, args.interval, args.loss, args.output)
    else:
        # Sauvegarder sans afficher
        import matplotlib
        matplotlib.use('Agg')
        plot_comparison(filtered_df, args.mobility, args.interval, args.loss, args.output)
        if args.bar:
            plot_bar_comparison(filtered_df, args.mobility, args.interval, args.loss, args.output)

if __name__ == "__main__":
    main()
