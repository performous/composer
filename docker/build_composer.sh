#!/bin/bash -ex
## Pull in /etc/os-release so we can see what we're running on
. /etc/os-release

## Default Vars
GIT_REPOSITORY='https://github.com/performous/composer.git'

## Function to print the help message
usage() {
  set +x
  echo ""
  echo "Usage: ${0}"
  echo ""
  echo "Optional Arguments:"
  echo "  -b <Git Branch>: Build the specified git branch, tag, or sha"
  echo "  -D </path/to/build/directory>: Disable cloning the repo from git and build in the specified directory"
  echo "  -E <'Extra Cmake Args'>: A quoted list of extra arguments to pass directly to cmake"
  echo "  -g : Generate Packages"
  echo "  -p <Pull Request #>: Build the specified Github Pull Request number"
  echo "  -r <Repository URL>: Git repository to pull from"
  echo "  -R : Perform a 'Release' Cmake Build (Default is 'RelWithDebInfo')"
  echo "  -h : Show this help message"
  exit 1
}

## Set up getopts
while getopts "b:D:E:gp:r:Rh" OPTION; do
  case ${OPTION} in
    "b")
      GIT_BRANCH=${OPTARG};;
    "D")
      BUILD_DIRECTORY=${OPTARG};;
    "E")
      EXTRA_CMAKE_ARGS=${OPTARG};;
    "g")
      GENERATE_PACKAGES=true;;
    "p")
      PULL_REQUEST=${OPTARG};;
    "r")
      GIT_REPOSITORY=${OPTARG};;
    "R")
      RELEASE_BUILD=true;;
    "h")
      HELP=true;;
  esac
done

if [ ${HELP} ]; then
  usage
fi

## All the git stuff
if [ -z ${BUILD_DIRECTORY} ]; then
  git clone ${GIT_REPOSITORY} performous_composer
  cd performous_composer
  if [ ${PULL_REQUEST} ]; then
    git fetch origin pull/${PULL_REQUEST}/head:pr
    git checkout pr
  elif [ ${GIT_BRANCH} ]; then
    git checkout ${GIT_BRANCH}
  fi
  git submodule update --init --recursive
else
  cd ${BUILD_DIRECTORY}
fi

## Set up some special cmake flags for fedora
if [ "${ID}" == "fedora" ]; then
  EXTRA_CMAKE_ARGS="${EXTRA_CMAKE_ARGS} -DUSE_BOOST_REGEX=1"
fi

if [ "${RELEASE_BUILD}" ]; then
  EXTRA_CMAKE_ARGS="${EXTRA_CMAKE_ARGS} -DCMAKE_BUILD_TYPE=Release"
fi

## Figure out what type of packages we need to generate
case ${ID} in
  'fedora')
    PACKAGE_TYPE='RPM';;
  'ubuntu'|'debian')
    PACKAGE_TYPE='DEB';;
  *)
    PACKAGE_TYPE='TGZ';;
esac

## Build with cmake 
mkdir build
cd build
cmake ${EXTRA_CMAKE_ARGS} -DENABLE_WEBSERVER=ON -DCMAKE_VERBOSE_MAKEFILE=1 -DENABLE_WEBCAM=ON ..
CPU_CORES=$(nproc --all)
make -j${CPU_CORES}
if [ ${GENERATE_PACKAGES} ]; then
  cpack -G ${PACKAGE_TYPE}
fi
cd ..
