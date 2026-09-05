cmake_minimum_required(VERSION 3.22)

project(ZCOMOpenKit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(RE_UE4SS_SOURCE_DIR "" CACHE PATH "Path to a pinned RE-UE4SS source checkout")
if(NOT EXISTS "${RE_UE4SS_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Set RE_UE4SS_SOURCE_DIR to a recursive RE-UE4SS checkout")
endif()

add_subdirectory("${RE_UE4SS_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/re-ue4ss")

add_library(ZCOMOpenKit SHARED src/dllmain.cpp src/helmet_render_fit.cpp src/expanded_colours.cpp)

target_compile_features(ZCOMOpenKit PRIVATE cxx_std_23)
target_link_libraries(ZCOMOpenKit PUBLIC UE4SS PolyHook_2)
if(MSVC)
    # Keep developer machine paths out of the PE debug directory in public builds.
    target_link_options(ZCOMOpenKit PRIVATE "/PDBALTPATH:ZCOMOpenKit.pdb")
endif()

set_target_properties(ZCOMOpenKit PROPERTIES OUTPUT_NAME "ZCOMOpenKit")
