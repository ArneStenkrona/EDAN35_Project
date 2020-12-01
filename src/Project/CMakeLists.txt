add_executable (EDAN35_Project)

target_sources (
	EDAN35_Project
	PRIVATE
		[[project.hpp]]
		[[project.cpp]]
)

target_link_libraries (EDAN35_Project PRIVATE assignment_setup EDAN35_Common)

install (TARGETS EDAN35_Project DESTINATION bin)

copy_dlls (EDAN35_Project "${CMAKE_CURRENT_BINARY_DIR}")
