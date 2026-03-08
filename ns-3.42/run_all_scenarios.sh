#!/bin/bash
# Run all 5 simulation scenarios in background (detached mode)
# Usage: nohup ./run_all_scenarios.sh > run_all.log 2>&1 &
# Or simply: ./run_all_scenarios.sh

cd "$(dirname "$0")"

echo "=========================================="
echo " Lancement des 5 scénarios en parallèle"
echo " $(date)"
echo "=========================================="

for i in 1 2 3 4 5; do
    echo "[$(date '+%H:%M:%S')] Démarrage scénario $i ..."
    nohup ./scratch/run_simulations_module.sh "$i" > "log_scenario_${i}.txt" 2>&1 &
    echo "  PID=$! -> log_scenario_${i}.txt"
done

echo ""
echo "=========================================="
echo " Tous les scénarios sont lancés."
echo " Suivre la progression :"
echo "   tail -f log_scenario_1.txt"
echo "   tail -f log_scenario_2.txt"
echo "   tail -f log_scenario_3.txt"
echo "   tail -f log_scenario_4.txt"
echo "   tail -f log_scenario_5.txt"
echo ""
echo " Vérifier les processus :"
echo "   jobs -l"
echo "   ps aux | grep run_simulations"
echo "=========================================="

# Wait for all background jobs to finish
wait
echo ""
echo "[$(date '+%H:%M:%S')] Tous les scénarios sont terminés."
