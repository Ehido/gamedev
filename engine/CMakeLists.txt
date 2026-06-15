add_library(engine STATIC
  src/Renderer.cpp
  src/Input.cpp
  src/Font.cpp
  src/App.cpp
)

target_include_directories(engine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${SDL2_INCLUDE_DIRS}
  ${SDL2IMG_INCLUDE_DIRS}
  ${SDL2TTF_INCLUDE_DIRS}
)

target_link_directories(engine PUBLIC
  ${SDL2_LIBRARY_DIRS}
  ${SDL2IMG_LIBRARY_DIRS}
  ${SDL2TTF_LIBRARY_DIRS}
)

target_link_libraries(engine PUBLIC
  ${SDL2_LIBRARIES}
  ${SDL2IMG_LIBRARIES}
  ${SDL2TTF_LIBRARIES}
)

target_compile_options(engine PRIVATE ${SDL2_CFLAGS_OTHER})
