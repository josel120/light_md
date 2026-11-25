find_package(Qt6 REQUIRED COMPONENTS Test)

# One executable per area; "ctest --test-dir build --output-on-failure" runs them all.
foreach(name export findbar formatting highlighter session spellcheck)
    qt_add_executable(tst_${name} tst_${name}.cpp)
    target_link_libraries(tst_${name} PRIVATE light_md_core Qt6::Test)
    add_test(NAME ${name} COMMAND tst_${name})
    # No screen needed: runs the same on a desktop and in CI.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

# The spelling tests use the dictionaries downloaded into the build folder.
target_compile_definitions(tst_spellcheck PRIVATE LIGHT_MD_TEST_DICTIONARIES="${LIGHT_MD_DICTIONARY_DIR}")
