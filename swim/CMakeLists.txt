option(SWIM_TRACING "Include code to trace SWIM event processing" ON)

configure_file(config.h.in config.h @ONLY)

add_library(
  swim STATIC
  event.c
  network.c
  state.c
  process.c
  cluster.c
  utils.c
  debug.c
  node.c
  encoding.c
  server.c)
set_target_properties(swim PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

add_library(swim_threads SHARED server_pthread.c)
target_link_libraries(swim_threads swim ${UUID_LIBRARY})
set_target_properties(
  swim_threads
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             POSITION_INDEPENDENT_CODE TRUE)

add_executable(encoding_test encoding_test.c)
target_link_libraries(encoding_test swim ${UUID_LIBRARY})

add_test(NAME test_encoding COMMAND encoding_test)
