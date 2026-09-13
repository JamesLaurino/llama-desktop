# LlamaBuilder — Cahier des charges

Application desktop Windows qui construit, sauvegarde et lance des commandes `llama.cpp`.

---

## 1. Objectif

Remplacer la saisie manuelle de commandes `llama.cpp` en ligne de commande par une interface graphique qui :

1. Construit la commande via des champs documentés (info-bulles).
2. Sauvegarde une configuration **par modèle**, sous un nom personnalisé choisi par l'utilisateur.
3. Affiche l'occupation RAM et VRAM en temps réel sous forme de jauges.
4. Permet de copier la commande générée, ou de la lancer directement.

### Hors périmètre (v1)

- Pas de chat, pas d'inférence intégrée, pas de client API.
- Pas de téléchargement de modèles, pas de conversion GGUF, pas de quantification.
- Pas de gestion multi-GPU avancée (une seule carte NVIDIA).
- Pas de multi-plateforme : Windows uniquement.
- Pas de traduction : interface en français.

L'application est **un générateur de commande**, rien d'autre.

---

## 2. Stack technique

| Élément | Choix | Justification |
|---|---|---|
| Langage | C++20 | Demandé. |
| Build | CMake ≥ 3.21 + Ninja | Standard Qt. |
| Dépendances | vcpkg (manifest mode) | Reproductible. |
| UI | **Qt 6.7+ / Qt Quick (QML)** | Meilleur compromis « UI soignée » : animations fluides, thème entièrement personnalisable, rendu GPU. |
| Style | Qt Quick Controls 2, style `Basic` + thème custom | Le style `Windows` natif empêche la personnalisation ; `Basic` laisse tout contrôler. |
| VRAM | NVML (`nvml.dll`, chargée dynamiquement) | Pilote NVIDIA, aucune dépendance CUDA requise. |
| RAM | Win32 : `GlobalMemoryStatusEx`, `GetProcessMemoryInfo` | Natif, zéro dépendance. |
| JSON | `QJsonDocument` | Déjà dans Qt, inutile d'ajouter nlohmann. |
| Processus | `QProcess` | Lancement et capture des logs. |

### Note sur la consommation mémoire

Qt Quick consomme ~90–150 Mo de RAM et ~80–150 Mo de VRAM (scene graph GPU) contre ~40 Mo RAM / ~0 VRAM pour Qt Widgets. Sur une carte de 24 Go, l'impact VRAM est inférieur à 1 %. Si cette consommation devient gênante, l'alternative est Qt Widgets + QSS : la logique métier (sections 4 à 8) est indépendante de l'UI et reste réutilisable telle quelle.

**Licence** : Qt 6 open source est sous LGPLv3 → lier **dynamiquement** (DLL), ne pas lier statiquement.

---

## 3. Architecture

```
llamabuilder/
├── CMakeLists.txt
├── vcpkg.json
├── resources/
│   ├── params.json           # Définition des paramètres llama.cpp (voir §6)
│   ├── qml/                  # Vues QML
│   └── fonts/, icons/
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── Profile.h/.cpp            # Modèle de données d'un profil
│   │   ├── ProfileStore.h/.cpp       # Chargement/sauvegarde JSON
│   │   ├── ParamRegistry.h/.cpp      # Parsing de params.json
│   │   ├── CommandBuilder.h/.cpp     # Génération + quoting de la ligne
│   │   └── Settings.h/.cpp           # Chemins exécutables, préférences
│   ├── monitor/
│   │   ├── NvmlMonitor.h/.cpp        # VRAM (chargement dynamique nvml.dll)
│   │   └── SystemMonitor.h/.cpp      # RAM système + RAM du processus enfant
│   ├── process/
│   │   └── LlamaRunner.h/.cpp        # QProcess, logs, arrêt
│   └── ui/
│       ├── ProfileListModel.h/.cpp   # QAbstractListModel exposé au QML
│       └── ParamFormModel.h/.cpp     # Modèle dynamique du formulaire
└── tests/
    └── test_commandbuilder.cpp
```

**Règle d'architecture** : `core/` ne dépend d'aucun type UI. `CommandBuilder` est une fonction pure `Profile + Settings → QStringList` + `QString` (version affichable). C'est la seule partie qui doit être couverte par des tests unitaires.

---

## 4. Modèle de données

### 4.1 Profil

Un profil = un modèle GGUF + une configuration + un nom lisible.

```json
{
  "id": "7f3a2c10-...",
  "name": "Qwen3 27B — contexte long",
  "modelPath": "D:/models/qwen3-27b-q4_k_m.gguf",
  "binary": "server",
  "params": {
    "n-gpu-layers": "999",
    "ctx-size": "16384",
    "flash-attn": "on",
    "cache-type-k": "q8_0",
    "temp": "0.7",
    "port": "8080"
  },
  "extraArgs": "--no-warmup",
  "notes": "Config validée le 12/09, tient en 22 Go",
  "createdAt": "2026-09-13T10:00:00Z",
  "lastUsedAt": "2026-09-13T14:22:00Z"
}
```

- `name` est **obligatoire, libre et jamais dérivé du nom de fichier**. À la création, le champ est vide avec un placeholder (« Mon profil ») ; l'enregistrement est bloqué tant qu'il est vide.
- `params` ne contient **que les paramètres explicitement activés** par l'utilisateur. Un paramètre laissé à sa valeur par défaut n'apparaît ni dans le JSON ni dans la commande.
- `binary` : `"server"` (`llama-server.exe`) ou `"cli"` (`llama-cli.exe`).
- `extraArgs` : texte libre ajouté en fin de commande, pour les flags non couverts par l'UI.

### 4.2 Réglages globaux

```json
{
  "llamaServerPath": "C:/llama.cpp/build/bin/llama-server.exe",
  "llamaCliPath": "C:/llama.cpp/build/bin/llama-cli.exe",
  "defaultModelsDir": "D:/models",
  "monitorIntervalMs": 1000,
  "theme": "dark"
}
```

### 4.3 Stockage

- Dossier : `%APPDATA%\LlamaBuilder\` (via `QStandardPaths::AppDataLocation`).
- `profiles.json` : tableau de profils. `settings.json` : réglages globaux.
- **Écriture atomique** : écrire dans `profiles.json.tmp`, puis `MoveFileEx` avec remplacement. Ne jamais laisser un fichier à moitié écrit.
- Sauvegarde déclenchée sur modification, avec un debounce de 500 ms.
- Au démarrage, si le JSON est corrompu : renommer en `profiles.corrupt-<date>.json`, démarrer avec une liste vide, afficher un bandeau d'avertissement. Ne jamais supprimer silencieusement.

---

## 5. Interface

### 5.1 Disposition

```
┌──────────────────────────────────────────────────────────────────────┐
│  LlamaBuilder                                        [⚙ Réglages]     │
├──────────────┬───────────────────────────────────────────────────────┤
│              │  Qwen3 27B — contexte long              [Dupliquer]   │
│ [+ Nouveau]  │  D:/models/qwen3-27b-q4_k_m.gguf         [Supprimer]  │
│              │                                                        │
│ ▸ Qwen3 27B  │  ┌─ Jauges ────────────────────────────────────────┐  │
│   Mistral 7B │  │ VRAM  ███████████████░░░░░  15.2 / 24.0 Go       │  │
│   Llama 8B   │  │ RAM   ██████░░░░░░░░░░░░░░   9.8 / 64.0 Go       │  │
│   ...        │  └─────────────────────────────────────────────────┘  │
│              │                                                        │
│ (recherche)  │  ◈ Modèle & binaire     ─────────────────────────── ▾ │
│              │  ◈ GPU                  ─────────────────────────── ▾ │
│              │  ◈ Contexte & mémoire   ─────────────────────────── ▾ │
│              │  ◈ CPU                  ─────────────────────────── ▸ │
│              │  ◈ Échantillonnage      ─────────────────────────── ▸ │
│              │  ◈ Serveur              ─────────────────────────── ▸ │
│              │  ◈ Avancé               ─────────────────────────── ▸ │
│              │                                                        │
├──────────────┴───────────────────────────────────────────────────────┤
│ llama-server.exe -m "D:/models/qwen3-27b..." -ngl 999 -c 16384 ...   │
│                                    [Copier]  [Lancer]  [Arrêter]     │
└──────────────────────────────────────────────────────────────────────┘
```

### 5.2 Panneau gauche — profils

- Liste triée par `lastUsedAt` décroissant, avec un champ de recherche filtrant sur `name`.
- Chaque entrée affiche : nom personnalisé (gras) + nom de fichier du modèle en petit et grisé.
- Un point vert animé sur le profil en cours d'exécution.
- Menu contextuel : Renommer, Dupliquer, Révéler le fichier modèle, Supprimer.
- Suppression avec confirmation.
- Sélectionner un profil charge instantanément sa configuration ; le bouton **Lancer** est immédiatement disponible, sans étape intermédiaire.

### 5.3 Panneau central — formulaire

- Sections repliables, générées dynamiquement à partir de `params.json`. Les sections « Modèle », « GPU » et « Contexte & mémoire » sont dépliées par défaut.
- Chaque paramètre affiche : libellé, contrôle adapté au type, un badge « modifié » si sa valeur diffère du défaut, et une icône **(?)**.
- **Info-bulle** : apparaît au survol de l'icône après 300 ms. Contenu = `flag` en monospace, puis explication en français, puis la valeur par défaut. Largeur max 360 px, texte non tronqué.
- Bouton « Réinitialiser » par section, qui retire les paramètres de la section du profil.
- La section **Serveur** est masquée si `binary == "cli"` ; la section **Échantillonnage** est masquée si `binary == "server"` (le serveur reçoit ces valeurs par requête, pas en ligne de commande — voir §6, note).

### 5.4 Barre de commande (bas, toujours visible)

- Zone de texte en lecture seule, police monospace, retour à la ligne activé, hauteur max ~4 lignes puis défilement.
- Mise à jour à chaque frappe (recalcul synchrone, l'opération est triviale).
- **Copier** : copie la commande exacte dans le presse-papier, avec un retour visuel de 1,5 s (« Copié ✓ »).
- **Lancer** : désactivé si la validation échoue (§8), avec la raison en info-bulle.
- **Arrêter** : visible uniquement si un processus tourne.

### 5.5 Panneau de logs

Quand un processus tourne, un panneau coulissant depuis le bas affiche `stdout`/`stderr` fusionnés : monospace, défilement automatique désactivable, tampon circulaire de 5 000 lignes, bouton « Copier tout ». Si `binary == "server"`, un bouton **Ouvrir dans le navigateur** pointant vers `http://<host>:<port>` apparaît dès que le log contient la ligne signalant que le serveur écoute.

### 5.6 Fenêtre Réglages

Chemins vers `llama-server.exe` et `llama-cli.exe` (champ + bouton « Parcourir »), dossier de modèles par défaut, intervalle de rafraîchissement des jauges, thème. Chaque chemin affiche un indicateur ✓ / ✗ selon l'existence du fichier, vérifiée en temps réel.

### 5.7 Direction artistique

Objectif : sobre, dense sans être chargé, sans effet gratuit.

```
Fond principal      #16171A
Fond surélevé       #1E2024
Bordures            #2C2F36
Texte principal     #E8E9EC
Texte secondaire    #8B8F99
Accent              #5B8DEF
Jauge normale       #4ADE80   (< 75 %)
Jauge tension       #FBBF24   (75–90 %)
Jauge saturation    #F87171   (> 90 %)
```

- Police UI : **Inter** (embarquée via QRC). Police monospace : **JetBrains Mono**.
- Échelle d'espacement : 4 / 8 / 12 / 16 / 24 / 32 px. Rayon des coins : 6 px (contrôles), 10 px (cartes).
- Animations : 150 ms `OutCubic` pour les survols, 200 ms pour les sections repliables. Les jauges s'animent vers leur nouvelle valeur sur 400 ms pour éviter les à-coups.
- Pas d'ombres portées marquées, pas de dégradés, pas d'icônes colorées.
- Fenêtre redimensionnable, minimum 1100 × 700. Position et taille restaurées au lancement.

---

## 6. Paramètres llama.cpp et info-bulles

**Point d'architecture important** : les flags de `llama.cpp` changent d'une version à l'autre. Ils ne doivent donc **pas** être codés en dur dans le C++. Ils sont décrits dans `resources/params.json`, chargé au démarrage. Ajouter un paramètre = éditer un JSON, sans recompiler.

### 6.1 Format d'une entrée

```json
{
  "key": "ctx-size",
  "flag": "-c",
  "section": "context",
  "type": "int",
  "default": 4096,
  "min": 256,
  "max": 1048576,
  "step": 1024,
  "label": "Taille du contexte",
  "tooltip": "Nombre maximum de tokens que le modèle peut garder en mémoire (prompt + réponse). Double le contexte ≈ double la VRAM occupée par le cache KV. 0 = utiliser la valeur inscrite dans le fichier GGUF.",
  "appliesTo": ["server", "cli"]
}
```

Types supportés : `int`, `float`, `bool` (flag seul, sans valeur), `enum` (liste de choix), `string`, `path`.

### 6.2 Contenu initial de `params.json`

Les flags ci-dessous correspondent aux builds récentes de `llama.cpp`. **Vérifie-les contre ton binaire** avec `llama-server.exe --help` avant de figer le fichier — c'est un projet qui bouge vite.

#### Section « Modèle & binaire »

| key | flag | type | Info-bulle |
|---|---|---|---|
| `model` | `-m` | path | Chemin vers le fichier GGUF à charger. |
| `alias` | `-a` | string | Nom sous lequel le modèle est exposé par l'API du serveur. Purement cosmétique. |
| `mmproj` | `--mmproj` | path | Projecteur multimodal, requis uniquement pour les modèles vision. |

#### Section « GPU »

| key | flag | type | Info-bulle |
|---|---|---|---|
| `n-gpu-layers` | `-ngl` | int | Nombre de couches déchargées sur le GPU. C'est **le** réglage de performance : tout ce qui reste sur le CPU est dix à cinquante fois plus lent. Mettre une valeur très élevée (999) pour tout décharger ; réduire seulement si le modèle ne tient pas en VRAM. |
| `main-gpu` | `-mg` | int | Index du GPU principal. Sans effet avec une seule carte. |
| `no-kv-offload` | `-nkvo` | bool | Garde le cache KV en RAM système au lieu de la VRAM. Libère de la VRAM mais ralentit sensiblement. Utile en dernier recours pour gagner quelques centaines de Mo. |

#### Section « Contexte & mémoire »

| key | flag | type | Info-bulle |
|---|---|---|---|
| `ctx-size` | `-c` | int | Fenêtre de contexte en tokens. Double le contexte ≈ double la VRAM du cache KV. 0 = valeur définie dans le GGUF. |
| `batch-size` | `-b` | int | Taille de lot logique pour le traitement du prompt. Augmente la vitesse d'ingestion au prix de mémoire. |
| `ubatch-size` | `-ub` | int | Taille de micro-lot réellement calculée en une passe. Baisser cette valeur réduit le pic de VRAM pendant le traitement d'un long prompt. |
| `flash-attn` | `-fa` | enum (`on`/`off`/`auto`) | Attention optimisée : moins de VRAM pour le cache KV et plus rapide sur les cartes récentes. Requis pour quantifier le cache KV. `auto` laisse llama.cpp décider. |
| `cache-type-k` | `-ctk` | enum (`f16`/`q8_0`/`q4_0`) | Quantification du cache K. `q8_0` divise par deux la VRAM du cache pour une perte de qualité négligeable. Nécessite Flash Attention. |
| `cache-type-v` | `-ctv` | enum (`f16`/`q8_0`/`q4_0`) | Idem pour le cache V. Même contrainte. |
| `mlock` | `--mlock` | bool | Verrouille le modèle en RAM pour empêcher Windows de le paginer sur disque. Évite des ralentissements brutaux, au prix de RAM immobilisée. |
| `no-mmap` | `--no-mmap` | bool | Charge le modèle entièrement en mémoire au lieu de le mapper depuis le disque. Démarrage plus lent, mais évite des accès disque ultérieurs. |
| `keep` | `--keep` | int | Nombre de tokens du prompt initial toujours conservés quand le contexte déborde. -1 = tout garder. |

#### Section « CPU »

| key | flag | type | Info-bulle |
|---|---|---|---|
| `threads` | `-t` | int | Threads pour la génération. Au-delà du nombre de cœurs physiques, les performances stagnent ou baissent. Peu d'impact si tout est sur le GPU. |
| `threads-batch` | `-tb` | int | Threads pour le traitement du prompt. Peut dépasser `-t` avec profit. |

#### Section « Échantillonnage » (`llama-cli`)

| key | flag | type | Info-bulle |
|---|---|---|---|
| `temp` | `--temp` | float | Contrôle l'aléatoire. Bas (0.1–0.3) = factuel et répétitif. Haut (0.8–1.2) = créatif et instable. |
| `top-k` | `--top-k` | int | Ne considère que les K tokens les plus probables. 0 = désactivé. |
| `top-p` | `--top-p` | float | Ne considère que les tokens dont la probabilité cumulée atteint P. 1.0 = désactivé. |
| `min-p` | `--min-p` | float | Écarte les tokens dont la probabilité est inférieure à P × (probabilité du meilleur token). Alternative moderne à top-p. |
| `repeat-penalty` | `--repeat-penalty` | float | Pénalise les tokens déjà apparus. 1.0 = aucune pénalité. Au-delà de 1.2, le texte se dégrade. |
| `repeat-last-n` | `--repeat-last-n` | int | Fenêtre de tokens sur laquelle la pénalité s'applique. |
| `seed` | `-s` | int | Graine aléatoire. Une valeur fixe rend la génération reproductible. -1 = aléatoire. |
| `n-predict` | `-n` | int | Nombre maximum de tokens à générer. -1 = illimité. |

> **Note** : avec `llama-server`, ces valeurs sont normalement fournies par requête API, pas en ligne de commande. Les builds récentes acceptent toutefois certaines d'entre elles comme valeurs par défaut du serveur. C'est pourquoi `appliesTo` est déclaratif par paramètre : ajuste-le selon ce que ton binaire accepte réellement.

#### Section « Serveur » (`llama-server`)

| key | flag | type | Info-bulle |
|---|---|---|---|
| `host` | `--host` | string | Adresse d'écoute. `127.0.0.1` = accessible uniquement depuis cette machine. `0.0.0.0` = exposé sur le réseau local. |
| `port` | `--port` | int | Port HTTP du serveur. |
| `api-key` | `--api-key` | string | Clé exigée dans l'en-tête `Authorization`. Indispensable si l'hôte est `0.0.0.0`. |
| `parallel` | `-np` | int | Nombre de requêtes traitées simultanément. Le contexte total est divisé entre les slots : `-np 4 -c 16384` donne 4096 tokens par slot. |
| `jinja` | `--jinja` | bool | Utilise le gabarit de chat Jinja embarqué dans le GGUF. Nécessaire pour l'appel d'outils et pour plusieurs modèles récents. |
| `chat-template` | `--chat-template` | string | Force un gabarit de conversation nommé, à la place de celui du GGUF. |
| `timeout` | `-to` | int | Délai d'attente serveur en secondes. |

#### Section « Avancé »

| key | flag | type | Info-bulle |
|---|---|---|---|
| `rope-scaling` | `--rope-scaling` | enum (`none`/`linear`/`yarn`) | Méthode d'extension du contexte au-delà de l'entraînement d'origine. Dégrade la qualité si mal réglé. |
| `rope-freq-base` | `--rope-freq-base` | float | Fréquence de base RoPE. 0 = valeur du modèle. |
| `rope-freq-scale` | `--rope-freq-scale` | float | Facteur d'échelle RoPE. Diviser par 2 double approximativement le contexte utilisable. |
| `verbose` | `-v` | bool | Logs détaillés. Utile pour diagnostiquer un échec de chargement. |

Plus le champ libre **Arguments supplémentaires**, avec l'info-bulle : « Ajouté tel quel à la fin de la commande. Pour les flags absents de l'interface. »

---

## 7. Génération de la commande

### Règles

1. Ordre : `<exécutable>` → `-m <modèle>` → paramètres dans l'ordre de `params.json` → `extraArgs`.
2. Seuls les paramètres présents dans `profile.params` sont émis. Aucun défaut implicite n'est écrit.
3. Un paramètre `bool` à `true` émet le flag seul ; à `false`, rien.
4. **Quoting Windows** : encadrer de guillemets doubles toute valeur contenant un espace, une tabulation ou l'un de `&|<>^`. Les guillemets internes sont échappés en `\"`. Les chemins sont normalisés en séparateurs `\` dans la chaîne affichée.
5. Deux sorties distinctes :
   - `QStringList` d'arguments bruts, non quotés → passé à `QProcess::start()` (Qt gère lui-même l'échappement).
   - `QString` affichable et quotée → zone de texte et presse-papier.

   Ne jamais générer la chaîne puis la re-découper : c'est la source classique de bugs sur les chemins avec espaces.
6. La commande copiée doit être collable telle quelle dans `cmd.exe` et fonctionner.

### Tests unitaires attendus

- Chemin contenant des espaces → correctement quoté.
- Profil vide → seulement l'exécutable et `-m`.
- Booléen à `false` → flag absent.
- `extraArgs` avec plusieurs flags → découpage respectant les guillemets.
- Valeur égale au défaut mais explicitement définie par l'utilisateur → présente dans la commande.

---

## 8. Validation

Contrôles effectués avant d'autoriser le lancement, affichés sous forme de bandeau sous l'en-tête :

| Condition | Sévérité | Message |
|---|---|---|
| Exécutable introuvable | Bloquant | « llama-server.exe est introuvable. Vérifie le chemin dans les Réglages. » |
| Fichier modèle introuvable | Bloquant | « Le fichier modèle n'existe plus à cet emplacement. » |
| Nom de profil vide | Bloquant | « Donne un nom à ce profil. » |
| Port déjà occupé | Avertissement | « Le port 8080 semble déjà utilisé. » |
| `-ctk`/`-ctv` ≠ `f16` sans `-fa` | Avertissement | « La quantification du cache KV requiert Flash Attention. » |
| Taille du fichier modèle > VRAM totale et `-ngl` élevé | Avertissement | « Le modèle (28 Go) dépasse la VRAM disponible (24 Go). Réduis `-ngl` ou attends-toi à un échec de chargement. » |

Les avertissements n'empêchent jamais le lancement. L'utilisateur sait ce qu'il fait.

---

## 9. Monitoring RAM / VRAM

### VRAM — NVML

Charger `nvml.dll` avec `LoadLibraryW` (chercher dans `System32`, puis `C:\Program Files\NVIDIA Corporation\NVSMI`). Ne **pas** lier statiquement : l'application doit démarrer même sans pilote NVIDIA, avec les jauges VRAM simplement désactivées et un message « GPU NVIDIA non détecté ».

Fonctions utilisées :

```
nvmlInit_v2
nvmlDeviceGetHandleByIndex_v2       → GPU 0
nvmlDeviceGetName                   → nom affiché sous la jauge
nvmlDeviceGetMemoryInfo_v2          → total / used / free
nvmlDeviceGetComputeRunningProcesses_v3  → usedGpuMemory par PID
nvmlDeviceGetUtilizationRates       → % d'utilisation GPU (optionnel)
nvmlShutdown
```

`nvmlDeviceGetComputeRunningProcesses_v3` permet d'isoler la VRAM du processus llama.cpp lancé par l'application — c'est ce qui donne la lecture « llama utilise 15,2 Go » plutôt qu'un total système. Si le PID n'est pas trouvé (processus non lancé par nous), afficher l'occupation globale de la carte.

### RAM — Win32

- Total et disponible : `GlobalMemoryStatusEx`.
- Processus enfant : `GetProcessMemoryInfo` sur le handle du `QProcess`, champ `PrivateUsage` de `PROCESS_MEMORY_COUNTERS_EX`.

### Boucle

- Thread dédié (`QThread`), intervalle 1000 ms configurable, résultats poussés vers le QML par signal.
- Le polling s'arrête quand la fenêtre est minimisée.
- NVML est initialisé une seule fois au démarrage, jamais par tick.

### Affichage

```
VRAM   ███████████████░░░░░░░   15.2 / 24.0 Go        (llama : 15.2 Go)
RAM    ██████░░░░░░░░░░░░░░░░    9.8 / 64.0 Go        (llama : 1.4 Go)
```

Barre pleine largeur, hauteur 8 px, coins arrondis, couleur selon les seuils (§5.7). Valeurs en Go avec une décimale. Transition animée sur 400 ms. La portion attribuée au processus llama est dessinée dans une teinte plus soutenue que le reste de l'occupation système.

---

## 10. Lancement du processus

- `QProcess` avec `setProgram()` + `setArguments()` (jamais `start(QString)`).
- `setProcessChannelMode(MergedChannels)` pour un flux de logs unique.
- Répertoire de travail : le dossier de l'exécutable llama.cpp.
- Un seul processus à la fois en v1. Lancer un autre profil propose d'arrêter le précédent.
- **Arrêter** : `terminate()`, puis `kill()` après 5 s sans réponse.
- À la fermeture de l'application, si un processus tourne : demander confirmation, puis le tuer. Ne pas laisser de processus orphelin.
- Le code de sortie est affiché dans le panneau de logs ; une sortie non nulle colore le bandeau en rouge et conserve les logs à l'écran.
- `lastUsedAt` du profil est mis à jour au lancement.

---

## 11. Phases d'implémentation

Construire dans cet ordre, chaque phase étant utilisable telle quelle.

**Phase 1 — Noyau sans UI**
`Profile`, `Settings`, `ProfileStore` (JSON + écriture atomique), `ParamRegistry`, `CommandBuilder` avec ses tests unitaires. Un exécutable console qui charge un profil et imprime la commande suffit à valider.

**Phase 2 — Coque UI**
Fenêtre Qt Quick, thème, liste de profils avec CRUD, formulaire dynamique généré depuis `params.json`, barre de commande avec bouton Copier. À la fin de cette phase, l'application répond déjà au besoin principal.

**Phase 3 — Monitoring**
`NvmlMonitor`, `SystemMonitor`, jauges animées, dégradation propre sans GPU NVIDIA.

**Phase 4 — Exécution**
`LlamaRunner`, panneau de logs, Arrêter, ouverture du navigateur, validation et bandeaux d'avertissement.

**Phase 5 — Finitions**
Recherche dans la liste, duplication de profil, restauration de la géométrie de fenêtre, raccourcis clavier (`Ctrl+N` nouveau, `Ctrl+C` copier la commande quand le focus n'est pas dans un champ, `Ctrl+R` lancer, `Ctrl+.` arrêter), notes de profil.

---

## 12. Critères d'acceptation

1. Créer un profil, choisir un GGUF, régler `-ngl` et `-c`, nommer « Mon Qwen », fermer et rouvrir l'application : le profil est intact et sélectionnable.
2. La commande affichée, copiée et collée dans `cmd.exe`, se lance sans modification, y compris avec un chemin contenant des espaces.
3. Survoler l'icône **(?)** de `-ngl` affiche une explication lisible en français en moins de 500 ms.
4. Modèle chargé sur le GPU : la jauge VRAM reflète l'occupation réelle à moins de 200 Mo près de ce qu'affiche `nvidia-smi`.
5. Changer le chemin de `llama-server.exe` dans les Réglages est pris en compte immédiatement, sans redémarrage.
6. L'application démarre et reste utilisable sans pilote NVIDIA installé (jauges VRAM désactivées).
7. RAM au repos, profil chargé, aucun processus lancé : inférieure à 200 Mo.

---

## 13. À vérifier avant de commencer

- Les flags exacts de **ta** version de `llama.cpp` : `llama-server.exe --help > flags.txt`, puis confronter à `params.json`. Notamment `-fa`, dont la syntaxe a changé (booléen puis `on`/`off`/`auto`).
- Version de Qt disponible via vcpkg sur ta machine (`qtbase`, `qtdeclarative`).
- Emplacement effectif de `nvml.dll` sur ton installation.

---

## 14. Idées pour une v2

Volontairement exclues de la v1, listées ici pour ne pas les intégrer par accident :

- Lecture des métadonnées du GGUF (architecture, quantification, nombre de couches) pour pré-remplir `-ngl`.
- Estimation prédictive de la VRAM avant lancement.
- Détection automatique des flags supportés en parsant `--help`.
- Import/export de profils pour les partager.
- Historique des commandes lancées.
- Multi-GPU (`--tensor-split`, `--split-mode`).
