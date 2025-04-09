set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Application for creating song notes for pitch-detecting karaoke games.")
set(CPACK_PACKAGE_CONTACT "http://performous.org/")
set(CPACK_SOURCE_IGNORE_FILES
   "/.cvsignore"
   "/.gitignore"
   "/songs/"
   "/build/"
   "/.svn/"
   "/.git/"
)
if(WIN32)
	set(CPACK_PACKAGE_EXECUTABLES "Composer" "${CMAKE_CURRENT_SOURCE_DIR}/icons\\\\composer.png")
	set(CPACK_PACKAGE_INSTALL_DIRECTORY "Performous\\\\Composer")
else()
	set(CPACK_PACKAGE_EXECUTABLES "composer")
endif()
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_GENERATOR "TBZ2")

# Debian specific settings
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SECTION "Games")

if("${CMAKE_BUILD_TYPE}" MATCHES "Release")
	set(CPACK_STRIP_FILES TRUE)
endif("${CMAKE_BUILD_TYPE}" MATCHES "Release")

if(APPLE)
	set(CPACK_GENERATOR "PACKAGEMAKER;OSXX11")
endif(APPLE)
if(UNIX)
	# Try to find architecture
	execute_process(COMMAND uname -m OUTPUT_VARIABLE CPACK_PACKAGE_ARCHITECTURE)
	string(STRIP "${CPACK_PACKAGE_ARCHITECTURE}" CPACK_PACKAGE_ARCHITECTURE)
	# Try to find distro name and distro-specific arch
	execute_process(COMMAND lsb_release -is OUTPUT_VARIABLE LSB_ID)
	execute_process(COMMAND lsb_release -rs OUTPUT_VARIABLE LSB_RELEASE)
	string(STRIP "${LSB_ID}" LSB_ID)
	string(STRIP "${LSB_RELEASE}" LSB_RELEASE)
	set(LSB_DISTRIB "${LSB_ID}${LSB_RELEASE}")
	if(NOT LSB_DISTRIB)
		set(LSB_DISTRIB "unix")
	endif(NOT LSB_DISTRIB)
	# For Debian-based distros we want to create DEB packages.
	if("${LSB_DISTRIB}" MATCHES "Ubuntu|Debian")
		set(CPACK_GENERATOR "DEB")
		set(CPACK_DEBIAN_PACKAGE_PRIORITY "extra")
		set(CPACK_DEBIAN_PACKAGE_SECTION "universe/editors")
		set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "performous")
		# We need to alter the architecture names as per distro rules
		if("${CPACK_PACKAGE_ARCHITECTURE}" MATCHES "i[3-6]86")
			set(CPACK_PACKAGE_ARCHITECTURE i386)
		endif("${CPACK_PACKAGE_ARCHITECTURE}" MATCHES "i[3-6]86")
		if("${CPACK_PACKAGE_ARCHITECTURE}" MATCHES "x86_64")
			set(CPACK_PACKAGE_ARCHITECTURE amd64)
		endif("${CPACK_PACKAGE_ARCHITECTURE}" MATCHES "x86_64")

		string(TOLOWER "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}-${LSB_DISTRIB}_${CPACK_PACKAGE_ARCHITECTURE}" CPACK_PACKAGE_FILE_NAME)
	endif("${LSB_DISTRIB}" MATCHES "Ubuntu|Debian")
	# For Fedora-based distros we want to create RPM packages.
	if("${LSB_DISTRIB}" MATCHES "Fedora")
		set(CPACK_GENERATOR "RPM")
		set(CPACK_RPM_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
		set(CPACK_RPM_PACKAGE_VERSION "${PROJECT_VERSION}")
		set(CPACK_RPM_PACKAGE_RELEASE "1")
#		set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
		set(CPACK_RPM_PACKAGE_LICENSE "GPLv3+")
		set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "TODO")
		set(CPACK_RPM_PACKAGE_DESCRIPTION "TODO")
		# We need to alter the architecture names as per distro rules
		if("${CPACK_PACKAGE_ARCHITECTURE}" MATCHES "i[3-6]86")
			set(CPACK_PACKAGE_ARCHITECTURE i386)
		endif("${CPACK_PACKAGE_ARCHITECTURE}" MATCHES "i[3-6]86")
		if("${CPACK_PACKAGE_ARCHITECTURE}" MATCHES "x86_64")
			set(CPACK_PACKAGE_ARCHITECTURE amd64)
		endif("${CPACK_PACKAGE_ARCHITECTURE}" MATCHES "x86_64")
		# Set the dependencies based on the distro version
		if("${LSB_DISTRIB}" MATCHES "Fedora14")
			#set(CPACK_RPM_PACKAGE_REQUIRES "TODO")
		endif("${LSB_DISTRIB}" MATCHES "Fedora14")
		if("${LSB_DISTRIB}" MATCHES "Fedora13")
			#set(CPACK_RPM_PACKAGE_REQUIRES "TODO")
		endif("${LSB_DISTRIB}" MATCHES "Fedora13")
		if(NOT CPACK_RPM_PACKAGE_REQUIRES)
			message("WARNING: ${LSB_DISTRIB} is not supported.\nPlease set deps in packaging.cmake before packaging.")
		endif(NOT CPACK_RPM_PACKAGE_REQUIRES)
	endif("${LSB_DISTRIB}" MATCHES "Fedora")
	set(CPACK_SYSTEM_NAME "${LSB_DISTRIB}-${CPACK_PACKAGE_ARCHITECTURE}")
	message(STATUS "Detected ${CPACK_SYSTEM_NAME}. Use make package to build packages (${CPACK_GENERATOR}).")
endif(UNIX)

include(CPack)
