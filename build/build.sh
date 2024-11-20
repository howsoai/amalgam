#!/bin/bash
#
# Build functions for the repository - predominantly used by build pipelines,
# to keep specific build logic output of pipeline yml files allowing control
# of build functions, outside of the generic build templates.
#
# usage: ./build/build.sh <build-function> {params}
#
#####

# set -x
set -eu # fail on error, and undefined var usage

cmake_preset() {
  PRESET=${1:-}
  if [[ -z "$PRESET" ]]; then
   echo "PRESET is required input"
   cmake --list-presets
   return
  fi
  echo "Building preset: $PRESET"
  ldd --version
  cmake --preset $PRESET # configure/generate (./out/build)
}

cmake_build() {
  PRESET=${1:-}
  if [[ -z "$PRESET" ]]; then
   echo "PRESET is required input"
   cmake --list-presets
   return
  fi
  cmake_preset $PRESET
  cmake --build --preset $PRESET # build
}

cmake_build_target() {
  PRESET=${1:-}
  TARGET=${2:-}
  if [[ -z "$PRESET" ]]; then
   echo "PRESET is required input"
   cmake --list-presets
   return
  fi
  cmake --build --preset $PRESET --target $TARGET
}

cmake_test() {
  PRESET=${1:-}
  cmake_build $PRESET
  cmake_build_target $PRESET test
}

cmake_install() {
  PRESET=${1:-}
  cmake_build $PRESET
  cmake_build_target $PRESET install
}

cmake_package() {
  PRESET=${1:-}
  cmake_build $PRESET
  cmake_build_target $PRESET package
}

docker_run_linux_228_build_container() {
  src_local="."
  src="/home/src/amalgam"
  ct_name="ghcr.io/howsoai/amalgam-build-container-linux-228"
  ct_tag="latest"
  echo "docker run -it -w $src -v $src_local:$src $ct_name:$ct_tag"
  docker run -it -w $src -v "$src_local:$src" "$ct_name:$ct_tag"
}

# Show usage, and print functions
help() {
  echo "usage: ./bin/build.sh <build-function> {params}"
  echo " where <build-function> one of :-"
  IFS=$'\n'
  for f in $(declare -F); do
    echo "    ${f:11}"
  done
}

# Takes the cli params, and runs them, defaulting to 'help()'
if [ ! ${1:-} ]; then
  help
else
  "$@"
fi
