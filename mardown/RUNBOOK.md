# RUNBOOK — LlamaBuilder

Comment compiler, lancer, tester et dépanner. Pour l'état d'avancement, voir
[`PROJECT_STATE.md`](PROJECT_STATE.md) ; pour le périmètre fonctionnel,
[`llama-builder-spec.md`](llama-builder-spec.md).

---

## 1. Prérequis

| Élément | Attendu | Comment c'est trouvé |
|---|---|---|
| Compilateur | **Build Tools 2022** (pas l'IDE), charge de travail `VCTools` + composant `VC.CMake.Project` | `vswhere -latest` |
| CMake / Ninja | fournis par les Build Tools | `Common7\IDE\CommonExtensions\Microsoft\CMake\` |
| Qt | **6.10.3 `msvc2022_64`** sous `C:\Qt` | glob `C:\Qt\6.*.*\msvc*_64`, version la plus récente |
| llama.cpp | build **b10586** CUDA (référence) | déclaré dans les Réglages de l'application |

Rien n'est codé en dur, sauf `CMAKE_PREFIX_PATH` dans `../CMakePresets.json`
(`C:/Qt/6.10.3/msvc2022_64`) — à corriger si tu changes de version de Qt.

Surcharges : `$env:VS_DIR`, `$env:QT_DIR` (doit contenir `bin\qmake.exe`).

Le compilateur n'a **pas** besoin de Visual Studio : les Build Tools fournissent
`cl.exe`, le SDK Windows, CMake et Ninja sans aucun IDE, et `vswhere` les liste
comme n'importe quelle installation. Pour les (ré)installer :

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact `
  --override "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended"
```

Le composant `VC.CMake.Project` n'est pas facultatif : c'est lui qui pose
`cmake.exe` et `ninja.exe` là où `dev-env.ps1` les cherche.

---

## 2. Charger l'environnement — une fois par terminal

```powershell
cd C:\dev\llama-wrapper
. .\scripts\dev-env.ps1
```

Le point initial est **obligatoire** : le script doit modifier la session
courante, pas un sous-processus. Il exécute `vcvars64.bat` et rapatrie son
environnement, puis ajoute au `PATH` CMake, Ninja et `$QT_DIR\bin`.

Il affiche ce qu'il a trouvé — vérifie ces cinq lignes en cas de doute :

```
Visual Studio : C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools
Qt            : C:\Qt\6.10.3\msvc2022_64
cl            : ...\VC\Tools\MSVC\14.44.35207\bin\HostX64\x64\cl.exe
cmake         : ...\CMake\bin\cmake.exe
ninja         : ...\Ninja\ninja.exe
```

Sans ce script, l'exécutable **compile mais ne démarre pas** : les DLL Qt ne
sont pas sur le `PATH` et Windows n'affiche aucun message.

---

## 3. Compiler

```powershell
.\scripts\build.ps1                      # Debug + les 199 tests
.\scripts\build.ps1 -Config Release
.\scripts\build.ps1 -NoTests             # compile seulement
.\scripts\build.ps1 -Clean               # supprime build\msvc puis reconfigure
```

Le script sourçant lui-même `dev-env.ps1`, il fonctionne aussi dans un terminal
neuf. Il s'arrête au premier échec en indiquant l'étape.

### Équivalents CMake directs

```powershell
cmake --preset msvc                      # configuration (Ninja Multi-Config)
cmake --build --preset debug
cmake --build --preset release
ctest --preset debug                     # --output-on-failure est déjà dans le preset
```

Générateur **Ninja Multi-Config** : une seule configuration CMake produit
`build\msvc\Debug\` et `build\msvc\Release\`. Inutile de reconfigurer pour
changer de configuration.

### Cibles utiles

```powershell
cmake --build build\msvc --config Debug --target all_qmllint    # analyse statique des 21 QML
cmake --build build\msvc --config Debug --target llamabuilder-cli
```

`all_qmllint` doit rendre **zéro diagnostic**. C'est la seule barrière qui
détecte un accès non qualifié ou une propriété QML inexistante — le compilateur
C++ n'en sait rien.

---

## 4. Lancer

```powershell
.\build\msvc\Debug\llamabuilder.exe        # console conservée : avertissements QML visibles
.\build\msvc\Release\llamabuilder.exe      # sans console (WIN32_EXECUTABLE)
```

L'application ne prend **aucun argument de ligne de commande**. Tout se règle
dans l'interface, et persiste dans `%APPDATA%\LlamaBuilder\`.

### Variables d'environnement Qt utiles au diagnostic

| Variable | Effet |
|---|---|
| `QT_LOGGING_RULES="qt.qml.binding.removal.info=true"` | signale les liaisons QML écrasées par une affectation |
| `QSG_INFO=1` | backend graphique, GPU, taux de rafraîchissement retenus |
| `QT_SCALE_FACTOR=1` | force l'échelle à 1 pour tester hors 200 % |
| `QT_QUICK_CONTROLS_STYLE=Basic` | déjà imposé dans `main.cpp` ; ne pas changer, les contrôles sont redécorés pour ce style |

```powershell
$env:QSG_INFO = 1; .\build\msvc\Debug\llamabuilder.exe
```

---

## 5. Emplacements des données

Créés au premier lancement, **hors du dépôt**.

```
%APPDATA%\LlamaBuilder\
    profiles.json      les profils (écrit avec 500 ms d'anti-rebond)
    settings.json      les réglages globaux
    params.json        (facultatif) surcharge du registre de paramètres
```

Repartir de zéro : ferme l'application et supprime le dossier.

```powershell
explorer "$env:APPDATA\LlamaBuilder"
Remove-Item "$env:APPDATA\LlamaBuilder" -Recurse    # remise à zéro complète
```

Un fichier JSON illisible n'est jamais écrasé : il est déplacé en
`profiles.corrupt-20260913T142200Z.json` et l'application démarre vide en l'annonçant.

### `settings.json`

```json
{
  "llamaServerPath": "C:/dev/llama-cpp/llama-b10586-bin-win-cuda-13.3-x64/llama-server.exe",
  "llamaCliPath":    "C:/dev/llama-cpp/llama-b10586-bin-win-cuda-13.3-x64/llama-cli.exe",
  "defaultModelsDir": "C:/dev/llama_desktop_modeles",
  "monitorIntervalMs": 1000,
  "theme": "dark"
}
```

Tant que `llamaServerPath` est vide ou pointe sur un fichier absent, **Lancer**
reste désactivé et la barre de commande affiche pourquoi.

### Résolution de `params.json`

Par ordre de priorité :

1. `%APPDATA%\LlamaBuilder\params.json` — surcharge utilisateur
2. `<dossier de l'exécutable>\params.json` — copié par le POST_BUILD
3. `:/resources/params.json` — copie embarquée, toujours présente

C'est ce qui permet **d'ajouter un flag llama.cpp sans recompiler** : copie
`resources\params.json` dans `%APPDATA%\LlamaBuilder\`, édite, relance.

```powershell
Copy-Item resources\params.json "$env:APPDATA\LlamaBuilder\"
.\build\msvc\Debug\llamabuilder-cli.exe params      # valide la surcharge avant de lancer l'UI
```

Le chemin retenu est affiché en tête de `llamabuilder-cli params` et dans la
fenêtre Réglages.

Un paramètre déclare un `flag` — la forme que l'application **écrit** — et
facultativement `aliases`, les autres écritures qu'elle **accepte à l'import**
(`"flag": "-c"`, `"aliases": ["--ctx-size"]`). Pour un `tristate`, `flagOff` et
`aliasesOff` jouent le même rôle du côté négatif. Deux paramètres ne peuvent pas
revendiquer la même écriture : le fichier serait refusé, avec le nom du coupable.

---

## 6. Harnais console — diagnostic du noyau sans UI

```powershell
.\build\msvc\Debug\llamabuilder-cli.exe params           # valide params.json, liste sections et flags
.\build\msvc\Debug\llamabuilder-cli.exe list             # profils enregistrés
.\build\msvc\Debug\llamabuilder-cli.exe show "Mon Qwen"  # commande d'un profil (nom partiel ou id)
.\build\msvc\Debug\llamabuilder-cli.exe demo             # profil d'exemple : espaces, enum, tristate
.\build\msvc\Debug\llamabuilder-cli.exe monitor          # un relevé RAM / VRAM
.\build\msvc\Debug\llamabuilder-cli.exe monitor --pid 14260   # + la part d'un processus
.\build\msvc\Debug\llamabuilder-cli.exe parse "<ligne>"  # relit une ligne et la régénère
.\build\msvc\Debug\llamabuilder-cli.exe run "Mon Qwen"   # lance le profil et diffuse son journal
```

Options : `--params-file <chemin>` pour tester un registre sans toucher à
`%APPDATA%` ; `--pid N` pour isoler un processus.

Codes de sortie : `0` succès · `1` usage · `2` `params.json` inutilisable ·
`3` profil introuvable · `4` ligne de commande inexploitable ·
`5` processus impossible à démarrer. Sinon, `run` rend **le code de sortie du
processus lancé**.

`run` utilise le même `LlamaRunner` que l'interface : c'est le moyen de vérifier
l'exécution sans passer par le QML. Ctrl+C atteint tout le groupe de console,
l'enfant le reçoit donc aussi.

`parse` est le pendant de `show` : l'un génère, l'autre relit. Il imprime le
binaire déduit, le modèle, les paramètres reconnus, les arguments libres, les
avertissements, puis **la commande régénérée** — qui doit être équivalente à celle
donnée.

**Piège PowerShell 5.1** : passé à un exécutable natif, PowerShell supprime les
guillemets internes d'une chaîne. Un chemin contenant des espaces arriverait donc
découpé. Il faut les échapper :

```powershell
.\build\msvc\Debug\llamabuilder-cli.exe parse 'llama-server.exe -m \"D:/mes modeles/q.gguf\" -c 16384'
```

Ce n'est qu'une contrainte du terminal : le dialogue « Importer » de l'interface
reçoit le texte tel quel, sans échappement.

`monitor` imprime aussi les valeurs en **Mio**, unité de `nvidia-smi` : c'est ce
qui rend le critère d'acceptation n°4 vérifiable d'un coup d'œil.

```powershell
nvidia-smi --query-gpu=memory.total,memory.used --format=csv,noheader
```

Attention : sous Windows en mode WDDM — donc sur toute GeForce pilotant un écran
— NVML refuse de chiffrer la VRAM **par processus**. `monitor --pid` affiche alors
« non chiffrable (WDDM) », et `nvidia-smi --query-compute-apps` affiche `[N/A]` au
même endroit. Ce n'est pas un défaut de l'application.

`show` et `demo` impriment **les deux** formes produites en une passe par
`CommandBuilder::build()` — la ligne citée collable dans `cmd.exe`, puis les
arguments bruts un par ligne entre crochets. Si les deux divergent, le bug est
dans le noyau, pas dans l'affichage.

---

## 7. Tests

```powershell
ctest --preset debug                     # les 6 binaires, 199 cas
ctest --preset debug -R uimodels         # un seul binaire
ctest --preset debug -V                  # sortie complète
```

| Binaire | Cas | Périmètre |
|---|---|---|
| `test_commandbuilder` | 53 | citation Windows, ordre des arguments, tous les types de paramètres |
| `test_commandparser` | 55 | alias longs, `--x=y`, continuations de ligne, aller-retour, drapeaux ambigus |
| `test_runner` | 23 | découpage du flux, ANSI, journal de llama-server, arrêt et escalade, orphelins |
| `test_store` | 22 | E/S atomiques, anti-rebond, fichiers corrompus, aller-retour JSON |
| `test_uimodels` | 24 | rôles des modèles, filtrage, CRUD, validation, import, journal, exécution |
| `test_monitor` | 22 | seuils aux bornes, diviseur 1024³, NVML absente, sentinelle WDDM, fil dédié |

Exécution directe, avec les options de QTest :

```powershell
.\build\msvc\Debug\test_commandbuilder.exe -functions
.\build\msvc\Debug\test_commandbuilder.exe argumentOrderFollowsRegistryNotMap
.\build\msvc\Debug\test_uimodels.exe -v2
```

Les tests lisent `resources\params.json` **depuis les sources**
(`LLAMABUILDER_PARAMS_JSON`), jamais la copie embarquée : une correction du
registre est donc couverte immédiatement.

`test_runner` et `test_uimodels` ont besoin d'un vrai processus à surveiller :
ils **se relancent eux-mêmes** comme faux enfant. La bascule se fait sur la
variable d'environnement `LLAMABUILDER_FAKE_CHILD`, posée par le test avant de
lancer l'enfant, qui en hérite. Ne la pose jamais dans ton terminal : le binaire
de test se comporterait en enfant au lieu d'exécuter la suite.

### Ce qu'il faut vérifier à la main dans l'interface

1. **Nouveau profil** → *Parcourir* → un `.gguf`. La coche verte confirme que le
   fichier existe ; le message de validation disparaît.
2. Modifie `-c`, `-ngl`, `-fa` : la commande se recompose à chaque frappe.
   Badge « modifié » = valeur ≠ défaut ; point d'accent = valeur posée égale au
   défaut (émise quand même dans la commande).
3. Bascule **serveur → cli** : la section « Serveur » disparaît entièrement
   (son compteur tombe à zéro). Reviens : les valeurs sont intactes.
4. **Copier**, coller dans `cmd.exe` : doit démarrer même si le chemin du modèle
   contient des espaces. C'est le critère d'acceptation n°2.
5. Vide un champ : le paramètre quitte la commande *et* le profil. Convention
   unique — valeur vide = paramètre non posé.
6. Ferme, relance : tout est rechargé. Il n'y a pas de bouton *Enregistrer*,
   l'anti-rebond de 500 ms écrit pour toi.
7. **Jauges** : colle la commande dans un `cmd.exe`, laisse le modèle se charger,
   et regarde la barre VRAM monter puis virer à l'ambre au-delà de 75 %. Compare
   avec `nvidia-smi`. Minimise la fenêtre : le sondage s'arrête ; restaure-la, un
   relevé arrive immédiatement.
8. Change l'**intervalle** dans les Réglages : pris en compte sans redémarrage.
9. **Importer** : colle une commande trouvée en ligne, en formes longues. L'aperçu
   liste les paramètres reconnus, les avertissements, et la commande que
   l'application produira. *Créer un profil* propose un nom déduit du fichier de
   modèle. Vérifie qu'un drapeau inconnu se retrouve bien en arguments libres.
10. **Copier** puis **Importer** la même commande : le profil obtenu doit produire
    une commande identique. C'est la symétrie des deux sens, vérifiable à l'œil.
11. **Lancer** : le panneau s'ouvre, la commande figure en tête du journal en
    couleur d'accent, les lignes défilent, le statut passe à
    « En cours — PID *n* » et une pastille verte apparaît sur le profil. Les
    jauges affichent « llama sur le GPU » et la part RAM du processus.
12. Sur la ligne `listening on http://…`, le bouton **Ouvrir dans le navigateur**
    apparaît. Avec `--host 0.0.0.0`, il doit ouvrir `127.0.0.1`.
13. **Arrêter** : le bouton devient « Forcer » pendant cinq secondes — `terminate()`
    ne fait rien sur un programme console, le processus est tué à l'échéance. Un
    clic sur « Forcer » n'attend pas. Le statut finit sur « Arrêté. », **sans**
    rouge : un arrêt demandé n'est pas un échec.
14. **Lancer** un autre profil pendant qu'un processus tourne : une confirmation
    propose d'arrêter le précédent, puis le nouveau démarre tout seul.
15. **Fermer la fenêtre** pendant une exécution : la fermeture est refusée et la
    confirmation s'affiche. Après « Arrêter et quitter », vérifie avec
    `Get-Process llama-server` qu'aucun processus ne subsiste.

---

## 8. Dépannage

| Symptôme | Cause | Correctif |
|---|---|---|
| L'exe se ferme sans rien afficher | DLL Qt absentes du `PATH` | `. .\scripts\dev-env.ps1` dans le terminal courant |
| `Aucun Qt 6 MSVC x64 trouvé sous C:\Qt` | Qt ailleurs | `$env:QT_DIR = 'D:\Qt\6.10.3\msvc2022_64'` |
| `Could not find a package configuration file providing Qt6` | `../CMakePresets.json` pointe une version de Qt absente | corriger `CMAKE_PREFIX_PATH`, puis `.\scripts\build.ps1 -Clean` |
| `vswhere.exe introuvable` | Visual Studio non installé, ou seuls les Build Tools | `$env:VS_DIR = '<racine>'` |
| Fenêtre vide, console silencieuse | échec de chargement QML | lancer la version **Debug**, la console porte le message |
| Erreur `module "LlamaBuilder" is not installed` | exécution depuis un dossier sans les métadonnées du module | lancer l'exe depuis `build\msvc\<Config>\`, ne pas le déplacer seul |
| Un flag est rejeté par llama.cpp | `params.json` en avance ou en retard sur le binaire | comparer avec `llama-server --help`, corriger la surcharge dans `%APPDATA%` |
| Les profils ont disparu | JSON corrompu mis de côté | chercher `*.corrupt-*.json` dans `%APPDATA%\LlamaBuilder\` |
| `build.ps1` s'arrête sans erreur visible | `stderr` natif redirigé + `$ErrorActionPreference='Stop'` (PowerShell 5.1) | lancer le script dans un terminal interactif, pas via un outil qui capture `stderr` |

---

## 9. Structure du dépôt

```
CMakeLists.txt CMakePresets.json
scripts/       dev-env.ps1 (environnement) · build.ps1 (compile + tests)
resources/     params.json (47 paramètres, 7 sections) · fonts/ (Inter, JetBrains Mono, OFL)
src/core/      noyau sans UI, Qt6::Core seul — AppPaths CommandBuilder CommandParser
               JsonFile LlamaRunner ParamRegistry Profile ProfileStore Settings
               SettingsStore
src/monitor/   seule cible touchant windows.h et psapi — MonitorSample NvmlLibrary
               NvmlMonitor SystemMonitor MonitorWorker
src/ui/        AppController (façade unique) · ParamFormModel · ParamFilterModel
               ProfileListModel · MonitorController · LogModel · Fonts
src/app/       main.cpp — QGuiApplication + QQmlApplicationEngine
src/cli/       main.cpp — harnais console
qml/           21 fichiers, Theme.qml en singleton
tests/         test_commandbuilder · test_commandparser · test_runner · test_store
               test_uimodels · test_monitor
mardown/       cahier des charges
build/msvc/    Debug\ et Release\ (ignoré par git)
```

Les sept sections engendrées : Modèle & binaire · GPU · Contexte & mémoire ·
CPU · Échantillonnage · Serveur · Avancé.

---

## 10. Contrat à ne pas casser

- **Pas de `organizationName`.** `QStandardPaths` insérerait un niveau
  supplémentaire et le dossier de données cesserait d'être
  `%APPDATA%\LlamaBuilder`, sans aucun message.
- **Pas de `applicationDisplayName`.** Qt le concatène au titre de la fenêtre.
- **Style `Basic` uniquement.** `Fusion` et `Windows` imposent leurs couleurs ;
  chaque contrôle est redécoré pour `Basic`.
- **Aucune couleur en littéral hors de `Theme.qml`.**
- **`../src/core` ne dépend que de `Qt6::Core`** : aucun type Qt Quick ne doit y
  entrer.
