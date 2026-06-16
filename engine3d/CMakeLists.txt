cmake_minimum_required(VERSION 3.16)
project(Engine3D CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2IMG REQUIRED SDL2_image)

add_executable(engine3d src/main.cpp src/swrender.cpp src/mesh.cpp)
target_include_directories(engine3d PRIVATE src ${SDL2_INCLUDE_DIRS} ${SDL2IMG_INCLUDE_DIRS})
target_link_directories(engine3d PRIVATE ${SDL2_LIBRARY_DIRS} ${SDL2IMG_LIBRARY_DIRS})
target_link_libraries(engine3d PRIVATE ${SDL2_LIBRARIES} ${SDL2IMG_LIBRARIES})

# GPU backend (OpenGL ES 3.0 via EGL). Optional: only built if EGL + GLES found.
pkg_check_modules(EGL egl)
pkg_check_modules(GLES glesv2)
if(EGL_FOUND AND GLES_FOUND)
  add_executable(engine3d_gl src/gl_main.cpp src/mesh.cpp)
  target_include_directories(engine3d_gl PRIVATE src
    ${SDL2_INCLUDE_DIRS} ${SDL2IMG_INCLUDE_DIRS} ${EGL_INCLUDE_DIRS} ${GLES_INCLUDE_DIRS})
  target_link_directories(engine3d_gl PRIVATE
    ${SDL2_LIBRARY_DIRS} ${SDL2IMG_LIBRARY_DIRS} ${EGL_LIBRARY_DIRS} ${GLES_LIBRARY_DIRS})
  target_link_libraries(engine3d_gl PRIVATE
    ${SDL2_LIBRARIES} ${SDL2IMG_LIBRARIES} ${EGL_LIBRARIES} ${GLES_LIBRARIES})
endif()
