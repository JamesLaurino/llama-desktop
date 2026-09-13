#pragma once

#include <QString>

namespace core::AppPaths {

/// %APPDATA%\LlamaBuilder (§4.3).
///
/// Contrat : l'application définit `applicationName` mais **pas**
/// `organizationName`. QStandardPaths insère sinon un niveau supplémentaire
/// (%APPDATA%\<organisation>\<application>) et le dossier de données change
/// silencieusement d'emplacement.
QString dataDir();

QString profilesFile();
QString settingsFile();

/// Emplacement effectif de params.json, dans cet ordre de priorité :
///   1. %APPDATA%\LlamaBuilder\params.json  (surcharge utilisateur)
///   2. <dossier de l'exécutable>\params.json
///   3. :/resources/params.json             (copie embarquée, toujours présente)
///
/// La surcharge sur disque est ce qui permet d'ajouter un flag llama.cpp sans
/// recompiler (§6).
QString resolveParamsFile();

} // namespace core::AppPaths
