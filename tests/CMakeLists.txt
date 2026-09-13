find_package(Qt6 6.7 REQUIRED COMPONENTS Test)

function(llamabuilder_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    qt_add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test llamabuilder_core ${ARG_LIBS})
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE OFF)
    # Le test valide le params.json réellement livré, pas une copie.
    target_compile_definitions(${name} PRIVATE
        LLAMABUILDER_PARAMS_JSON="${CMAKE_SOURCE_DIR}/resources/params.json")
    add_test(NAME ${name} COMMAND ${name})
endfunction()

llamabuilder_add_test(test_commandbuilder SOURCES test_commandbuilder.cpp)
llamabuilder_add_test(test_commandparser SOURCES test_commandparser.cpp)
llamabuilder_add_test(test_runner SOURCES test_runner.cpp)
llamabuilder_add_test(test_store SOURCES test_store.cpp)

# Les sources de présentation sont recompilées ici : elles vivent dans la cible
# de l'application, qui porte le module QML. Aucune bibliothèque à partager.
llamabuilder_add_test(test_uimodels
    SOURCES test_uimodels.cpp ${LLAMABUILDER_UI_SOURCES}
    LIBS llamabuilder_monitor Qt6::Gui Qt6::Qml)

llamabuilder_add_test(test_monitor
    SOURCES test_monitor.cpp ${CMAKE_SOURCE_DIR}/src/ui/MonitorController.cpp
            ${CMAKE_SOURCE_DIR}/src/ui/MonitorController.h
    LIBS llamabuilder_monitor Qt6::Qml)
