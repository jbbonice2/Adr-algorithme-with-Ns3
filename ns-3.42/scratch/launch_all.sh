#!/bin/bash
# Lance les 5 modules de simulation en mode détaché avec nohup
# Chaque module écrit ses logs dans log_<module>.txt

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

for i in 1 2 3 4 5; do
    echo "Lancement du module $i ..."
    nohup ./run_simulations_module.sh "$i" > "log_${i}.txt" 2>&1 &
    echo "  PID: $!"
done

echo ""
echo "Tous les modules ont été lancés en arrière-plan."
echo "Suivre les logs : tail -f log_1.txt log_2.txt log_3.txt log_4.txt log_5.txt"
echo "Vérifier les processus : jobs -l  ou  ps aux | grep run_simulations_module"
