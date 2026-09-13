cmake_minimum_required(VERSION 3.21)

project(LlamaBuilder
    VERSION 0.1.0
    DESCRIPTION "Générateur de commandes llama.cpp"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    # /utf-8 est indispensable : les libellés et info-bulles sont en français.
    add_compile_options(/utf-8 /W4 /permissive- /EHsc)
    # Les en-têtes Qt sont inclus avec <> : /W4 ne doit pas rapporter leurs
    # avertissements, seuls les nôtres comptent.
    # C4702 est rapporté depuis qvariant.h et qjsengine.h : /external ne le filtre
    # pas, la génération de code attribue l'avertissement après coup.
    add_compile_options(/external:anglebrackets /external:W0 /wd4702)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Qt 6 est lié dynamiquement : l'édition open source est sous LGPLv3 (§2).
find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.7)

# Les fichiers QML vivent dans qml/ : un qmldir est engendré pour ce dossier.
if (QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

# --- Noyau : aucune dépendance UI, aucun type Qt Quick -----------------------
qt_add_library(llamabuilder_core STATIC
    src/core/AppPaths.cpp
    src/core/AppPaths.h
    src/core/CommandBuilder.cpp
    src/core/CommandBuilder.h
    src/core/JsonFile.cpp
    src/core/JsonFile.h
    src/core/ParamRegistry.cpp
    src/core/ParamRegistry.h
    src/core/Profile.cpp
    src/core/Profile.h
    src/core/ProfileStore.cpp
    src/core/ProfileStore.h
    src/core/Settings.cpp
    src/core/Settings.h
    src/core/SettingsStore.cpp
    src/core/SettingsStore.h
)

target_link_libraries(llamabuilder_core PUBLIC Qt6::Core)
target_include_directories(llamabuilder_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# params.json est embarqué comme filet de sécurité ; une copie posée dans
# %APPDATA%\LlamaBuilder ou à côté de l'exécutable la remplace (§6).
qt_add_resources(llamabuilder_core "llamabuilder_data"
    PREFIX "/"
    FILES resources/params.json
)

# --- Couche de présentation ---------------------------------------------------
# Modèles et façade exposés au QML. Ces sources sont compilées dans la cible qui
# porte le module QML : c'est ce qui permet à qmltyperegistrar de publier les
# types et donc à qmllint d'analyser statiquement les fichiers QML. Les tests
# compilent la même liste, sans module QML (les macros y sont alors inertes).
set(LLAMABUILDER_UI_SOURCES
    src/ui/AppController.cpp
    src/ui/AppController.h
    src/ui/Fonts.cpp
    src/ui/Fonts.h
    src/ui/ParamFilterModel.cpp
    src/ui/ParamFilterModel.h
    src/ui/ParamFormModel.cpp
    src/ui/ParamFormModel.h
    src/ui/ProfileListModel.cpp
    src/ui/ProfileListModel.h
)

# Chemins absolus : la même liste est réutilisée depuis tests/.
list(TRANSFORM LLAMABUILDER_UI_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

# --- Application Qt Quick -----------------------------------------------------
qt_add_executable(llamabuilder src/app/main.cpp ${LLAMABUILDER_UI_SOURCES})
target_link_libraries(llamabuilder PRIVATE llamabuilder_core Qt6::Quick Qt6::QuickControls2)

# qmltyperegistrar engendre des #include <AppController.h> sans le préfixe de
# dossier : src/ui doit donc être atteignable directement. Notre propre code
# continue d'écrire "ui/AppController.h".
target_include_directories(llamabuilder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/ui)

# Inter et JetBrains Mono sont embarquées (§5.7) : l'apparence ne dépend pas des
# polices installées sur la machine. Licences OFL dans resources/fonts/.
qt_add_resources(llamabuilder "llamabuilder_fonts"
    PREFIX "/"
    FILES
        resources/fonts/Inter-Regular.ttf
        resources/fonts/Inter-Medium.ttf
        resources/fonts/Inter-SemiBold.ttf
        resources/fonts/JetBrainsMono-Regular.ttf
)

# Console conservée en Debug pour voir les avertissements QML ; masquée en Release.
set_target_properties(llamabuilder PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(llamabuilder
    URI LlamaBuilder
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/CommandBar.qml
        qml/ConfirmDialog.qml
        qml/FlatButton.qml
        qml/InfoTip.qml
        qml/NamePrompt.qml
        qml/ParamRow.qml
        qml/ParamSection.qml
        qml/ProfilePanel.qml
        qml/Segmented.qml
        qml/SettingsWindow.qml
        qml/ThemedCheckBox.qml
        qml/ThemedComboBox.qml
        qml/ThemedDialog.qml
        qml/ThemedMenu.qml
        qml/ThemedTextField.qml
)

add_custom_command(TARGET llamabuilder POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/params.json
            $<TARGET_FILE_DIR:llamabuilder>/params.json
    COMMENT "Copie de params.json à côté de l'application"
)

# --- Harnais console (diagnostic du noyau) -----------------------------------
qt_add_executable(llamabuilder-cli src/cli/main.cpp)
target_link_libraries(llamabuilder-cli PRIVATE llamabuilder_core)
set_target_properties(llamabuilder-cli PROPERTIES WIN32_EXECUTABLE OFF)

add_custom_command(TARGET llamabuilder-cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/params.json
            $<TARGET_FILE_DIR:llamabuilder-cli>/params.json
    COMMENT "Copie de params.json à côté de l'exécutable"
)

# --- Tests --------------------------------------------------------------------
include(CTest)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif()
