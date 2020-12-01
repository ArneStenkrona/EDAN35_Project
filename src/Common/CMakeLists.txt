add_library(EDAN35_Common)

target_sources (
	EDAN35_Common
	PRIVATE
		[[parametric_shapes.hpp]]
		[[parametric_shapes.cpp]]
)

target_link_libraries (EDAN35_Common PRIVATE assignment_setup)

install (TARGETS EDAN35_Common DESTINATION bin)

copy_dlls (EDAN35_Common "${CMAKE_CURRENT_BINARY_DIR}")
