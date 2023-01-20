# Set CMake minimum version and CMake policy required by UrhoCommon module
cmake_minimum_required (VERSION 3.2.3)
if (COMMAND cmake_policy)
    # Libraries linked via full path no longer produce linker search paths
    cmake_policy (SET CMP0003 NEW)
    # INTERFACE_LINK_LIBRARIES defines the link interface
    cmake_policy (SET CMP0022 NEW)
    # Disallow use of the LOCATION target property - so we set to OLD as we still need it
    cmake_policy (SET CMP0026 OLD)
    # MACOSX_RPATH is enabled by default
    cmake_policy (SET CMP0042 NEW)
    # Honor the visibility properties for SHARED target types only
    cmake_policy (SET CMP0063 OLD)
endif ()
# Set project name
project (heva)

set (WEBCAM_ENABLE ON CACHE BOOL "Enable dlib face recognitioni")
set (X11_TRANSPARENT_WINDOW OFF CACHE BOOL "Transparent window support for X11 (linux only)")
set (ALL_IN_ONE OFF CACHE BOOL "Build all depends together")
set (VMC_OSC_SENDER ON CACHE BOOL "Send data over VMC OSC protocol")

if (ALL_IN_ONE)
	add_compile_definitions( ALL_IN_ONE=${ALL_IN_ONE} )
else()
	#set (URHO3D_HOME /usr/local/share/Urho3D)
	#set (URHO3D_HOME /home/scaled/git/Urho3D)
endif()

# Set CMake modules search path
set (CMAKE_MODULE_PATH ${URHO3D_HOME}/cmake/Modules)

# Include UrhoCommon.cmake module after setting project name
include (UrhoCommon)
# Define target name
set (TARGET_NAME heva)
# Define source files
define_source_files ()
set (SOURCE_FILES heva.cpp heva.h)
if (WEBCAM_ENABLE)
	set (SOURCE_FILES ${SOURCE_FILES} face_recognition.cpp face_recognition.h)
	add_compile_definitions( WEBCAM_ENABLE=${WEBCAM_ENABLE} )
endif()
if (X11_TRANSPARENT_WINDOW)
	set (SOURCE_FILES ${SOURCE_FILES} x11_transparent_window.cpp)
	add_compile_definitions( X11_TRANSPARENT_WINDOW=${X11_TRANSPARENT_WINDOW} )
endif()
if (VMC_OSC_SENDER)
	set (SOURCE_FILES ${SOURCE_FILES} tinyosc.h tinyosc.c vmc_osc_sender.h vmc_osc_sender.cpp)
	add_compile_definitions( VMC_OSC_SENDER=${VMC_OSC_SENDER} )
endif()
if (ALL_IN_ONE)
	set (SOURCE_FILES ${SOURCE_FILES} inih-r53/cpp/INIReader.h inih-r53/cpp/INIReader.cpp inih-r53/ini.h inih-r53/ini.c)
endif()

#define_resource_dirs (GLOB_PATTERNS ${CMAKE_SOURCE_DIR}/Data ${URHO3D_HOME}/bin/CoreData)
define_resource_dirs (GLOB_PATTERNS ${CMAKE_SOURCE_DIR}/Data ${CMAKE_SOURCE_DIR}/CoreData)
# Setup target with resource copying
setup_main_executable ()

# Link to OpenCV
if (WEBCAM_ENABLE)
	if (ALL_IN_ONE)
		add_subdirectory(${CMAKE_SOURCE_DIR}/dlib-19.21 dlib_build)

		find_package (OpenCV REQUIRED)
		if (OpenCV_FOUND)
			include_directories(${OpenCV_INCLUDE_DIRS})
			
			if (X11_TRANSPARENT_WINDOW)
				target_link_libraries(heva dlib::dlib ${OpenCV_LIBS} pthread X11 Xrender)
			else()
				target_link_libraries(heva dlib::dlib ${OpenCV_LIBS} pthread)
			endif()
		endif()
	else()
		find_package (dlib)
		find_package (OpenCV REQUIRED)
		if (OpenCV_FOUND)
			include_directories(${OpenCV_INCLUDE_DIRS})
			if (X11_TRANSPARENT_WINDOW)
				target_link_libraries(heva dlib ${OpenCV_LIBS} openblas pthread X11 Xrender INIReader)
			else()
				target_link_libraries(heva dlib ${OpenCV_LIBS} openblas pthread INIReader)
			endif()
		endif()
	endif()
else()
	if (X11_TRANSPARENT_WINDOW)
		target_link_libraries(heva X11 Xrender INIReader)
	else()
		target_link_libraries(heva INIReader)
	endif()
endif()
