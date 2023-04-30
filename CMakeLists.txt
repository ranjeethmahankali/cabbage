cmake_minimum_required(VERSION 3.22.0)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#VCPKG
if(NOT DEFINED ENV{VCPKG_PATH})
  message(FATAL_ERROR "Please clone vcpkg and set the VCPKG_PATH environment variable")
endif()

set(VCPKG_FEATURE_FLAGS versions)
set(VCPKG_TARGET_TRIPLET x64-linux)
set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake)

project(baller)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/modules/" ${CMAKE_MODULE_PATH})

find_package(Corrade REQUIRED Main)
find_package(Magnum REQUIRED
  GL Primitives SceneGraph Shaders Sdl2Application
)
find_package(MagnumIntegration REQUIRED ImGui)

add_executable(baller
  Main.cpp
)
target_link_libraries(baller PRIVATE
  Corrade::Main
  Magnum::Application
  Magnum::GL
  Magnum::Magnum
  Magnum::Primitives
  Magnum::SceneGraph
  Magnum::Shaders
  MagnumIntegration::ImGui
)
