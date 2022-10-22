target_sources(tricky
	PRIVATE
    include/tricky/tricky.h
    include/tricky/payload.h
    include/tricky/data.h
    include/tricky/handlers.h
  )

set_property(TARGET tricky APPEND PROPERTY SRC_DIRS "${CMAKE_CURRENT_LIST_DIR}/include")
target_include_directories(tricky INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
