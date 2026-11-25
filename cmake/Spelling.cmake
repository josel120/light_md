# Spell checking: the Hunspell library and the dictionaries light_md ships with.
#
# Hunspell comes from the system when pkg-config finds it (Linux packages), otherwise its
# release is downloaded and built here. The dictionaries (from LibreOffice, pinned to one
# commit) are downloaded into <build>/dictionaries, next to the program, and installed as
# separate files: each keeps its own license (see dictionaries/README.md).

include(FetchContent)
# Unpacked files get the time they were unpacked, so rebuilds after an update are complete.
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(HUNSPELL IMPORTED_TARGET hunspell)
endif()

if(HUNSPELL_FOUND)
    message(STATUS "Spell checking: using the system's Hunspell ${HUNSPELL_VERSION}")
    add_library(light_md_hunspell INTERFACE)
    target_link_libraries(light_md_hunspell INTERFACE PkgConfig::HUNSPELL)
else()
    message(STATUS "Spell checking: building Hunspell 1.7.2")
    FetchContent_Declare(hunspell
        URL https://github.com/hunspell/hunspell/releases/download/v1.7.2/hunspell-1.7.2.tar.gz
        URL_HASH SHA256=11ddfa39afe28c28539fe65fc4f1592d410c1e9b6dd7d8a91ca25d85e9ec65b8
    )
    # Hunspell has no CMakeLists.txt, so this only downloads and unpacks it.
    FetchContent_MakeAvailable(hunspell)
    set(src ${hunspell_SOURCE_DIR}/src/hunspell)
    add_library(light_md_hunspell STATIC
        ${src}/affentry.cxx ${src}/affixmgr.cxx ${src}/csutil.cxx ${src}/filemgr.cxx
        ${src}/hashmgr.cxx ${src}/hunspell.cxx ${src}/hunzip.cxx ${src}/phonet.cxx
        ${src}/replist.cxx ${src}/suggestmgr.cxx
    )
    target_include_directories(light_md_hunspell SYSTEM PUBLIC ${src})
    target_compile_definitions(light_md_hunspell PUBLIC HUNSPELL_STATIC)
    set_target_properties(light_md_hunspell PROPERTIES POSITION_INDEPENDENT_CODE ON)
    # Third-party code: its warnings are not ours to fix.
    if(NOT MSVC)
        target_compile_options(light_md_hunspell PRIVATE -w)
    endif()
endif()

# name | file in the LibreOffice dictionaries repository | SHA-256
set(LIGHT_MD_DICTIONARY_COMMIT 79195b017e6d70151337856c59e7d13d311d6630)
set(LIGHT_MD_DICTIONARY_FILES
    "en_US.aff|en/en_US.aff|e746c882dd6f303c2c46e7452804b9201115a6942cfeb15f18f8edf774d2e24e"
    "en_US.dic|en/en_US.dic|f0b1a234bd178bdd01875b2a392a9647f888b8fe879f79c52aae62c2759b3647"
    "es_ES.aff|es/es_ES.aff|e73a9bf8e1383f4986a5dc9e2fbed49371c0c61f511c626d15586bd433c1cad9"
    "es_ES.dic|es/es_ES.dic|6975dddec3d5d2c676069537bc67b4b5f786c65c5d4cf6703a82acf779ac9ec1"
    "pt_BR.aff|pt_BR/pt_BR.aff|21d8ad2a769a60e17e2b5ea4ef11d4d593a58b9e2a82d642ef82d6a4c5523865"
    "pt_BR.dic|pt_BR/pt_BR.dic|a38bfb26b68ece2834e79fe83e48d5792652970ace12db89d1b9674bf9933183"
    "fr.aff|fr_FR/dictionaries/fr.aff|c176610cd5dc4846806a65ddd029f422d87978bf58f224aa44222662a16a2de5"
    "fr.dic|fr_FR/dictionaries/fr.dic|b78a868e31dd6e373b6c3217969afb898a9acde828a5e7ef97308da42218c88c"
    "de_DE_frami.aff|de/de_DE_frami.aff|646bf3333ac69c23e9d794533ee5241d6f755c359e8fe10a648f87613743d594"
    "de_DE_frami.dic|de/de_DE_frami.dic|4ca3c958b0e5545910999bc246f668840bf8ede3df8e5e6790d05edd5a586c38"
)
option(LIGHT_MD_DOWNLOAD_DICTIONARIES "Download the spell checking dictionaries" ON)
set(LIGHT_MD_DICTIONARY_DIR ${CMAKE_BINARY_DIR}/dictionaries)
file(MAKE_DIRECTORY ${LIGHT_MD_DICTIONARY_DIR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dictionaries/README.md ${LIGHT_MD_DICTIONARY_DIR}/README.md COPYONLY)
set(LIGHT_MD_DICTIONARIES ${LIGHT_MD_DICTIONARY_DIR}/README.md)
foreach(entry IN LISTS LIGHT_MD_DICTIONARY_FILES)
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 name)
    list(GET parts 1 remote)
    list(GET parts 2 hash)
    set(target ${LIGHT_MD_DICTIONARY_DIR}/${name})
    if(NOT EXISTS ${target} AND LIGHT_MD_DOWNLOAD_DICTIONARIES)
        message(STATUS "Downloading dictionary ${name}")
        file(DOWNLOAD
            https://raw.githubusercontent.com/LibreOffice/dictionaries/${LIGHT_MD_DICTIONARY_COMMIT}/${remote}
            ${target}.part EXPECTED_HASH SHA256=${hash} STATUS status)
        list(GET status 0 code)
        if(code EQUAL 0)
            file(RENAME ${target}.part ${target})
        else()
            file(REMOVE ${target}.part)
            message(WARNING "Could not download the dictionary ${name}; spell checking in that language "
                            "will need a system dictionary (/usr/share/hunspell).")
        endif()
    endif()
    if(EXISTS ${target})
        list(APPEND LIGHT_MD_DICTIONARIES ${target})
    endif()
endforeach()
