cmake_minimum_required(VERSION 3.22)

project(DegrainWindowsBuild VERSION 1.0.0)

# CI-only entry point: builds *just* Degrain against a CI-fetched JUCE tree,
# bypassing the repo root CMakeLists.txt's auto-glob of every plugin
# (which would also configure unrelated, untested-on-Windows plugins).
set(JUCE_PATH "" CACHE PATH "Path to JUCE source tree")
add_subdirectory(${JUCE_PATH} JUCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../plugins/Degrain Degrain)
