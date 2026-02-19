# Simulations LoRaWAN Logistique Complètes

Ce projet contient un ensemble complet de simulations NS-3 pour l'analyse des performances d'un réseau LoRaWAN dans différents environnements logistiques (statique, mobile, mixte) avec et sans interférences.

## 📋 Prérequis

- NS-3.42 installé et configuré
- Python 3.x avec les bibliothèques suivantes :
  ```bash
  pip install -r requirements.txt
  ```
  
  Ou installer manuellement :
  ```bash
  pip install pandas matplotlib seaborn numpy argparse
  ```

## 🔧 Compilation et Configuration

### 1. Compilation complète du projet NS-3

```bash
# Se placer dans le dossier NS-3
cd ns-3.42

# Configuration avec tous les modules (incluant LoRaWAN)
./ns3 configure --enable-examples --enable-tests

# Compilation complète
./ns3 build
```

### 2. Activation spécifique du module LoRaWAN

```bash
# Vérifier que le module LoRaWAN est disponible
./ns3 show modules | grep lorawan

# Configuration avec LoRaWAN explicitement activé
./ns3 configure --enable-modules=lorawan --enable-examples --enable-tests

# Ou configuration complète (recommandé)
./ns3 configure --enable-examples --enable-tests --enable-logs

# Compilation
./ns3 build
```

### 3. Vérification de l'installation

```bash
# Tester la compilation des simulations LoRaWAN
./ns3 build lorawan-logistics-mab-static

# Vérifier les exemples LoRaWAN disponibles
ls scratch/lorawan-*

# Tester l'exécution d'une simulation simple
./ns3 run /src/lorawan/examples/adr-example.cc
```

## 📊 Analyse des résultats

### 1. Installation des dépendances

```bash
# Vérifier l'environnement
python3 check_environment.py

# Installer les dépendances Python
pip install -r requirements.txt
```

## 🏗️ Structure Complète du Projet

### Vue d'ensemble
```
ns-allinone-3.42/                                     # Dossier racine du projet
├── .config                                           # Configuration système
├── .gitignore                                        # Fichiers à ignorer Git
├── .vscode/                                         # Configuration VS Code
├── __pycache__/                                     # Cache Python
├── bake/                                            # Outil de build NS-3
│   ├── bake.py                                      # Script principal bake
│   ├── bakeconf.xml                                 # Configuration bake
│   ├── generate-binary.py                           # Génération binaire
│   ├── TODO                                         # Tâches à faire
│   ├── bake/                                        # Sous-modules bake
│   ├── doc/                                         # Documentation bake
│   ├── examples/                                    # Exemples bake
│   └── test/                                        # Tests bake
├── build.py                                         # Script de build NS-3
├── constants.py                                     # Constantes Python
├── netanim-3.109/                                   # NetAnim (animateur réseau)
│   ├── (plus de 100 fichiers .cpp/.h/.o)          # Interface graphique Qt
│   ├── main.cpp                                     # Point d'entrée NetAnim
│   ├── Makefile                                     # Makefile NetAnim
│   └── (fichiers ressources .png/.svg)             # Icônes et ressources
├── util.py                                          # Utilitaires Python
├── 
├── ══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
├── 📚 DOCUMENTATION COMPLÈTE
├── ══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
├── README.md                                        # 📖 Documentation principale (ce fichier)
├── EXECUTION_GUIDE.md                               # 🚀 Guide d'exécution rapide
├── QUICKSTART.md                                    # ⚡ Guide de démarrage rapide
├── SYNTHESIS.md                                     # 📊 Synthèse complète du projet
├── 
├── ══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
├── ⚙️ CONFIGURATION ET DÉPENDANCES
├── ══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
├── requirements.txt                                 # 📦 Dépendances Python (pandas, matplotlib, etc.)
├── config.ini                                       # 🔧 Configuration des simulations
├── 
├── ══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
├── 🤖 SCRIPTS D'AUTOMATISATION
├── ══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
├── run_simulation.sh                                # 🚀 Script d'automatisation principal
├── check_environment.py                             # ✅ Vérification de l'environnement
├── test_visualization.py                            # 🧪 Test des scripts de visualisation
├── 
├── ══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
├── 🎯 DOSSIER NS-3 PRINCIPAL
├── ══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
├── ns-3.42/
│   ├── .clang-format                                # Format de code C++
│   ├── .clang-tidy                                  # Linting C++
│   ├── .editorconfig                                # Configuration éditeur
│   ├── .lock-ns3_linux_build                        # Verrou de build Linux
│   ├── .vscode/                                     # Configuration VS Code NS-3
│   ├── AUTHORS                                      # Auteurs NS-3
│   ├── CHANGES.md                                   # Changements NS-3
│   ├── CMakeLists.txt                               # Configuration CMake
│   ├── CONTRIBUTING.md                              # Guide de contribution
│   ├── LICENSE                                      # Licence NS-3
│   ├── README.md                                    # README NS-3
│   ├── README_SIMULATION_LORAWAN.md                 # README spécifique LoRaWAN
│   ├── RELEASE_NOTES.md                             # Notes de version
│   ├── VERSION                                      # Version NS-3
│   ├── 
│   ├── ═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
│   ├── 📊 SCRIPTS DE VISUALISATION GLOBAUX
│   ├── ═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
│   ├── plot_all_lorawan.py                          # 📈 Script de visualisation global
│   ├── plot_lorawan_mobile_interf.py                # 📈 Visualisation mobile (legacy)
│   ├── test.py                                      # 🧪 Script de test
│   ├── utils.py                                     # 🔧 Utilitaires NS-3
│   ├── visualize.py                                 # 📊 Visualisation NS-3
│   ├── visualize_simple.py                          # 📊 Visualisation simple
│   ├── 
│   ├── ═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
│   ├── 🏗️ CONFIGURATION BUILD
│   ├── ═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
│   ├── build/                                       # 📁 Dossier de compilation (généré)
│   │   ├── examples/                                # Exemples compilés
│   │   ├── include/                                 # Headers compilés
│   │   ├── lib/                                     # Librairies compilées
│   │   ├── scratch/                                 # Simulations scratch compilées
│   │   ├── src/                                     # Sources compilées
│   │   └── utils/                                   # Utilitaires compilés
│   ├── build-support/                               # Support de build
│   ├── cmake-cache/                                 # Cache CMake
│   ├── ns3                                          # 🚀 Script de build NS-3
│   ├── pyproject.toml                               # Configuration Python
│   ├── setup.cfg                                    # Configuration setup
│   ├── setup.py                                     # Script setup Python
│   ├── 
│   ├── ═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
│   ├── 🎯 DOSSIER SIMULATIONS LoRaWAN
│   ├── ═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
│   ├── scratch/
│   │   ├── CMakeLists.txt                           # Configuration CMake scratch
│   │   ├── 
│   │   ├── ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
│   │   ├── 🚀 SIMULATIONS LORAWAN PRINCIPALES (6 simulations)
│   │   ├── ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
│   │   ├── lorawan-logistics-mab-static.cc          # 📶 Simulation statique
│   │   ├── lorawan-logistics-mab-static-interf.cc   # 📶 Simulation statique avec interférences
│   │   ├── lorawan-logistics-mab-mobile.cc          # 🚚 Simulation mobile
│   │   ├── lorawan-logistics-mab-mobile-interf.cc   # 🚚 Simulation mobile avec interférences
│   │   ├── lorawan-logistics-mab-mixed.cc           # 🔄 Simulation mixte
│   │   ├── lorawan-logistics-mab-mixed-interf.cc    # 🔄 Simulation mixte avec interférences
│   │   ├── 
│   │   ├── ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
│   │   ├── 🏞️ SIMULATIONS LORAWAN ADDITIONNELLES (environnements)
│   │   ├── ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
│   │   ├── lorawan-tow-mab-rural.cc                 # 🏞️ Simulation rurale
│   │   ├── lorawan-tow-mab-test.cc                  # 🧪 Simulation de test
│   │   ├── lorawan-tow-mab-urban.cc                 # 🏙️ Simulation urbaine
│   │   ├── 
│   │   ├── ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
│   │   ├── 📊 SCRIPTS DE VISUALISATION SPÉCIALISÉS (3 scripts)
│   │   ├── ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
│   │   ├── plot_lorawan_static.py                   # 📈 Visualisation simulations statiques
│   │   ├── plot_lorawan_mobile.py                   # 📈 Visualisation simulations mobiles
│   │   ├── plot_lorawan_mixed.py                    # 📈 Visualisation simulations mixtes
│   │   ├── 
│   │   ├── ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
│   │   ├── 📚 DOCUMENTATION SCRATCH
│   │   ├── ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
│   │   ├── README-rural.md                          # 📖 Documentation simulation rurale
│   │   ├── README-urban.md                          # 📖 Documentation simulation urbaine
│   │   ├── README_plots.md                          # 📖 Documentation des graphiques
│   │   ├── 
│   │   ├── ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
│   │   ├── 🔧 AUTRES FICHIERS
│   │   ├── ──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
│   │   ├── scratch-simulator.cc                     # 🎯 Simulateur scratch
│   │   ├── nested-subdir/                           # 📁 Sous-dossier imbriqué
│   │   └── subdir/                                  # 📁 Sous-dossier
│   │   
│   ├── ═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
│   ├── 📊 DOSSIERS DE RÉSULTATS (générés automatiquement)
│   ├── ═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
│   ├── lorawan_static_results/                      # 📊 Résultats simulation statique
│   ├── lorawan_static_results_interf/               # 📊 Résultats simulation statique avec interférences
│   ├── lorawan_mobile_results/                      # 📊 Résultats simulation mobile
│   ├── lorawan_mobile_results_interf/               # 📊 Résultats simulation mobile avec interférences
│   ├── lorawan_mixed_results/                       # 📊 Résultats simulation mixte
│   ├── lorawan_mixed_results_interf/                # 📊 Résultats simulation mixte avec interférences
│   ├── 
│   ├── ═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
│   ├── 📈 FICHIERS DE PERFORMANCE (générés automatiquement)
│   ├── ═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
│   ├── buildings.txt                                # 🏢 Données de bâtiments
│   ├── durations.txt                                # ⏱️ Durées de simulation
│   ├── globalPerformance.txt                        # 📊 Performance globale
│   ├── nodeData.txt                                 # 📡 Données des nœuds
│   ├── phyPerformance.txt                           # 📡 Performance PHY
│   ├── rural_simulation.log                         # 📝 Log simulation rurale
│   ├── urban_simulation.log                         # 📝 Log simulation urbaine
│   ├── plot.gp                                      # 📊 Script gnuplot
│   ├── 
│   ├── ═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
│   ├── 🔧 DOSSIERS NS-3 CORE
│   ├── ═══════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════════
│   ├── bindings/                                    # 🐍 Bindings Python
│   ├── contrib/                                     # 🤝 Modules contribués
│   ├── doc/                                         # 📚 Documentation
│   ├── examples/                                    # 💡 Exemples NS-3
│   │   ├── channel-models/                          # Modèles de canal
│   │   ├── energy/                                  # Gestion d'énergie
│   │   ├── error-model/                             # Modèles d'erreur
│   │   ├── ipv6/                                    # IPv6
│   │   ├── matrix-topology/                         # Topologie matricielle
│   │   ├── naming/                                  # Nommage
│   │   ├── realtime/                                # Temps réel
│   │   ├── routing/                                 # Routage
│   │   ├── socket/                                  # Sockets
│   │   ├── stats/                                   # Statistiques
│   │   ├── tcp/                                     # TCP
│   │   ├── traffic-control/                         # Contrôle du trafic
│   │   ├── tutorial/                                # Tutoriels
│   │   ├── udp/                                     # UDP
│   │   ├── udp-client-server/                       # Client-serveur UDP
│   │   └── wireless/                                # Sans fil
│   ├── src/                                         # 💻 Code source NS-3
│   │   ├── antenna/                                 # Antennes
│   │   ├── aodv/                                    # AODV
│   │   ├── applications/                            # Applications
│   │   ├── bridge/                                  # Pont
│   │   ├── brite/                                   # BRITE
│   │   ├── buildings/                               # Bâtiments
│   │   ├── click/                                   # Click
│   │   ├── config-store/                            # Configuration
│   │   ├── core/                                    # Noyau
│   │   ├── csma/                                    # CSMA
│   │   ├── csma-layout/                             # Layout CSMA
│   │   ├── dsdv/                                    # DSDV
│   │   ├── dsr/                                     # DSR
│   │   ├── energy/                                  # Énergie
│   │   ├── fd-net-device/                           # Périphériques réseau
│   │   ├── flow-monitor/                            # Moniteur de flux
│   │   ├── internet/                                # Internet
│   │   ├── internet-apps/                           # Applications Internet
│   │   ├── lorawan/                                 # 📡 LoRaWAN (module principal)
│   │   ├── lr-wpan/                                 # LR-WPAN
│   │   ├── lte/                                     # LTE
│   │   ├── mesh/                                    # Maillage
│   │   ├── mobility/                                # Mobilité
│   │   ├── mpi/                                     # MPI
│   │   ├── netanim/                                 # NetAnim
│   │   ├── network/                                 # Réseau
│   │   ├── nix-vector-routing/                      # Routage NIX
│   │   ├── olsr/                                    # OLSR
│   │   ├── openflow/                                # OpenFlow
│   │   ├── point-to-point/                          # Point-à-point
│   │   ├── point-to-point-layout/                   # Layout point-à-point
│   │   ├── propagation/                             # Propagation
│   │   ├── sixlowpan/                               # 6LoWPAN
│   │   ├── spectrum/                                # Spectre
│   │   ├── stats/                                   # Statistiques
│   │   ├── tap-bridge/                              # Pont TAP
│   │   ├── test/                                    # Tests
│   │   ├── topology-read/                           # Lecture topologie
│   │   ├── traffic-control/                         # Contrôle trafic
│   │   ├── uan/                                     # UAN
│   │   ├── virtual-net-device/                      # Périphériques virtuels
│   │   ├── visualizer/                              # Visualiseur
│   │   ├── wifi/                                    # WiFi
│   │   └── wimax/                                   # WiMAX
│   ├── test/                                        # 🧪 Tests NS-3
│   └── utils/                                       # 🔧 Utilitaires NS-3
│   
└── scripts/                                         # 📁 Scripts d'analyse (vide pour l'instant)
```

## 🎯 Simulations Disponibles

Le projet comprend 6 simulations différentes couvrant tous les scénarios logistiques :

### 1. **Simulations Statiques**
- **`lorawan-logistics-mab-static.cc`** : Dispositifs statiques sans interférences
- **`lorawan-logistics-mab-static-interf.cc`** : Dispositifs statiques avec interférences

### 2. **Simulations Mobiles**
- **`lorawan-logistics-mab-mobile.cc`** : Dispositifs mobiles sans interférences
- **`lorawan-logistics-mab-mobile-interf.cc`** : Dispositifs mobiles avec interférences

### 3. **Simulations Mixtes**
- **`lorawan-logistics-mab-mixed.cc`** : Dispositifs mixtes (statique + mobile) sans interférences
- **`lorawan-logistics-mab-mixed-interf.cc`** : Dispositifs mixtes avec interférences

### 🔧 Paramètres d'interférence modélisés
- **Bâtiments** : 0-10 dB de perte
- **Vent** : 0-2 dB de perte
- **Arbres** : 0-5 dB de perte
- **Pluie** : 0-6 dB de perte
- **Réseaux voisins** : 0-8 dB de perte

## 🚀 Exécution des Simulations

### Processus complet : Compilation → Simulation → Génération CSV → Visualisation

**Important :** Les simulations NS-3 génèrent automatiquement des fichiers CSV contenant les données de performance. Ces fichiers CSV sont ensuite utilisés par les scripts de visualisation pour créer les graphiques.

### Liste complète des fichiers de simulation

Le projet comprend **6 simulations différentes** couvrant tous les scénarios logistiques :

| Fichier | Description | Dossier de résultats |
|---------|-------------|---------------------|
| `lorawan-logistics-mab-static.cc` | Dispositifs statiques (position fixe) | `lorawan_static_results/` |
| `lorawan-logistics-mab-static-interf.cc` | Dispositifs statiques avec interférences | `lorawan_static_results_interf/` |
| `lorawan-logistics-mab-mobile.cc` | Dispositifs mobiles (RandomWaypoint) | `lorawan_mobile_results/` |
| `lorawan-logistics-mab-mobile-interf.cc` | Dispositifs mobiles avec interférences | `lorawan_mobile_results_interf/` |
| `lorawan-logistics-mab-mixed.cc` | Dispositifs mixtes (50% statiques, 50% mobiles) | `lorawan_mixed_results/` |
| `lorawan-logistics-mab-mixed-interf.cc` | Dispositifs mixtes avec interférences | `lorawan_mixed_results_interf/` |

### Scripts de visualisation disponibles

| Script | Description | Usage |
|--------|-------------|-------|
| `plot_lorawan_static.py` | Visualisation simulations statiques | `python3 plot_lorawan_static.py [dossier_résultats]` |
| `plot_lorawan_mobile.py` | Visualisation simulations mobiles | `python3 plot_lorawan_mobile.py [dossier_résultats]` |
| `plot_lorawan_mixed.py` | Visualisation simulations mixtes | `python3 plot_lorawan_mixed.py [dossier_résultats]` |

## 🚀 Exécution Complète

### Script automatisé (recommandé)

```bash
# Rendre le script exécutable
chmod +x run_simulation.sh

# Exécuter tout automatiquement (compile + simule + génère CSV + visualise)
./run_simulation.sh all

# Ou étape par étape
./run_simulation.sh compile    # Compilation uniquement
./run_simulation.sh run-all    # Toutes les simulations (génère les CSV)
./run_simulation.sh plot-all   # Tous les graphiques (à partir des CSV)
./run_simulation.sh summary    # Résumé des résultats
```

### Exécution manuelle étape par étape

#### Étape 1: Compilation
```bash
cd ns-3.42
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

#### Étape 2: Exécution des simulations (génération des fichiers CSV)

**Simulations statiques:**
```bash
# Sans interférences - génère lorawan_static_results/*.csv
./ns3 run lorawan-logistics-mab-static

# Avec interférences - génère lorawan_static_results_interf/*.csv
./ns3 run lorawan-logistics-mab-static-interf
```

**Simulations mobiles:**
```bash
# Sans interférences - génère lorawan_mobile_results/*.csv
./ns3 run lorawan-logistics-mab-mobile

# Avec interférences - génère lorawan_mobile_results_interf/*.csv
./ns3 run lorawan-logistics-mab-mobile-interf
```

**Simulations mixtes:**
```bash
# Sans interférences - génère lorawan_mixed_results/*.csv
./ns3 run lorawan-logistics-mab-mixed

# Avec interférences - génère lorawan_mixed_results_interf/*.csv
./ns3 run lorawan-logistics-mab-mixed-interf
```

#### Étape 3: Génération des graphiques (à partir des fichiers CSV)

```bash
# Sortir du dossier ns-3.42
cd ..

# Graphiques pour simulations statiques (utilise les CSV générés)
python3 ns-3.42/scratch/plot_lorawan_static.py lorawan_static_results/
python3 ns-3.42/scratch/plot_lorawan_static.py lorawan_static_results_interf/

# Graphiques pour simulations mobiles (utilise les CSV générés)
python3 ns-3.42/scratch/plot_lorawan_mobile.py lorawan_mobile_results/
python3 ns-3.42/scratch/plot_lorawan_mobile.py lorawan_mobile_results_interf/

# Graphiques pour simulations mixtes (utilise les CSV générés)
python3 ns-3.42/scratch/plot_lorawan_mixed.py lorawan_mixed_results/
python3 ns-3.42/scratch/plot_lorawan_mixed.py lorawan_mixed_results_interf/
```

### Exécution avec paramètres personnalisés

```bash
# Exemples avec arguments (si supportés par la simulation)
./ns3 run "lorawan-logistics-mab-mixed-interf --nDevices=500 --simTime=600"
./ns3 run "lorawan-logistics-mab-static --areaRadius=2000"
```

### Exécution sélective

```bash
# Compiler uniquement
./run_simulation.sh compile

# Exécuter une simulation spécifique
./run_simulation.sh run lorawan-logistics-mab-mixed-interf

# Générer les graphiques pour une simulation
./run_simulation.sh plot lorawan-logistics-mab-mixed-interf

# Afficher le résumé des résultats
./run_simulation.sh summary
```

## � Paramètres de simulation

#### **Paramètres LoRa testés (toutes simulations)**
- **Spreading Factor (SF)** : 7, 8, 9, 10, 11 (ou 12 selon la simulation)
- **Puissance de transmission** : 2 dBm, 8 dBm
- **Taille du payload** : 50, 100, 150, 200, 250 octets
- **Bande passante** : 125 kHz, 250 kHz
- **Coding Rate** : 4/5 (CR=1)

#### **Configuration par défaut**
- **Nombre de dispositifs** : 1000 (configurable)
- **Nombre de messages** : 20 par dispositif
- **Intervalle entre messages** : 15 secondes
- **Rayon de couverture** : 1000-5000 mètres (selon simulation)
- **Hauteur gateway** : 20 mètres

#### **Configurations spécifiques par type**

**Simulation statique :**
- Dispositifs à position fixe aléatoire
- Modèle de mobilité : `ConstantPositionMobilityModel`

**Simulation mobile :**
- Dispositifs en mouvement continu
- Modèle de mobilité : `RandomWaypointMobilityModel`

**Simulation mixte :**
- 50% de dispositifs mobiles par défaut
- 50% de dispositifs statiques
- Ratio configurable dans le code

#### **Modèle d'interférences (versions avec "interf")**
- **Bâtiments** : 0-10 dB de perte supplémentaire
- **Vent** : 0-2 dB de perte supplémentaire
- **Arbres** : 0-5 dB de perte supplémentaire
- **Pluie** : 0-6 dB de perte supplémentaire
- **Réseaux voisins** : 0-8 dB de perte supplémentaire

## � Fichiers de Résultats Générés

### Structure des dossiers de résultats
```
# Simulations sans interférences
lorawan_static_results/
├── lorawan-static_ALL.csv
└── lorawan-static_ALL_plots/

lorawan_mobile_results/
├── lorawan-logistics-mab-mobile_dynamic_ALL.csv
└── lorawan-logistics-mab-mobile_dynamic_ALL_plots/

lorawan_mixed_results/
├── lorawan-logistics-mab-mixed_ALL.csv
└── lorawan-logistics-mab-mixed_ALL_plots/

# Simulations avec interférences
lorawan_static_results_interf/
├── lorawan-static-interf_ALL.csv
└── lorawan-static-interf_ALL_plots/

lorawan_mobile_results_interf/
├── lorawan-logistics-mab-mobile_interf_ALL.csv
└── lorawan-logistics-mab-mobile_interf_ALL_plots/

lorawan_mixed_results_interf/
├── lorawan-logistics-mab-mixed_ALL.csv
└── lorawan-logistics-mab-mixed_ALL_plots/
```

### Format des fichiers CSV
Chaque simulation génère un fichier CSV avec les colonnes suivantes :
- `deviceId` : ID du dispositif
- `messageId` : ID du message
- `time` : Timestamp
- `x`, `y`, `z` : Position du dispositif
- `distance` : Distance à la gateway
- `txPower` : Puissance de transmission (dBm)
- `sf` : Spreading Factor
- `bw` : Bande passante (Hz)
- `payload` : Taille du payload (octets)
- `rssi` : RSSI mesuré (dBm)
- `snr` : SNR mesuré (dB)
- `success` : Succès de transmission (0/1)
- `energyConsumed` : Énergie consommée (mWh)
- `timeOnAir` : Temps d'émission (ms)
- `interferenceLoss` : Perte d'interférence (dB) - si applicable

## �📊 Analyse des résultats

### 1. Installation des dépendances

```bash
# Vérifier l'environnement
python3 check_environment.py

# Installer les dépendances Python
pip install -r requirements.txt
```

### 2. Scripts de visualisation disponibles

Le projet contient plusieurs scripts Python pour analyser et visualiser les résultats :

#### **Scripts de visualisation principaux**

1. **`plot_lorawan_mixed_interf.py`** - Analyse mixte avec interférences
   ```bash
   # Utilisation avec dossier par défaut
   python plot_lorawan_mixed_interf.py
   
   # Utilisation avec dossier personnalisé
   python plot_lorawan_mixed_interf.py lorawan_mixed_results_interf/
   ```

2. **`plot_lorawan_mobile_interf.py`** - Analyse mobile avec interférences
   ```bash
   # Utilisation avec dossier par défaut
   python plot_lorawan_mobile_interf.py
   
   # Utilisation avec dossier personnalisé
   python plot_lorawan_mobile_interf.py lorawan_mobile_results_interf/
   ```

#### **Scripts pour tous les types de simulation**

Pour les autres simulations, vous pouvez utiliser les scripts existants en spécifiant le bon dossier :

```bash
# Simulation statique avec interférences
python plot_lorawan_mixed_interf.py lorawan_static_results_interf/

# Simulation mobile sans interférences
python plot_lorawan_mobile_interf.py lorawan_mobile_results/

# Simulation mixte sans interférences
python plot_lorawan_mixed_interf.py lorawan_mixed_results/

# Simulation statique sans interférences
python plot_lorawan_mixed_interf.py lorawan_static_results/
```

#### **Script de test**
```bash
# Tester les scripts avec des données simulées
python3 test_visualization.py
```

### 3. Graphiques générés

Les scripts génèrent automatiquement de nombreux graphiques dans des sous-dossiers `*_plots/` :

#### **Graphiques de taux de succès :**
- `success_rate_sf_txPower.png` - Taux de succès par SF et puissance
- `success_rate_sf_payload.png` - Taux de succès par SF et payload
- `success_rate_sf_bw.png` - Taux de succès par SF et bande passante
- `heatmap_success_sf_txPower.png` - Heatmap des taux de succès

#### **Graphiques de métriques par SF :**
- `rssi_vs_sf.png` - RSSI moyen par SF
- `snr_vs_sf.png` - SNR moyen par SF
- `energy_vs_sf.png` - Énergie consommée par SF
- `toa_vs_sf.png` - Time on Air par SF

#### **Graphiques temporels :**
- `snr_vs_time.png` - SNR global en fonction du temps
- `rssi_vs_time.png` - RSSI global en fonction du temps
- `snr_vs_time_sf.png` - SNR par SF en fonction du temps
- `rssi_vs_time_sf.png` - RSSI par SF en fonction du temps
- `rssi_vs_time_txPower.png` - RSSI par puissance en fonction du temps
- `toa_vs_time_sf.png` - Time on Air par SF en fonction du temps
- `energie_vs_time_sf.png` - Énergie consommée par SF en fonction du temps
- `efficacite_vs_time_sf.png` - Efficacité énergétique par SF en fonction du temps

#### **Graphiques par nombre de dispositifs :**
- `pdr_vs_nDevices.png` - PDR global en fonction du nombre de dispositifs
- `pdr_vs_nDevices_sf.png` - PDR par SF en fonction du nombre de dispositifs
- `rssi_vs_nDevices.png` - RSSI moyen par nombre de dispositifs
- `snr_vs_nDevices.png` - SNR moyen par nombre de dispositifs
- `energy_vs_nDevices.png` - Énergie moyenne par nombre de dispositifs
- `efficacite_vs_nDevices.png` - Efficacité énergétique par nombre de dispositifs
- `toa_vs_nDevices.png` - Time on Air par nombre de dispositifs

#### **Graphiques par message :**
- `snr_vs_message_sf_payload.png` - SNR par message (SF × payload)
- `rssi_vs_message_sf_payload.png` - RSSI par message (SF × payload)
- `snr_vs_message_sf_txPower.png` - SNR par message (SF × puissance)
- `rssi_vs_message_sf_txPower.png` - RSSI par message (SF × puissance)
- `snr_vs_message_txPower_bw.png` - SNR par message (puissance × bande)
- `rssi_vs_message_txPower_bw.png` - RSSI par message (puissance × bande)

#### **Simulation spécifique**
```bash
# Exemple : Simulation mixte avec interférences
cd ns-3.42
./ns3 run lorawan-logistics-mab-mixed-interf  # Génère le CSV
cd ..
python3 ns-3.42/scratch/plot_lorawan_mixed.py lorawan_mixed_results_interf/  # Utilise le CSV
```

## 🔧 Analyse personnalisée (optionnel)

Pour créer votre propre script d'analyse, créez le fichier `scripts/analyze_results.py` :

```python
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import os

def load_and_analyze_data(csv_file):
    """Charge et analyse les données de simulation"""
    print(f"Chargement des données depuis {csv_file}")
    df = pd.read_csv(csv_file)
    
    # Conversion de la colonne time en datetime
    df['time'] = pd.to_datetime(df['time'])
    
    print(f"Nombre total d'enregistrements: {len(df)}")
    print(f"Période de simulation: {df['time'].min()} à {df['time'].max()}")
    
    return df

def create_plots(df, output_dir):
    """Crée les graphiques d'analyse"""
    os.makedirs(output_dir, exist_ok=True)
    
    # Style matplotlib
    plt.style.use('seaborn-v0_8')
    plt.rcParams['figure.figsize'] = (12, 8)
    
    # 1. Taux de succès par SF et TxPower
    plt.figure(figsize=(12, 8))
    success_rate = df.groupby(['sf', 'txPower'])['success'].mean().reset_index()
    pivot_success = success_rate.pivot(index='sf', columns='txPower', values='success')
    sns.heatmap(pivot_success, annot=True, fmt='.2%', cmap='RdYlGn', 
                cbar_kws={'label': 'Taux de succès'})
    plt.title('Taux de succès par Spreading Factor et Puissance de transmission')
    plt.xlabel('Puissance de transmission (dBm)')
    plt.ylabel('Spreading Factor')
    plt.tight_layout()
    plt.savefig(f'{output_dir}/success_rate_sf_txPower.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # 2. RSSI vs Distance par SF
    plt.figure(figsize=(12, 8))
    for sf in sorted(df['sf'].unique()):
        sf_data = df[df['sf'] == sf]
        plt.scatter(sf_data['distance'], sf_data['rssi'], alpha=0.5, 
                   label=f'SF{sf}', s=1)
    plt.xlabel('Distance (m)')
    plt.ylabel('RSSI (dBm)')
    plt.title('RSSI vs Distance par Spreading Factor')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(f'{output_dir}/rssi_vs_distance_sf.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # 3. Consommation énergétique par SF
    plt.figure(figsize=(12, 8))
    energy_stats = df.groupby('sf')['energyConsumed'].agg(['mean', 'std']).reset_index()
    plt.bar(energy_stats['sf'], energy_stats['mean'], 
            yerr=energy_stats['std'], capsize=5, alpha=0.7)
    plt.xlabel('Spreading Factor')
    plt.ylabel('Consommation énergétique moyenne (J)')
    plt.title('Consommation énergétique par Spreading Factor')
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(f'{output_dir}/energy_consumption_sf.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # 4. Impact des interférences
    plt.figure(figsize=(12, 8))
    interference_stats = df.groupby('sf')['interferenceLoss'].agg(['mean', 'std']).reset_index()
    plt.bar(interference_stats['sf'], interference_stats['mean'], 
            yerr=interference_stats['std'], capsize=5, alpha=0.7, color='red')
    plt.xlabel('Spreading Factor')
    plt.ylabel('Perte d\'interférence moyenne (dB)')
    plt.title('Impact des interférences par Spreading Factor')
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(f'{output_dir}/interference_impact_sf.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # 5. Time on Air par payload et SF
    plt.figure(figsize=(12, 8))
    toa_stats = df.groupby(['payload', 'sf'])['timeOnAir'].mean().reset_index()
    pivot_toa = toa_stats.pivot(index='payload', columns='sf', values='timeOnAir')
    sns.heatmap(pivot_toa, annot=True, fmt='.1f', cmap='YlOrRd', 
                cbar_kws={'label': 'Time on Air (ms)'})
    plt.title('Time on Air par Payload et Spreading Factor')
    plt.xlabel('Spreading Factor')
    plt.ylabel('Payload (octets)')
    plt.tight_layout()
    plt.savefig(f'{output_dir}/toa_payload_sf.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # 6. Évolution temporelle du taux de succès
    plt.figure(figsize=(12, 8))
    df['hour'] = df['time'].dt.hour
    temporal_success = df.groupby(['hour', 'sf'])['success'].mean().reset_index()
    for sf in sorted(df['sf'].unique()):
        sf_data = temporal_success[temporal_success['sf'] == sf]
        plt.plot(sf_data['hour'], sf_data['success'], 
                marker='o', label=f'SF{sf}', linewidth=2)
    plt.xlabel('Heure')
    plt.ylabel('Taux de succès')
    plt.title('Évolution temporelle du taux de succès')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(f'{output_dir}/temporal_success_rate.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Graphiques sauvegardés dans {output_dir}")

def generate_report(df, output_dir):
    """Génère un rapport statistique"""
    report_file = f'{output_dir}/simulation_report.txt'
    
    with open(report_file, 'w') as f:
        f.write("=== RAPPORT DE SIMULATION LORAWAN ===\n\n")
        
        f.write(f"Période de simulation: {df['time'].min()} à {df['time'].max()}\n")
        f.write(f"Nombre total de messages: {len(df)}\n")
        f.write(f"Nombre de dispositifs: {df['deviceId'].nunique()}\n\n")
        
        f.write("=== STATISTIQUES GÉNÉRALES ===\n")
        f.write(f"Taux de succès global: {df['success'].mean():.2%}\n")
        f.write(f"RSSI moyen: {df['rssi'].mean():.2f} dBm\n")
        f.write(f"SNR moyen: {df['snr'].mean():.2f} dB\n")
        f.write(f"Consommation énergétique moyenne: {df['energyConsumed'].mean():.6f} J\n")
        f.write(f"Perte d'interférence moyenne: {df['interferenceLoss'].mean():.2f} dB\n\n")
        
        f.write("=== ANALYSE PAR SPREADING FACTOR ===\n")
        sf_stats = df.groupby('sf').agg({
            'success': 'mean',
            'rssi': 'mean',
            'snr': 'mean',
            'energyConsumed': 'mean',
            'interferenceLoss': 'mean'
        }).round(3)
        f.write(sf_stats.to_string())
        f.write("\n\n")
        
        f.write("=== ANALYSE PAR PUISSANCE DE TRANSMISSION ===\n")
        power_stats = df.groupby('txPower').agg({
            'success': 'mean',
            'rssi': 'mean',
            'snr': 'mean',
            'energyConsumed': 'mean'
        }).round(3)
        f.write(power_stats.to_string())
        
    print(f"Rapport sauvegardé dans {report_file}")

def main():
    # Chemin vers le fichier CSV
    csv_file = 'lorawan_mixed_results_interf/lorawan-logistics-mab-mixed_ALL.csv'
    output_dir = 'lorawan_mixed_results_interf/analysis_plots'
    
    if not os.path.exists(csv_file):
        print(f"Erreur: Le fichier {csv_file} n'existe pas.")
        print("Veuillez d'abord exécuter la simulation.")
        return
    
    # Chargement et analyse des données
    df = load_and_analyze_data(csv_file)
    
    # Création des graphiques
    create_plots(df, output_dir)
    
    # Génération du rapport
    generate_report(df, output_dir)
    
    print("Analyse terminée avec succès!")

if __name__ == "__main__":
    main()
```

### 6. Exécution de l'analyse personnalisée

```bash
# Créer le dossier scripts si nécessaire
mkdir -p scripts

# Copier le script d'analyse dans scripts/analyze_results.py
# Puis exécuter l'analyse
python scripts/analyze_results.py
```

## 📈 Résultats générés

### **Scripts automatisés (plot_lorawan_*.py)**
- Graphiques de performance par paramètre LoRa (SF, puissance, payload, bande passante)
- Évolution temporelle des métriques (RSSI, SNR, énergie, efficacité)
- Analyse par nombre de dispositifs et par message
- Heatmaps de taux de succès
- Graphiques d'efficacité énergétique

### **Script personnalisé (analyze_results.py)**
1. **Taux de succès** par SF et puissance de transmission
2. **RSSI vs Distance** par Spreading Factor
3. **Consommation énergétique** par SF
4. **Impact des interférences** par SF
5. **Time on Air** par payload et SF
6. **Évolution temporelle** du taux de succès
2. **RSSI vs Distance** par Spreading Factor
3. **Consommation énergétique** par SF
4. **Impact des interférences** par SF
5. **Time on Air** par payload et SF
6. **Évolution temporelle** du taux de succès

## 🔧 Personnalisation

### Modification des paramètres de simulation

Éditez les fichiers `.cc` pour modifier :

```cpp
// Nombre de dispositifs
uint32_t nDevices = 1000;

// Ratio de dispositifs mobiles (simulations mixtes)
double mobileRatio = 0.5;

// Intervalle entre paquets
double packetIntervalSeconds = 15.0;

// Nombre de messages par dispositif
uint32_t nMessages = 20;

// Paramètres LoRa testés
std::vector<int> sfList = {7,8,9,10,11};
std::vector<int> txPowerList = {2,8};
std::vector<int> payloadList = {50,100,150,200,250};
std::vector<int> bwList = {125000, 250000};
```

### Ajout d'analyses personnalisées

Modifiez les scripts Python pour ajouter vos propres métriques et visualisations.

## 🐛 Dépannage

### Erreurs de compilation
```bash
# Nettoyer et recompiler
./ns3 clean
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

### Problèmes spécifiques LoRaWAN
```bash
# Vérifier que le module LoRaWAN est bien compilé
./ns3 show modules | grep lorawan

# Si le module LoRaWAN n'est pas trouvé, recompiler avec :
./ns3 configure --enable-modules=lorawan --enable-examples --enable-tests
./ns3 build

# Vérifier les dépendances LoRaWAN
ls src/lorawan/

# Tester une simulation simple pour vérifier LoRaWAN
./ns3 run "lorawan-logistics-mab-static --help"
```

### Erreurs de modules manquants
```bash
# Si des modules NS-3 sont manquants, recompiler tout :
./ns3 clean
./ns3 configure --enable-examples --enable-tests --enable-logs
./ns3 build --verbose

# Vérifier tous les modules disponibles
./ns3 show modules
```

### Problèmes avec les résultats
```bash
# Vérifier que le dossier de résultats existe
ls -la lorawan_mixed_results_interf/

# Vérifier le contenu du fichier CSV
head -n 5 lorawan_mixed_results_interf/lorawan-logistics-mab-mixed_ALL.csv
```

### Erreurs Python
```bash
# Installer les dépendances manquantes
pip install -r requirements.txt

# Ou installer manuellement
pip install pandas matplotlib seaborn numpy argparse
```

### Problèmes avec les scripts de visualisation
```bash
# Vérifier l'environnement complet
python3 check_environment.py

# Vérifier la structure des fichiers CSV
head -n 5 lorawan_mixed_results_interf/lorawan-logistics-mab-mixed_ALL.csv

# Exécuter avec dossier spécifique
python plot_lorawan_mixed_interf.py lorawan_mixed_results_interf/

# Vérifier les graphiques générés
ls -la lorawan_mixed_results_interf/lorawan-logistics-mab-mixed_ALL_plots/
```

### Vérification complète de l'environnement
```bash
# Script de diagnostic complet
python3 check_environment.py

# Installation automatique des dépendances
./run_simulation.sh all
```

## 📚 Documentation

- [NS-3 Documentation](https://www.nsnam.org/documentation/)
- [LoRaWAN Module](https://github.com/signetlabdei/lorawan)
- [Matplotlib Documentation](https://matplotlib.org/stable/users/index.html)

## 🤝 Contribution

Pour contribuer au projet :
1. Fork le repository
2. Créer une branche pour votre feature
3. Commit vos changements
4. Ouvrir une Pull Request

## 📄 License

Ce projet est sous licence MIT. Voir le fichier LICENSE pour plus de détails.

---

## 🔍 Analyse Technique Détaillée

### Paramètres de Simulation Vérifiés

Les fichiers de simulation utilisent les paramètres suivants (vérifiés dans le code source) :

- **Spreading Factor (SF)**: 7, 8, 9, 10, 11 (définis dans `sfList = {7,8,9,10,11}`)
- **Puissance de transmission**: 2 dBm, 8 dBm (définis dans `txPowerList = {2,8}`)
- **Bande passante**: 125 kHz, 250 kHz (définis dans `bwList = {125000, 250000}`)
- **Coding Rate**: 1 (fixe)
- **Payload**: 30 octets (par défaut)
- **Nombre total de combinaisons**: 5 × 2 × 2 = 20 combinaisons par simulation

### Algorithme Multi-Armed Bandit

Le code implémente un algorithme epsilon-greedy avec deux phases :

1. **Phase d'exploration** : Chaque combinaison de paramètres est testée au moins une fois
2. **Phase d'exploitation** : Sélection des meilleures combinaisons basée sur le score de réussite

### Génération des Résultats CSV

Chaque transmission génère une ligne dans le fichier CSV avec :
- Position du dispositif (x, y, z)
- Paramètres utilisés (SF, puissance, bande passante)
- Métriques de performance (RSSI, SNR, succès)
- Consommation énergétique et temps d'émission
- Pertes dues aux interférences

### Métriques Calculées

- **Time on Air** : Calculé selon la formule LoRaWAN standard
- **Consommation énergétique** : Basée sur la puissance de transmission et le temps d'émission
- **Score de réussite** : Utilisé par l'algorithme MAB pour optimiser les paramètres

---

## Installation NS-3 (Information originale)

This is **_ns-3-allinone_**, a repository with some scripts to download
and build the core components around the 
[ns-3 network simulator](https://www.nsnam.org).
More information about this can be found in the
[ns-3 tutorial](https://www.nsnam.org/documentation/).

If you have downloaded this in tarball release format, this directory
contains some released ns-3 version, along with the repository for
the [NetAnim network animator](https://gitlab.com/nsnam/netanim/).
In this case, just run the script `build.py`, which attempts to build 
NetAnim (if dependencies are met) and then ns-3 itself.
If you want to build ns-3 examples and tests (a full ns-3 build),
instead type:
```
./build.py --enable-examples --enable-tests
```
or you can simply enter into the ns-3 directory directly and use the
build tools therein (see the tutorial).

This directory also contains the [bake build tool](https://www.gitlab.com/nsnam/bake/), which allows access to
other extensions of ns-3, including the Direct Code Execution environment,
BRITE, click and openflow extensions for ns-3.  Consult the ns-3 tutorial
on how to use bake to access optional ns-3 components.

If you have downloaded this from Git, the `download.py` script can be used to
download bake, netanim, and ns-3-dev.  The usage to use
basic ns-3 (netanim and ns-3-dev) is to type:
```
./download.py
./build.py --enable-examples --enable-tests
```
and change directory to ns-3-dev for further work.
