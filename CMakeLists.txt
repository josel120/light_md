cmake_minimum_required(VERSION 3.16)
project(light_md VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport Network LinguistTools)

qt_standard_project_setup()

include(cmake/Spelling.cmake)

# Everything except main() lives in a small library, so the unit tests can use it too.
set(LIGHT_MD_CORE_SOURCES
    src/editor.cpp
    src/editor.h
    src/export.cpp
    src/export.h
    src/findbar.cpp
    src/findbar.h
    src/formatting.cpp
    src/formatting.h
    src/highlighter.cpp
    src/highlighter.h
    src/mainwindow.cpp
    src/mainwindow.h
    src/session.cpp
    src/spellcheck.cpp
    src/spellcheck.h
    src/tabs.cpp
    src/theme.cpp
    src/theme.h
    src/updates.cpp
)
qt_add_library(light_md_core STATIC ${LIGHT_MD_CORE_SOURCES})
target_include_directories(light_md_core PUBLIC src)
target_link_libraries(light_md_core PUBLIC Qt6::Widgets Qt6::PrintSupport Qt6::Network)
target_link_libraries(light_md_core PRIVATE light_md_hunspell)
target_compile_definitions(light_md_core PUBLIC LIGHT_MD_VERSION="${PROJECT_VERSION}")

qt_add_executable(light_md
    src/main.cpp
)

# Interface translations (translations/*.ts), built into the app and loaded at startup by language.
# After changing texts in the code, "cmake --build build --target update_translations" refreshes the .ts files.
# Almost all texts are in light_md_core, so it has to be scanned too: before Qt 6.7 only the
# executable's own sources are, and every other text would be marked as gone.
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.7)
    set(LIGHT_MD_TRANSLATED SOURCE_TARGETS light_md light_md_core)
else()
    set(LIGHT_MD_TRANSLATED SOURCES src/main.cpp ${LIGHT_MD_CORE_SOURCES})
endif()
qt_add_translations(light_md
    ${LIGHT_MD_TRANSLATED}
    TS_FILES
        translations/light_md_de.ts
        translations/light_md_es.ts
        translations/light_md_fr.ts
        translations/light_md_pt.ts
    RESOURCE_PREFIX "/i18n"
)

# The icon shown on the window (Qt resource) and on the .exe in File Explorer (Windows resource).
qt_add_resources(light_md "icons"
    PREFIX "/"
    FILES icons/light_md.png
)
if(WIN32)
    target_sources(light_md PRIVATE icons/light_md.rc)
endif()

# macOS app bundle: icon, version and the .md file association (Info.plist).
if(APPLE)
    target_sources(light_md PRIVATE icons/light_md.icns)
    set_source_files_properties(icons/light_md.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    # The dictionaries go inside the app, in Contents/Resources/dictionaries.
    target_sources(light_md PRIVATE ${LIGHT_MD_DICTIONARIES})
    set_source_files_properties(${LIGHT_MD_DICTIONARIES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/dictionaries)
    set_target_properties(light_md PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in
        MACOSX_BUNDLE_BUNDLE_NAME light_md
        MACOSX_BUNDLE_GUI_IDENTIFIER io.github.josel120.light_md
        MACOSX_BUNDLE_ICON_FILE light_md.icns
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    )
endif()

target_link_libraries(light_md PRIVATE light_md_core)

set_target_properties(light_md PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# Unit tests (Qt Test): cmake -DLIGHT_MD_TESTS=ON, then ctest. See tests/.
option(LIGHT_MD_TESTS "Build the unit tests" OFF)
if(LIGHT_MD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# "cmake --install build --prefix dist" creates a folder ready to copy
# to another computer, with the Qt libraries included where deploy supports it.
install(TARGETS light_md BUNDLE DESTINATION . RUNTIME DESTINATION bin)
# Spell checking dictionaries, next to the program (inside the app on macOS, see above).
if(WIN32)
    install(FILES ${LIGHT_MD_DICTIONARIES} DESTINATION bin/dictionaries)
elseif(UNIX AND NOT APPLE)
    install(FILES ${LIGHT_MD_DICTIONARIES} DESTINATION share/light_md/dictionaries)
endif()

if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/light_md.desktop
            DESTINATION share/applications)
    install(FILES packaging/linux/light_md.svg
            DESTINATION share/icons/hicolor/scalable/apps)
endif()

# Bundle Qt next to the binary on Windows and macOS portable/app installs.
# Linux packages link against system Qt (see packaging/arch/PKGBUILD).
if(WIN32 OR APPLE)
    qt_generate_deploy_app_script(
        TARGET light_md
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()
