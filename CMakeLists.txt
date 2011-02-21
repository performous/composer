project("Composer" CXX C)
cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)
set(PROJECT_VERSION "alpha")

set(EXENAME ${CMAKE_PROJECT_NAME})
if(UNIX)
	# On UNIX, binary name is lowercase with no spaces
	string(TOLOWER ${EXENAME} EXENAME)
	string(REPLACE " " "-" EXENAME ${EXENAME})
endif()

# Avoid source tree pollution
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not permitted. Make a separate folder for building:\nmkdir build; cd build; cmake ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

# Add a sensible build type default and warning because empty means no optimization and no debug info.
if(NOT CMAKE_BUILD_TYPE)
	message("WARNING: CMAKE_BUILD_TYPE is not defined!\n         Defaulting to CMAKE_BUILD_TYPE=RelWithDebInfo. Use ccmake to set a proper value.")
	SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# Find all the libs that don't require extra parameters
foreach(lib AVFormat)
	find_package(${lib} REQUIRED)
	include_directories(${${lib}_INCLUDE_DIRS})
	list(APPEND LIBS ${${lib}_LIBRARIES})
	add_definitions(${${lib}_DEFINITIONS})
endforeach(lib)

find_package(Phonon)
if(Phonon_FOUND)
	include_directories(${Phonon_INCLUDE_DIRS})
	list(APPEND LIBS ${Phonon_LIBRARIES})
	find_package(Qt4 COMPONENTS QtCore QtGui QtXml REQUIRED)
	include(${QT_USE_FILE})
	list(APPEND LIBS ${QT_LIBRARIES})
else(Phonon_FOUND)
	find_package(Qt4 COMPONENTS QtCore QtGui QtXml Phonon REQUIRED)
	include(${QT_USE_FILE})
	list(APPEND LIBS ${QT_LIBRARIES})
endif(Phonon_FOUND)

# Headers that need MOC need to be defined separately
file(GLOB MOC_HEADER_FILES editorapp.hh notelabel.hh notegraphwidget.hh textcodecselector.hh gettingstarted.hh pitchvis.hh synth.hh)

file(GLOB SOURCE_FILES "*.cc")
file(GLOB HEADER_FILES "*.hh")
file(GLOB RESOURCE_FILES "*.qrc")
file(GLOB UI_FILES "*.ui")
file(GLOB LANG_FILES "lang/*.ts")

QT4_ADD_RESOURCES(RESOURCE_SOURCES ${RESOURCE_FILES})
QT4_ADD_TRANSLATION(TRANSLATIONS ${LANG_FILES})
QT4_WRAP_UI(UI_SOURCES ${UI_FILES} )
QT4_WRAP_CPP(MOC_SOURCES ${MOC_HEADER_FILES})

# Generate config.hh
configure_file("${CMAKE_SOURCE_DIR}/config.cmake.hh" "${CMAKE_BINARY_DIR}/config.hh" @ONLY)

include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})


add_executable(${EXENAME} ${HEADER_FILES} ${SOURCE_FILES} ${MOC_SOURCES} ${RESOURCE_SOURCES} ${UI_SOURCES} ${TRANSLATIONS})
target_link_libraries(${EXENAME} ${LIBS})

# We don't currently have any assets, so on Windows, we just install to the root installation folder
if(UNIX)
	install(TARGETS ${EXENAME} DESTINATION bin)
else()
	install(TARGETS ${EXENAME} DESTINATION .)
endif()

