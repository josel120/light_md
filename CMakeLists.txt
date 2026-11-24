cmake_minimum_required(VERSION 3.16)
project(light_md VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

qt_standard_project_setup()

qt_add_executable(light_md
    src/main.cpp
)

# The icon shown on the window (Qt resource) and on the .exe in File Explorer (Windows resource).
qt_add_resources(light_md "icons"
    PREFIX "/"
    FILES icons/light_md.png
)
if(WIN32)
    target_sources(light_md PRIVATE icons/light_md.rc)
endif()

target_link_libraries(light_md PRIVATE Qt6::Widgets Qt6::PrintSupport)

set_target_properties(light_md PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# "cmake --install build --prefix dist" creates a folder ready to copy
# to another computer, with the Qt libraries included.
install(TARGETS light_md BUNDLE DESTINATION . RUNTIME DESTINATION bin)

qt_generate_deploy_app_script(
    TARGET light_md
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
