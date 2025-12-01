add_library(swim SHARED event.c)

set_target_properties(swim PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)
