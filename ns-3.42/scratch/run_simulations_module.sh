#!/bin/bash

# Script d'automatisation pour les simulations LoRaWAN ADR avec le module lorawan officiel
# Usage: ./run_simulations_module.sh [1|2|3|4]
# Exemple: ./run_simulations_module.sh 1  (pour exécuter uniquement le scénario 1)
# Exécution séquentielle (une simulation après l'autre)

# Couleurs pour l'affichage
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Vérifier l'argument
if [ $# -eq 0 ]; then
    echo -e "${RED}Erreur: Veuillez spécifier le numéro du scénario (1, 2, 3 ou 4)${NC}"
    echo -e "${YELLOW}Usage: $0 [1|2|3|4]${NC}"
    echo -e "${YELLOW}Exemples:${NC}"
    echo -e "  $0 1  # Exécute le scénario 1 (variation densité)"
    echo -e "  $0 2  # Exécute le scénario 2 (variation mobilité)"
    echo -e "  $0 3  # Exécute le scénario 3 (variation sigma/perte aléatoire)"
    echo -e "  $0 4  # Exécute le scénario 4 (variation intervalle d'envoi)"
    exit 1
fi

SCENARIO=$1

# Valider le numéro de scénario
if [[ ! "$SCENARIO" =~ ^[1-4]$ ]]; then
    echo -e "${RED}Erreur: Le numéro de scénario doit être 1, 2, 3 ou 4${NC}"
    exit 1
fi

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}  LoRaWAN ADR Module - Scénario $SCENARIO${NC}"
echo -e "${CYAN}  (Exécution séquentielle)${NC}"
echo -e "${CYAN}========================================${NC}"

# Configuration du simulateur NS-3
NS3_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." >/dev/null 2>&1 && pwd)"
SIMULATION_NAME="lorawan-adr-simulation-module"

# Nombre de messages par device par combinaison
NUM_MESSAGES=110

# Nombre de répétitions par configuration
NUM_RUNS=1

# Répertoires de résultats (créés par la simulation C++)
RESULTS_DIR="$NS3_DIR/resultsfinal"
RESULTS_SUMMARIES_DIR="$RESULTS_DIR/summaries"

# Algorithmes ADR à tester
ADR_ALGOS=("No-ADR" "ADR-MAX" "ADR-AVG" "ADR-MIN" "ADR-Lite")

# Compteur de progression
current_sim=0
total_sims=0
scenario_start_time=0

# ==============================================================================
# Fonction pour formater le temps en heures:minutes:secondes
# ==============================================================================
format_time() {
    local total_seconds=$1
    local hours=$((total_seconds / 3600))
    local minutes=$(((total_seconds % 3600) / 60))
    local seconds=$((total_seconds % 60))
    printf "%02d:%02d:%02d" $hours $minutes $seconds
}

# ==============================================================================
# Fonction pour afficher la barre de progression
# ==============================================================================
show_progress_bar() {
    local current=$1
    local total=$2
    local bar_width=40
    local percent=$((current * 100 / total))
    local filled=$((current * bar_width / total))
    local empty=$((bar_width - filled))
    
    # Calculer le temps écoulé et estimé
    local now=$(date +%s)
    local elapsed=$((now - scenario_start_time))
    local elapsed_fmt=$(format_time $elapsed)
    
    local eta="--:--:--"
    if [ $current -gt 0 ]; then
        local remaining=$(( (elapsed * (total - current)) / current ))
        eta=$(format_time $remaining)
    fi
    
    # Construire la barre
    local bar=""
    for ((i=0; i<filled; i++)); do bar+="█"; done
    for ((i=0; i<empty; i++)); do bar+="░"; done
    
    # Afficher la barre de progression
    echo -e "${CYAN}┌────────────────────────────────────────────────────────┐${NC}"
    echo -e "${CYAN}│${NC} [${GREEN}${bar}${NC}] ${YELLOW}${percent}%${NC}        ${CYAN}│${NC}"
    echo -e "${CYAN}│${NC} Progression: ${current}/${total} simulations               ${CYAN}│${NC}"
    echo -e "${CYAN}│${NC} Temps écoulé: ${elapsed_fmt} | Restant estimé: ${eta}  ${CYAN}│${NC}"
    echo -e "${CYAN}└────────────────────────────────────────────────────────┘${NC}"
}

# ==============================================================================
# Fonction pour ajouter une ligne au summary à partir du fichier sim
# ==============================================================================
append_summary_from_sim() {
    local sim_file=$1
    local summary_file=$2
    local scenario_name=$3
    
    if [ ! -f "$sim_file" ]; then
        return 1
    fi
    
    # Format du fichier sim: Scenario,NumDevices,MobilitySpeed,TrafficInterval,MaxRandomLoss,ADR,RunNumber,TotalPackets,SuccessfulPackets,PDR_Percent,TotalEnergy_J,AvgEnergy_mJ
    # On extrait les colonnes nécessaires et on les réorganise
    tail -n +2 "$sim_file" | while IFS=',' read -r scen dev mob traf sig adr run total success pdr total_e avg_e; do
        echo "$scenario_name,$adr,$dev,$mob,$traf,$sig,$run,$total,$success,$pdr,$avg_e" >> "$summary_file"
    done
    
    return 0
}

# ==============================================================================
# Fonction pour initialiser le fichier summary unique du scénario
# ==============================================================================
init_scenario_summary() {
    local summary_file=$1
    # Créer le fichier avec le header
    echo "Scenario,Algorithm,NumDevices,MobilitySpeed,TrafficInterval,MaxRandomLoss,RunNumber,TotalPackets,SuccessfulPackets,PDR_Percent,AvgEnergy_mJ" > "$summary_file"
}

# ==============================================================================
# Fonction pour exécuter une simulation unique
# ==============================================================================
run_single_simulation() {
    local scenario=$1
    local density=$2
    local mobility=$3
    local traffic=$4
    local maxRandomLoss=$5
    local adr_algo=$6
    local run=$7
    local scenario_name=$8
    local summary_file=$9

    cd "$NS3_DIR"
    
    # Calculer le temps de simulation pour NUM_MESSAGES messages par device
    # simTime = NUM_MESSAGES * trafficInterval (chaque device envoie un message tous les trafficInterval secondes)
    local simTime=$(awk -v t="$traffic" -v n="$NUM_MESSAGES" 'BEGIN{printf "%.0f", n*t}')
    
    # Incrémenter le compteur
    ((current_sim++))
    
    # Afficher la barre de progression
    show_progress_bar $current_sim $total_sims
    echo -e "${BLUE}Simulation:${NC} D=$density, M=$mobility, T=$traffic, L=$maxRandomLoss, A=$adr_algo, R=$run"
    
    # Formater les nombres comme la simulation le fait
    local mob_fmt=$(printf "%.1f" "$mobility")
    local sig_fmt=$(printf "%.2f" "$maxRandomLoss")
    
    # Fichier sim généré par la simulation C++
    local sim_file="$NS3_DIR/resultsfinal/sim_scen${scenario}_dev${density}_mob${mob_fmt}_traf${traffic}_sig${sig_fmt}_${adr_algo}_run${run}.csv"
    
    # Supprimer l'ancien fichier sim pour cette config
    rm -f "$sim_file" 2>/dev/null
    
    # Exécuter la simulation (sans enregistrer les logs)
    ./ns3 run "$SIMULATION_NAME --scenario=$scenario --numDevices=$density --mobilitySpeed=$mobility --trafficInterval=$traffic --maxRandomLoss=$maxRandomLoss --adrAlgo=$adr_algo --runNumber=$run --simulationTime=$simTime" 2>&1
    
    # Ajouter au summary à partir du fichier sim
    if [ -f "$sim_file" ]; then
        if append_summary_from_sim "$sim_file" "$summary_file" "$scenario_name"; then
            echo -e "${GREEN}✓ Ligne ajoutée au summary${NC}"
            
            # Supprimer le fichier sim après traitement
            rm -f "$sim_file" 2>/dev/null
            echo -e "${CYAN}  (fichier sim supprimé)${NC}"
        else
            echo -e "${RED} Échec ajout au summary${NC}"
        fi
    else
        echo -e "${RED} Fichier sim non créé: $(basename $sim_file)${NC}"
    fi
    
    # Supprimer le fichier nodeData
    rm -f "$NS3_DIR/resultsfinal/nodeData_run${run}.txt" 2>/dev/null
    
    echo ""
}

# ==============================================================================
# SCENARIO 1: Variation de la densité (nombre de dispositifs)
# ==============================================================================
run_scenario1() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}  SCENARIO 1: Variation de la densité${NC}"
    echo -e "${BLUE}========================================${NC}"

    local scenario_name="density"
    local summary_dir="$NS3_DIR/resultsfinal/summaries"
    mkdir -p "$summary_dir"
    local summary_file="$summary_dir/summary_scenario1.csv"
    
    # Initialiser le fichier summary
    init_scenario_summary "$summary_file"

    DENSITIES=(100 200 300 400 500 600 700 800 900 1000)
    MOBILITIES_S1=(0 33.33 60)
    TRAFFIC_INTERVALS_S1=(3600 145 72)
    MAX_RANDOM_LOSS_S1=(0 3.96 7.92)

    # DENSITIES=(4 10)
    # MOBILITIES_S1=(0 )
    # TRAFFIC_INTERVALS_S1=(3600 )
    # MAX_RANDOM_LOSS_S1=(0 )

    total_sims=$((${#DENSITIES[@]} * ${#MOBILITIES_S1[@]} * ${#TRAFFIC_INTERVALS_S1[@]} * ${#MAX_RANDOM_LOSS_S1[@]} * ${#ADR_ALGOS[@]} * NUM_RUNS))
    current_sim=0
    scenario_start_time=$(date +%s)

    echo -e "${YELLOW}Total simulations: $total_sims${NC}"
    echo -e "${GREEN}Démarrage de l'exécution séquentielle...${NC}"
    echo -e "${YELLOW}Heure de début: $(date '+%Y-%m-%d %H:%M:%S')${NC}"

    for density in "${DENSITIES[@]}"; do
        for mobility in "${MOBILITIES_S1[@]}"; do
            for traffic in "${TRAFFIC_INTERVALS_S1[@]}"; do
                for maxLoss in "${MAX_RANDOM_LOSS_S1[@]}"; do
                    for run in $(seq 1 $NUM_RUNS); do
                        for adr_algo in "${ADR_ALGOS[@]}"; do
                            run_single_simulation "$SCENARIO" "$density" "$mobility" "$traffic" "$maxLoss" "$adr_algo" "$run" "$scenario_name" "$summary_file"
                        done
                    done
                done
            done
        done
    done

    local scenario_end_time=$(date +%s)
    local total_elapsed=$((scenario_end_time - scenario_start_time))
    echo -e "${GREEN}Scenario 1 completed!${NC}"
    echo -e "${CYAN}Temps total: $(format_time $total_elapsed)${NC}"
    echo -e "${GREEN}Summary: $summary_file${NC}"
}

# ==============================================================================
# SCENARIO 2: Variation de la mobilité
# ==============================================================================
run_scenario2() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}  SCENARIO 2: Variation de la mobilité${NC}"
    echo -e "${BLUE}========================================${NC}"

    local scenario_name="mobilite"
    local summary_dir="$NS3_DIR/resultsfinal/summaries"
    mkdir -p "$summary_dir"
    local summary_file="$summary_dir/summary_scenario2.csv"
    
    # Initialiser le fichier summary
    init_scenario_summary "$summary_file"

    MOBILITIES=(0 6.67 13.33 20 26.67 33.33 40 46.67 53.33 60)
    DENSITIES_S2=(100 550 1000)
    TRAFFIC_INTERVALS_S2=(3600 145 72)
    MAX_RANDOM_LOSS_S2=(0 3.96 7.92)

    #  MOBILITIES=(0 60)
    # DENSITIES_S2=(100)
    # TRAFFIC_INTERVALS_S2=(3600)
    # MAX_RANDOM_LOSS_S2=(0)

    total_sims=$((${#MOBILITIES[@]} * ${#DENSITIES_S2[@]} * ${#TRAFFIC_INTERVALS_S2[@]} * ${#MAX_RANDOM_LOSS_S2[@]} * ${#ADR_ALGOS[@]} * NUM_RUNS))
    current_sim=0
    scenario_start_time=$(date +%s)

    echo -e "${YELLOW}Total simulations: $total_sims${NC}"
    echo -e "${GREEN}Démarrage de l'exécution séquentielle...${NC}"
    echo -e "${YELLOW}Heure de début: $(date '+%Y-%m-%d %H:%M:%S')${NC}"

    for mobility in "${MOBILITIES[@]}"; do
        for density in "${DENSITIES_S2[@]}"; do
            for traffic in "${TRAFFIC_INTERVALS_S2[@]}"; do
                for maxLoss in "${MAX_RANDOM_LOSS_S2[@]}"; do
                    for run in $(seq 1 $NUM_RUNS); do
                        for adr_algo in "${ADR_ALGOS[@]}"; do
                            run_single_simulation "$SCENARIO" "$density" "$mobility" "$traffic" "$maxLoss" "$adr_algo" "$run" "$scenario_name" "$summary_file"
                        done
                    done
                done
            done
        done
    done

    local scenario_end_time=$(date +%s)
    local total_elapsed=$((scenario_end_time - scenario_start_time))
    echo -e "${GREEN}Scenario 2 completed!${NC}"
    echo -e "${CYAN}Temps total: $(format_time $total_elapsed)${NC}"
    echo -e "${GREEN}Summary: $summary_file${NC}"
}

# ==============================================================================
# SCENARIO 3: Variation de la perte aléatoire (sigma)
# ==============================================================================
run_scenario3() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}  SCENARIO 3: Variation de la perte aléatoire${NC}"
    echo -e "${BLUE}========================================${NC}"

    local scenario_name="sigma"
    local summary_dir="$NS3_DIR/resultsfinal/summaries"
    mkdir -p "$summary_dir"
    local summary_file="$summary_dir/summary_scenario3.csv"
    
    # Initialiser le fichier summary
    init_scenario_summary "$summary_file"

    MAX_RANDOM_LOSS=(0 1.98 3.96 5.94 7.92)
    DENSITIES_S3=(100 550 1000)
    MOBILITIES_S3=(0 33.33 60)
    TRAFFIC_INTERVALS_S3=(3600 145 72)

    # MAX_RANDOM_LOSS=(0 1.98)
    # DENSITIES_S3=(100)
    # MOBILITIES_S3=(0)
    # TRAFFIC_INTERVALS_S3=(3600)

    total_sims=$((${#MAX_RANDOM_LOSS[@]} * ${#DENSITIES_S3[@]} * ${#MOBILITIES_S3[@]} * ${#TRAFFIC_INTERVALS_S3[@]} * ${#ADR_ALGOS[@]} * NUM_RUNS))
    current_sim=0
    scenario_start_time=$(date +%s)

    echo -e "${YELLOW}Total simulations: $total_sims${NC}"
    echo -e "${GREEN}Démarrage de l'exécution séquentielle...${NC}"
    echo -e "${YELLOW}Heure de début: $(date '+%Y-%m-%d %H:%M:%S')${NC}"

    for maxLoss in "${MAX_RANDOM_LOSS[@]}"; do
        for density in "${DENSITIES_S3[@]}"; do
            for mobility in "${MOBILITIES_S3[@]}"; do
                for traffic in "${TRAFFIC_INTERVALS_S3[@]}"; do
                    for run in $(seq 1 $NUM_RUNS); do
                        for adr_algo in "${ADR_ALGOS[@]}"; do
                            run_single_simulation "$SCENARIO" "$density" "$mobility" "$traffic" "$maxLoss" "$adr_algo" "$run" "$scenario_name" "$summary_file"
                        done
                    done
                done
            done
        done
    done

    local scenario_end_time=$(date +%s)
    local total_elapsed=$((scenario_end_time - scenario_start_time))
    echo -e "${GREEN}Scenario 3 completed!${NC}"
    echo -e "${CYAN}Temps total: $(format_time $total_elapsed)${NC}"
    echo -e "${GREEN}Summary: $summary_file${NC}"
}

# ==============================================================================
# SCENARIO 4: Variation de la charge du trafic (intervalle d'envoi)
# ==============================================================================
run_scenario4() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}  SCENARIO 4: Variation du trafic${NC}"
    echo -e "${BLUE}========================================${NC}"

    local scenario_name="intervalle_d_envoie"
    local summary_dir="$NS3_DIR/resultsfinal/summaries"
    mkdir -p "$summary_dir"
    local summary_file="$summary_dir/summary_scenario4.csv"
    
    # Initialiser le fichier summary
    init_scenario_summary "$summary_file"

    TRAFFIC_INTERVALS=(3600 327 240 180 145 120 103 90 80 72)
    DENSITIES_S4=(100 550 1000)
    MOBILITIES_S4=(0 33.33 60)
    MAX_RANDOM_LOSS_S4=(0 3.96 7.92)


    # TRAFFIC_INTERVALS=(3600)
    # DENSITIES_S4=(100)
    # MOBILITIES_S4=(0)
    # MAX_RANDOM_LOSS_S4=(0)

    total_sims=$((${#TRAFFIC_INTERVALS[@]} * ${#DENSITIES_S4[@]} * ${#MOBILITIES_S4[@]} * ${#MAX_RANDOM_LOSS_S4[@]} * ${#ADR_ALGOS[@]} * NUM_RUNS))
    current_sim=0
    scenario_start_time=$(date +%s)

    echo -e "${YELLOW}Total simulations: $total_sims${NC}"
    echo -e "${GREEN}Démarrage de l'exécution séquentielle...${NC}"
    echo -e "${YELLOW}Heure de début: $(date '+%Y-%m-%d %H:%M:%S')${NC}"

    for traffic in "${TRAFFIC_INTERVALS[@]}"; do
        for density in "${DENSITIES_S4[@]}"; do
            for mobility in "${MOBILITIES_S4[@]}"; do
                for maxLoss in "${MAX_RANDOM_LOSS_S4[@]}"; do
                    for run in $(seq 1 $NUM_RUNS); do
                        for adr_algo in "${ADR_ALGOS[@]}"; do
                            run_single_simulation "$SCENARIO" "$density" "$mobility" "$traffic" "$maxLoss" "$adr_algo" "$run" "$scenario_name" "$summary_file"
                        done
                    done
                done
            done
        done
    done

    local scenario_end_time=$(date +%s)
    local total_elapsed=$((scenario_end_time - scenario_start_time))
    echo -e "${GREEN}Scenario 4 completed!${NC}"
    echo -e "${CYAN}Temps total: $(format_time $total_elapsed)${NC}"
    echo -e "${GREEN}Summary: $summary_file${NC}"
}

# ==============================================================================
# Exécution du scénario sélectionné
# ==============================================================================
case $SCENARIO in
    1)
        run_scenario1
        ;;
    2)
        run_scenario2
        ;;
    3)
        run_scenario3
        ;;
    4)
        run_scenario4
        ;;
esac

# ==============================================================================
# Nettoyage et résumé final
# ==============================================================================

# Supprimer tous les fichiers temporaires restants
rm -f "$NS3_DIR/resultsfinal"/sim_scen*.csv 2>/dev/null
rm -f "$NS3_DIR/resultsfinal"/nodeData_*.txt 2>/dev/null
rm -rf "$NS3_DIR/resultsfinal/details" 2>/dev/null

# Supprimer les anciens sous-dossiers de scénarios
rm -rf "$NS3_DIR/resultsfinal/summaries/density" 2>/dev/null
rm -rf "$NS3_DIR/resultsfinal/summaries/mobilite" 2>/dev/null
rm -rf "$NS3_DIR/resultsfinal/summaries/sigma" 2>/dev/null
rm -rf "$NS3_DIR/resultsfinal/summaries/intervalle_d_envoie" 2>/dev/null

echo -e "\n${CYAN}========================================${NC}"
echo -e "${CYAN}  SIMULATION SCÉNARIO $SCENARIO TERMINÉE!${NC}"
echo -e "${CYAN}========================================${NC}"

echo -e "\n${BLUE}Résultats dans:${NC}"
echo -e "${GREEN}  $NS3_DIR/resultsfinal/summaries/${NC}"

summary_file="$NS3_DIR/resultsfinal/summaries/summary_scenario${SCENARIO}.csv"
if [ -f "$summary_file" ]; then
    num_lines=$(($(wc -l < "$summary_file") - 1))
    echo -e "${GREEN}Fichier summary: summary_scenario${SCENARIO}.csv (${num_lines} lignes)${NC}"
else
    echo -e "${RED}Fichier summary non créé${NC}"
fi
