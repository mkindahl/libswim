option(SWIM_TRACING "Include code to trace SWIM event processing" ON)

configure_file(config.h.in config.h @ONLY)

add_library(swim SHARED event.c network.c state.c process.c cluster.c)

set_target_properties(swim PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_link_libraries(swim ${UUID_LIBRARY})
