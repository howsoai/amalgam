#!/bin/bash

CC=g++

INCLUDE_DIRS="-I${PWD}/src/3rd_party -I${PWD}/src/Amalgam -I${PWD}/src/Amalgam/entity -I${PWD}/src/Amalgam/evaluablenode -I${PWD}/src/Amalgam/importexport -I${PWD}/src/Amalgam/interpreter -I${PWD}/src/Amalgam/rand -I${PWD}/src/Amalgam/string"

SOURCES=$(find ${PWD}/src -name '*.cpp')

THIRD_PARTY_FLAGS="-DUSE_OS_TZDB=1 -Wno-deprecated-declarations"

# debug flags
COMPILER_FLAGS="-g -O0"
#release flags
#COMPILER_FLAGS="-O3"

LINKER_FLAGS="-lpthread"

${CC} ${THIRD_PARTY_FLAGS} ${COMPILER_FLAGS} ${SOURCES} ${INCLUDE_DIRS} -o amalgam ${LINKER_FLAGS}

ls
