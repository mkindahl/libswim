add_executable(swim_server swim_server.c)
target_link_libraries(swim_server ${UUID_LIBRARY} swim)

add_executable(swim_client swim_client.c)
target_link_libraries(swim_client ${UUID_LIBRARY} swim)
