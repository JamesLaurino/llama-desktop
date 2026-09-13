# État du projet — LlamaBuilder

> Cahier des charges : [`mardown/llama-builder-spec.md`](mardown/llama-builder-spec.md)

## Avancement

| Phase | Périmètre | État |
|---|---|---|
| **1** | Noyau sans UI : `Profile`, `Settings`, `ProfileStore`, `ParamRegistry`, `CommandBuilder`, tests | **terminée** |
| 2 | Coque Qt Quick : thème, liste de profils, formulaire dynamique, barre de commande | à faire |
| 3 | Monitoring : `NvmlMonitor`, `SystemMonitor`, jauges animées | à faire |
| 4 | Exécution : `LlamaRunner`, panneau de logs, validation | à faire |
| 5 | Finitions : recherche, duplication, géométrie, raccourcis | à faire |

## Environnement retenu

| Élément | Choix effectif |
|---|---|
| Compilateur | MSVC 14.51 (Visual Studio 18 Community) |
| CMake / Ninja | 4.3.1 / 1.13.2, fournis par Visual Studio |
| Qt | **6.10.3 `msvc2022_64`**, installé dans `C:\Qt` via `aqtinstall` |
| llama.cpp de référence | build **b10586** (`C:\dev\llama-cpp\llama-b10586-bin-win-cuda-13.3-x64`) |
| GPU | RTX 5090 Laptop, 24 Go — `nvml.dll` présent dans `System32` |

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
  vérité (vérifié par un test).
- **Deux fichiers ajoutés au noyau** par rapport au §3 : `JsonFile` (lecture et
  écriture atomiques, mise à l'écart des fichiers corrompus, mutualisée entre les
  deux dépôts) et `AppPaths` (résolution des emplacements).

### Corrections apportées à `params.json` contre le binaire réel

`-c` défaut 0 (et non 4096) · `-b` 2048 · `-ub` 512 · `-ngl` défaut `auto` ·
`-ctk`/`-ctv` étendus à 9 valeurs · `-fa` confirmé `on|off|auto`.
Paramètres récents ajoutés : `-ncmoe`, `-cmoe`, `--swa-full`, `-cram`, `-fit`,
`-fitt`, `-sm`, `--context-shift`, `--no-webui`, `--metrics`, `--check-tensors`.

## Phase 1 — ce qui est livré

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

### Contrat à ne pas casser

L'application définit `applicationName` mais **pas** `organizationName` :
`QStandardPaths` insérerait sinon un niveau supplémentaire et le dossier de
données ne serait plus `%APPDATA%\LlamaBuilder`.

## Validation exécutée

- 75 cas de test au vert (`ctest --preset debug`), compilation sans avertissement
  en `/W4 /permissive-`.
- Commande générée pour un modèle situé dans `C:\dev\mes modèles\` (espace +
  accent), copiée telle quelle et exécutée par `cmd.exe` : `llama-server`
  journalise `loading model 'C:\dev\mes modèles\qwen3 27b q5.gguf'`, puis
  `model loaded` et `listening on http://127.0.0.1:8099`. Aucun flag rejeté.
  → critère d'acceptation n°2 tenu.
- Repli sur la ressource embarquée vérifié en retirant le `params.json` voisin.

### Artefacts de test créés hors du dépôt (supprimables)

- `%APPDATA%\LlamaBuilder\settings.json` et `profiles.json` (profil « Mon Qwen —
  chemin avec espaces »).
- `C:\dev\mes modèles\` : deux **liens matériels** vers des GGUF de
  `C:\dev\llama-cpp\models` (aucun espace disque consommé).

## Compiler

```powershell
.\scripts\build.ps1              # Debug + tests
.\scripts\build.ps1 -Config Release
.\scripts\build.ps1 -Clean
```

Les chemins de Visual Studio et de Qt sont découverts automatiquement
(`vswhere`, glob sur `C:\Qt`). Surcharges : `$env:VS_DIR`, `$env:QT_DIR`.

## Prochaine étape — phase 2

Coque Qt Quick : fenêtre et thème du §5.7, `ProfileListModel` et
`ParamFormModel` exposés au QML, sections repliables générées depuis
`params.json`, info-bulles, barre de commande avec bouton Copier. Nécessitera
d'ajouter `Qt6::Quick` et `Qt6::QuickControls2` au `CMakeLists.txt`.
