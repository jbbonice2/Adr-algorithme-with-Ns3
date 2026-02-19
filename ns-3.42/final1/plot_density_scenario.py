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

def plot_comparison(df, mobility_speed, traffic_interval, max_random_loss, output_dir=None):
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
    
    # --- Graphique 1: PDR vs NumDevices ---
    ax1 = axes[0]
    for algo in algorithms:
        algo_data = df[df['Algorithm'] == algo].sort_values('NumDevices')
        if not algo_data.empty:
            ax1.plot(algo_data['NumDevices'], algo_data['PDR_Percent'], 
                    marker=markers[algo], color=colors[algo], 
                    label=algo, linewidth=2, markersize=8)
    
    ax1.set_xlabel('Nombre de Devices', fontsize=12)
    ax1.set_ylabel('PDR (%)', fontsize=12)
    ax1.set_title(f'Taux de Livraison de Paquets (PDR)\n{title_suffix}', fontsize=11)
    ax1.legend(loc='best', fontsize=10)
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim([max(0, df['PDR_Percent'].min() - 5), 102])
    
    # --- Graphique 2: Énergie vs NumDevices ---
    ax2 = axes[1]
    for algo in algorithms:
        algo_data = df[df['Algorithm'] == algo].sort_values('NumDevices')
        if not algo_data.empty:
            ax2.plot(algo_data['NumDevices'], algo_data['AvgEnergy_mJ'], 
                    marker=markers[algo], color=colors[algo], 
                    label=algo, linewidth=2, markersize=8)
    
    ax2.set_xlabel('Nombre de Devices', fontsize=12)
    ax2.set_ylabel('Énergie Moyenne (mJ)', fontsize=12)
    ax2.set_title(f'Consommation Énergétique\n{title_suffix}', fontsize=11)
    ax2.legend(loc='best', fontsize=10)
    ax2.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    # Sauvegarder si répertoire spécifié
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
        filename = f"density_mob{mobility_speed}_traf{traffic_interval}_loss{max_random_loss}.png"
        filepath = os.path.join(output_dir, filename)
        plt.savefig(filepath, dpi=150, bbox_inches='tight')
        print(f"Graphique sauvegardé: {filepath}")
    
    plt.show()

def plot_bar_comparison(df, mobility_speed, traffic_interval, max_random_loss, output_dir=None):
    """
    Trace des graphiques en barres pour une meilleure visualisation.
    """
    algorithms = ['No-ADR', 'ADR-MAX', 'ADR-AVG', 'ADR-Lite']
    colors = {'No-ADR': '#2196F3', 'ADR-MAX': '#4CAF50', 'ADR-AVG': '#FF9800', 'ADR-Lite': '#E91E63'}
    
    num_devices_list = sorted(df['NumDevices'].unique())
    x = np.arange(len(num_devices_list))
    width = 0.2
    
    fig, axes = plt.subplots(1, 2, figsize=(16, 6))
    title_suffix = f"(Mobilité={mobility_speed} km/h, Intervalle={traffic_interval}s, Perte={max_random_loss} dB)"
    
    # --- Graphique 1: PDR en barres ---
    ax1 = axes[0]
    for i, algo in enumerate(algorithms):
        algo_data = df[df['Algorithm'] == algo].sort_values('NumDevices')
        if not algo_data.empty:
            pdr_values = algo_data['PDR_Percent'].values
            ax1.bar(x + i*width, pdr_values, width, label=algo, color=colors[algo], alpha=0.85)
    
    ax1.set_xlabel('Nombre de Devices', fontsize=12)
    ax1.set_ylabel('PDR (%)', fontsize=12)
    ax1.set_title(f'Taux de Livraison de Paquets (PDR)\n{title_suffix}', fontsize=11)
    ax1.set_xticks(x + width * 1.5)
    ax1.set_xticklabels(num_devices_list)
    ax1.legend(loc='best', fontsize=10)
    ax1.grid(True, alpha=0.3, axis='y')
    ax1.set_ylim([max(0, df['PDR_Percent'].min() - 5), 102])
    
    # --- Graphique 2: Énergie en barres ---
    ax2 = axes[1]
    for i, algo in enumerate(algorithms):
        algo_data = df[df['Algorithm'] == algo].sort_values('NumDevices')
        if not algo_data.empty:
            energy_values = algo_data['AvgEnergy_mJ'].values
            ax2.bar(x + i*width, energy_values, width, label=algo, color=colors[algo], alpha=0.85)
    
    ax2.set_xlabel('Nombre de Devices', fontsize=12)
    ax2.set_ylabel('Énergie Moyenne (mJ)', fontsize=12)
    ax2.set_title(f'Consommation Énergétique\n{title_suffix}', fontsize=11)
    ax2.set_xticks(x + width * 1.5)
    ax2.set_xticklabels(num_devices_list)
    ax2.legend(loc='best', fontsize=10)
    ax2.grid(True, alpha=0.3, axis='y')
    
    plt.tight_layout()
    
    # Sauvegarder si répertoire spécifié
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
        filename = f"density_bar_mob{mobility_speed}_traf{traffic_interval}_loss{max_random_loss}.png"
        filepath = os.path.join(output_dir, filename)
        plt.savefig(filepath, dpi=150, bbox_inches='tight')
        print(f"Graphique en barres sauvegardé: {filepath}")
    
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

def generate_all_plots(df, mobility_speeds, traffic_intervals, max_random_losses, output_dir, include_bar=False):
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
                plot_comparison(filtered_df, mobility, interval, loss, output_dir)
                plt.close('all')
                
                # Graphique en barres si demandé
                if include_bar:
                    plot_bar_comparison(filtered_df, mobility, interval, loss, output_dir)
                    plt.close('all')
                
                generated += 1
    
    print(f"\n=== Résumé ===")
    print(f"Graphiques générés: {generated}")
    print(f"Combinaisons ignorées (pas de données): {skipped}")
    print(f"Répertoire de sortie: {output_dir}")

def main():
    parser = argparse.ArgumentParser(description='Tracer les graphiques du scénario de densité')
    parser.add_argument('--csv', type=str, default='summaries/summary_scenario1.csv',
                        help='Chemin vers le fichier CSV')
    parser.add_argument('--mobility', type=float, default=0.0,
                        help='Vitesse de mobilité (km/h)')
    parser.add_argument('--interval', type=float, default=3600,
                        help='Intervalle de trafic (s)')
    parser.add_argument('--loss', type=float, default=0.0,
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
