add_executable(haul main.cpp)
target_link_libraries(haul PRIVATE engine)

# Copy bundled assets (fonts, etc.) next to the executable so the game can
# find them at runtime regardless of where it's launched from.
add_custom_command(TARGET haul POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:haul>/assets
)
