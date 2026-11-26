find_package(Qt6 REQUIRED COMPONENTS Test)

# One executable per area; "ctest --test-dir build --output-on-failure" runs them all.
foreach(name export findbar formatting highlighter outline session spellcheck)
    qt_add_executable(tst_${name} tst_${name}.cpp)
    target_link_libraries(tst_${name} PRIVATE light_md_core Qt6::Test)
    # Also written to tests/<name>.log, which CI prints when a test fails or hangs.
    add_test(NAME ${name} COMMAND tst_${name} -o ${name}.log,txt -o -,txt)
    # No screen needed: runs the same on a desktop and in CI.
    # A stuck test (a dialog nobody answers) fails after 5 minutes instead of hanging CI for hours.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300)
endforeach()

# The spelling tests use the dictionaries downloaded into the build folder.
target_compile_definitions(tst_spellcheck PRIVATE LIGHT_MD_TEST_DICTIONARIES="${LIGHT_MD_DICTIONARY_DIR}")
