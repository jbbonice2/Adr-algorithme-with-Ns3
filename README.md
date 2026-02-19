# Simulation LoRaWAN ADR - Comparaison d'Algorithmes

Simulation NS-3 pour comparer les performances de différents algorithmes ADR (Adaptive Data Rate) dans un réseau LoRaWAN.

## 📋 Prérequis

- NS-3.42 avec le module LoRaWAN installé pas besoin de les installé
- Bash (Linux/macOS)
- Installer g++ et cmake si vous en posseder pas

## 🔧 Compilation

```bash
cd ns-3.42
./ns3 clean     # pour supprimer le build car il vous faut ce qui a été construit sur votre machine
sudo apt install cmake # pour installer cmake
sudo apt install g++ build-essential  # pour installer le compilateur g++
./ns3 configure --enable-examples --enable-tests  # pour configurer le projet
./ns3 build  # pour builder le projet
```

## 🚀 Exécution des Simulations

### Lancer un scénario

```bash
cd ns-3.42/scratch
chmod +x run_simulations_module.sh

# Exécuter un scénario spécifique
./run_simulations_module.sh 1   # Scénario 1: Variation densité
./run_simulations_module.sh 2   # Scénario 2: Variation mobilité
./run_simulations_module.sh 3   # Scénario 3: Variation perte aléatoire (sigma)
./run_simulations_module.sh 4   # Scénario 4: Variation intervalle d'envoi
```

### Exécution manuelle d'une simulation

```bash
cd ns-3.42
./ns3 run "lorawan-adr-simulation-module --numDevices=100 --mobilitySpeed=0 --trafficInterval=3600 --maxRandomLoss=0 --adrAlgo=ADR-AVG --scenario=1 --runNumber=1 --simulationTime=360000"
```

## 📊 Algorithmes ADR Comparés

| Algorithme | Description |
|------------|-------------|
| **No-ADR** | ADR désactivé, SF et TxPower assignés aléatoirement |
| **ADR-MAX** | Utilise le SNR maximum de l'historique des paquets |
| **ADR-AVG** | Utilise le SNR moyen (algorithme ADR standard LoRaWAN) |
| **ADR-MIN** | Utilise le SNR minimum (approche conservatrice) |
| **ADR-Lite** | Recherche binaire sans historique de paquets |

## 📈 Scénarios de Test

### Scénario 1: Variation de la Densité
- **Densités**: 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000 devices
- **Mobilité**: 0, 33.33, 60 km/h
- **Intervalle d'envoi**: 3600, 145, 72 secondes
- **Perte aléatoire**: 0, 3.96, 7.92 dB

### Scénario 2: Variation de la Mobilité
- **Mobilité**: 0, 6.67, 13.33, 20, 26.67, 33.33, 40, 46.67, 53.33, 60 km/h
- **Densités**: 100, 550, 1000 devices
- **Intervalle d'envoi**: 3600, 145, 72 secondes
- **Perte aléatoire**: 0, 3.96, 7.92 dB

### Scénario 3: Variation de la Perte Aléatoire
- **Perte aléatoire**: 0, 1.98, 3.96, 5.94, 7.92 dB
- **Densités**: 100, 550, 1000 devices
- **Mobilité**: 0, 33.33, 60 km/h
- **Intervalle d'envoi**: 3600, 145, 72 secondes

### Scénario 4: Variation de l'Intervalle d'Envoi
- **Intervalle**: 3600, 327, 240, 180, 145, 120, 103, 90, 80, 72 secondes
- **Densités**: 100, 550, 1000 devices
- **Mobilité**: 0, 33.33, 60 km/h
- **Perte aléatoire**: 0, 3.96, 7.92 dB

## 📁 Structure des Résultats

```
ns-3.42/resultsfinal/
└── summaries/
    ├── summary_scenario1.csv   # Résultats scénario densité
    ├── summary_scenario2.csv   # Résultats scénario mobilité
    ├── summary_scenario3.csv   # Résultats scénario sigma
    └── summary_scenario4.csv   # Résultats scénario intervalle
```

### Format des fichiers CSV

| Colonne | Description |
|---------|-------------|
| Scenario | Nom du scénario (density, mobilite, sigma, intervalle_d_envoie) |
| Algorithm | Algorithme ADR utilisé |
| NumDevices | Nombre de dispositifs |
| MobilitySpeed | Vitesse de mobilité (km/h) |
| TrafficInterval | Intervalle d'envoi (secondes) |
| MaxRandomLoss | Perte aléatoire max (dB) |
| RunNumber | Numéro de la répétition |
| TotalPackets | Nombre total de paquets envoyés |
| SuccessfulPackets | Nombre de paquets reçus avec succès |
| PDR_Percent | Packet Delivery Ratio (%) |
| AvgEnergy_mJ | Énergie moyenne par paquet reçu (millijoules) |

## 📂 Fichiers Principaux

```
ns-3.42/scratch/
├── lorawan-adr-simulation-module.cc   # Code source de la simulation
└── run_simulations_module.sh          # Script d'automatisation
```

## ⚙️ Paramètres de Simulation

| Paramètre | Valeur par défaut | Description |
|-----------|-------------------|-------------|
| `numDevices` | 100 | Nombre de dispositifs LoRa |
| `mobilitySpeed` | 0.0 | Vitesse de mobilité (km/h) |
| `trafficInterval` | 50.0 | Intervalle entre envois (secondes) |
| `maxRandomLoss` | 10.0 | Perte aléatoire max (dB) |
| `adrAlgo` | ADR-AVG | Algorithme ADR |
| `simulationTime` | 3600.0 | Durée de simulation (secondes) |
| `radius` | 500.0 | Rayon de déploiement (mètres) |
| `enableEnergyModel` | true | Activer le modèle d'énergie |

## 📚 Documentation

- [NS-3 Documentation](https://www.nsnam.org/documentation/)
- [LoRaWAN Module](https://github.com/signetlabdei/lorawan)

---

## Installation NS-3

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
