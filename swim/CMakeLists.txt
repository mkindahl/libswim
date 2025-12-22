option(SWIM_TRACING "Include code to trace SWIM event processing" ON)

configure_file(config.h.in config.h @ONLY)

add_library(
  swim SHARED
  event.c
  network.c
  state.c
  process.c
  cluster.c
  utils.c
  debug.c
  node.c
  encoding.c)

set_target_properties(swim PROPERTIES VERSION ${PROJECT_VERSION}
                                      SOVERSION ${PROJECT_VERSION_MAJOR})
target_link_libraries(swim ${UUID_LIBRARY})

add_executable(encoding_test encoding_test.c)
target_link_libraries(encoding_test swim ${UUID_LIBRARY})

add_test(NAME test_encoding COMMAND encoding_test)
