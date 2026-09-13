# État du projet — LlamaBuilder

> Cahier des charges : [`mardown/llama-builder-spec.md`](mardown/llama-builder-spec.md)

## Avancement

| Phase | Périmètre | État |
|---|---|---|
| **1** | Noyau sans UI : `Profile`, `Settings`, `ProfileStore`, `ParamRegistry`, `CommandBuilder`, tests | **terminée** |
| **2** | Coque Qt Quick : thème, liste de profils, formulaire dynamique, barre de commande, Réglages | **terminée** |
| 3 | Monitoring : `NvmlMonitor`, `SystemMonitor`, jauges animées | à faire |
| 4 | Exécution : `LlamaRunner`, panneau de logs, validation | à faire |
| 5 | Finitions : recherche, duplication au clavier, géométrie, raccourcis | à faire |

## Environnement retenu

| Élément | Choix effectif |
|---|---|
| Compilateur | MSVC 14.51 (Visual Studio 18 Community) |
| CMake / Ninja | 4.3.1 / 1.13.2, fournis par Visual Studio |
| Qt | **6.10.3 `msvc2022_64`**, installé dans `C:\Qt` via `aqtinstall` |
| llama.cpp de référence | build **b10586** (`C:\dev\llama-cpp\llama-b10586-bin-win-cuda-13.3-x64`) |
| GPU | RTX 5090 Laptop, 24 Go — `nvml.dll` présent dans `System32` |
| Écran de développement | 3840 × 2400 à 200 % — l'application est testée sous facteur d'échelle 2 |

### Écarts assumés par rapport au cahier des charges

- **vcpkg abandonné.** Le mode manifest imposerait de compiler Qt 6 depuis les
  sources (des heures, des dizaines de Go). Les binaires officiels Qt sont
  installés par `aqtinstall` et localisés par `CMAKE_PREFIX_PATH`. Qt reste lié
  dynamiquement, comme l'exige la LGPLv3.
- **Section « Échantillonnage » visible pour les deux binaires.** Le §5.3 prévoit
  de la masquer pour `llama-server`, mais le build b10586 accepte réellement
  `--temp`, `--top-k/p`, `--min-p`, `--repeat-*`, `-s` et `-n` en ligne de
  commande. Les masquer priverait l'utilisateur de réglages fonctionnels.
- **`--mlock` et `--no-mmap` remplacés par `-lm/--load-mode`**, les deux premiers
  étant dépréciés dans ce build.
- **Nouveau type de paramètre `tristate`.** llama.cpp a généralisé les paires
  `--x` / `--no-x` pour des options déjà actives par défaut (`--jinja`,
  `--context-shift`, `--webui`…). Un simple booléen ne permettrait pas d'émettre
  `--no-x`, donc de désactiver quoi que ce soit.
- **Nouveau type `intOrKeyword`** pour `-ngl`, qui accepte désormais un entier,
  `auto` ou `all`.
- **`model` retiré de `params.json`.** Le chemin du modèle a son propre champ
  dans le profil ; le déclarer aussi comme paramètre créerait deux sources de
  vérité (vérifié par un test). Le formulaire lui réserve donc une carte
  « Profil » distincte, au-dessus des sections engendrées.
- **Deux marqueurs d'état au lieu d'un.** Le §5.3 ne prévoit qu'un badge
  « modifié ». Un paramètre posé explicitement à sa valeur par défaut est pourtant
  bien émis dans la commande : il reçoit un point d'accent, le badge restant
  réservé aux valeurs qui diffèrent du défaut.
- **Pas de bouton « Enregistrer ».** Le §5.2 exige qu'une sélection charge
  instantanément et que « Lancer » soit aussitôt disponible : l'édition modifie le
  profil en place et l'anti-rebond de `ProfileStore` écrit pour elle.
- **Deux fichiers ajoutés au noyau** par rapport au §3 : `JsonFile` (lecture et
  écriture atomiques, mise à l'écart des fichiers corrompus, mutualisée entre les
  deux dépôts) et `AppPaths` (résolution des emplacements).

### Corrections apportées à `params.json` contre le binaire réel

`-c` défaut 0 (et non 4096) · `-b` 2048 · `-ub` 512 · `-ngl` défaut `auto` ·
`-ctk`/`-ctv` étendus à 9 valeurs · `-fa` confirmé `on|off|auto`.
Paramètres récents ajoutés : `-ncmoe`, `-cmoe`, `--swa-full`, `-cram`, `-fit`,
`-fitt`, `-sm`, `--context-shift`, `--no-webui`, `--metrics`, `--check-tensors`.

## Phase 1 — noyau

```
CMakeLists.txt · CMakePresets.json · scripts/dev-env.ps1 · scripts/build.ps1
resources/params.json          47 paramètres, 7 sections
src/core/  AppPaths · CommandBuilder · JsonFile · ParamRegistry
           Profile · ProfileStore · Settings · SettingsStore
src/cli/main.cpp               harnais : params | list | show <nom|id> | demo
tests/     test_commandbuilder (53 cas) · test_store (22 cas)
```

Points d'architecture :

- `core/` ne dépend que de `Qt6::Core`. Seuls `ProfileStore` et `SettingsStore`
  sont des `QObject`, uniquement pour l'anti-rebond et les signaux.
- `CommandBuilder::build()` est une fonction pure. Elle produit **en une passe**
  les arguments bruts (pour `QProcess`) et la ligne citée (pour l'affichage et le
  presse-papier) ; aucune n'est obtenue en redécoupant l'autre.
- La citation suit la règle canonique de `CommandLineToArgvW`, antislashs devant
  guillemet inclus. Un test de propriété vérifie que citer puis redécouper rend
  l'argument intact.
- L'ordre des arguments suit `params.json`, jamais l'ordre de la table du profil
  (test dédié).
- `params.json` est embarqué comme filet de sécurité, mais une copie dans
  `%APPDATA%\LlamaBuilder\` ou à côté de l'exécutable le remplace — ajouter un
  flag ne demande donc aucune recompilation.

## Phase 2 — coque Qt Quick

```
src/ui/    AppController · ParamFormModel · ParamFilterModel
           ProfileListModel · Fonts
src/app/main.cpp               point d'entrée QGuiApplication + QQmlApplicationEngine
qml/       Main · Theme · ProfilePanel · ParamSection · ParamRow · CommandBar
           SettingsWindow · InfoTip · Segmented · FlatButton · ThemedTextField
           ThemedComboBox · ThemedCheckBox · ThemedMenu · ThemedDialog
           ConfirmDialog · NamePrompt
resources/fonts/               Inter (Regular/Medium/SemiBold) + JetBrains Mono, OFL
tests/test_uimodels.cpp        13 cas
```

Points d'architecture :

- **Un seul modèle plat pour le formulaire.** `ParamFormModel` porte les 47
  paramètres dans l'ordre de `params.json` et détient les valeurs du profil
  courant ; toute écriture passe par `setValue()`. Le regroupement par section et
  le masquage selon le binaire sont assurés par `ParamFilterModel`, un proxy de
  filtrage instancié une fois par section depuis le QML. Une section dont le
  `count` tombe à zéro se masque d'elle-même : le §5.3 (« Serveur » invisible en
  mode cli) est une conséquence du filtre, pas une condition écrite dans la vue.
- **Convention d'absence unique** : une valeur vide signifie « paramètre non
  posé », donc retiré de la table du profil et absent de la commande. Tous les
  contrôles l'appliquent — case décochée, « (défaut) » d'une liste, champ vidé.
- **`AppController` est la seule façade** entre le noyau et le QML. Il détient le
  registre, les deux dépôts et les modèles ; c'est par lui que la phase 4
  branchera `LlamaRunner` sans toucher aux vues.
- **Enregistrement déclaratif des types** (`QML_ELEMENT`, `QML_SINGLETON`) : les
  sources de `src/ui/` sont compilées dans la cible qui porte le module QML, ce
  qui permet à `qmltyperegistrar` de publier les types et donc à **qmllint
  d'analyser statiquement les 17 fichiers QML**. Les tests recompilent la même
  liste de sources sans module QML.
- **Le thème du §5.7 vit dans un unique singleton `Theme.qml`** : couleurs,
  échelle d'espacement 4/8/12/16/24/32, rayons 6/10, durées 150/200/400 ms.
  Aucune couleur en littéral ailleurs.
- **Style `Basic` de Qt Quick Controls**, seul style qui n'impose pas ses propres
  couleurs ; chaque contrôle est redécoré (`background`, `contentItem`, `popup`).
- **Recalcul synchrone de la commande** à chaque frappe, comme le §5.4
  l'autorise : `CommandBuilder::build()` est pure, rien à différer.
- **Polices embarquées** en ressource Qt : l'apparence ne dépend pas de ce qui est
  installé sur la machine. Repli sur Segoe UI / Cascadia Mono si la ressource
  manque.
- `pragma ComponentBehavior: Bound` dans tous les fichiers à délégués : les
  identifiants extérieurs y sont liés, jamais résolus dynamiquement.

### Contrat à ne pas casser

L'application définit `applicationName` mais **pas** `organizationName` :
`QStandardPaths` insérerait sinon un niveau supplémentaire et le dossier de
données ne serait plus `%APPDATA%\LlamaBuilder`. Elle ne définit pas non plus
`applicationDisplayName`, que Qt concaténerait au titre de la fenêtre.

## Validation exécutée

- **88 cas de test au vert** (`ctest --preset debug` et `--preset release`) :
  53 + 22 + 13.
- Compilation **sans aucun avertissement** en `/W4 /permissive-`, en Debug comme
  en Release. Les en-têtes Qt sont traités comme externes ; `C4702`, émis depuis
  `qvariant.h` et `qjsengine.h` à la génération de code, est désactivé.
- **`qmllint` sans aucun diagnostic** sur les 17 fichiers QML
  (`cmake --build build/msvc --config Debug --target all_qmllint`).
- Application lancée : aucun avertissement QML à l'exécution, profil réel chargé,
  commande affichée identique à celle du harnais console, sections engendrées
  depuis `params.json`, badge « modifié » et compteurs par section corrects.
- Commande générée pour un modèle dont le chemin contient des espaces, copiée
  telle quelle et exécutée par `cmd.exe` : `llama-server` journalise
  `loading model 'C:\dev\llama_desktop_modeles\qwen3 27b q5.gguf'`, puis
  `model loaded` et `listening on http://127.0.0.1:8099`. Aucun flag rejeté.
  → critère d'acceptation n°2 tenu.
- Repli sur la ressource embarquée vérifié en retirant le `params.json` voisin.

### Artefacts de test créés hors du dépôt (supprimables)

- `%APPDATA%\LlamaBuilder\settings.json` et `profiles.json` (profil « Mon Qwen —
  chemin avec espaces »).
- `C:\dev\llama_desktop_modeles\` : deux **liens matériels** vers des GGUF de
  `C:\dev\llama-cpp\models` (aucun espace disque consommé). C'est aussi le
  `defaultModelsDir` de `settings.json`.

## Compiler et lancer

```powershell
.\scripts\build.ps1              # Debug + tests
.\scripts\build.ps1 -Config Release
.\scripts\build.ps1 -Clean

.\build\msvc\Debug\llamabuilder.exe        # l'application
.\build\msvc\Debug\llamabuilder-cli.exe params   # diagnostic du noyau
```

Les chemins de Visual Studio et de Qt sont découverts automatiquement
(`vswhere`, glob sur `C:\Qt`). Surcharges : `$env:VS_DIR`, `$env:QT_DIR`.
Le script doit être appelé depuis une session où `scripts\dev-env.ps1` a été
sourcé, sinon les DLL Qt manquent au lancement.

## Prochaine étape — phase 3

Monitoring : `NvmlMonitor` (chargement dynamique de `nvml.dll`, dégradation
propre si absent), `SystemMonitor` pour la RAM, et les jauges animées du §5.1.
L'emplacement est déjà réservé en haut du panneau central, et l'intervalle de
rafraîchissement est déjà réglable dans la fenêtre Réglages.
