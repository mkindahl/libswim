add_executable(swim_server swim_server.c)
target_link_libraries(swim_server swim_threads)

add_executable(swim_client swim_client.c)
target_link_libraries(swim_client swim ${UUID_LIBRARY})

find_package(Curses)
if(CURSES_FOUND)
  add_executable(swim_server_tui swim_server_tui.c)
  target_include_directories(swim_server_tui PRIVATE ${CURSES_INCLUDE_DIRS})
  target_link_libraries(swim_server_tui swim_threads ${CURSES_LIBRARIES})
endif()
