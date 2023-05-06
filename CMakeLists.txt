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
if (WIN32)
   set(VCPKG_TARGET_TRIPLET x64-windows-static)
   file(TO_CMAKE_PATH $ENV{VCPKG_PATH} VCPKG_PATH)
   set(CMAKE_TOOLCHAIN_FILE ${VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake)
   add_compile_definitions(_USE_MATH_DEFINES)
else()
   set(VCPKG_TARGET_TRIPLET x64-linux)
   set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake)
endif()

project(cabbage)

find_package(box2d CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glew REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_executable(cabbage
  Main.cpp
  GLUtil.cpp
  Game.cpp
)
target_link_libraries(cabbage PRIVATE
  box2d::box2d
  GLEW::GLEW
  glfw
  glm::glm
  spdlog::spdlog
  spdlog::spdlog_header_only
  fmt::fmt
)
target_include_directories(cabbage PRIVATE "./")

if (WIN32)
  set_property(TARGET cabbage PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
