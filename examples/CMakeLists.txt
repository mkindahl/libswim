add_executable(swim_server swim_server.c)
target_link_libraries(swim_server swim)

add_executable(swim_client swim_client.c)
target_link_libraries(swim_client swim ${UUID_LIBRARY})
