cmake_minimum_required(VERSION 3.0)
cmake_policy(VERSION 3.0)

set(EXENAME ${CMAKE_PROJECT_NAME})
set(CMAKE_AUTOMOC FALSE)
if(UNIX)
	# On UNIX, binary name is lowercase with no spaces
	string(TOLOWER ${EXENAME} EXENAME)
	string(REPLACE " " "-" EXENAME ${EXENAME})
endif()

# Headers that need MOC need to be defined separately
file(GLOB MOC_HEADER_FILES editorapp.hh notelabel.hh notegraphwidget.hh textcodecselector.hh gettingstarted.hh pitchvis.hh synth.hh)

file(GLOB SOURCE_FILES "*.cc")
file(GLOB HEADER_FILES "*.hh")
file(GLOB RESOURCE_FILES "../*.qrc")
file(GLOB UI_FILES "../ui/*.ui")

# Find all the libs that don't require extra parameters

# Final binary
add_executable(${EXENAME})

foreach(lib AVFormat SWResample SWScale Qt5Core Qt5Widgets Qt5Gui Qt5Xml Qt5Multimedia)
	find_package(${lib} REQUIRED)
	message(STATUS "${lib} includes: ${${lib}_INCLUDE_DIRS}")
	target_include_directories(${EXENAME} SYSTEM PRIVATE ${${lib}_INCLUDE_DIRS})
	target_link_libraries(${EXENAME} PRIVATE ${${lib}_LIBRARIES})
endforeach(lib)

# Qt pre-processors
QT5_ADD_RESOURCES(RESOURCE_SOURCES ${RESOURCE_FILES})
QT5_WRAP_UI(UI_SOURCES ${UI_FILES} )
QT5_WRAP_CPP(MOC_SOURCES ${MOC_HEADER_FILES})

target_sources(${EXENAME} PRIVATE ${HEADER_FILES} ${SOURCE_FILES} ${MOC_SOURCES} ${RESOURCE_SOURCES} ${UI_SOURCES})


# Generate config.hh
configure_file(config.cmake.hh "${CMAKE_BINARY_DIR}/src/config.hh" @ONLY)

include_directories(${CMAKE_BINARY_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src)

# We don't currently have any assets, so on Windows, we just install to the root installation folder
if(UNIX)
	install(TARGETS ${EXENAME} DESTINATION bin)
	install(FILES "../platform/composer.desktop" DESTINATION "share/applications/")
	install(FILES "../icons/composer.png" DESTINATION "share/pixmaps")
else()
	install(TARGETS ${EXENAME} DESTINATION .)
endif() 
