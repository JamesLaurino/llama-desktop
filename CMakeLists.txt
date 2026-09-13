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
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Qt 6 est lié dynamiquement : l'édition open source est sous LGPLv3 (§2).
find_package(Qt6 6.7 REQUIRED COMPONENTS Core)
qt_standard_project_setup()

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

# --- Harnais console de la phase 1 -------------------------------------------
qt_add_executable(llamabuilder-cli src/cli/main.cpp)
target_link_libraries(llamabuilder-cli PRIVATE llamabuilder_core)
set_target_properties(llamabuilder-cli PROPERTIES WIN32_EXECUTABLE OFF)

# Le fichier de données livré est copié à côté de l'exécutable : c'est ainsi
# qu'on l'édite sans recompiler.
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
