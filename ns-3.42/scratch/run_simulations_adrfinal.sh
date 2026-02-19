#!/bin/bash

# Script d'automatisation pour les simulations LoRaWAN ADR - Par scénario avec exécution parallèle
# Usage: ./run_scenario.sh [1|2|3|4]
# Exemple: ./run_scenario.sh 1  (pour exécuter uniquement le scénario 1)

# Couleurs pour l'affichage
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Nombre de processus parallèles par scénario
MAX_PARALLEL_S1=270  # Scénario 1
MAX_PARALLEL_S2=270  # Scénario 2
MAX_PARALLEL_S3=135  # Scénario 3
MAX_PARALLEL_S4=270  # Scénario 4

# Vérifier l'argument
if [ $# -eq 0 ]; then
    echo -e "${RED}Erreur: Veuillez spécifier le numéro du scénario (1, 2, 3 ou 4)${NC}"
    echo -e "${YELLOW}Usage: $0 [1|2|3|4]${NC}"
    echo -e "${YELLOW}Exemples:${NC}"
    echo -e "  $0 1  # Exécute le scénario 1 (variation densité)"
    echo -e "  $0 2  # Exécute le scénario 2 (variation mobilité)"
    echo -e "  $0 3  # Exécute le scénario 3 (variation trafic)"
    echo -e "  $0 4  # Exécute le scénario 4 (variation sigma)"
    exit 1
fi

SCENARIO=$1

# Valider le numéro de scénario
if [[ ! "$SCENARIO" =~ ^[1-4]$ ]]; then
    echo -e "${RED}Erreur: Le numéro de scénario doit être 1, 2, 3 ou 4${NC}"
    exit 1
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Simulation LoRaWAN ADR - Scénario $SCENARIO${NC}"
echo -e "${GREEN}========================================${NC}"

# Configuration du simulateur NS-3
NS3_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." >/dev/null 2>&1 && pwd)"
SIMULATION_NAME="lorawan-adr-simulationfinal"

# Nombre de répétitions par configuration
NUM_RUNS=100
# Nombre de messages par nœud (passé au simulateur)
MAX_MESSAGES=110

# Répertoires de résultats (sous le dossier du projet ns-3)
RESULTS_DIR="$NS3_DIR/resultsfinal"
RESULTS_SUMMARIES_DIR="$RESULTS_DIR/summaries"
mkdir -p "$RESULTS_SUMMARIES_DIR"

# Map scenario number to folder name and ensure it exists
scenarioName="scenario${SCENARIO}"
if [ "$SCENARIO" -eq 1 ]; then
    scenarioName="density"
elif [ "$SCENARIO" -eq 2 ]; then
    scenarioName="mobilite"
elif [ "$SCENARIO" -eq 3 ]; then
    scenarioName="sigma"
elif [ "$SCENARIO" -eq 4 ]; then
    scenarioName="intervalle_d_envoie"
fi
SCENARIO_SUMMARY_DIR="$RESULTS_SUMMARIES_DIR/$scenarioName"
mkdir -p "$SCENARIO_SUMMARY_DIR"

# Temps de simulation (1 heure = 3600 secondes)
SIM_TIME=3600

# Algorithmes ADR à tester
ADR_ALGOS=("No-ADR" "ADR-MAX" "ADR-AVG" "ADR-Lite")

# ==============================================================================
# Fonction pour gérer l'exécution parallèle des simulations
# ==============================================================================

# Fonction pour exécuter une simulation unique
run_single_simulation() {
    local scenario=$1
    local density=$2
    local mobility=$3
    local traffic=$4
    local sigma=$5
    local adr_algo=$6
    local run=$7
    local max_messages=$8
    local ns3_dir=$9
    local simulation_name=${10}
    local results_dir=${11}

    cd "$ns3_dir"
    simTime=$(awk -v m="$max_messages" -v t="$traffic" 'BEGIN{printf "%.0f", m*t + 60}')
    ./ns3 run "$simulation_name --scenario=$scenario --numDevices=$density --mobilitySpeed=$mobility --trafficInterval=$traffic --sigma=$sigma --adrAlgo=$adr_algo --maxMessages=$max_messages --runNumber=$run --simulationTime=$simTime" >> "$results_dir/simulations_${scenario}.log" 2>&1

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓${NC} S$scenario: Density=$density, Mob=$mobility, Traf=$traffic, Sig=$sigma, Algo=$adr_algo, Run=$run"
    else
        echo -e "${RED}✗${NC} S$scenario: Density=$density, Mob=$mobility, Traf=$traffic, Sig=$sigma, Algo=$adr_algo, Run=$run [ERROR]"
    fi
}

# Export de la fonction pour qu'elle soit accessible par GNU Parallel
export -f run_single_simulation

# Export des variables pour les sous-processus
export RED GREEN YELLOW BLUE NC

# Fonction pour vérifier si GNU Parallel est installé
check_parallel() {
    if ! command -v parallel &> /dev/null; then
        echo -e "${RED}Erreur: GNU Parallel n'est pas installé!${NC}"
        echo -e "${YELLOW}Installation sur Ubuntu/Debian: sudo apt-get install parallel${NC}"
        echo -e "${YELLOW}Installation sur Fedora/RHEL: sudo dnf install parallel${NC}"
        exit 1
    fi
}

check_parallel

# ==============================================================================
# SCENARIO 1: Variation de la densité (nombre de dispositifs)
# ==============================================================================
run_scenario1() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}  SCENARIO 1: Variation de la densité${NC}"
    echo -e "${BLUE}========================================${NC}"

    DENSITIES=(100 200 300 400 500 600 700 800 900 1000)
    MOBILITIES_S1=(0 33.33 60)
    TRAFFIC_INTERVALS_S1=(3600 145 72)
    SIGMAS_S1=(0 3.96 7.92)

    total_configs=$((${#DENSITIES[@]} * ${#MOBILITIES_S1[@]} * ${#TRAFFIC_INTERVALS_S1[@]} * ${#SIGMAS_S1[@]} * ${#ADR_ALGOS[@]} * NUM_RUNS))

    echo -e "${YELLOW}Total configurations Scenario 1: $total_configs${NC}"
    echo -e "${YELLOW}Processus parallèles: $MAX_PARALLEL_S1${NC}"

    # Créer un fichier temporaire avec toutes les combinaisons
    TASK_FILE=$(mktemp)

    for density in "${DENSITIES[@]}"; do
        for mobility in "${MOBILITIES_S1[@]}"; do
            for traffic in "${TRAFFIC_INTERVALS_S1[@]}"; do
                for sigma in "${SIGMAS_S1[@]}"; do
                    for run in $(seq 1 $NUM_RUNS); do
                        for adr_algo in "${ADR_ALGOS[@]}"; do
                            echo "$SCENARIO $density $mobility $traffic $sigma $adr_algo $run $MAX_MESSAGES $NS3_DIR $SIMULATION_NAME $RESULTS_DIR" >> "$TASK_FILE"
                        done
                    done
                done
            done
        done
    done

    # Exécuter toutes les simulations en parallèle
    echo -e "${GREEN}Démarrage de l'exécution parallèle...${NC}"
    parallel -j $MAX_PARALLEL_S1 --colsep ' ' run_single_simulation :::: "$TASK_FILE"

    # Nettoyer le fichier temporaire
    rm -f "$TASK_FILE"

    echo -e "${GREEN}Scenario 1 completed!${NC}"
}

# ==============================================================================
# SCENARIO 2: Variation de la mobilité
# ==============================================================================
run_scenario2() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}  SCENARIO 2: Variation de la mobilité${NC}"
    echo -e "${BLUE}========================================${NC}"

    MOBILITIES=(0 6.67 13.33 20 26.67 33.33 40 46.67 53.33 60)
    DENSITIES_S2=(100 550 1000)
    TRAFFIC_INTERVALS_S2=(3600 145 72)
    SIGMAS_S2=(0 3.96 7.92)

    total_configs=$((${#MOBILITIES[@]} * ${#DENSITIES_S2[@]} * ${#TRAFFIC_INTERVALS_S2[@]} * ${#SIGMAS_S2[@]} * ${#ADR_ALGOS[@]} * NUM_RUNS))

    echo -e "${YELLOW}Total configurations Scenario 2: $total_configs${NC}"
    echo -e "${YELLOW}Processus parallèles: $MAX_PARALLEL_S2${NC}"

    # Créer un fichier temporaire avec toutes les combinaisons
    TASK_FILE=$(mktemp)

    for mobility in "${MOBILITIES[@]}"; do
        for density in "${DENSITIES_S2[@]}"; do
            for traffic in "${TRAFFIC_INTERVALS_S2[@]}"; do
                for sigma in "${SIGMAS_S2[@]}"; do
                    for run in $(seq 1 $NUM_RUNS); do
                        for adr_algo in "${ADR_ALGOS[@]}"; do
                            echo "$SCENARIO $density $mobility $traffic $sigma $adr_algo $run $MAX_MESSAGES $NS3_DIR $SIMULATION_NAME $RESULTS_DIR" >> "$TASK_FILE"
                        done
                    done
                done
            done
        done
    done

    # Exécuter toutes les simulations en parallèle
    echo -e "${GREEN}Démarrage de l'exécution parallèle...${NC}"
    parallel -j $MAX_PARALLEL_S2 --colsep ' ' run_single_simulation :::: "$TASK_FILE"

    # Nettoyer le fichier temporaire
    rm -f "$TASK_FILE"

    echo -e "${GREEN}Scenario 2 completed!${NC}"
}

# ==============================================================================
# SCENARIO 3: Variation de sigma (saturation du canal)
# ==============================================================================
run_scenario3() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}  SCENARIO 3: Variation de sigma${NC}"
    echo -e "${BLUE}========================================${NC}"

    SIGMAS=(0 1.98 3.96 5.94 7.92)
    DENSITIES_S3=(100 550 1000)
    MOBILITIES_S3=(0 33.33 60)
    TRAFFIC_INTERVALS_S3=(3600 145 72)

    total_configs=$((${#SIGMAS[@]} * ${#DENSITIES_S3[@]} * ${#MOBILITIES_S3[@]} * ${#TRAFFIC_INTERVALS_S3[@]} * ${#ADR_ALGOS[@]} * NUM_RUNS))

    echo -e "${YELLOW}Total configurations Scenario 3: $total_configs${NC}"
    echo -e "${YELLOW}Processus parallèles: $MAX_PARALLEL_S3${NC}"

    # Créer un fichier temporaire avec toutes les combinaisons
    TASK_FILE=$(mktemp)

    for sigma in "${SIGMAS[@]}"; do
        for density in "${DENSITIES_S3[@]}"; do
            for mobility in "${MOBILITIES_S3[@]}"; do
                for traffic in "${TRAFFIC_INTERVALS_S3[@]}"; do
                    for run in $(seq 1 $NUM_RUNS); do
                        for adr_algo in "${ADR_ALGOS[@]}"; do
                            echo "$SCENARIO $density $mobility $traffic $sigma $adr_algo $run $MAX_MESSAGES $NS3_DIR $SIMULATION_NAME $RESULTS_DIR" >> "$TASK_FILE"
                        done
                    done
                done
            done
        done
    done

    # Exécuter toutes les simulations en parallèle
    echo -e "${GREEN}Démarrage de l'exécution parallèle...${NC}"
    parallel -j $MAX_PARALLEL_S3 --colsep ' ' run_single_simulation :::: "$TASK_FILE"

    # Nettoyer le fichier temporaire
    rm -f "$TASK_FILE"

    echo -e "${GREEN}Scenario 3 completed!${NC}"
}

# ==============================================================================
# SCENARIO 4: Variation de la charge du trafic (intervalle d'envoi)
# ==============================================================================
run_scenario4() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}  SCENARIO 4: Variation du trafic${NC}"
    echo -e "${BLUE}========================================${NC}"

    TRAFFIC_INTERVALS=(3600 327 240 180 145 120 103 90 80 72)
    DENSITIES_S4=(100 550 1000)
    MOBILITIES_S4=(0 33.33 60)
    SIGMAS_S4=(0 3.96 7.92)

    total_configs=$((${#TRAFFIC_INTERVALS[@]} * ${#DENSITIES_S4[@]} * ${#MOBILITIES_S4[@]} * ${#SIGMAS_S4[@]} * ${#ADR_ALGOS[@]} * NUM_RUNS))

    echo -e "${YELLOW}Total configurations Scenario 4: $total_configs${NC}"
    echo -e "${YELLOW}Processus parallèles: $MAX_PARALLEL_S4${NC}"

    # Créer un fichier temporaire avec toutes les combinaisons
    TASK_FILE=$(mktemp)

    for traffic in "${TRAFFIC_INTERVALS[@]}"; do
        for density in "${DENSITIES_S4[@]}"; do
            for mobility in "${MOBILITIES_S4[@]}"; do
                for sigma in "${SIGMAS_S4[@]}"; do
                    for run in $(seq 1 $NUM_RUNS); do
                        for adr_algo in "${ADR_ALGOS[@]}"; do
                            echo "$SCENARIO $density $mobility $traffic $sigma $adr_algo $run $MAX_MESSAGES $NS3_DIR $SIMULATION_NAME $RESULTS_DIR" >> "$TASK_FILE"
                        done
                    done
                done
            done
        done
    done

    # Exécuter toutes les simulations en parallèle
    echo -e "${GREEN}Démarrage de l'exécution parallèle...${NC}"
    parallel -j $MAX_PARALLEL_S4 --colsep ' ' run_single_simulation :::: "$TASK_FILE"

    # Nettoyer le fichier temporaire
    rm -f "$TASK_FILE"

    echo -e "${GREEN}Scenario 4 completed!${NC}"
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

# After completing the scenario, aggregate all runs into a single CSV per run
# Each run file will contain results from all 4 algorithms
echo -e "\n${BLUE}Aggregating results for each run...${NC}"

aggregate_runs_for_scenario() {
    # Find all individual summary files and group them by run number
    shopt -s nullglob
    local files=("$SCENARIO_SUMMARY_DIR"/summary_scen*.csv)
    
    if [ ${#files[@]} -eq 0 ]; then
        echo -e "${YELLOW}No summary files found in $SCENARIO_SUMMARY_DIR${NC}"
        return
    fi
    
    # Group files by run number and parameter combination
    declare -A run_groups
    
    for f in "${files[@]}"; do
        base=$(basename "$f")
        # Extract run number from filename
        if [[ $base =~ _run([0-9]+)\.csv$ ]]; then
            run_num="${BASH_REMATCH[1]}"
            # Extract parameters (everything before algorithm and run)
            params=$(echo "$base" | sed 's/_No-ADR_run[0-9]*\.csv$//' | sed 's/_ADR-MAX_run[0-9]*\.csv$//' | sed 's/_ADR-AVG_run[0-9]*\.csv$//' | sed 's/_ADR-Lite_run[0-9]*\.csv$//')
            key="${params}_run${run_num}"
            run_groups[$key]+="$f "
        fi
    done
    
    # Create aggregated file for each run
    for key in "${!run_groups[@]}"; do
        local run_files=(${run_groups[$key]})
        local outFile="$SCENARIO_SUMMARY_DIR/summary_${scenarioName}_${key}.csv"
        
        # Create header
        echo "Scenario,NumDevices,MobilitySpeed,TrafficInterval,Sigma,Algorithm,RunNumber,TotalPackets,SuccessfulPackets,PDR_Percent,AvgEnergy_mJ" > "$outFile"
        
        # Append data from each algorithm
        for f in ${run_files[@]}; do
            base=$(basename "$f")
            # Extract algorithm
            if [[ $base =~ _(No-ADR|ADR-MAX|ADR-AVG|ADR-Lite)_run ]]; then
                algo="${BASH_REMATCH[1]}"
                # Skip header and append data with algorithm column
                tail -n +2 "$f" | awk -v alg="$algo" -v scen="$scenarioName" 'BEGIN{OFS=","} {
                    # Insert scenario as first column, algorithm before last two columns
                    print scen,$1,$2,$3,$4,alg,$5,$6,$7,$8,$9
                }' >> "$outFile"
            fi
        done
        
        echo -e "${GREEN}Created: $outFile${NC}"
    done
}

aggregate_runs_for_scenario

# Create one aggregated file per repetition (run) that contains all parameter combinations
# and all 4 algorithms (expected 1080 rows per run for scenario 1: 270 combos * 4 algs)
aggregate_by_run() {
    for runnum in $(seq 1 $NUM_RUNS); do
        # Match original per-algo summary filenames produced by the C++ simulation
        files=("$SCENARIO_SUMMARY_DIR"/summary_scen${SCENARIO}_*"_run${runnum}.csv")
        # Skip if no files for this run
        if [ ! -e "${files[0]}" ]; then
            continue
        fi

        outFile="$SCENARIO_SUMMARY_DIR/summary_${scenarioName}_run${runnum}.csv"

        # Use the header from the first file and add alg,scenario columns
        header=$(head -n1 "${files[0]}" 2>/dev/null || echo "")
        if [ -z "$header" ]; then
            echo -e "${YELLOW}Could not read header for run $runnum, skipping${NC}"
            continue
        fi
        echo "$header,alg,scenario" > "$outFile"

        # Append rows from each per-algo summary for this run
        for f in "${files[@]}"; do
            # Extract algorithm from filename (No-ADR, ADR-MAX, ADR-AVG, ADR-Lite)
            base=$(basename "$f")
            alg=$(echo "$base" | sed -n 's/.*_\(No-ADR\|ADR-MAX\|ADR-AVG\|ADR-Lite\)_run[0-9]\+\.csv/\1/p')
            if [ -z "$alg" ]; then
                alg="$(basename "$f" .csv)"
            fi
            tail -n +2 "$f" | awk -v alg="$alg" -v scen="$scenarioName" 'BEGIN{OFS=","} {print $0,alg,scen}' >> "$outFile"
        done

        echo -e "${GREEN}Created per-run aggregated file: $outFile${NC}"
    done
}

aggregate_by_run

# ==============================================================================
# Résumé final
# ==============================================================================
echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}  SIMULATION SCÉNARIO $SCENARIO TERMINÉE!${NC}"
echo -e "${GREEN}========================================${NC}"

echo -e "\n${BLUE}Les résultats sont disponibles dans le répertoire 'resultsfinal/summaries/$scenarioName/'${NC}"

# Compter les fichiers générés
num_files=$(ls -1 "$SCENARIO_SUMMARY_DIR"/summary_${scenarioName}_*.csv 2>/dev/null | wc -l)
echo -e "${GREEN}Fichiers de résumé agrégés générés: $num_files${NC}"
echo -e "${YELLOW}Chaque fichier contient les résultats des 4 algorithmes pour une combinaison de paramètres et un run${NC}"