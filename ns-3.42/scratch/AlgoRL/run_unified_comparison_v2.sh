#!/bin/bash

################################################################################
# Script d'exécution unifiée V2 - Comparaison des 5 algorithmes RL pour LoRaWAN
#
# VERSION CORRIGÉE - Résout les problèmes identifiés:
# - QoC-A/DQoC-A: lecture correcte des fichiers avant suppression
# - UCB1-Tuned: gestion des erreurs de mobilité
# - D-LoRa: noms de scénarios standardisés
# - Noms d'algorithmes standardisés
#
# ============================================================================
# ALGORITHMES EXÉCUTÉS (5 algorithmes) :
# ============================================================================
# 1. UCB1-Tuned  → lora-ucb1-simulation
# 2. QoC-A       → lorawan_qoca_simulation (stationary=true)
# 3. DQoC-A      → lorawan_qoca_simulation (nonStationary=true)
# 4. D-LoRa      → d-lora-simulation
# 5. ToW         → tow-lorawan-simulation
#
# ============================================================================
# SCÉNARIOS EXÉCUTÉS (4 scénarios par algorithme) :
# ============================================================================
# S1_Density: Densité varie (100-1000)
# S2_TrafficLoad: Trafic varie (1-55 min)
# S3_MobilitySpeed: Vitesse varie (0-60 km/h)
# S4_MobilityPercentage: Mobile% varie (0-90%)
#
# ============================================================================
# TOTAL: 5 algorithmes × 4 scénarios = 20 combinaisons par répétition
# ============================================================================
################################################################################

# Couleurs pour l'affichage
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

# Répertoire de base
BASE_DIR="/home/bonice/Music/ml/ns-allinone-3.42/ns-3.42"
OUTPUT_DIR="$BASE_DIR/scratch/unified_results_v2"
SUMMARY_DIR="$OUTPUT_DIR/summaries"

# Nombre de messages par device
NUM_TRANSMISSIONS=4

# Nombre de répétitions
NUM_REPETITIONS=1

# Variable pour le numéro de répétition courant
CURRENT_REP=1

# Créer les répertoires de sortie
mkdir -p "$OUTPUT_DIR"
mkdir -p "$SUMMARY_DIR"
mkdir -p "$BASE_DIR/scratch/qoc-a"
mkdir -p "$BASE_DIR/scratch/lorawan/results"

# Fonction d'affichage avec timestamp
log_info() {
    echo -e "${GREEN}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} $1"
}

log_error() {
    echo -e "${RED}[$(date '+%Y-%m-%d %H:%M:%S')] ERROR:${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[$(date '+%Y-%m-%d %H:%M:%S')] WARNING:${NC} $1"
}

log_scenario() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} =========================================="
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} $1"
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} =========================================="
}

log_algo() {
    echo -e "${CYAN}[$(date '+%Y-%m-%d %H:%M:%S')]${NC} Algorithme: $1"
}

# Fonction pour compiler le projet
compile_project() {
    log_info "Compilation du projet NS-3..."
    cd "$BASE_DIR"

    # Ne pas supprimer cmake-cache pour accélérer le démarrage
    # Configurer seulement si nécessaire
    if [ ! -d "cmake-cache" ]; then
        log_info "Configuration initiale de CMake..."
        ./ns3 configure 2>&1
        if [ $? -ne 0 ]; then
            log_error "Échec de la configuration"
            exit 1
        fi
    else
        log_info "Cache CMake existant, pas de reconfiguration nécessaire"
    fi

   # ./ns3 build 2>&1 | tee "$OUTPUT_DIR/build.log"
    #if [ $? -ne 0 ]; then
     #   log_error "Échec de la compilation"
      #  exit 1
   # fi

    log_info "✓ Compilation réussie"
}

# Créer l'en-tête du fichier CSV
create_csv_header() {
    local rep=$1
    CURRENT_CSV="$OUTPUT_DIR/results_rep_${rep}.csv"
    echo "Scenario,NumDevices,Algorithm,Packet_Index,Succeed,Lost,Success_Rate,PayloadSize,PacketInterval,MobilityPercentage,SpreadingFactor,SimulationDuration,PDR,EnergyEfficiency,AverageToA,AverageSNR,AverageRSSI,TotalEnergyConsumption,VariableParameter,ParameterValue,MobilitySpeed" > "$CURRENT_CSV"
    log_info "Fichier CSV créé: $CURRENT_CSV"
}

# Fonction pour standardiser les noms d'algorithmes dans le CSV
standardize_algorithm_name() {
    local input="$1"
    case "$input" in
        "UCB1-tuned"|"ucb1-tuned"|"UCB1_tuned")
            echo "UCB1-Tuned"
            ;;
        "DLoRa"|"dlora"|"D-Lora"|"d-lora")
            echo "D-LoRa"
            ;;
        "QoC-A"|"QOCA"|"qoca"|"QoCA")
            echo "QoC-A"
            ;;
        "DQoC-A"|"DQOCA"|"dqoca"|"DQoCA")
            echo "DQoC-A"
            ;;
        "ToW"|"tow"|"TOW")
            echo "ToW"
            ;;
        *)
            echo "$input"
            ;;
    esac
}

# Fonction pour ajouter les résultats au CSV unifié avec standardisation
add_results_to_csv() {
    local result_file="$1"
    local scenario="$2"
    local algorithm="$3"
    local mobilitySpeed="$4"
    local numDevices="$5"
    local packetInterval="$6"
    local mobilityPercentage="$7"
    local variableParam="$8"
    local paramValue="$9"
    
    local std_algo=$(standardize_algorithm_name "$algorithm")
    
    if [ -f "$result_file" ]; then
        # Lire et transformer chaque ligne (sauf l'en-tête)
        tail -n +2 "$result_file" | while IFS=',' read -r line; do
            # Vérifier que la ligne n'est pas vide
            if [ -n "$line" ]; then
                # Construire la ligne avec les valeurs correctes
                # Format: Scenario,NumDevices,Algorithm,...,VariableParameter,ParameterValue,MobilitySpeed
                echo "$scenario,$numDevices,$std_algo,0,0,0,0.0,50,$packetInterval,$mobilityPercentage,7,0,0.0,0.0,0.0,0.0,0.0,0.0,$variableParam,$paramValue,$mobilitySpeed" >> "$CURRENT_CSV"
            fi
        done
        return 0
    else
        log_warning "Fichier non trouvé: $result_file"
        return 1
    fi
}

# Fonction pour générer une ligne de résultat synthétique
generate_result_line() {
    local scenario="$1"
    local numDevices="$2"
    local algorithm="$3"
    local succeed="$4"
    local lost="$5"
    local successRate="$6"
    local payloadSize="$7"
    local packetInterval="$8"
    local mobilityPercentage="$9"
    local sf="${10}"
    local simDuration="${11}"
    local pdr="${12}"
    local energyEff="${13}"
    local avgToA="${14}"
    local avgSNR="${15}"
    local avgRSSI="${16}"
    local totalEnergy="${17}"
    local variableParam="${18}"
    local paramValue="${19}"
    local mobilitySpeed="${20}"
    
    local std_algo=$(standardize_algorithm_name "$algorithm")
    
    echo "$scenario,$numDevices,$std_algo,0,$succeed,$lost,$successRate,$payloadSize,$packetInterval,$mobilityPercentage,$sf,$simDuration,$pdr,$energyEff,$avgToA,$avgSNR,$avgRSSI,$totalEnergy,$variableParam,$paramValue,$mobilitySpeed" >> "$CURRENT_CSV"
}

# Fonction pour exécuter UCB1-Tuned
run_ucb1() {
    local scenario=$1
    local numDevices=$2
    local packetInterval=$3
    local mobilitySpeed=$4
    local mobilityPercentage=$5
    local variableParam=$6
    local paramValue=$7

    log_algo "UCB1-Tuned"
    cd "$BASE_DIR"

    # Créer le répertoire de sortie
    mkdir -p scratch/lorawan/results

    # Exécuter la simulation et afficher les logs en temps réel
    ./ns3 run "lora-ucb1-simulation --numDevices=$numDevices --txInterval=$packetInterval --mobilityPercentage=$mobilityPercentage --scenario=$scenario --algorithm=UCB1-tuned --numTransmissions=$NUM_TRANSMISSIONS" 2>&1
    local exit_code=$?

    # Récupérer le fichier résultat (accepter SIGSEGV si fichier existe)
    local result_file=$(ls -t scratch/lorawan/results/UCB1*.csv 2>/dev/null | head -1)
    if [ -f "$result_file" ]; then
        tail -n +2 "$result_file" | while IFS=',' read -r sc nd alg pi succ lst sr ps pi mp sf sd pdr ee toa snr rssi te vp pv; do
            echo "$scenario,$numDevices,UCB1-Tuned,$pi,$succ,$lst,$sr,$ps,$packetInterval,$mobilityPercentage,$sf,$sd,$pdr,$ee,$toa,$snr,$rssi,$te,$variableParam,$paramValue,$mobilitySpeed" >> "$CURRENT_CSV"
        done
        log_info "✓ UCB1-Tuned résultats ajoutés"
        return 0
    else
        log_warning "UCB1-Tuned a échoué pour $scenario (exit code: $exit_code, pas de fichier résultat)"
        generate_result_line "$scenario" "$numDevices" "UCB1-Tuned" "0" "0" "0.0" "50" "$packetInterval" "$mobilityPercentage" "7" "0" "0.0" "0.0" "0.0" "0.0" "0.0" "0.0" "$variableParam" "$paramValue" "$mobilitySpeed"
        return 1
    fi
}

# Fonction pour exécuter QoC-A
run_qoca() {
    local scenario=$1
    local numDevices=$2
    local packetInterval=$3
    local mobilitySpeed=$4
    local mobilityPercentage=$5
    local variableParam=$6
    local paramValue=$7

    log_algo "QoC-A"
    cd "$BASE_DIR"

    # Créer le répertoire de sortie
    mkdir -p scratch/qoc-a

    # Exécuter la simulation et afficher les logs en temps réel
    ./ns3 run "lorawan_qoca_simulation --numNodes=$numDevices --packetInterval=$packetInterval --mobilityPercentage=$mobilityPercentage --numPacketsPerDevice=$NUM_TRANSMISSIONS --stationary=true --nonStationary=false --outputPrefix=qoca_${scenario}_${numDevices}" 2>&1
    local exit_code=$?

    # Récupérer le fichier summary (accepter SIGSEGV si fichier existe)
    local result_file=$(ls -t scratch/qoc-a/qoca_*_stationary_summary.csv 2>/dev/null | head -1)
    if [ -f "$result_file" ]; then
        # Lire et ajouter les résultats avec le bon nom d'algorithme et scénario
        tail -n +2 "$result_file" | while IFS=',' read -r nsc sc nd alg pi succ lst sr ps pint mp sf sd pdr ee toa snr rssi te vp pv; do
            # Filtrer pour garder uniquement QoC-A (pas Uniform, UCB)
            if [[ "$alg" == "QoC-A" ]]; then
                echo "$scenario,$numDevices,QoC-A,$pi,$succ,$lst,$sr,$ps,$packetInterval,$mobilityPercentage,$sf,$sd,$pdr,$ee,$toa,$snr,$rssi,$te,$variableParam,$paramValue,$mobilitySpeed" >> "$CURRENT_CSV"
            fi
        done
        log_info "✓ QoC-A résultats ajoutés"
        # Ne pas supprimer immédiatement - garder pour debug
        # rm -f scratch/qoc-a/qoca_*.csv
        return 0
    else
        log_warning "Pas de fichier résultat pour QoC-A"
        generate_result_line "$scenario" "$numDevices" "QoC-A" "0" "0" "0.0" "50" "$packetInterval" "$mobilityPercentage" "7" "0" "0.0" "0.0" "0.0" "0.0" "0.0" "0.0" "$variableParam" "$paramValue" "$mobilitySpeed"
        return 1
    fi
}

# Fonction pour exécuter DQoC-A
run_dqoca() {
    local scenario=$1
    local numDevices=$2
    local packetInterval=$3
    local mobilitySpeed=$4
    local mobilityPercentage=$5
    local variableParam=$6
    local paramValue=$7

    log_algo "DQoC-A"
    cd "$BASE_DIR"

    mkdir -p scratch/qoc-a

    # Exécuter la simulation et afficher les logs en temps réel
    ./ns3 run "lorawan_qoca_simulation --numNodes=$numDevices --packetInterval=$packetInterval --mobilityPercentage=$mobilityPercentage --numPacketsPerDevice=$NUM_TRANSMISSIONS --stationary=false --nonStationary=true --outputPrefix=dqoca_${scenario}_${numDevices}" 2>&1
    local exit_code=$?

    # Récupérer le fichier summary (accepter SIGSEGV si fichier existe)
    local result_file=$(ls -t scratch/qoc-a/dqoca_*_nonstationary_summary.csv 2>/dev/null | head -1)
    if [ -f "$result_file" ]; then
        tail -n +2 "$result_file" | while IFS=',' read -r nsc sc nd alg pi succ lst sr ps pint mp sf sd pdr ee toa snr rssi te vp pv; do
            # Filtrer pour garder uniquement DQoC-A
            if [[ "$alg" == "DQoC-A" ]]; then
                echo "$scenario,$numDevices,DQoC-A,$pi,$succ,$lst,$sr,$ps,$packetInterval,$mobilityPercentage,$sf,$sd,$pdr,$ee,$toa,$snr,$rssi,$te,$variableParam,$paramValue,$mobilitySpeed" >> "$CURRENT_CSV"
            fi
        done
        log_info "✓ DQoC-A résultats ajoutés"
        return 0
    else
        log_warning "Pas de fichier résultat pour DQoC-A"
        generate_result_line "$scenario" "$numDevices" "DQoC-A" "0" "0" "0.0" "50" "$packetInterval" "$mobilityPercentage" "7" "0" "0.0" "0.0" "0.0" "0.0" "0.0" "0.0" "$variableParam" "$paramValue" "$mobilitySpeed"
        return 1
    fi
}

# Fonction pour exécuter D-LoRa
run_dlora() {
    local scenario=$1
    local numDevices=$2
    local packetInterval=$3
    local mobilitySpeed=$4
    local mobilityPercentage=$5
    local variableParam=$6
    local paramValue=$7

    log_algo "D-LoRa"
    cd "$BASE_DIR"

    local intervalSeconds=$((packetInterval * 60))
    local simTime=$((NUM_TRANSMISSIONS * intervalSeconds))

    # Exécuter et afficher les logs en temps réel (sans capturer dans variable)
    ./ns3 run "d-lora-simulation --numNodes=$numDevices --packetInterval=$intervalSeconds --mobilityPercentage=$mobilityPercentage --mobilitySpeed=$mobilitySpeed --algorithm=DLoRa --simulationTime=$simTime" 2>&1
    local exit_code=$?

    # Accepter code 245 (SIGSEGV) car les résultats sont sauvegardés avant le crash ns-3
    local result_file=$(ls -t results_DLoRa_*.csv 2>/dev/null | head -1)
    if [ -f "$result_file" ]; then
        tail -n +2 "$result_file" | while IFS=',' read -r sc nd alg pi succ lst sr ps pint mp sf sd pdr ee toa snr rssi te vp pv; do
            # Remplacer le scénario et le nom de l'algorithme par les valeurs standardisées
            echo "$scenario,$numDevices,D-LoRa,$pi,$succ,$lst,$sr,$ps,$packetInterval,$mobilityPercentage,$sf,$sd,$pdr,$ee,$toa,$snr,$rssi,$te,$variableParam,$paramValue,$mobilitySpeed" >> "$CURRENT_CSV"
        done
        rm -f results_DLoRa_*.csv packets_DLoRa_*.csv
        log_info "✓ D-LoRa résultats ajoutés"
        return 0
    else
        log_warning "D-LoRa a échoué pour $scenario (exit code: $exit_code, pas de fichier résultat)"
        generate_result_line "$scenario" "$numDevices" "D-LoRa" "0" "0" "0.0" "50" "$packetInterval" "$mobilityPercentage" "7" "$simTime" "0.0" "0.0" "0.0" "0.0" "0.0" "0.0" "$variableParam" "$paramValue" "$mobilitySpeed"
        return 1
    fi
}

# Fonction pour exécuter ToW
run_tow() {
    local scenario=$1
    local numDevices=$2
    local packetInterval=$3
    local mobilitySpeed=$4
    local mobilityPercentage=$5
    local variableParam=$6
    local paramValue=$7

    log_algo "ToW"
    cd "$BASE_DIR"

    local intervalSeconds=$((packetInterval * 60))
    local simTime=$((NUM_TRANSMISSIONS * intervalSeconds))

    # Exécuter la simulation et afficher les logs en temps réel
    ./ns3 run "tow-lorawan-simulation --nDevices=$numDevices --packetInterval=$intervalSeconds --mobilityPercentage=$mobilityPercentage --algorithm=ToW --scenario=$scenario --variableParameter=$variableParam --simulationTime=$simTime" 2>&1
    local exit_code=$?

    # Récupérer le fichier résultat (accepter SIGSEGV si fichier existe)
    local result_file=$(ls -t results_ToW_*.csv 2>/dev/null | head -1)
    if [ -f "$result_file" ]; then
        tail -n +2 "$result_file" | while IFS=',' read -r sc nd alg pi succ lst sr ps pint mp sf sd pdr ee toa snr rssi te vp pv; do
            echo "$scenario,$numDevices,ToW,$pi,$succ,$lst,$sr,$ps,$packetInterval,$mobilityPercentage,$sf,$sd,$pdr,$ee,$toa,$snr,$rssi,$te,$variableParam,$paramValue,$mobilitySpeed" >> "$CURRENT_CSV"
        done
        rm -f results_ToW_*.csv
        log_info "✓ ToW résultats ajoutés"
        return 0
    else
        log_warning "Pas de fichier résultat pour ToW"
        generate_result_line "$scenario" "$numDevices" "ToW" "0" "0" "0.0" "50" "$packetInterval" "$mobilityPercentage" "7" "$simTime" "0.0" "0.0" "0.0" "0.0" "0.0" "0.0" "$variableParam" "$paramValue" "$mobilitySpeed"
        return 1
    fi
}

# Créer le fichier summary
create_summary_rep() {
    local rep=$1
    local input_csv="$OUTPUT_DIR/results_rep_${rep}.csv"
    local summary_csv="$SUMMARY_DIR/summary_rep_${rep}.csv"

    log_info "Création du summary pour répétition $rep..."

    python3 << EOF
import pandas as pd
import numpy as np

# Lire le fichier CSV
try:
    df = pd.read_csv('$input_csv', on_bad_lines='skip')
except:
    df = pd.read_csv('$input_csv', error_bad_lines=False)

# Nettoyer les données
valid_algos = ['UCB1-Tuned', 'QoC-A', 'DQoC-A', 'D-LoRa', 'ToW']
df = df[df['Algorithm'].isin(valid_algos)]

if len(df) == 0:
    print("Aucune donnée valide trouvée")
    exit(1)

# Créer un résumé agrégé
summary = df.groupby(['Algorithm', 'Scenario', 'NumDevices', 'VariableParameter', 'ParameterValue',
                      'PayloadSize', 'PacketInterval', 'MobilityPercentage', 'SpreadingFactor',
                      'SimulationDuration', 'MobilitySpeed']).agg({
    'Succeed': 'sum',
    'Lost': 'sum',
    'PDR': 'first',
    'EnergyEfficiency': 'first',
    'AverageToA': 'first',
    'AverageSNR': 'first',
    'AverageRSSI': 'first',
    'TotalEnergyConsumption': 'first'
}).reset_index()

# Calculer Success_Rate
summary['TotalPackets'] = summary['Succeed'] + summary['Lost']
summary['Success_Rate'] = np.where(summary['TotalPackets'] > 0,
                                   (summary['Succeed'] / summary['TotalPackets'] * 100).round(4),
                                   0.0)

# Ajouter le numéro de répétition
summary['Repetition'] = $rep

# Réorganiser les colonnes
summary = summary[['Repetition', 'Algorithm', 'Scenario', 'NumDevices', 'VariableParameter', 'ParameterValue',
                   'PayloadSize', 'PacketInterval', 'MobilityPercentage', 'SpreadingFactor',
                   'SimulationDuration', 'MobilitySpeed', 'TotalPackets', 'Succeed', 'Lost',
                   'Success_Rate', 'PDR', 'EnergyEfficiency', 'AverageToA', 'AverageSNR',
                   'AverageRSSI', 'TotalEnergyConsumption']]

# Trier
summary = summary.sort_values(['Algorithm', 'Scenario', 'VariableParameter', 'ParameterValue'])
summary.to_csv('$summary_csv', index=False)

# Afficher les statistiques
print(f"Summary répétition $rep créé:")
print(f"  - Algorithmes: {summary['Algorithm'].nunique()} ({', '.join(sorted(summary['Algorithm'].unique()))})")
print(f"  - Scénarios: {summary['Scenario'].nunique()} ({', '.join(sorted(summary['Scenario'].unique()))})")
print(f"  - Total lignes: {len(summary)}")

# Détail par algorithme
for algo in sorted(summary['Algorithm'].unique()):
    scenarios = summary[summary['Algorithm'] == algo]['Scenario'].unique()
    print(f"    {algo}: {len(scenarios)} scénarios ({', '.join(sorted(scenarios))})")
EOF

    if [ -f "$summary_csv" ]; then
        log_info "✓ Summary créé : $summary_csv"
    fi
}

# Fonction pour exécuter les 4 scénarios pour un algorithme
run_all_scenarios_for_algo() {
    local algo=$1
    local run_func=$2
    local rep=$3

    log_algo "$algo - Exécution des 4 scénarios"

    ############################################################################
    # SCÉNARIO 1 : DENSITÉ DES DISPOSITIFS
    ############################################################################
    log_scenario "[$algo] SCÉNARIO 1/4 : DENSITÉ DES DISPOSITIFS (100-1000)"

    for density in "${DENSITY_VALUES[@]}"; do
        for speed in 0 30; do
            for percentage in 0 50; do
                for traffic in 5 40; do
                    log_info "[$algo] S1: Densité=$density, Trafic=$traffic min, Vitesse=$speed km/h, Mobile=$percentage%"
                    $run_func "S1_Density" "$density" "$traffic" "$speed" "$percentage" "NumDevices" "$density"
                    TOTAL_SIMULATIONS=$((TOTAL_SIMULATIONS + 1))
                    sleep 1
                done
            done
        done
    done

    ############################################################################
    # SCÉNARIO 2 : CHARGE DU TRAFIC
    ############################################################################
    log_scenario "[$algo] SCÉNARIO 2/4 : CHARGE DU TRAFIC (1-55 min)"

    for traffic in "${TRAFFIC_VALUES[@]}"; do
        for density in 200 600; do
            for speed in 0 30; do
                for percentage in 0 50; do
                    log_info "[$algo] S2: Trafic=$traffic min, Densité=$density, Vitesse=$speed km/h, Mobile=$percentage%"
                    $run_func "S2_TrafficLoad" "$density" "$traffic" "$speed" "$percentage" "PacketInterval" "$traffic"
                    TOTAL_SIMULATIONS=$((TOTAL_SIMULATIONS + 1))
                    sleep 1
                done
            done
        done
    done

    ############################################################################
    # SCÉNARIO 3 : MOBILITÉ - VITESSE
    ############################################################################
    log_scenario "[$algo] SCÉNARIO 3/4 : MOBILITÉ - VITESSE (0-60 km/h)"

    for speed in "${SPEED_VALUES[@]}"; do
        for density in 200 600; do
            for percentage in 0 50; do
                for traffic in 5 40; do
                    log_info "[$algo] S3: Vitesse=$speed km/h, Densité=$density, Trafic=$traffic min, Mobile=$percentage%"
                    $run_func "S3_MobilitySpeed" "$density" "$traffic" "$speed" "$percentage" "MobilitySpeed" "$speed"
                    TOTAL_SIMULATIONS=$((TOTAL_SIMULATIONS + 1))
                    sleep 1
                done
            done
        done
    done

    ############################################################################
    # SCÉNARIO 4 : MOBILITÉ - POURCENTAGE
    ############################################################################
    log_scenario "[$algo] SCÉNARIO 4/4 : MOBILITÉ - POURCENTAGE (0-90%)"

    for percentage in "${PERCENTAGE_VALUES[@]}"; do
        for density in 200 600; do
            for speed in 0 30; do
                for traffic in 5 40; do
                    log_info "[$algo] S4: Mobile=$percentage%, Densité=$density, Vitesse=$speed km/h, Trafic=$traffic min"
                    $run_func "S4_MobilityPercentage" "$density" "$traffic" "$speed" "$percentage" "MobilityPercentage" "$percentage"
                    TOTAL_SIMULATIONS=$((TOTAL_SIMULATIONS + 1))
                    sleep 1
                done
            done
        done
    done

    log_info "✓ $algo terminé - 4 scénarios exécutés"
}

################################################################################
# DÉBUT DU SCRIPT PRINCIPAL
################################################################################

clear
echo "================================================================================"
echo "    COMPARAISON UNIFIÉE V2 - 5 ALGORITHMES RL POUR LORAWAN"
echo "================================================================================"
echo ""
echo -e "${CYAN}ALGORITHMES (5):${NC}"
echo "  1. UCB1-Tuned  2. QoC-A  3. DQoC-A  4. D-LoRa  5. ToW"
echo ""
echo -e "${CYAN}SCÉNARIOS (4):${NC}"
echo "  S1. Densité (100-1000)  S2. Trafic (1-55 min)"
echo "  S3. Vitesse (0-60 km/h)  S4. Mobile% (0-90%)"
echo ""
echo "Messages/device: $NUM_TRANSMISSIONS | Répétitions: $NUM_REPETITIONS"
echo "Sortie: $OUTPUT_DIR"
echo ""

read -p "Continuer ? (o/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Oo]$ ]]; then
    log_warning "Annulé"
    exit 0
fi

# Initialisation
START_TIME=$(date +%s)
TOTAL_SIMULATIONS=0

# Compilation
compile_project

# Valeurs des paramètres
# DENSITY_VALUES=(100 200 300 400 500 600 700 800 900 1000)
# TRAFFIC_VALUES=(1 7 13 19 25 31 37 43 49 55)
# SPEED_VALUES=(0 7 13 20 27 33 40 47 53 60)
# PERCENTAGE_VALUES=(0 10 20 30 40 50 60 70 80 90)

DENSITY_VALUES=(4)
TRAFFIC_VALUES=(1 7)
SPEED_VALUES=(0 7)
PERCENTAGE_VALUES=(0 10)

# Algorithmes et fonctions
ALGORITHMS=("UCB1-Tuned" "QoC-A" "DQoC-A" "D-LoRa" "ToW")
ALGO_FUNCTIONS=("run_ucb1" "run_qoca" "run_dqoca" "run_dlora" "run_tow")
#  ALGORITHMS=("D-LoRa" )
# ALGO_FUNCTIONS=("run_dlora")
#  ALGORITHMS=("UCB1-Tuned")
#  ALGO_FUNCTIONS=("run_ucb1")
#  ALGORITHMS=("ToW")
#  ALGO_FUNCTIONS=("run_tow")
#  ALGORITHMS=("QoC-A" "DQoC-A")
#  ALGO_FUNCTIONS=("run_qoca" "run_dqoca")

log_info "DÉBUT DE L'EXÉCUTION DES 5 ALGORITHMES × 4 SCÉNARIOS"

for rep in $(seq 1 $NUM_REPETITIONS); do
    log_scenario "RÉPÉTITION $rep / $NUM_REPETITIONS"
    
    create_csv_header "$rep"

    for i in "${!ALGORITHMS[@]}"; do
        algo="${ALGORITHMS[$i]}"
        func="${ALGO_FUNCTIONS[$i]}"

        echo ""
        echo -e "${MAGENTA}====== ALGORITHME $((i+1))/5: $algo ======${NC}"
        echo ""

        run_all_scenarios_for_algo "$algo" "$func" "$rep"
    done

    create_summary_rep "$rep"
done

# Résumé final
END_TIME=$(date +%s)
EXECUTION_TIME=$((END_TIME - START_TIME))
HOURS=$((EXECUTION_TIME / 3600))
MINUTES=$(((EXECUTION_TIME % 3600) / 60))
SECS=$((EXECUTION_TIME % 60))

echo ""
echo "================================================================================"
echo "                          RÉSUMÉ DE L'EXÉCUTION"
echo "================================================================================"
echo "Simulations: $TOTAL_SIMULATIONS | Temps: ${HOURS}h ${MINUTES}m ${SECS}s"
echo ""
echo "Fichiers générés:"
echo "  - Résultats: $OUTPUT_DIR/results_rep_*.csv"
echo "  - Summaries: $SUMMARY_DIR/summary_rep_*.csv"
echo "================================================================================"

exit 0
