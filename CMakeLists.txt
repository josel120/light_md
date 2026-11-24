cmake_minimum_required(VERSION 3.16)
project(light_md VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

qt_standard_project_setup()

qt_add_executable(light_md
    src/main.cpp
    src/theme.cpp
    src/theme.h
)

target_include_directories(light_md PRIVATE src)
target_link_libraries(light_md PRIVATE Qt6::Widgets Qt6::PrintSupport)
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
