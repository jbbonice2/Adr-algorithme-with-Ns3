#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Script de visualisation pour l'analyse des performances des algorithmes ADR en LoRaWAN
Auteur: Bonice
Date: 2025
"""

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
from pathlib import Path

# Configuration du style
plt.style.use('seaborn-v0_8-darkgrid')
sns.set_palette("husl")
plt.rcParams['figure.figsize'] = (14, 8)
plt.rcParams['font.size'] = 10
plt.rcParams['axes.titlesize'] = 12
plt.rcParams['axes.labelsize'] = 11
plt.rcParams['legend.fontsize'] = 9

# Couleurs pour chaque algorithme
COLORS = {
    'ADR-AVG': '#e74c3c',
    'ADR-Lite': '#3498db',
    'ADR-MAX': '#2ecc71',
    'No-ADR': '#f39c12'
}

MARKERS = {
    'ADR-AVG': 'o',
    'ADR-Lite': 's',
    'ADR-MAX': '^',
    'No-ADR': 'D'
}

def load_data(file_path):
    """Charge les données depuis un fichier CSV et normalise les colonnes.

    - Nettoie les noms de colonnes (trim, underscore pour espaces).
    - Crée la colonne `alg` si elle n'existe pas (recherche variantes comme
      "Algorithm", "algorithm" ou extraction depuis le nom de fichier).
    Retourne un DataFrame prêt à être utilisé par les fonctions de plot.
    """
    df = pd.read_csv(file_path)

    # Normaliser les noms de colonnes (retirer espaces, trim)
    new_cols = []
    for c in df.columns:
        if isinstance(c, str):
            nc = c.strip().replace(' ', '_')
        else:
            nc = c
        new_cols.append(nc)
    df.columns = new_cols

    # Assurer que nous avons une colonne 'alg' (format attendu par le reste du script)
    if 'alg' not in df.columns:
        # Chercher variantes communes
        for candidate in ['Algorithm', 'algorithm', 'Algorithme', 'algorithme']:
            if candidate in df.columns:
                df['alg'] = df[candidate]
                break

    # Si toujours absent, essayer d'extraire l'algorithme depuis le nom de fichier
    if 'alg' not in df.columns:
        fname = Path(file_path).name
        found = None
        known_algs = ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR', 'ADR_AVG', 'ADR_MAX', 'No_ADR']
        for ka in known_algs:
            if ka in fname:
                found = ka
                break
        if found is None:
            # essai plus permissif : chercher tokens ADR ou NO-ADR
            lower = fname.lower()
            if 'adr-lite' in lower:
                found = 'ADR-Lite'
            elif 'adr-avg' in lower or 'adr_avg' in lower:
                found = 'ADR-AVG'
            elif 'adr-max' in lower or 'adr_max' in lower:
                found = 'ADR-MAX'
            elif 'no-adr' in lower or 'no_adr' in lower:
                found = 'No-ADR'

        df['alg'] = found if found is not None else 'Unknown'

    return df


def find_summary_path(scenario, filename):
    """Cherche le fichier de résumé :
    - d'abord tel quel dans le répertoire courant
    - ensuite dans `resultsfinal/summaries/<scenario>/` ou `resultsfinal2/summaries/<scenario>/`
    - enfin par glob dans ce dossier
    Retourne le chemin (str) ou None si introuvable.
    """
    p = Path(filename)
    if p.exists():
        return str(p)

    # Check multiple possible result folders (support resultsfinal and resultsfinal2)
    for base_name in ['resultsfinal', 'resultsfinal2']:
        base = Path(base_name) / 'summaries'
        alt = base / scenario / filename
        if alt.exists():
            return str(alt)

        # glob alternatives in scenario folder
        scen_dir = base / scenario
        if scen_dir.exists():
            candidates = list(scen_dir.glob(f'summary*{scenario}*run*.csv')) + list(scen_dir.glob('summary_*_run*.csv'))
            if candidates:
                return str(candidates[0])

        # last resort: search anywhere under this base for a file containing scenario
        if base.exists():
            for f in base.rglob(f'*{scenario}*.csv'):
                return str(f)

    return None


def plot_pdr_energy_by_scenario(df, scenario_name, output_dir='output'):
    """
    Crée 4 graphiques par scénario:
    1. packet delivery rate en fonction du paramètre variable
    2. Énergie en fonction du paramètre variable
    3. packet delivery rate vs Énergie (scatter plot)
    4. Boxplot comparatif packet delivery rate et Énergie
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    # Déterminer le paramètre variable selon le scénario
    param_map = {
        'mobilite': 'MobilitySpeed',
        'density': 'NumDevices',
        'intervalle_d_envoie': 'TrafficInterval',
        'sigma': 'Sigma'
    }
    
    param = param_map.get(scenario_name, 'MobilitySpeed')
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle(f'Analyse des performances - Scénario: {scenario_name.upper()}', 
                 fontsize=16, fontweight='bold')
    
    # 1. packet delivery rate en fonction du paramètre
    ax1 = axes[0, 0]
    for alg in df['alg'].unique():
        alg_data = df[df['alg'] == alg].groupby(param)['PDR_Percent'].mean().reset_index()
        ax1.plot(alg_data[param], alg_data['PDR_Percent'], 
                marker=MARKERS[alg], color=COLORS[alg], 
                label=alg, linewidth=2, markersize=8)
    
    ax1.set_xlabel(f'{param}', fontweight='bold')
    ax1.set_ylabel('packet delivery rate (%)', fontweight='bold')
    ax1.set_title('Taux de livraison de paquets (packet delivery rate)', fontweight='bold')
    # For packet delivery rate ensure 0-100 with steps of 20
    ax1.set_ylim(0, 100)
    ax1.set_yticks(np.arange(0, 101, 20))
    # If x param is density or traffic interval clamp ticks/range as requested
    if param == 'NumDevices':
        ax1.set_xlim(100, 1000)
        ax1.set_xticks(np.arange(100, 1001, 100))
    if param == 'TrafficInterval':
        ax1.set_xlim(72, 3600)
        # friendly ticks for traffic interval
        ax1.set_xticks([72, 300, 600, 900, 1200, 1800, 2400, 3600])
    ax1.legend(loc='best')
    ax1.grid(True, alpha=0.3)
    
    # 2. Énergie en fonction du paramètre
    ax2 = axes[0, 1]
    for alg in df['alg'].unique():
        alg_data = df[df['alg'] == alg].groupby(param)['AvgEnergy_mJ'].mean().reset_index()
        ax2.plot(alg_data[param], alg_data['AvgEnergy_mJ'], 
                marker=MARKERS[alg], color=COLORS[alg], 
                label=alg, linewidth=2, markersize=8)
    
    ax2.set_xlabel(f'{param}', fontweight='bold')
    ax2.set_ylabel('Énergie moyenne (mJ)', fontweight='bold')
    ax2.set_title('Consommation énergétique', fontweight='bold')
    ax2.legend(loc='best')
    ax2.grid(True, alpha=0.3)
    
    # 3. packet delivery rate vs Énergie (scatter plot)
    ax3 = axes[1, 0]
    for alg in df['alg'].unique():
        alg_data = df[df['alg'] == alg]
        ax3.scatter(alg_data['AvgEnergy_mJ'], alg_data['PDR_Percent'], 
                   marker=MARKERS[alg], color=COLORS[alg], 
                   label=alg, s=100, alpha=0.6, edgecolors='black')
    
    ax3.set_xlabel('Énergie moyenne (mJ)', fontweight='bold')
    ax3.set_ylabel('packet delivery rate (%)', fontweight='bold')
    ax3.set_title('Compromis packet delivery rate vs Énergie', fontweight='bold')
    ax3.set_ylim(0, 100)
    ax3.set_yticks(np.arange(0, 101, 20))
    ax3.legend(loc='best')
    ax3.grid(True, alpha=0.3)
    
    # 4. Boxplot comparatif
    ax4 = axes[1, 1]
    
    # Normaliser les données pour la comparaison (échelle 0-100)
    df_normalized = df.copy()
    # guard against division by zero
    max_energy = df['AvgEnergy_mJ'].max() if df['AvgEnergy_mJ'].max() and not np.isnan(df['AvgEnergy_mJ'].max()) else 1.0
    df_normalized['Energy_Normalized'] = (df['AvgEnergy_mJ'] / max_energy) * 100
    
    # Préparer les données pour le boxplot
    data_to_plot = []
    labels_to_plot = []
    
    for alg in df['alg'].unique():
        data_to_plot.append(df[df['alg'] == alg]['PDR_Percent'].values)
        labels_to_plot.append(f'{alg}\n(packet delivery rate)')
        data_to_plot.append(df_normalized[df_normalized['alg'] == alg]['Energy_Normalized'].values)
        labels_to_plot.append(f'{alg}\n(Énergie)')
    
    bp = ax4.boxplot(data_to_plot, labels=labels_to_plot, patch_artist=True)
    
    # Colorer les boxplots
    colors_list = []
    for alg in df['alg'].unique():
        colors_list.extend([COLORS[alg], COLORS[alg]])
    
    for patch, color in zip(bp['boxes'], colors_list):
        patch.set_facecolor(color)
        patch.set_alpha(0.6)
    
    ax4.set_ylabel('Valeur (%)', fontweight='bold')
    # keep y-axis for comparisons within 0-100
    ax4.set_ylim(0, 100)
    ax4.set_yticks(np.arange(0, 101, 20))
    ax4.set_title('Distribution comparée (packet delivery rate et Énergie normalisée)', fontweight='bold')
    ax4.grid(True, alpha=0.3, axis='y')
    plt.setp(ax4.xaxis.get_majorticklabels(), rotation=45, ha='right')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/analyse_complete_{scenario_name}.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Graphique sauvegardé: analyse_complete_{scenario_name}.png")


def plot_parameter_comparison(dfs_dict, output_dir='output'):
    """
    Compare l'effet de différents paramètres sur les performances
    dfs_dict: dictionnaire {scenario_name: dataframe}
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('Comparaison des paramètres sur packet delivery rate et Énergie', 
                 fontsize=16, fontweight='bold')
    
    scenarios = list(dfs_dict.keys())
    
    # Pour chaque algorithme
    for idx, alg in enumerate(['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']):
        ax = axes[idx // 2, idx % 2]
        
        # Calculer les moyennes pour chaque scénario
        pdr_means = []
        energy_means = []
        scenario_labels = []
        
        for scenario_name, df in dfs_dict.items():
            alg_data = df[df['alg'] == alg]
            pdr_means.append(alg_data['PDR_Percent'].mean())
            energy_means.append(alg_data['AvgEnergy_mJ'].mean())
            scenario_labels.append(scenario_name.replace('_', ' ').title())
        
        x = np.arange(len(scenario_labels))
        width = 0.35
        
        bars1 = ax.bar(x - width/2, pdr_means, width, label='packet delivery rate (%)', 
                      color=COLORS[alg], alpha=0.8)
        
        # Créer un deuxième axe Y pour l'énergie
        ax2 = ax.twinx()
        bars2 = ax2.bar(x + width/2, energy_means, width, label='Énergie (mJ)', 
                       color=COLORS[alg], alpha=0.4, hatch='//')
        
        ax.set_xlabel('Scénario', fontweight='bold')
        ax.set_ylabel('packet delivery rate (%)', fontweight='bold', color='black')
        ax2.set_ylabel('Énergie (mJ)', fontweight='bold', color='gray')
        ax.set_title(f'Algorithme: {alg}', fontweight='bold')
        ax.set_xticks(x)
        ax.set_xticklabels(scenario_labels, rotation=45, ha='right')
        
        # Combiner les légendes
        lines1, labels1 = ax.get_legend_handles_labels()
        lines2, labels2 = ax2.get_legend_handles_labels()
        ax.legend(lines1 + lines2, labels1 + labels2, loc='upper left')
        
        ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/comparaison_scenarios.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Graphique sauvegardé: comparaison_scenarios.png")


def plot_heatmap_pdr(df, scenario_name, output_dir='output'):
    """
    Crée une heatmap du packet delivery rate en fonction de deux paramètres
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    # Identifier les paramètres variables
    params = []
    for col in ['MobilitySpeed', 'NumDevices', 'TrafficInterval', 'Sigma']:
        if df[col].nunique() > 1:
            params.append(col)
    
    if len(params) < 2:
        print(f"⚠ Pas assez de paramètres variables pour créer une heatmap pour {scenario_name}")
        return
    
    param1, param2 = params[0], params[1]
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle(f'Heatmap packet delivery rate - Scénario: {scenario_name.upper()}', 
                 fontsize=16, fontweight='bold')
    
    for idx, alg in enumerate(['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']):
        ax = axes[idx // 2, idx % 2]
        
        # Créer la matrice pour la heatmap
        alg_data = df[df['alg'] == alg]
        pivot_table = alg_data.pivot_table(
            values='PDR_Percent',
            index=param2,
            columns=param1,
            aggfunc='mean'
        )
        
        sns.heatmap(pivot_table, annot=True, fmt='.1f', cmap='RdYlGn', 
               ax=ax, cbar_kws={'label': 'packet delivery rate (%)'}, 
                   vmin=0, vmax=100, linewidths=0.5)
        
        ax.set_title(f'{alg}', fontweight='bold')
        ax.set_xlabel(param1, fontweight='bold')
        ax.set_ylabel(param2, fontweight='bold')
        # Ensure PDR axis ticks from 0..100
        ax.set_yticks(ax.get_yticks())
    
    plt.tight_layout()
    out_name = f'heatmap_packet_delivery_rate_{scenario_name}.png'
    plt.savefig(f'{output_dir}/{out_name}', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Graphique sauvegardé: {out_name}")


def find_all_summary_paths(scenario):
    """Retourne une liste de chemins CSV trouvés pour un scénario en cherchant
    dans le répertoire courant, `resultsfinal/summaries/<scenario>/` et
    `resultsfinal2/summaries/<scenario>/`.
    """
    paths = []
    # cwd
    for p in Path('.').glob(f'*{scenario}*.csv'):
        paths.append(str(p))

    # Also check under the repository root where this script lives (ns-3.42)
    repo_root = Path(__file__).resolve().parent.parent
    for base_name in ['resultsfinal', 'resultsfinal2']:
        # check both relative to cwd and relative to repo root
        candidates_bases = [Path(base_name) / 'summaries', repo_root / base_name / 'summaries']
        for base in candidates_bases:
            alt = base / scenario
            if alt.exists():
                for p in alt.glob('*.csv'):
                    paths.append(str(p))

        # last resort: recursive search under each base
        for base in candidates_bases:
            if base.exists():
                for p in base.rglob(f'*{scenario}*.csv'):
                    paths.append(str(p))

    # remove duplicates while preserving order
    seen = set()
    unique = []
    for p in paths:
        if p not in seen:
            seen.add(p)
            unique.append(p)
    return unique


def plot_energy_efficiency(df, scenario_name, output_dir='output'):
    """
    Calcule et affiche l'efficacité énergétique (packet delivery rate/Énergie)
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    # Calculer l'efficacité énergétique
    df['Efficiency'] = df['PDR_Percent'] / df['AvgEnergy_mJ']
    
    fig, ax = plt.subplots(figsize=(12, 6))
    
    # Grouper par algorithme
    efficiency_data = df.groupby('alg')['Efficiency'].agg(['mean', 'std']).reset_index()
    
    x = np.arange(len(efficiency_data))
    width = 0.6
    
    bars = ax.bar(x, efficiency_data['mean'], width, 
                  yerr=efficiency_data['std'], 
                  capsize=10, alpha=0.8)
    
    # Colorer les barres
    for i, (bar, alg) in enumerate(zip(bars, efficiency_data['alg'])):
        bar.set_color(COLORS[alg])
        bar.set_edgecolor('black')
        bar.set_linewidth(1.5)
    
    ax.set_xlabel('Algorithme', fontweight='bold', fontsize=12)
    ax.set_ylabel('Efficacité énergétique (packet delivery rate/mJ)', fontweight='bold', fontsize=12)
    ax.set_title(f'Efficacité énergétique - Scénario: {scenario_name.upper()}', 
                fontweight='bold', fontsize=14)
    ax.set_xticks(x)
    ax.set_xticklabels(efficiency_data['alg'])
    ax.grid(True, alpha=0.3, axis='y')
    
    # Ajouter les valeurs sur les barres
    for i, (bar, val, std) in enumerate(zip(bars, efficiency_data['mean'], efficiency_data['std'])):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{val:.2f}\n±{std:.2f}',
                ha='center', va='bottom', fontweight='bold')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/efficacite_energetique_{scenario_name}.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Graphique sauvegardé: efficacite_energetique_{scenario_name}.png")


def plot_sigma_impact(df_sigma, output_dir='output'):
    """
    Analyse l'impact du paramètre Sigma sur les performances
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    fig, axes = plt.subplots(1, 2, figsize=(16, 6))
    fig.suptitle('Impact du paramètre Sigma (écart-type de shadow fading)', 
                 fontsize=16, fontweight='bold')
    
    # packet delivery rate en fonction de Sigma
    ax1 = axes[0]
    for alg in df_sigma['alg'].unique():
        alg_data = df_sigma[df_sigma['alg'] == alg].groupby('Sigma')['PDR_Percent'].mean().reset_index()
        ax1.plot(alg_data['Sigma'], alg_data['PDR_Percent'], 
                marker=MARKERS[alg], color=COLORS[alg], 
                label=alg, linewidth=2.5, markersize=10)
    
    ax1.set_xlabel('Sigma (dB)', fontweight='bold', fontsize=12)
    ax1.set_ylabel('packet delivery rate (%)', fontweight='bold', fontsize=12)
    ax1.set_title('packet delivery rate vs Sigma', fontweight='bold')
    ax1.set_ylim(0, 100)
    ax1.set_yticks(np.arange(0, 101, 20))
    ax1.legend(loc='best')
    ax1.grid(True, alpha=0.3)
    
    # Énergie en fonction de Sigma
    ax2 = axes[1]
    for alg in df_sigma['alg'].unique():
        alg_data = df_sigma[df_sigma['alg'] == alg].groupby('Sigma')['AvgEnergy_mJ'].mean().reset_index()
        ax2.plot(alg_data['Sigma'], alg_data['AvgEnergy_mJ'], 
                marker=MARKERS[alg], color=COLORS[alg], 
                label=alg, linewidth=2.5, markersize=10)
    
    ax2.set_xlabel('Sigma (dB)', fontweight='bold', fontsize=12)
    ax2.set_ylabel('Énergie moyenne (mJ)', fontweight='bold', fontsize=12)
    ax2.set_title('Énergie vs Sigma', fontweight='bold')
    ax2.legend(loc='best')
    ax2.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/impact_sigma.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Graphique sauvegardé: impact_sigma.png")


def plot_density_impact_mobility0(df_density, output_dir='output'):
    """
    Trace l'impact de la densité (NumDevices) sur le packet delivery rate
    et l'énergie en filtrant uniquement les points où MobilitySpeed == 0.
    Sauvegarde des images séparées: `pdr_density_mob0.png` et `energy_density_mob0.png`.
    """
    Path(output_dir).mkdir(exist_ok=True)

    # Filter for MobilitySpeed == 0 (tolerate numeric strings)
    if 'MobilitySpeed' in df_density.columns:
        dff = df_density.copy()
        dff['MobilitySpeed'] = pd.to_numeric(dff['MobilitySpeed'], errors='coerce')
        dff = dff[dff['MobilitySpeed'] == 0.0]
    else:
        dff = df_density.copy()

    if dff.empty:
        print("⚠ Pas de données avec MobilitySpeed == 0 pour l'impact densité")
        return

    # PDR vs NumDevices (separate plot)
    plt.figure(figsize=(10, 6))
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in dff['alg'].unique():
            continue
        grp = dff[dff['alg'] == alg].groupby('NumDevices')['PDR_Percent'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='NumDevices')
        plt.plot(grp['NumDevices'], grp['PDR_Percent'], marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), label=alg, linewidth=2)

    plt.xlabel('NumDevices', fontweight='bold')
    plt.ylabel('packet delivery rate (%)', fontweight='bold')
    plt.title('Impact densité: packet delivery rate vs NumDevices (MobilitySpeed=0)', fontweight='bold')
    plt.xlim(100, 1000)
    plt.xticks(np.arange(100, 1001, 100))
    plt.ylim(0, 100)
    plt.yticks(np.arange(0, 101, 20))
    plt.grid(True)
    plt.legend()

    # Annotate NumDevices present
    try:
        nd_vals = sorted(pd.to_numeric(dff['NumDevices'], errors='coerce').dropna().unique())
        if len(nd_vals) > 0:
            nd_str = ','.join(str(int(v)) if float(v).is_integer() else str(v) for v in nd_vals)
            plt.gca().text(0.99, 0.02, f'NumDevices présents: {nd_str}', transform=plt.gca().transAxes, ha='right', va='bottom', fontsize=9, bbox=dict(facecolor='white', alpha=0.7, edgecolor='gray'))
    except Exception:
        pass

    out_pdr = f'{output_dir}/pdr_density_mob0.png'
    plt.tight_layout()
    plt.savefig(out_pdr, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Graphique sauvegardé: {out_pdr}")

    # Energy vs NumDevices (separate plot)
    plt.figure(figsize=(10, 6))
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in dff['alg'].unique():
            continue
        grp = dff[dff['alg'] == alg].groupby('NumDevices')['AvgEnergy_mJ'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='NumDevices')
        plt.plot(grp['NumDevices'], grp['AvgEnergy_mJ'], marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), label=alg, linewidth=2)

    plt.xlabel('NumDevices', fontweight='bold')
    plt.ylabel('Énergie moyenne (mJ)', fontweight='bold')
    plt.title('Impact densité: Énergie vs NumDevices (MobilitySpeed=0)', fontweight='bold')
    plt.xlim(100, 1000)
    plt.xticks(np.arange(100, 1001, 100))
    plt.grid(True)
    plt.legend()

    # Annotate NumDevices present
    try:
        nd_vals = sorted(pd.to_numeric(dff['NumDevices'], errors='coerce').dropna().unique())
        if len(nd_vals) > 0:
            nd_str = ','.join(str(int(v)) if float(v).is_integer() else str(v) for v in nd_vals)
            plt.gca().text(0.99, 0.02, f'NumDevices présents: {nd_str}', transform=plt.gca().transAxes, ha='right', va='bottom', fontsize=9, bbox=dict(facecolor='white', alpha=0.7, edgecolor='gray'))
    except Exception:
        pass

    out_energy = f'{output_dir}/energy_density_mob0.png'
    plt.tight_layout()
    plt.savefig(out_energy, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Graphique sauvegardé: {out_energy}")


def plot_traffic_impact_mobility0(df_traffic, output_dir='output'):
    """
    Trace l'impact de l'intervalle de trafic (TrafficInterval) sur PDR et Énergie
    en filtrant MobilitySpeed == 0. Sauvegarde des images séparées: `pdr_traffic_mob0.png` et `energy_traffic_mob0.png`.
    """
    Path(output_dir).mkdir(exist_ok=True)

    if 'MobilitySpeed' in df_traffic.columns:
        dff = df_traffic.copy()
        dff['MobilitySpeed'] = pd.to_numeric(dff['MobilitySpeed'], errors='coerce')
        dff = dff[dff['MobilitySpeed'] == 0.0]
    else:
        dff = df_traffic.copy()

    if dff.empty:
        print("⚠ Pas de données avec MobilitySpeed == 0 pour l'impact trafic")
        return

    # PDR vs TrafficInterval (separate plot)
    plt.figure(figsize=(10, 6))
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in dff['alg'].unique():
            continue
        grp = dff[dff['alg'] == alg].groupby('TrafficInterval')['PDR_Percent'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='TrafficInterval')
        plt.plot(grp['TrafficInterval'], grp['PDR_Percent'], marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), label=alg, linewidth=2)

    plt.xlabel('Intervalle de trafic (s)', fontweight='bold')
    plt.ylabel('packet delivery rate (%)', fontweight='bold')
    plt.title('Impact trafic: packet delivery rate vs TrafficInterval (MobilitySpeed=0)', fontweight='bold')
    plt.xlim(72, 3600)
    plt.xticks([72, 300, 600, 900, 1200, 1800, 2400, 3600])
    plt.ylim(0, 100)
    plt.yticks(np.arange(0, 101, 20))
    plt.grid(True, alpha=0.3)
    plt.legend()

    # Annotate NumDevices present
    try:
        nd_vals = sorted(pd.to_numeric(dff['NumDevices'], errors='coerce').dropna().unique())
        if len(nd_vals) > 0:
            nd_str = ','.join(str(int(v)) if float(v).is_integer() else str(v) for v in nd_vals)
            plt.gca().text(0.99, 0.02, f'NumDevices présents: {nd_str}', transform=plt.gca().transAxes, ha='right', va='bottom', fontsize=9, bbox=dict(facecolor='white', alpha=0.7, edgecolor='gray'))
    except Exception:
        pass

    out_pdr = f'{output_dir}/pdr_traffic_mob0.png'
    plt.tight_layout()
    plt.savefig(out_pdr, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Graphique sauvegardé: {out_pdr}")

    # Energy vs TrafficInterval (separate plot)
    plt.figure(figsize=(10, 6))
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in dff['alg'].unique():
            continue
        grp = dff[dff['alg'] == alg].groupby('TrafficInterval')['AvgEnergy_mJ'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='TrafficInterval')
        plt.plot(grp['TrafficInterval'], grp['AvgEnergy_mJ'], marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), label=alg, linewidth=2)

    plt.xlabel('Intervalle de trafic (s)', fontweight='bold')
    plt.ylabel('Énergie moyenne (mJ)', fontweight='bold')
    plt.title('Impact trafic: Énergie vs TrafficInterval (MobilitySpeed=0)', fontweight='bold')
    plt.xlim(72, 3600)
    plt.xticks([72, 300, 600, 900, 1200, 1800, 2400, 3600])
    plt.grid(True, alpha=0.3)
    plt.legend()

    # Annotate NumDevices present
    try:
        nd_vals = sorted(pd.to_numeric(dff['NumDevices'], errors='coerce').dropna().unique())
        if len(nd_vals) > 0:
            nd_str = ','.join(str(int(v)) if float(v).is_integer() else str(v) for v in nd_vals)
            plt.gca().text(0.99, 0.02, f'NumDevices présents: {nd_str}', transform=plt.gca().transAxes, ha='right', va='bottom', fontsize=9, bbox=dict(facecolor='white', alpha=0.7, edgecolor='gray'))
    except Exception:
        pass

    out_energy = f'{output_dir}/energy_traffic_mob0.png'
    plt.tight_layout()
    plt.savefig(out_energy, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Graphique sauvegardé: {out_energy}")


def plot_sigma_impact_mobility0(df_sigma, output_dir='output'):
    """
    Trace l'impact du paramètre Sigma sur PDR et Énergie
    en filtrant MobilitySpeed == 0. Sauvegarde des images séparées: `pdr_sigma_mob0.png` et `energy_sigma_mob0.png`.
    """
    Path(output_dir).mkdir(exist_ok=True)

    if 'MobilitySpeed' in df_sigma.columns:
        dff = df_sigma.copy()
        dff['MobilitySpeed'] = pd.to_numeric(dff['MobilitySpeed'], errors='coerce')
        dff = dff[dff['MobilitySpeed'] == 0.0]
    else:
        dff = df_sigma.copy()

    if dff.empty:
        print("⚠ Pas de données avec MobilitySpeed == 0 pour l'impact sigma")
        return

    # PDR vs Sigma (separate plot)
    plt.figure(figsize=(10, 6))
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in dff['alg'].unique():
            continue
        grp = dff[dff['alg'] == alg].groupby('Sigma')['PDR_Percent'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='Sigma')
        plt.plot(grp['Sigma'], grp['PDR_Percent'], marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), label=alg, linewidth=2.5, markersize=10)

    plt.xlabel('Sigma (dB)', fontweight='bold')
    plt.ylabel('packet delivery rate (%)', fontweight='bold')
    plt.title('Impact Sigma: packet delivery rate vs Sigma (MobilitySpeed=0)', fontweight='bold')
    plt.ylim(0, 100)
    plt.yticks(np.arange(0, 101, 20))
    plt.grid(True, alpha=0.3)
    plt.legend()

    # Annotate NumDevices present
    try:
        nd_vals = sorted(pd.to_numeric(dff['NumDevices'], errors='coerce').dropna().unique())
        if len(nd_vals) > 0:
            nd_str = ','.join(str(int(v)) if float(v).is_integer() else str(v) for v in nd_vals)
            plt.gca().text(0.99, 0.02, f'NumDevices présents: {nd_str}', transform=plt.gca().transAxes, ha='right', va='bottom', fontsize=9, bbox=dict(facecolor='white', alpha=0.7, edgecolor='gray'))
    except Exception:
        pass

    out_pdr = f'{output_dir}/pdr_sigma_mob0.png'
    plt.tight_layout()
    plt.savefig(out_pdr, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Graphique sauvegardé: {out_pdr}")

    # Energy vs Sigma (separate plot)
    plt.figure(figsize=(10, 6))
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in dff['alg'].unique():
            continue
        grp = dff[dff['alg'] == alg].groupby('Sigma')['AvgEnergy_mJ'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='Sigma')
        plt.plot(grp['Sigma'], grp['AvgEnergy_mJ'], marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), label=alg, linewidth=2.5, markersize=10)

    plt.xlabel('Sigma (dB)', fontweight='bold')
    plt.ylabel('Énergie moyenne (mJ)', fontweight='bold')
    plt.title('Impact Sigma: Énergie vs Sigma (MobilitySpeed=0)', fontweight='bold')
    plt.grid(True, alpha=0.3)
    plt.legend()

    # Annotate NumDevices present
    try:
        nd_vals = sorted(pd.to_numeric(dff['NumDevices'], errors='coerce').dropna().unique())
        if len(nd_vals) > 0:
            nd_str = ','.join(str(int(v)) if float(v).is_integer() else str(v) for v in nd_vals)
            plt.gca().text(0.99, 0.02, f'NumDevices présents: {nd_str}', transform=plt.gca().transAxes, ha='right', va='bottom', fontsize=9, bbox=dict(facecolor='white', alpha=0.7, edgecolor='gray'))
    except Exception:
        pass

    out_energy = f'{output_dir}/energy_sigma_mob0.png'
    plt.tight_layout()
    plt.savefig(out_energy, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Graphique sauvegardé: {out_energy}")


def plot_traffic_impact_fixed_params(dfs_dict, output_dir='output'):
    """
    Génère 2 graphiques: PDR et énergie en fonction de l'intervalle de trafic
    avec MobilitySpeed=0, NumDevices=550 et Sigma=3.96
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    # Utiliser toutes les données disponibles (combiner tous les scénarios)
    all_df_list = []
    for scenario_name, df in dfs_dict.items():
        all_df_list.append(df)
    
    if not all_df_list:
        print("⚠ Aucune donnée disponible pour l'analyse de l'intervalle de trafic")
        return
        
    all_df = pd.concat(all_df_list, ignore_index=True)
    
    # Normaliser les colonnes numériques
    for col in ['NumDevices', 'TrafficInterval', 'Sigma', 'MobilitySpeed']:
        if col in all_df.columns:
            all_df[col] = pd.to_numeric(all_df[col], errors='coerce')
    
    # Paramètres fixes
    mobility_speed = 0
    num_devices = 550
    sigma = 3.96
    
    # Filtrer les données selon les paramètres spécifiés
    filtered = all_df[
        (all_df['MobilitySpeed'] == mobility_speed) &
        (all_df['NumDevices'] == num_devices) &
        (all_df['Sigma'] == sigma)
    ]
    
    if filtered.empty:
        print(f"⚠ Pas de données pour MobilitySpeed={mobility_speed}, NumDevices={num_devices}, Sigma={sigma}")
        return
    
    # PDR vs TrafficInterval
    plt.figure(figsize=(10, 6))
    plotted = False
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in filtered['alg'].unique():
            continue
        grp = filtered[filtered['alg'] == alg].groupby('TrafficInterval')['PDR_Percent'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='TrafficInterval')
        # Convertir en messages par heure
        grp['MessagesPerHour'] = 3600 / grp['TrafficInterval']
        plt.plot(grp['MessagesPerHour'], grp['PDR_Percent'], 
                marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                label=alg, linewidth=2.5, markersize=8)
        plotted = True
    
    if plotted:
        # Définir les ticks en messages par heure
        traffic_seconds = [72, 300, 600, 900, 1200, 1800, 2400, 3600]
        messages_per_hour_ticks = [int(3600/t) for t in traffic_seconds]
        plt.xlabel('Messages per Hour', fontweight='bold')
        plt.ylabel('Packet Delivery Rate (%)', fontweight='bold')
        plt.title(f'PDR vs Messages per Hour (MobilitySpeed={mobility_speed}, NumDevices={num_devices}, Sigma={sigma})', fontweight='bold')
        plt.xlim(1, 50)
        plt.xticks(messages_per_hour_ticks, [f'{m}' for m in messages_per_hour_ticks])
        plt.ylim(0, 100)
        plt.yticks(np.arange(0, 101, 20))
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_pdr = f'{output_dir}/traffic_pdr_mobility{mobility_speed}_density{num_devices}_sigma{sigma}.png'
        plt.tight_layout()
        plt.savefig(out_pdr, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_pdr}")
    else:
        plt.close()
    
    # Energy vs TrafficInterval
    plt.figure(figsize=(10, 6))
    plotted = False
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in filtered['alg'].unique():
            continue
        grp = filtered[filtered['alg'] == alg].groupby('TrafficInterval')['AvgEnergy_mJ'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='TrafficInterval')
        # Convertir en messages par heure
        grp['MessagesPerHour'] = 3600 / grp['TrafficInterval']
        plt.plot(grp['MessagesPerHour'], grp['AvgEnergy_mJ'], 
                marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                label=alg, linewidth=2.5, markersize=8)
        plotted = True
    
    if plotted:
        # Définir les ticks en messages par heure
        traffic_seconds = [72, 300, 600, 900, 1200, 1800, 2400, 3600]
        messages_per_hour_ticks = [int(3600/t) for t in traffic_seconds]
        plt.xlabel('Messages per Hour', fontweight='bold')
        plt.ylabel('Energy Consumption (mJ)', fontweight='bold')
        plt.title(f'Energy vs Messages per Hour (MobilitySpeed={mobility_speed}, NumDevices={num_devices}, Sigma={sigma})', fontweight='bold')
        plt.xlim(1, 50)
        plt.xticks(messages_per_hour_ticks, [f'{m}' for m in messages_per_hour_ticks])
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_energy = f'{output_dir}/traffic_energy_mobility{mobility_speed}_density{num_devices}_sigma{sigma}.png'
        plt.tight_layout()
        plt.savefig(out_energy, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_energy}")
    else:
        plt.close()


def plot_sigma_impact_fixed_params(dfs_dict, output_dir='output'):
    """
    Génère 2 graphiques: PDR et énergie en fonction de Sigma (canal de saturation)
    avec MobilitySpeed=0, NumDevices=550 et TrafficInterval=3600
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    # Utiliser toutes les données disponibles (combiner tous les scénarios)
    all_df_list = []
    for scenario_name, df in dfs_dict.items():
        all_df_list.append(df)
    
    if not all_df_list:
        print("⚠ Aucune donnée disponible pour l'analyse de sigma")
        return
        
    all_df = pd.concat(all_df_list, ignore_index=True)
    
    # Normaliser les colonnes numériques
    for col in ['NumDevices', 'TrafficInterval', 'Sigma', 'MobilitySpeed']:
        if col in all_df.columns:
            all_df[col] = pd.to_numeric(all_df[col], errors='coerce')
    
    # Paramètres fixes
    mobility_speed = 0
    num_devices = 550
    traffic_interval = 3600
    
    # Filtrer les données selon les paramètres spécifiés
    filtered = all_df[
        (all_df['MobilitySpeed'] == mobility_speed) &
        (all_df['NumDevices'] == num_devices) &
        (all_df['TrafficInterval'] == traffic_interval)
    ]
    
    if filtered.empty:
        print(f"⚠ Pas de données pour MobilitySpeed={mobility_speed}, NumDevices={num_devices}, TrafficInterval={traffic_interval}")
        return
    
    # PDR vs Sigma
    plt.figure(figsize=(10, 6))
    plotted = False
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in filtered['alg'].unique():
            continue
        grp = filtered[filtered['alg'] == alg].groupby('Sigma')['PDR_Percent'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='Sigma')
        plt.plot(grp['Sigma'], grp['PDR_Percent'], 
                marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                label=alg, linewidth=2.5, markersize=8)
        plotted = True
    
    if plotted:
        plt.xlabel('Sigma (dB)', fontweight='bold')
        plt.ylabel('Packet Delivery Rate (%)', fontweight='bold')
        plt.title(f'PDR vs Sigma (MobilitySpeed={mobility_speed}, NumDevices={num_devices}, TrafficInterval={traffic_interval}s)', fontweight='bold')
        plt.ylim(0, 100)
        plt.yticks(np.arange(0, 101, 20))
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_pdr = f'{output_dir}/sigma_pdr_mobility{mobility_speed}_density{num_devices}_traffic{traffic_interval}.png'
        plt.tight_layout()
        plt.savefig(out_pdr, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_pdr}")
    else:
        plt.close()
    
    # Energy vs Sigma
    plt.figure(figsize=(10, 6))
    plotted = False
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in filtered['alg'].unique():
            continue
        grp = filtered[filtered['alg'] == alg].groupby('Sigma')['AvgEnergy_mJ'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='Sigma')
        plt.plot(grp['Sigma'], grp['AvgEnergy_mJ'], 
                marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                label=alg, linewidth=2.5, markersize=8)
        plotted = True
    
    if plotted:
        plt.xlabel('Sigma (dB)', fontweight='bold')
        plt.ylabel('Energy Consumption (mJ)', fontweight='bold')
        plt.title(f'Energy vs Sigma (MobilitySpeed={mobility_speed}, NumDevices={num_devices}, TrafficInterval={traffic_interval}s)', fontweight='bold')
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_energy = f'{output_dir}/sigma_energy_mobility{mobility_speed}_density{num_devices}_traffic{traffic_interval}.png'
        plt.tight_layout()
        plt.savefig(out_energy, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_energy}")
    else:
        plt.close()


def plot_density_impact_fixed_params(dfs_dict, output_dir='output'):
    """
    Génère 2 graphiques: PDR et énergie en fonction de la densité
    avec MobilitySpeed=0, TrafficInterval=3600 et Sigma=3.96
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    # Utiliser toutes les données disponibles (combiner tous les scénarios)
    all_df_list = []
    for scenario_name, df in dfs_dict.items():
        all_df_list.append(df)
    
    if not all_df_list:
        print("⚠ Aucune donnée disponible pour l'analyse de densité")
        return
        
    all_df = pd.concat(all_df_list, ignore_index=True)
    
    # Normaliser les colonnes numériques
    for col in ['NumDevices', 'TrafficInterval', 'Sigma', 'MobilitySpeed']:
        if col in all_df.columns:
            all_df[col] = pd.to_numeric(all_df[col], errors='coerce')
    
    # Paramètres fixes
    mobility_speed = 0
    traffic_interval = 3600
    sigma = 3.96
    
    # Filtrer les données selon les paramètres spécifiés
    filtered = all_df[
        (all_df['MobilitySpeed'] == mobility_speed) &
        (all_df['TrafficInterval'] == traffic_interval) &
        (all_df['Sigma'] == sigma)
    ]
    
    if filtered.empty:
        print(f"⚠ Pas de données pour MobilitySpeed={mobility_speed}, TrafficInterval={traffic_interval}s, Sigma={sigma}")
        return
    
    # PDR vs NumDevices
    plt.figure(figsize=(10, 6))
    plotted = False
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in filtered['alg'].unique():
            continue
        grp = filtered[filtered['alg'] == alg].groupby('NumDevices')['PDR_Percent'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='NumDevices')
        plt.plot(grp['NumDevices'], grp['PDR_Percent'], 
                marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                label=alg, linewidth=2.5, markersize=8)
        plotted = True
    
    if plotted:
        plt.xlabel('Number of Nodes', fontweight='bold')
        plt.ylabel('Packet Delivery Rate (%)', fontweight='bold')
        plt.title(f'PDR vs Node Density (MobilitySpeed={mobility_speed}, TrafficInterval={traffic_interval}s, Sigma={sigma})', fontweight='bold')
        plt.xlim(100, 1000)
        plt.xticks(np.arange(100, 1001, 100))
        plt.ylim(0, 100)
        plt.yticks(np.arange(0, 101, 20))
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_pdr = f'{output_dir}/density_pdr_mobility{mobility_speed}_traffic{traffic_interval}_sigma{sigma}.png'
        plt.tight_layout()
        plt.savefig(out_pdr, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_pdr}")
    else:
        plt.close()
    
    # Energy vs NumDevices
    plt.figure(figsize=(10, 6))
    plotted = False
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in filtered['alg'].unique():
            continue
        grp = filtered[filtered['alg'] == alg].groupby('NumDevices')['AvgEnergy_mJ'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='NumDevices')
        plt.plot(grp['NumDevices'], grp['AvgEnergy_mJ'], 
                marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                label=alg, linewidth=2.5, markersize=8)
        plotted = True
    
    if plotted:
        plt.xlabel('Number of Nodes', fontweight='bold')
        plt.ylabel('Energy Consumption (mJ)', fontweight='bold')
        plt.title(f'Energy vs Node Density (MobilitySpeed={mobility_speed}, TrafficInterval={traffic_interval}s, Sigma={sigma})', fontweight='bold')
        plt.xlim(100, 1000)
        plt.xticks(np.arange(100, 1001, 100))
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_energy = f'{output_dir}/density_energy_mobility{mobility_speed}_traffic{traffic_interval}_sigma{sigma}.png'
        plt.tight_layout()
        plt.savefig(out_energy, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_energy}")
    else:
        plt.close()


def plot_density_scenario_specific(df_density, output_dir='output'):
    """
    Génère 2 graphiques pour le scénario density: PDR et énergie en fonction de la densité
    avec MobilitySpeed=0, TrafficInterval=3600 et Sigma=3.96
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    # Normaliser les colonnes numériques
    for col in ['NumDevices', 'TrafficInterval', 'Sigma', 'MobilitySpeed']:
        if col in df_density.columns:
            df_density[col] = pd.to_numeric(df_density[col], errors='coerce')
    
    # Paramètres fixes
    mobility_speed = 0
    traffic_interval = 3600
    sigma = 3.96
    
    # Filtrer les données selon les paramètres spécifiés
    filtered = df_density[
        (df_density['MobilitySpeed'] == mobility_speed) &
        (df_density['TrafficInterval'] == traffic_interval) &
        (df_density['Sigma'] == sigma)
    ]
    
    if filtered.empty:
        print(f"⚠ Pas de données pour le scénario density avec MobilitySpeed={mobility_speed}, TrafficInterval={traffic_interval}s, Sigma={sigma}")
        return
    
    # PDR vs NumDevices
    plt.figure(figsize=(10, 6))
    plotted = False
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in filtered['alg'].unique():
            continue
        grp = filtered[filtered['alg'] == alg].groupby('NumDevices')['PDR_Percent'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='NumDevices')
        plt.plot(grp['NumDevices'], grp['PDR_Percent'], 
                marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                label=alg, linewidth=2.5, markersize=8)
        plotted = True
    
    if plotted:
        plt.xlabel('Number of Nodes', fontweight='bold')
        plt.ylabel('Packet Delivery Rate (%)', fontweight='bold')
        plt.title(f'PDR vs Node Density - Density Scenario (MobilitySpeed={mobility_speed}, TrafficInterval={traffic_interval}s, Sigma={sigma})', fontweight='bold')
        plt.xlim(100, 1000)
        plt.xticks(np.arange(100, 1001, 100))
        plt.ylim(0, 100)
        plt.yticks(np.arange(0, 101, 20))
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_pdr = f'{output_dir}/density_scenario_pdr_mobility{mobility_speed}_traffic{traffic_interval}_sigma{sigma}.png'
        plt.tight_layout()
        plt.savefig(out_pdr, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_pdr}")
    else:
        plt.close()
    
    # Energy vs NumDevices
    plt.figure(figsize=(10, 6))
    plotted = False
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in filtered['alg'].unique():
            continue
        grp = filtered[filtered['alg'] == alg].groupby('NumDevices')['AvgEnergy_mJ'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='NumDevices')
        plt.plot(grp['NumDevices'], grp['AvgEnergy_mJ'], 
                marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                label=alg, linewidth=2.5, markersize=8)
        plotted = True
    
    if plotted:
        plt.xlabel('Number of Nodes', fontweight='bold')
        plt.ylabel('Energy Consumption (mJ)', fontweight='bold')
        plt.title(f'Energy vs Node Density - Density Scenario (MobilitySpeed={mobility_speed}, TrafficInterval={traffic_interval}s, Sigma={sigma})', fontweight='bold')
        plt.xlim(100, 1000)
        plt.xticks(np.arange(100, 1001, 100))
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_energy = f'{output_dir}/density_scenario_energy_mobility{mobility_speed}_traffic{traffic_interval}_sigma{sigma}.png'
        plt.tight_layout()
        plt.savefig(out_energy, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_energy}")
    else:
        plt.close()


def plot_sigma_scenario_specific(df_sigma, output_dir='output'):
    """
    Génère 2 graphiques pour le scénario sigma: PDR et énergie en fonction de Sigma
    avec MobilitySpeed=0, NumDevices=550 et TrafficInterval=3600
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    # Normaliser les colonnes numériques
    for col in ['NumDevices', 'TrafficInterval', 'Sigma', 'MobilitySpeed']:
        if col in df_sigma.columns:
            df_sigma[col] = pd.to_numeric(df_sigma[col], errors='coerce')
    
    # Paramètres fixes
    mobility_speed = 0
    num_devices = 1000
    traffic_interval = 3600
    
    # Filtrer les données selon les paramètres spécifiés
    filtered = df_sigma[
        (df_sigma['MobilitySpeed'] == mobility_speed) &
        (df_sigma['NumDevices'] == num_devices) &
        (df_sigma['TrafficInterval'] == traffic_interval)
    ]
    
    if filtered.empty:
        print(f"⚠ Pas de données pour le scénario sigma avec MobilitySpeed={mobility_speed}, NumDevices={num_devices}, TrafficInterval={traffic_interval}s")
        return
    
    # PDR vs Sigma
    plt.figure(figsize=(10, 6))
    plotted = False
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in filtered['alg'].unique():
            continue
        grp = filtered[filtered['alg'] == alg].groupby('Sigma')['PDR_Percent'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='Sigma')
        plt.plot(grp['Sigma'], grp['PDR_Percent'], 
                marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                label=alg, linewidth=2.5, markersize=8)
        plotted = True
    
    if plotted:
        plt.xlabel('Sigma (dB)', fontweight='bold')
        plt.ylabel('Packet Delivery Rate (%)', fontweight='bold')
        plt.title(f'PDR vs Sigma - Sigma Scenario (MobilitySpeed={mobility_speed}, NumDevices={num_devices}, TrafficInterval={traffic_interval}s)', fontweight='bold')
        plt.ylim(0, 100)
        plt.yticks(np.arange(0, 101, 20))
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_pdr = f'{output_dir}/sigma_scenario_pdr_mobility{mobility_speed}_density{num_devices}_traffic{traffic_interval}.png'
        plt.tight_layout()
        plt.savefig(out_pdr, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_pdr}")
    else:
        plt.close()
    
    # Energy vs Sigma
    plt.figure(figsize=(10, 6))
    plotted = False
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in filtered['alg'].unique():
            continue
        grp = filtered[filtered['alg'] == alg].groupby('Sigma')['AvgEnergy_mJ'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='Sigma')
        plt.plot(grp['Sigma'], grp['AvgEnergy_mJ'], 
                marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                label=alg, linewidth=2.5, markersize=8)
        plotted = True
    
    if plotted:
        plt.xlabel('Sigma (dB)', fontweight='bold')
        plt.ylabel('Energy Consumption (mJ)', fontweight='bold')
        plt.title(f'Energy vs Sigma - Sigma Scenario (MobilitySpeed={mobility_speed}, NumDevices={num_devices}, TrafficInterval={traffic_interval}s)', fontweight='bold')
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_energy = f'{output_dir}/sigma_scenario_energy_mobility{mobility_speed}_density{num_devices}_traffic{traffic_interval}.png'
        plt.tight_layout()
        plt.savefig(out_energy, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_energy}")
    else:
        plt.close()


def plot_traffic_scenario_specific(df_traffic, output_dir='output'):
    """
    Génère 2 graphiques pour le scénario intervalle_d_envoie: PDR et énergie en fonction de TrafficInterval
    avec MobilitySpeed=0, NumDevices=550 et Sigma=3.96, puis avec NumDevices=1000
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    # Normaliser les colonnes numériques
    for col in ['NumDevices', 'TrafficInterval', 'Sigma', 'MobilitySpeed']:
        if col in df_traffic.columns:
            df_traffic[col] = pd.to_numeric(df_traffic[col], errors='coerce')
    
    # Paramètres fixes communs
    mobility_speed = 0
    sigma = 3.96
    
    # Générer des graphiques pour différentes densités
    densities = [1000]
    
    for num_devices in densities:
        
        # Filtrer les données selon les paramètres spécifiés
        filtered = df_traffic[
            (df_traffic['MobilitySpeed'] == mobility_speed) &
            (df_traffic['NumDevices'] == num_devices) &
            (df_traffic['Sigma'] == sigma)
        ]
        
        if filtered.empty:
            print(f"⚠ Pas de données pour le scénario intervalle_d_envoie avec MobilitySpeed={mobility_speed}, NumDevices={num_devices}, Sigma={sigma}")
            continue
        
        # Définir les ticks pour TrafficInterval et convertir en messages par heure
        traffic_ticks = [72, 360, 1800, 3600]
        messages_per_hour = [int(3600/t) for t in traffic_ticks]  # Conversion en messages/heure
        
        # PDR vs TrafficInterval (en messages par heure)
        plt.figure(figsize=(10, 6))
        plotted = False
        for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
            if alg not in filtered['alg'].unique():
                continue
            grp = filtered[filtered['alg'] == alg].groupby('TrafficInterval')['PDR_Percent'].mean().reset_index()
            if grp.empty:
                continue
            grp = grp.sort_values(by='TrafficInterval')
            # Convertir TrafficInterval en messages par heure pour l'affichage
            grp['MessagesPerHour'] = 3600 / grp['TrafficInterval']
            plt.plot(grp['MessagesPerHour'], grp['PDR_Percent'], 
                    marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                    label=alg, linewidth=2.5, markersize=8)
            plotted = True
        
        if plotted:
            plt.xlabel('Messages per Hour', fontweight='bold')
            plt.ylabel('Packet Delivery Rate (%)', fontweight='bold')
            plt.title(f'PDR vs Messages per Hour - Traffic Scenario (MobilitySpeed={mobility_speed}, NumDevices={num_devices}, Sigma={sigma})', fontweight='bold')
            plt.ylim(0, 100)
            plt.yticks(np.arange(0, 101, 20))
            plt.xlim(1, 60)
            plt.xticks(messages_per_hour, [f'{m} messages per hour' for m in messages_per_hour])
            plt.grid(True, alpha=0.3)
            plt.legend()
            
            out_pdr = f'{output_dir}/traffic_scenario_pdr_mobility{mobility_speed}_density{num_devices}_sigma{sigma}.png'
            plt.tight_layout()
            plt.savefig(out_pdr, dpi=300, bbox_inches='tight')
            plt.close()
            print(f"✓ Graphique sauvegardé: {out_pdr}")
        else:
            plt.close()
        
        # Energy vs TrafficInterval (en messages par heure)
        plt.figure(figsize=(10, 6))
        plotted = False
        for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
            if alg not in filtered['alg'].unique():
                continue
            grp = filtered[filtered['alg'] == alg].groupby('TrafficInterval')['AvgEnergy_mJ'].mean().reset_index()
            if grp.empty:
                continue
            grp = grp.sort_values(by='TrafficInterval')
            # Convertir TrafficInterval en messages par heure pour l'affichage
            grp['MessagesPerHour'] = 3600 / grp['TrafficInterval']
            plt.plot(grp['MessagesPerHour'], grp['AvgEnergy_mJ'], 
                    marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                    label=alg, linewidth=2.5, markersize=8)
            plotted = True
        
        if plotted:
            plt.xlabel('Messages per Hour', fontweight='bold')
            plt.ylabel('Energy Consumption (mJ)', fontweight='bold')
            plt.title(f'Energy vs Messages per Hour - Traffic Scenario (MobilitySpeed={mobility_speed}, NumDevices={num_devices}, Sigma={sigma})', fontweight='bold')
            plt.xlim(1, 60)
            plt.xticks(messages_per_hour, [f'{m} messages per hour' for m in messages_per_hour])
            plt.grid(True, alpha=0.3)
            plt.legend()
            
            out_energy = f'{output_dir}/traffic_scenario_energy_mobility{mobility_speed}_density{num_devices}_sigma{sigma}.png'
            plt.tight_layout()
            plt.savefig(out_energy, dpi=300, bbox_inches='tight')
            plt.close()
            print(f"✓ Graphique sauvegardé: {out_energy}")
        else:
            plt.close()


def plot_traffic_scenario_mobility33(df_traffic, output_dir='output'):
    """
    Génère 2 graphiques pour le scénario intervalle_d_envoie: PDR et énergie en fonction de TrafficInterval
    avec MobilitySpeed=33.33, NumDevices=1000 et Sigma=3.96
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    # Normaliser les colonnes numériques
    for col in ['NumDevices', 'TrafficInterval', 'Sigma', 'MobilitySpeed']:
        if col in df_traffic.columns:
            df_traffic[col] = pd.to_numeric(df_traffic[col], errors='coerce')
    
    # Paramètres fixes
    mobility_speed = 0
    num_devices = 1000
    sigma = 3.96
    
    # Filtrer les données selon les paramètres spécifiés
    filtered = df_traffic[
        (df_traffic['MobilitySpeed'] == mobility_speed) &
        (df_traffic['NumDevices'] == num_devices) &
        (df_traffic['Sigma'] == sigma)
    ]
    
    if filtered.empty:
        print(f"⚠ Pas de données pour le scénario intervalle_d_envoie avec MobilitySpeed={mobility_speed}, NumDevices={num_devices}, Sigma={sigma}")
        return
    
    # Définir les ticks pour TrafficInterval et convertir en messages par heure
    traffic_ticks = [72, 360, 1800, 3600]
    messages_per_hour = [int(3600/t) for t in traffic_ticks]
    
    # PDR vs TrafficInterval (en messages par heure)
    plt.figure(figsize=(10, 6))
    plotted = False
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in filtered['alg'].unique():
            continue
        grp = filtered[filtered['alg'] == alg].groupby('TrafficInterval')['PDR_Percent'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='TrafficInterval')
        # Convertir TrafficInterval en messages par heure
        grp['MessagesPerHour'] = 3600 / grp['TrafficInterval']
        plt.plot(grp['MessagesPerHour'], grp['PDR_Percent'], 
                marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                label=alg, linewidth=2.5, markersize=8)
        plotted = True
    
    if plotted:
        plt.xlabel('Messages per Hour', fontweight='bold')
        plt.ylabel('Packet Delivery Rate (%)', fontweight='bold')
        plt.title(f'PDR vs Messages per Hour - Traffic Scenario (MobilitySpeed={mobility_speed}, NumDevices={num_devices}, Sigma={sigma})', fontweight='bold')
        plt.ylim(0, 100)
        plt.yticks(np.arange(0, 101, 20))
        plt.xlim(1, 60)
        plt.xticks(messages_per_hour, [f'{m} messages per hour' for m in messages_per_hour])
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_pdr = f'{output_dir}/traffic_scenario_pdr_mobility{mobility_speed}_density{num_devices}_sigma{sigma}.png'
        plt.tight_layout()
        plt.savefig(out_pdr, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_pdr}")
    else:
        plt.close()
    
    # Energy vs TrafficInterval
    plt.figure(figsize=(10, 6))
    plotted = False
    for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
        if alg not in filtered['alg'].unique():
            continue
        grp = filtered[filtered['alg'] == alg].groupby('TrafficInterval')['AvgEnergy_mJ'].mean().reset_index()
        if grp.empty:
            continue
        grp = grp.sort_values(by='TrafficInterval')
        # Convertir TrafficInterval en messages par heure
        grp['MessagesPerHour'] = 3600 / grp['TrafficInterval']
        plt.plot(grp['MessagesPerHour'], grp['AvgEnergy_mJ'], 
                marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                label=alg, linewidth=2.5, markersize=8)
        plotted = True
    
    if plotted:
        plt.xlabel('Messages per Hour', fontweight='bold')
        plt.ylabel('Energy Consumption (mJ)', fontweight='bold')
        plt.title(f'Energy vs Messages per Hour - Traffic Scenario (MobilitySpeed={mobility_speed}, NumDevices={num_devices}, Sigma={sigma})', fontweight='bold')
        plt.xlim(1, 60)
        plt.xticks(messages_per_hour, [f'{m} messages per hour' for m in messages_per_hour])
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_energy = f'{output_dir}/traffic_scenario_energy_mobility{mobility_speed}_density{num_devices}_sigma{sigma}.png'
        plt.tight_layout()
        plt.savefig(out_energy, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_energy}")
    else:
        plt.close()


def plot_mobility_impact_specific_params(dfs_dict, output_dir='output'):
    """
    Génère 6 courbes (3 PDR + 3 énergie) en fonction de la mobilité avec des paramètres spécifiques:
    - densité 100, intervalle 3600s, sigma 3.96
    - densité 550, intervalle 3600s, sigma 3.96  
    - densité 1000, intervalle 3600s, sigma 3.96
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    # Configuration des 3 cas
    configs = [
        {'NumDevices': 100, 'TrafficInterval': 3600, 'Sigma': 3.96, 'label': '100 nodes'},
        {'NumDevices': 550, 'TrafficInterval': 3600, 'Sigma': 3.96, 'label': '550 nodes'},
        {'NumDevices': 1000, 'TrafficInterval': 3600, 'Sigma': 3.96, 'label': '1000 nodes'}
    ]
    
    # Utiliser toutes les données disponibles (combiner tous les scénarios)
    all_df_list = []
    for scenario_name, df in dfs_dict.items():
        all_df_list.append(df)
    
    if not all_df_list:
        print("⚠ Aucune donnée disponible pour l'analyse de mobilité")
        return
        
    all_df = pd.concat(all_df_list, ignore_index=True)
    
    # Normaliser les colonnes numériques
    for col in ['NumDevices', 'TrafficInterval', 'Sigma', 'MobilitySpeed']:
        if col in all_df.columns:
            all_df[col] = pd.to_numeric(all_df[col], errors='coerce')
    
    # Génerer les graphiques PDR
    for i, config in enumerate(configs):
        # Filtrer les données selon la configuration
        filtered = all_df[
            (all_df['NumDevices'] == config['NumDevices']) &
            (all_df['TrafficInterval'] == config['TrafficInterval']) &
            (all_df['Sigma'] == config['Sigma'])
        ]
        
        if filtered.empty:
            print(f"⚠ Pas de données pour la configuration: {config}")
            continue
        
        # PDR vs MobilitySpeed
        plt.figure(figsize=(10, 6))
        plotted = False
        for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
            if alg not in filtered['alg'].unique():
                continue
            grp = filtered[filtered['alg'] == alg].groupby('MobilitySpeed')['PDR_Percent'].mean().reset_index()
            if grp.empty:
                continue
            grp = grp.sort_values(by='MobilitySpeed')
            plt.plot(grp['MobilitySpeed'], grp['PDR_Percent'], 
                    marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                    label=alg, linewidth=2.5, markersize=8)
            plotted = True
        
        if not plotted:
            plt.close()
            continue
            
        plt.xlabel('Mobilité des nœuds (m/s)', fontweight='bold')
        plt.ylabel('packet delivery rate (%)', fontweight='bold')
        plt.title(f'PDR vs Mobilité - {config["label"]} (TrafficInterval=3600s, Sigma=3.96)', fontweight='bold')
        plt.ylim(0, 100)
        plt.yticks(np.arange(0, 101, 20))
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_pdr = f'{output_dir}/pdr_mobility_{config["NumDevices"]}nodes_3600s_396sigma.png'
        plt.tight_layout()
        plt.savefig(out_pdr, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_pdr}")
        
        # Energy vs MobilitySpeed
        plt.figure(figsize=(10, 6))
        plotted = False
        for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
            if alg not in filtered['alg'].unique():
                continue
            grp = filtered[filtered['alg'] == alg].groupby('MobilitySpeed')['AvgEnergy_mJ'].mean().reset_index()
            if grp.empty:
                continue
            grp = grp.sort_values(by='MobilitySpeed')
            plt.plot(grp['MobilitySpeed'], grp['AvgEnergy_mJ'], 
                    marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), 
                    label=alg, linewidth=2.5, markersize=8)
            plotted = True
        
        if not plotted:
            plt.close()
            continue
            
        plt.xlabel('Mobilité des nœuds (m/s)', fontweight='bold')
        plt.ylabel('Énergie moyenne (mJ)', fontweight='bold')
        plt.title(f'Énergie vs Mobilité - {config["label"]} (TrafficInterval=3600s, Sigma=3.96)', fontweight='bold')
        plt.grid(True, alpha=0.3)
        plt.legend()
        
        out_energy = f'{output_dir}/energy_mobility_{config["NumDevices"]}nodes_3600s_396sigma.png'
        plt.tight_layout()
        plt.savefig(out_energy, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"✓ Graphique sauvegardé: {out_energy}")


def plot_traffic_interval_analysis(df_traffic, output_dir='output'):
    """
    Analyse l'impact de l'intervalle de trafic
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('Impact de l\'intervalle de trafic (Traffic Interval)', 
                 fontsize=16, fontweight='bold')
    
    # 1. packet delivery rate en fonction de l'intervalle
    ax1 = axes[0, 0]
    for alg in df_traffic['alg'].unique():
        alg_data = df_traffic[df_traffic['alg'] == alg].groupby('TrafficInterval')['PDR_Percent'].mean().reset_index()
        ax1.plot(alg_data['TrafficInterval'], alg_data['PDR_Percent'], 
                marker=MARKERS[alg], color=COLORS[alg], 
                label=alg, linewidth=2.5, markersize=10)
    
    ax1.set_xlabel('Intervalle de trafic (s)', fontweight='bold')
    ax1.set_ylabel('packet delivery rate (%)', fontweight='bold')
    ax1.set_title('packet delivery rate vs Intervalle de trafic', fontweight='bold')
    # limit and ticks for traffic interval requested by user
    ax1.set_xlim(72, 3600)
    ax1.set_xticks([72, 300, 600, 900, 1200, 1800, 2400, 3600])
    ax1.set_ylim(0, 100)
    ax1.set_yticks(np.arange(0, 101, 20))
    ax1.set_xscale('log')
    ax1.legend(loc='best')
    ax1.grid(True, alpha=0.3, which='both')
    
    # 2. Énergie en fonction de l'intervalle
    ax2 = axes[0, 1]
    for alg in df_traffic['alg'].unique():
        alg_data = df_traffic[df_traffic['alg'] == alg].groupby('TrafficInterval')['AvgEnergy_mJ'].mean().reset_index()
        ax2.plot(alg_data['TrafficInterval'], alg_data['AvgEnergy_mJ'], 
                marker=MARKERS[alg], color=COLORS[alg], 
                label=alg, linewidth=2.5, markersize=10)
    
    ax2.set_xlabel('Intervalle de trafic (s)', fontweight='bold')
    ax2.set_ylabel('Énergie moyenne (mJ)', fontweight='bold')
    ax2.set_title('Énergie vs Intervalle de trafic', fontweight='bold')
    ax2.set_xscale('log')
    ax2.legend(loc='best')
    ax2.grid(True, alpha=0.3, which='both')
    
    # 3. Nombre de paquets réussis
    ax3 = axes[1, 0]
    for alg in df_traffic['alg'].unique():
        alg_data = df_traffic[df_traffic['alg'] == alg].groupby('TrafficInterval')['SuccessfulPackets'].mean().reset_index()
        ax3.plot(alg_data['TrafficInterval'], alg_data['SuccessfulPackets'], 
                marker=MARKERS[alg], color=COLORS[alg], 
                label=alg, linewidth=2.5, markersize=10)
    
    ax3.set_xlabel('Intervalle de trafic (s)', fontweight='bold')
    ax3.set_ylabel('Paquets réussis', fontweight='bold')
    ax3.set_title('Paquets réussis vs Intervalle de trafic', fontweight='bold')
    ax3.set_xscale('log')
    ax3.legend(loc='best')
    ax3.grid(True, alpha=0.3, which='both')
    
    # 4. Efficacité
    ax4 = axes[1, 1]
    for alg in df_traffic['alg'].unique():
        alg_data = df_traffic[df_traffic['alg'] == alg].copy()
        alg_data['Efficiency'] = alg_data['PDR_Percent'] / alg_data['AvgEnergy_mJ']
        efficiency_grouped = alg_data.groupby('TrafficInterval')['Efficiency'].mean().reset_index()
        ax4.plot(efficiency_grouped['TrafficInterval'], efficiency_grouped['Efficiency'], 
                marker=MARKERS[alg], color=COLORS[alg], 
                label=alg, linewidth=2.5, markersize=10)
    
    ax4.set_xlabel('Intervalle de trafic (s)', fontweight='bold')
    ax4.set_ylabel('Efficacité (packet delivery rate/mJ)', fontweight='bold')
    ax4.set_title('Efficacité vs Intervalle de trafic', fontweight='bold')
    ax4.set_xscale('log')
    ax4.legend(loc='best')
    ax4.grid(True, alpha=0.3, which='both')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/analyse_traffic_interval.png', dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✓ Graphique sauvegardé: analyse_traffic_interval.png")


def plot_histograms_per_scenario(dfs_dict, metrics=None, output_dir='output'):
    """Pour chaque scénario, crée un histogramme par metric contenant les 4 algorithmes sur la même image."""
    Path(output_dir).mkdir(exist_ok=True)
    if metrics is None:
        metrics = ['PDR_Percent', 'AvgEnergy_mJ']

    for scenario_name, df in dfs_dict.items():
        for metric in metrics:
            if metric not in df.columns:
                continue
            plt.figure(figsize=(10, 6))
            for alg in ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']:
                if alg not in df['alg'].unique():
                    continue
                data = pd.to_numeric(df[df['alg'] == alg][metric], errors='coerce').dropna()
                if data.empty:
                    continue
                plt.hist(data, bins=20, alpha=0.5, label=alg, color=COLORS.get(alg))
            plt.xlabel(metric)
            # Replace metric label for PDR
            if metric == 'PDR_Percent':
                plt.xlabel('packet delivery rate (%)')
                plt.xlim(0, 100)
                plt.xticks(np.arange(0, 101, 20))
            else:
                plt.xlabel(metric)
            plt.ylabel('Count')
            plt.title(f'Histogramme {metric} - Scénario: {scenario_name}')
            plt.legend()
            plt.grid(alpha=0.3)
            out = f'{output_dir}/hist_{scenario_name}_{metric}.png'
            plt.tight_layout()
            plt.savefig(out, dpi=200)
            plt.close()
            print(f"✓ Histogramme sauvegardé: {out}")


def plot_per_scenario_metric_vs_x(dfs_dict, x_params=None, metrics=None, output_dir='output'):
    """Pour chaque scénario, trace des courbes metric vs x où chaque image contient les 4 algorithmes.

    Sauvegarde un fichier par (scenario, metric, x).
    """
    Path(output_dir).mkdir(exist_ok=True)
    if x_params is None:
        x_params = ['NumDevices', 'TrafficInterval', 'Sigma', 'MobilitySpeed']
    if metrics is None:
        metrics = ['PDR_Percent', 'AvgEnergy_mJ']

    alg_order = ['ADR-AVG', 'ADR-Lite', 'ADR-MAX', 'No-ADR']

    for scenario_name, df in dfs_dict.items():
        for metric in metrics:
            for x in x_params:
                if x not in df.columns or metric not in df.columns:
                    continue
                plt.figure(figsize=(10, 6))
                plotted = False
                for alg in alg_order:
                    if alg not in df['alg'].unique():
                        continue
                    sub = df[df['alg'] == alg]
                    x_vals = pd.to_numeric(sub[x], errors='coerce')
                    y_vals = pd.to_numeric(sub[metric], errors='coerce')
                    temp = pd.concat([x_vals, y_vals], axis=1).dropna()
                    if temp.empty:
                        continue
                    grp = temp.groupby(x)[metric].mean().reset_index()
                    if grp.empty:
                        continue
                    grp = grp.sort_values(by=grp.columns[0])
                    plt.plot(grp.iloc[:,0], grp.iloc[:,1], marker=MARKERS.get(alg, 'o'), color=COLORS.get(alg), label=alg, linewidth=2)
                    plotted = True
                if not plotted:
                    plt.close()
                    continue
                # axis labels and ticks adjustments requested by user
                plt.xlabel(x)
                if metric == 'PDR_Percent':
                    plt.ylabel('packet delivery rate (%)')
                    plt.ylim(0, 100)
                    plt.yticks(np.arange(0, 101, 20))
                else:
                    plt.ylabel(metric)

                # x-axis adjustments
                if x == 'NumDevices':
                    plt.xlim(100, 1000)
                    plt.xticks(np.arange(100, 1001, 100))
                if x == 'TrafficInterval':
                    plt.xlim(72, 3600)
                    plt.xticks([72, 300, 600, 900, 1200, 1800, 2400, 3600])

                plt.title(f'{metric} vs {x} - Scénario: {scenario_name}')
                plt.grid(alpha=0.3)
                plt.legend()
                # Préciser le nombre de noeuds présents dans les données (NumDevices)
                if 'NumDevices' in df.columns:
                    try:
                        nd_vals = sorted(pd.to_numeric(df['NumDevices'], errors='coerce').dropna().unique())
                        if len(nd_vals) > 0:
                            # format values as integers when appropriate
                            nd_str = ','.join(str(int(v)) if float(v).is_integer() else str(v) for v in nd_vals)
                            plt.gca().text(0.99, 0.02, f'NumDevices: {nd_str}', transform=plt.gca().transAxes,
                                           ha='right', va='bottom', fontsize=9,
                                           bbox=dict(facecolor='white', alpha=0.7, edgecolor='gray'))
                    except Exception:
                        pass

                out = f"{output_dir}/{scenario_name}_{metric}_vs_{x}.png"
                plt.tight_layout()
                plt.savefig(out, dpi=200)
                plt.close()
                print(f"✓ Graphique sauvegardé: {out}")


def generate_summary_table(dfs_dict, output_dir='output'):
    """
    Génère un tableau récapitulatif des performances moyennes
    """
    Path(output_dir).mkdir(exist_ok=True)
    
    summary_data = []
    
    for scenario_name, df in dfs_dict.items():
        for alg in df['alg'].unique():
            alg_data = df[df['alg'] == alg]
            summary_data.append({
                'Scénario': scenario_name,
                'Algorithme': alg,
                'packet delivery rate moyen (%)': f"{alg_data['PDR_Percent'].mean():.2f}",
                'packet delivery rate std': f"{alg_data['PDR_Percent'].std():.2f}",
                'Énergie moyenne (mJ)': f"{alg_data['AvgEnergy_mJ'].mean():.2f}",
                'Énergie std': f"{alg_data['AvgEnergy_mJ'].std():.2f}",
                'Efficacité': f"{(alg_data['PDR_Percent'] / alg_data['AvgEnergy_mJ']).mean():.2f}"
            })
    
    summary_df = pd.DataFrame(summary_data)
    summary_df.to_csv(f'{output_dir}/resume_performances.csv', index=False)
    print(f"✓ Tableau récapitulatif sauvegardé: resume_performances.csv")
    
    return summary_df


def main():
    """Fonction principale"""
    print("="*70)
    print(" ANALYSE DES PERFORMANCES DES ALGORITHMES ADR - LoRaWAN".center(70))
    print("="*70)
    print()
    
    # Chemins des fichiers
    files = {
        'mobilite': 'summary_mobilite_run1.csv',
        'density': 'summary_density_run1.csv',
        'intervalle_d_envoie': 'summary_intervalle_d_envoie_run1.csv',
        'sigma': 'summary_sigma_run1.csv'
    }
    
    # Charger les données (auto-discover des CSVs par scénario)
    print("📁 Chargement des données...")
    dfs = {}
    for scenario, filename in files.items():
        # find all matching CSVs for this scenario in resultsfinal/resultsfinal2 and cwd
        found_paths = find_all_summary_paths(scenario)
        if not found_paths:
            print(f"   ✗ Aucun fichier trouvé pour le scénario '{scenario}' (checked cwd, resultsfinal/summaries/{scenario}/, resultsfinal2/summaries/{scenario}/)")
            continue

        # load and concatenate all found CSVs for the scenario
        df_list = []
        loaded_paths = []
        failed_paths = []
        total_rows = 0
        for path in found_paths:
            try:
                df_part = load_data(path)
                df_list.append(df_part)
                loaded_paths.append(path)
                total_rows += len(df_part)
            except Exception as e:
                failed_paths.append((path, str(e)))
        if df_list:
            try:
                dfs[scenario] = pd.concat(df_list, ignore_index=True)
            except Exception:
                # fallback: if concatenation fails, take first
                dfs[scenario] = df_list[0]
            print(f"   ✓ {len(loaded_paths)} fichiers chargés pour le scénario '{scenario}' ({total_rows} lignes au total)")
            if failed_paths:
                print(f"   ⚠ {len(failed_paths)} fichiers n'ont pas pu être chargés (voir logs).")
    
    if not dfs:
        print("\n❌ Aucun fichier de données trouvé!")
        return
    
    print(f"\n✓ {len(dfs)} fichiers chargés avec succès\n")
    
    # Créer le dossier de sortie
    output_dir = 'output'
    Path(output_dir).mkdir(exist_ok=True)
    
    # Générer les graphiques pour chaque scénario
    print("📊 Génération des graphiques par scénario...")
    for scenario_name, df in dfs.items():
        print(f"\n   → Scénario: {scenario_name.upper()}")
        plot_pdr_energy_by_scenario(df, scenario_name, output_dir)
        plot_energy_efficiency(df, scenario_name, output_dir)
        plot_heatmap_pdr(df, scenario_name, output_dir)

    # Graphiques individuels par scénario (metric vs paramètre) et histogrammes
    print("\n📊 Génération des graphiques individuels par scénario (métriques vs paramètres)...")
    plot_per_scenario_metric_vs_x(dfs, x_params=['NumDevices','TrafficInterval','Sigma','MobilitySpeed'],
                                 metrics=['PDR_Percent','AvgEnergy_mJ'], output_dir=output_dir)
    print("\n📊 Génération des histogrammes par scénario (packet delivery rate et énergie)...")
    plot_histograms_per_scenario(dfs, metrics=['PDR_Percent','AvgEnergy_mJ'], output_dir=output_dir)
    
    # Graphiques comparatifs
    print("\n📊 Génération des graphiques comparatifs...")
    plot_parameter_comparison(dfs, output_dir)
    
    # Analyses spécifiques
    if 'sigma' in dfs:
        print("\n📊 Analyse de l'impact de Sigma...")
        plot_sigma_impact(dfs['sigma'], output_dir)
        # additionally produce sigma-impact plot filtered to MobilitySpeed == 0
        plot_sigma_impact_mobility0(dfs['sigma'], output_dir)
    
    if 'intervalle_d_envoie' in dfs:
        print("\n📊 Analyse de l'impact de l'intervalle de trafic...")
        plot_traffic_interval_analysis(dfs['intervalle_d_envoie'], output_dir)
        # additionally produce traffic-impact plots filtered to MobilitySpeed == 0
        plot_traffic_impact_mobility0(dfs['intervalle_d_envoie'], output_dir)

    if 'density' in dfs:
        # produce density-impact plot filtered to MobilitySpeed == 0
        plot_density_impact_mobility0(dfs['density'], output_dir)

    # Analyse spécifique: densité en fonction de paramètres fixes (MobilitySpeed=0, TrafficInterval=3600s, Sigma=3.96)
    print("\n📊 Génération des graphiques densité avec paramètres fixes...")
    plot_density_impact_fixed_params(dfs, output_dir)
    
    # Analyse spécifique: intervalle de trafic en fonction de paramètres fixes (MobilitySpeed=0, NumDevices=550, Sigma=3.96)
    print("\n📊 Génération des graphiques intervalle de trafic avec paramètres fixes...")
    plot_traffic_impact_fixed_params(dfs, output_dir)
    
    # Analyse spécifique: densité pour le scénario density uniquement (MobilitySpeed=0, TrafficInterval=3600, Sigma=3.96)
    if 'density' in dfs:
        print("\n📊 Génération des graphiques densité spécifiques pour le scénario density...")
        plot_density_scenario_specific(dfs['density'], output_dir)
    
    # Analyse spécifique: sigma pour le scénario sigma uniquement (MobilitySpeed=0, NumDevices=550, TrafficInterval=3600)
    if 'sigma' in dfs:
        print("\n📊 Génération des graphiques sigma spécifiques pour le scénario sigma...")
        plot_sigma_scenario_specific(dfs['sigma'], output_dir)
    
    # Analyse spécifique: traffic pour le scénario intervalle_d_envoie uniquement (MobilitySpeed=0, NumDevices=550/1000, Sigma=3.96)
    if 'intervalle_d_envoie' in dfs:
        print("\n📊 Génération des graphiques traffic spécifiques pour le scénario intervalle_d_envoie...")
        plot_traffic_scenario_specific(dfs['intervalle_d_envoie'], output_dir)
    
    # Analyse spécifique: traffic avec mobilité 33.33 pour le scénario intervalle_d_envoie uniquement (MobilitySpeed=33.33, NumDevices=1000, Sigma=3.96)
    if 'intervalle_d_envoie' in dfs:
        print("\n📊 Génération des graphiques traffic avec mobilité 33.33 pour le scénario intervalle_d_envoie...")
        plot_traffic_scenario_mobility33(dfs['intervalle_d_envoie'], output_dir)
    
    # Graphiques spécifiques: mobilité vs PDR/énergie avec paramètres fixes
    print("\n📊 Génération des graphiques mobilité avec paramètres spécifiques...")
    plot_mobility_impact_specific_params(dfs, output_dir)
    
    # Tableau récapitulatif
    print("\n📋 Génération du tableau récapitulatif...")
    summary_df = generate_summary_table(dfs, output_dir)
    
    print("\n" + "="*70)
    print(" ANALYSE TERMINÉE".center(70))
    print("="*70)
    print(f"\n✓ Tous les graphiques ont été sauvegardés dans le dossier '{output_dir}/'")
    print("\nRésumé des performances:")
    print(summary_df.to_string(index=False))
    print()


if __name__ == "__main__":
    main()
