target_sources(tricky
	PRIVATE
    include/tricky/tricky.h
    include/tricky/data.h
    include/tricky/handlers.h
    include/tricky/lazy_load.h
    include/tricky/state.h
    include/tricky/context.h
    include/tricky/error.h
  )

target_include_directories(tricky INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
