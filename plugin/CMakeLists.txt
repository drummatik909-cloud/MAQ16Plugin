cmake_minimum_required(VERSION 3.22)
project(MAQ16Plugin VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MAQ_BUILD_TESTS  "Compiler les tests du moteur" ON)
option(MAQ_BUILD_PLUGIN "Compiler le plugin JUCE/CLAP/VST3 (télécharge JUCE et clap-juce-extensions)" OFF)

# Moteur, presets et réglages de dossiers : C++ standard, sans dépendance.
add_library(maq_engine STATIC
    src/engine/Unit.cpp
    src/engine/Events.cpp
    src/presets/Bank.cpp
    src/presets/FolderSettings.cpp)
target_include_directories(maq_engine PUBLIC src)
if(MSVC)
    target_compile_options(maq_engine PRIVATE /W4 /utf-8)
else()
    target_compile_options(maq_engine PRIVATE -Wall -Wextra)
endif()

if(MAQ_BUILD_TESTS)
    enable_testing()
    add_executable(maq_tests tests/test_main.cpp)
    target_link_libraries(maq_tests PRIVATE maq_engine)
    add_test(NAME engine COMMAND maq_tests)
endif()

if(MAQ_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()
