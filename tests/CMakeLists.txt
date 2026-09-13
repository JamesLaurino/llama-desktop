find_package(Qt6 6.7 REQUIRED COMPONENTS Test)

function(llamabuilder_add_test name source)
    qt_add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE llamabuilder_core Qt6::Test)
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE OFF)
    # Le test valide le params.json réellement livré, pas une copie.
    target_compile_definitions(${name} PRIVATE
        LLAMABUILDER_PARAMS_JSON="${CMAKE_SOURCE_DIR}/resources/params.json")
    add_test(NAME ${name} COMMAND ${name})
endfunction()

llamabuilder_add_test(test_commandbuilder test_commandbuilder.cpp)
llamabuilder_add_test(test_store test_store.cpp)
