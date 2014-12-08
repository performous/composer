cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)

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

# Qt pre-processors
QT5_ADD_RESOURCES(RESOURCE_SOURCES ${RESOURCE_FILES})
QT5_WRAP_UI(UI_SOURCES ${UI_FILES} )
QT5_WRAP_CPP(MOC_SOURCES ${MOC_HEADER_FILES})

# Generate config.hh
configure_file(config.cmake.hh "${CMAKE_BINARY_DIR}/src/config.hh" @ONLY)

include_directories(${CMAKE_BINARY_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Final binary
add_executable(${EXENAME} ${HEADER_FILES} ${SOURCE_FILES} ${MOC_SOURCES} ${RESOURCE_SOURCES} ${UI_SOURCES})
target_link_libraries(${EXENAME} ${LIBS})
target_link_libraries(${EXENAME} avresample) #FIXME!! should be included in ${LIBS}

# We don't currently have any assets, so on Windows, we just install to the root installation folder
if(UNIX)
	install(TARGETS ${EXENAME} DESTINATION bin)
	install(FILES "../platform/composer.desktop" DESTINATION "share/applications/")
	install(FILES "../icons/composer.png" DESTINATION "share/pixmaps")
else()
	install(TARGETS ${EXENAME} DESTINATION .)
endif() 
