# - Try to find Phonon
# Once done, this will define
#
#  Phonon_FOUND - system has Phonon
#  Phonon_INCLUDE_DIRS - the Phonon include directories
#  Phonon_LIBRARIES - link these to use Phonon
#
# See documentation on how to write CMake scripts at
# http://www.cmake.org/Wiki/CMake:How_To_Find_Libraries

include(LibFindMacros)

libfind_pkg_check_modules(Phonon_PKGCONF phonon)

find_path(Phonon_INCLUDE_DIR
  NAMES phonon/Global
  PATHS ${Phonon_PKGCONF_INCLUDE_DIRS}
)

find_library(Phonon_LIBRARY
  NAMES phonon
  PATHS ${Phonon_PKGCONF_LIBRARY_DIRS}
)

set(Phonon_PROCESS_INCLUDES Phonon_INCLUDE_DIR)
set(Phonon_PROCESS_LIBS Phonon_LIBRARY)
libfind_process(Phonon)

