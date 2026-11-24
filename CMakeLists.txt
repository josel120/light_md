cmake_minimum_required(VERSION 3.16)
project(light_md VERSION 1.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport Network)

qt_standard_project_setup()

qt_add_executable(light_md
    src/main.cpp
    src/theme.cpp
    src/theme.h
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
    set_target_properties(light_md PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in
        MACOSX_BUNDLE_BUNDLE_NAME light_md
        MACOSX_BUNDLE_GUI_IDENTIFIER io.github.josel120.light_md
        MACOSX_BUNDLE_ICON_FILE light_md.icns
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    )
endif()

target_include_directories(light_md PRIVATE src)
target_link_libraries(light_md PRIVATE Qt6::Widgets Qt6::PrintSupport Qt6::Network)
target_compile_definitions(light_md PRIVATE LIGHT_MD_VERSION="${PROJECT_VERSION}")

set_target_properties(light_md PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# "cmake --install build --prefix dist" creates a folder ready to copy
# to another computer, with the Qt libraries included where deploy supports it.
install(TARGETS light_md BUNDLE DESTINATION . RUNTIME DESTINATION bin)

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
