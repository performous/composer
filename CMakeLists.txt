project(Editor CXX C)
cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)
set(PROJECT_VERSION "alpha")

# Avoid source tree pollution
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not permitted. Make a separate folder for building:\nmkdir build; cd build; cmake ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)

# Add a sensible build type default and warning because empty means no optimization and no debug info.
if(NOT CMAKE_BUILD_TYPE)
	message("WARNING: CMAKE_BUILD_TYPE is not defined!\n         Defaulting to CMAKE_BUILD_TYPE=RelWithDebInfo. Use ccmake to set a proper value.")
	SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

find_package(Qt4 REQUIRED) # find and setup Qt4 for this project
include(${QT_USE_FILE})

file(GLOB SOURCE_FILES "*.cc")
file(GLOB HEADER_FILES "*.hh")
file(GLOB RESOURCE_FILES "*.qrc")
file(GLOB UI_FILES "*.ui")

QT4_ADD_RESOURCES(RESOURCE_SOURCES ${RESOURCE_FILES})
QT4_WRAP_UI(UI_SOURCES ${UI_FILES} )
QT4_AUTOMOC(MOC_SOURCES ${SOURCE_FILES})

include_directories(${CMAKE_BINARY_DIR})

add_executable(editor ${SOURCE_FILES} ${MOC_SOURCES} ${RESOURCE_SOURCES} ${UI_SOURCES})
target_link_libraries(editor ${QT_LIBRARIES})
