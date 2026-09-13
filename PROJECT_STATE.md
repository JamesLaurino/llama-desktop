# État du projet — LlamaBuilder

> Cahier des charges : [`mardown/llama-builder-spec.md`](mardown/llama-builder-spec.md)

## Avancement

| Phase | Périmètre | État |
|---|---|---|
| **1** | Noyau sans UI : `Profile`, `Settings`, `ProfileStore`, `ParamRegistry`, `CommandBuilder`, tests | **terminée** |
| **2** | Coque Qt Quick : thème, liste de profils, formulaire dynamique, barre de commande, Réglages | **terminée** |
| **3** | Monitoring : `NvmlMonitor`, `SystemMonitor`, jauges animées | **terminée** |
| **3bis** | Import : `CommandParser`, alias dans `params.json`, dialogue « Importer » | **terminée** |
| 4 | Exécution : `LlamaRunner`, panneau de logs, validation | à faire |
| 5 | Finitions : recherche, duplication au clavier, géométrie, raccourcis | à faire |

## Environnement retenu

| Élément | Choix effectif |
|---|---|
| Compilateur | MSVC 14.44 (**Build Tools 2022**, sans IDE) |
| CMake / Ninja | fournis par les Build Tools, découverts par `vswhere` |
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
- **La VRAM par processus n'existe pas sous Windows WDDM.** Le §9 fait de
  `nvmlDeviceGetComputeRunningProcesses_v3` le moyen d'afficher « llama utilise
  15,2 Go ». Mesuré sur la RTX 5090 avec un `llama-server` chargé : NVML place
  bien le PID dans la liste des clients de calcul, mais répond
  `NVML_VALUE_NOT_AVAILABLE` pour sa consommation — `nvidia-smi` affiche `[N/A]`
  au même endroit. C'est la limite de WDDM, le mode de toute GeForce pilotant un
  écran. La jauge affiche donc « llama sur le GPU » sans chiffre, plutôt qu'un
  « 0,0 Go » mensonger. `vramProcess` reste implémenté et testé : une carte en
  mode TCC le renseignerait.
- **Diviseur 1024³ avec l'étiquette « Go ».** Diviser par 10⁹ afficherait 25,6 Go
  là où `nvidia-smi` rapporte 24 463 Mio, rendant le critère d'acceptation n°4
  invérifiable.
- **Une sous-commande `monitor` ajoutée au harnais console.** Le §9 ne la prévoit
  pas, mais confronter nos chiffres à `nvidia-smi` demandait de pointer le
  moniteur sur un PID avant que la phase 4 ne lance elle-même le processus.
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

## Phase 3 — monitoring

```
src/monitor/  MonitorSample · NvmlLibrary · NvmlMonitor · SystemMonitor
              MonitorWorker                     cible statique llamabuilder_monitor
src/ui/       MonitorController                 exposé par App.monitor
qml/          Gauge · MonitorCard
src/cli/      sous-commande monitor [--pid N]
tests/test_monitor.cpp                          22 cas
```

Points d'architecture :

- **`llamabuilder_monitor` est une cible séparée**, la seule à toucher `windows.h`
  et `psapi`. `core/` conserve sa promesse « rien d'autre que `Qt6::Core` », et la
  frontière est déjà tracée si le monitoring devenait multiplateforme.
- **NVML est déclarée, pas incluse.** `nvml.h` n'est distribué qu'avec le CUDA
  Toolkit, absent des postes de développement comme des postes utilisateurs. Huit
  signatures et trois structures sont déclarées dans `NvmlLibrary`, et trois
  `static_assert` gèlent leurs tailles : une erreur d'ABI devient une erreur de
  compilation au lieu d'une corruption de pile. Les suffixes `_v2`/`_v3` désignent
  des ABI figées par NVIDIA, c'est ce qui rend la déclaration légitime.
- **Le fil dédié n'est pas là pour le coût moyen d'un tick** — quelques centaines
  de microsecondes — mais pour ses pires cas : `nvmlInit_v2` prend 100 à 300 ms, et
  les appels NVML se sérialisent avec le pilote, donc bloquent au moment même où
  l'on regarde la jauge : le chargement d'un modèle de 20 Go.
- **Le `QTimer` est un enfant du worker, pas un membre par valeur.**
  `moveToThread` n'emporte que les enfants ; un minuteur sans parent reste dans le
  fil principal et ne tire jamais. Le défaut est invisible à l'œil nu parce que
  `start()` émet un relevé immédiat — d'où le test `controllerSamplesPeriodically`,
  qui compte les relevés au lieu d'en attendre un seul.
- **Les seuils et le formatage vivent en C++**, pas en QML : `monitor::loadFor` et
  `formatGigabytes` sont testés aux bornes exactes, et `Theme.gaugeColor` ne fait
  que traduire un niveau en couleur.
- **Communication par signaux typés** vers le worker plutôt que `invokeMethod`
  avec un nom de méthode en chaîne : la connexion est vérifiée à la compilation.
- `MonitorCard` déclare sa propriété comme `MonitorController` et non `var`, ce qui
  place chaque lecture de propriété sous le contrôle de qmllint.

## Phase 3bis — import de ligne de commande

Fonctionnalité **hors cahier des charges**, demandée en cours de route : coller une
commande et retrouver le profil correspondant.

```
src/core/     CommandParser                     fonction pure, miroir de CommandBuilder
resources/    params.json : champs aliases / aliasesOff sur 27 paramètres
src/ui/       AppController::analyseCommand / importCommandIntoCurrent
                                                 / importCommandAsNewProfile
qml/          ImportDialog                      bouton « Importer » dans CommandBar
src/cli/      sous-commande parse "<ligne>"
tests/test_commandparser.cpp                    55 cas
tests/test_uimodels.cpp                         4 cas d'import supplémentaires
```

Points d'architecture :

- **Les alias vivent dans `params.json`, pas dans le C++.** `params.json` ne
  déclarait que les formes courtes (`-c`, `-ngl`, `-fa`) ; une ligne copiée d'un
  README utilise presque toujours les formes longues. Les 27 alias manquants ont
  été relevés dans le `--help` du binaire b10586, pas devinés. Coder cette table
  en C++ aurait cassé le contrat « ajouter un paramètre ne demande aucune
  recompilation ».
- **Un drapeau ne peut désigner qu'un seul paramètre.** `ParamRegistry` refuse le
  fichier si deux entrées revendiquent la même écriture : un alias recouvrant le
  drapeau d'un autre paramètre ferait importer le mauvais réglage, sans bruit.
- **Deux pièges du `--help` évités.** `-kvo/--kv-offload` est listé sur la même
  ligne que `-nkvo/--no-kv-offload` alors qu'il en est l'inverse ; `--warmup` de
  même face à `--no-warmup`. Les prendre pour des alias aurait inversé le réglage
  en silence. Un test interdit leur reconnaissance.
- **Le parseur ne redécoupe rien.** Il consomme `CommandBuilder::splitArgumentLine`,
  déjà écrit et déjà testé pour les règles `CommandLineToArgvW`.
- **La propriété d'aller-retour est le test central** : `build(parse(build(p)))`
  doit être identique au caractère à `build(p)`, vérifié sur les huit types de
  paramètres. C'est elle qui a révélé que les chemins revenaient en antislashs.
- **Ce qui n'est pas reconnu est conservé, jamais perdu** : drapeau inconnu de ce
  build, jeton orphelin, drapeau sans valeur — tout part en arguments libres avec
  une note affichée dans l'aperçu.
- **Un refus est total.** Une ligne enchaînant plusieurs commandes (`&&`, `|`, `>`,
  `2>&1`) n'est pas tronquée pour en garder la première moitié : elle est rejetée,
  et rien n'est écrit.

### Écarts assumés propres à l'import

- **Écritures acceptées en lecture, jamais émises** : formes longues,
  `--option=valeur`, continuations de ligne des trois shells (`\`, `` ` ``, `^`),
  opérateur d'appel PowerShell `&`. L'application, elle, continue de n'écrire
  qu'une seule forme.
- **`-fa` seul vaut `on`.** Le paramètre est un enum dans ce build, mais c'était un
  booléen avant, et c'est encore l'écriture la plus répandue en ligne. Une note le
  signale.
- **Les chemins sont ramenés en séparateurs `/` à l'import**, comme ceux que rend
  le sélecteur de fichiers. `CommandBuilder` n'écrit des antislashs que dans la
  ligne affichée ; sans cette normalisation, le même chemin s'écrirait de deux
  façons selon son origine.
- **Un paramètre reconnu mais hors binaire est conservé** dans le profil, signalé
  dans l'aperçu, et non émis par la génération. Refuser l'import ou le déplacer en
  arguments libres perdrait l'intention de l'utilisateur.
- **L'import remplace, il ne fusionne pas.** Les paramètres absents de la ligne
  collée disparaissent du profil ; le nom, les notes et l'identifiant survivent.

### Contrat à ne pas casser

L'application définit `applicationName` mais **pas** `organizationName` :
`QStandardPaths` insérerait sinon un niveau supplémentaire et le dossier de
données ne serait plus `%APPDATA%\LlamaBuilder`. Elle ne définit pas non plus
`applicationDisplayName`, que Qt concaténerait au titre de la fenêtre.

## Validation exécutée

- **169 cas de test au vert** (`ctest --preset debug` et `--preset release`) :
  53 + 55 + 22 + 17 + 22.
- Compilation **sans aucun avertissement** en `/W4 /permissive-`, en Debug comme
  en Release. Les en-têtes Qt sont traités comme externes ; `C4702`, émis depuis
  `qvariant.h` et `qjsengine.h` à la génération de code, est désactivé.
- **`qmllint` sans aucun diagnostic** sur les 20 fichiers QML
  (`cmake --build build/msvc --config Debug --target all_qmllint`).
- **Jauges confrontées à `nvidia-smi`, un `llama-server` chargé** (Qwen3 27B Q5,
  `-ngl 99 -c 2048`) : `llamabuilder-cli monitor --pid <PID>` rapporte
  `18937 / 24463 Mio`, `nvidia-smi` rapporte `18938 / 24463 Mio` — **1 Mio
  d'écart**, très en deçà des 200 Mo du critère d'acceptation n°4. La jauge passe
  en ambre à 77 %, conformément au seuil de 75 % du §5.7.
- Chemin « sans pilote NVIDIA » exercé sur une machine qui en possède un, en
  passant un nom de bibliothèque inexistant : jauge RAM intacte, message affiché,
  aucun plantage → critère d'acceptation n°6.
- Application lancée : aucun avertissement QML à l'exécution, profil réel chargé,
  commande affichée identique à celle du harnais console, sections engendrées
  depuis `params.json`, badge « modifié » et compteurs par section corrects.
- Commande générée pour un modèle dont le chemin contient des espaces, copiée
  telle quelle et exécutée par `cmd.exe` : `llama-server` journalise
  `loading model 'C:\dev\llama_desktop_modeles\qwen3 27b q5.gguf'`, puis
  `model loaded` et `listening on http://127.0.0.1:8099`. Aucun flag rejeté.
  → critère d'acceptation n°2 tenu.
- Repli sur la ressource embarquée vérifié en retirant le `params.json` voisin.
- **Import exercé de bout en bout dans l'interface** sur une ligne écrite comme on
  les trouve en ligne — formes longues, `--ctx-size=16384`, `-fa` sans valeur,
  chemin cité contenant des espaces, `--mlock` inconnu du registre. Les
  8 paramètres attendus sont reconnus, le chemin cité survit, le nom du profil est
  déduit du fichier de modèle, et le profil écrit dans `%APPDATA%` contient
  exactement les 8 clés avec `--mlock` en arguments libres. Le profil de test a
  ensuite été retiré du dépôt de profils.

### Artefacts de test créés hors du dépôt (supprimables)

- `%APPDATA%\LlamaBuilder\settings.json` et `profiles.json` (profil « Mon Qwen —
  chemin avec espaces »).
- `C:\dev\llama_desktop_modeles\` : deux **liens matériels** vers des GGUF de
  `C:\dev\llama-cpp\models` (aucun espace disque consommé). C'est aussi le
  `defaultModelsDir` de `settings.json`.

## Compiler et lancer

> Procédures complètes, variables d'environnement, emplacements des données et
> dépannage : [`RUNBOOK.md`](RUNBOOK.md).

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

## Prochaine étape — phase 4

`LlamaRunner`, panneau de logs, « Arrêter », ouverture du navigateur, bandeaux
d'avertissement. `MonitorController::setTrackedPid()` est déjà en place et testé :
il suffira de lui passer le PID du `QProcess`. Le parcours « coller une commande
trouvée en ligne, puis la lancer » sera alors complet.

Deux arbitrages de la phase 1 restent ouverts, décrits plus haut : la section
« Échantillonnage » visible pour `llama-server` contre le §5.3, et `-lm` en
remplacement de `--mlock` / `--no-mmap`.
