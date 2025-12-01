add_library(swim SHARED event.c network.c)

set_target_properties(swim PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_link_libraries(swim ${UUID_LIBRARY})
