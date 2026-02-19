#!/bin/bash

# Script d'aide pour l'exécution des simulations LoRaWAN
# Usage: ./run_simulation.sh [compile|run|run-all|plot|plot-all|all|help] [simulation_type]

set -e

NS3_DIR="ns-3.42"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Liste des simulations disponibles
SIMULATIONS=(
    "lorawan-logistics-mab-static"
    "lorawan-logistics-mab-static-interf"
    "lorawan-logistics-mab-mobile"
    "lorawan-logistics-mab-mobile-interf"
    "lorawan-logistics-mab-mixed"
    "lorawan-logistics-mab-mixed-interf"
)

# Correspondance simulation -> dossier de résultats
declare -A RESULT_DIRS
RESULT_DIRS["lorawan-logistics-mab-static"]="lorawan_static_results"
RESULT_DIRS["lorawan-logistics-mab-static-interf"]="lorawan_static_results_interf"
RESULT_DIRS["lorawan-logistics-mab-mobile"]="lorawan_mobile_results"
RESULT_DIRS["lorawan-logistics-mab-mobile-interf"]="lorawan_mobile_results_interf"
RESULT_DIRS["lorawan-logistics-mab-mixed"]="lorawan_mixed_results"
RESULT_DIRS["lorawan-logistics-mab-mixed-interf"]="lorawan_mixed_results_interf"

# Correspondance simulation -> script de visualisation
declare -A PLOT_SCRIPTS
PLOT_SCRIPTS["lorawan-logistics-mab-static"]="plot_lorawan_static.py"
PLOT_SCRIPTS["lorawan-logistics-mab-static-interf"]="plot_lorawan_static.py"
PLOT_SCRIPTS["lorawan-logistics-mab-mobile"]="plot_lorawan_mobile.py"
PLOT_SCRIPTS["lorawan-logistics-mab-mobile-interf"]="plot_lorawan_mobile.py"
PLOT_SCRIPTS["lorawan-logistics-mab-mixed"]="plot_lorawan_mixed.py"
PLOT_SCRIPTS["lorawan-logistics-mab-mixed-interf"]="plot_lorawan_mixed.py"

print_help() {
    echo "Usage: $0 [compile|run|run-all|plot|plot-all|all|help] [simulation_type]"
    echo ""
    echo "Commandes:"
    echo "  compile      - Compiler NS-3 et toutes les simulations"
    echo "  run          - Exécuter une simulation spécifique"
    echo "  run-all      - Exécuter toutes les simulations"
    echo "  plot         - Générer les graphiques pour une simulation"
    echo "  plot-all     - Générer tous les graphiques"
    echo "  all          - Exécuter toutes les étapes (compile + run-all + plot-all)"
    echo "  help         - Afficher cette aide"
    echo ""
    echo "Simulations disponibles:"
    for sim in "${SIMULATIONS[@]}"; do
        echo "  - $sim"
    done
    echo ""
    echo "Exemples:"
    echo "  $0 run lorawan-logistics-mab-mixed-interf"
    echo "  $0 plot lorawan-logistics-mab-static"
    echo "  $0 run-all"
    echo "  $0 all"
    echo ""
    echo "Prérequis:"
    echo "  - NS-3.42 installé"
    echo "  - Python 3.x avec les dépendances (pip install -r requirements.txt)"
}

install_dependencies() {
    echo "=== Installation des dépendances Python ==="
    if [ -f "requirements.txt" ]; then
        pip install -r requirements.txt
        echo "✓ Dépendances installées"
    else
        echo "❌ Fichier requirements.txt non trouvé"
        exit 1
    fi
}

compile_ns3() {
    echo "=== Compilation NS-3 ==="
    cd "$NS3_DIR"
    ./ns3 configure --enable-examples --enable-tests
    ./ns3 build
    cd ..
    echo "✓ Compilation terminée"
}

run_simulation() {
    local sim_name="$1"
    if [ -z "$sim_name" ]; then
        echo "❌ Nom de simulation requis"
        echo "Simulations disponibles:"
        for sim in "${SIMULATIONS[@]}"; do
            echo "  - $sim"
        done
        exit 1
    fi
    
    # Vérifier si la simulation existe
    if [[ ! " ${SIMULATIONS[@]} " =~ " $sim_name " ]]; then
        echo "❌ Simulation '$sim_name' non trouvée"
        echo "Simulations disponibles:"
        for sim in "${SIMULATIONS[@]}"; do
            echo "  - $sim"
        done
        exit 1
    fi
    
    echo "=== Exécution de la simulation: $sim_name ==="
    cd "$NS3_DIR"
    ./ns3 run "$sim_name"
    cd ..
    echo "✓ Simulation '$sim_name' terminée"
}

run_all_simulations() {
    echo "=== Exécution de toutes les simulations ==="
    for sim in "${SIMULATIONS[@]}"; do
        echo "🚀 Exécution de: $sim"
        run_simulation "$sim"
    done
    echo "✓ Toutes les simulations terminées"
}

generate_plots() {
    local sim_name="$1"
    if [ -z "$sim_name" ]; then
        echo "❌ Nom de simulation requis pour les graphiques"
        exit 1
    fi
    
    local result_dir="${RESULT_DIRS[$sim_name]}"
    local plot_script="${PLOT_SCRIPTS[$sim_name]}"
    
    if [ -z "$result_dir" ] || [ -z "$plot_script" ]; then
        echo "❌ Configuration manquante pour la simulation '$sim_name'"
        exit 1
    fi
    
    echo "=== Génération des graphiques pour: $sim_name ==="
    
    # Vérifier si les résultats existent
    if [ ! -d "$result_dir" ]; then
        echo "❌ Dossier de résultats non trouvé: $result_dir"
        echo "Exécutez d'abord la simulation: $sim_name"
        exit 1
    fi
    
    # Vérifier si le script existe
    if [ ! -f "$NS3_DIR/$plot_script" ]; then
        echo "❌ Script de visualisation non trouvé: $plot_script"
        exit 1
    fi
    
    # Exécuter le script de visualisation
    cd "$NS3_DIR"
    python "$plot_script" "../$result_dir/"
    cd ..
    echo "✓ Graphiques générés pour '$sim_name'"
}

generate_all_plots() {
    echo "=== Génération de tous les graphiques ==="
    for sim in "${SIMULATIONS[@]}"; do
        local result_dir="${RESULT_DIRS[$sim]}"
        if [ -d "$result_dir" ]; then
            echo "📊 Génération des graphiques pour: $sim"
            generate_plots "$sim"
        else
            echo "⚠ Pas de résultats pour: $sim (dossier $result_dir manquant)"
        fi
    done
    echo "✓ Génération de tous les graphiques terminée"
}

check_prerequisites() {
    echo "=== Vérification des prérequis ==="
    
    # Vérifier NS-3
    if [ ! -d "$NS3_DIR" ]; then
        echo "❌ Dossier NS-3 non trouvé ($NS3_DIR)"
        exit 1
    fi
    
    # Vérifier Python
    if ! command -v python3 &> /dev/null; then
        echo "❌ Python 3 non installé"
        exit 1
    fi
    
    # Vérifier les dépendances Python
    python3 -c "import pandas, matplotlib, seaborn, numpy" 2>/dev/null || {
        echo "❌ Dépendances Python manquantes"
        echo "Exécutez: pip install -r requirements.txt"
        exit 1
    }
    
    echo "✓ Prérequis vérifiés"
}

case "$1" in
    compile)
        check_prerequisites
        compile_ns3
        ;;
    run)
        check_prerequisites
        run_simulation "$2"
        ;;
    run-all)
        check_prerequisites
        run_all_simulations
        ;;
    plot)
        check_prerequisites
        generate_plots "$2"
        ;;
    plot-all)
        check_prerequisites
        generate_all_plots
        ;;
    all)
        check_prerequisites
        install_dependencies
        compile_ns3
        run_all_simulations
        generate_all_plots
        ;;
    help|--help|-h)
        print_help
        ;;
    "")
        print_help
        ;;
    *)
        echo "❌ Option inconnue: $1"
        print_help
        exit 1
        ;;
esac

echo "✅ Terminé avec succès!"
