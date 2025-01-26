#!/usr/bin/env bash

set -e

PROJECT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." &> /dev/null && pwd)

BUILD_DIR="$(pwd)/build"

NJOBS=1

USE_GCBOEHM="-DSHARD_USE_GCBOEHM"

CONFIGURATION_FILE="${PROJECT_DIR}/configuration.shard"
STORE_DIR="${PROJECT_DIR}/store"

RUN_AFTERWARDS=0

function error() {
    BRED="\033[1;31m"
    RST="\033[0m"
    printf "%s: ${BRED}error:${RST} %s\n" "$0" "$1"
    exit 1
}

function show_help() {
    echo "$0: Script for bootstrapping the Amethyst OS."
    echo
    echo "Usage: $0 [OPTION]... [VAR=VALUE]..."
    echo 
    echo "To assign environment variables (e.g., CC, CFLAGS...), specify them as"
    echo "VAR=VALUE.  See below for descriptions of some of the useful variables."
    echo
    echo "Defaults for the options are specified in brackets."
    echo
    echo "Options:"
    echo "  -h, --help              display this help text and exit"
    echo "  -r, --run               execute \`run.sh\` after bootstrapping"
    echo "  -c, --clean             clean the build files and exit"
    echo "  -C, --clean-all         clean the build files of all subprojects"
    echo "  -jN, --jobs=N           use N parallel threads for building"
    echo "      --build-dir=<path>  put build files in <path> [$BUILD_DIR]"
    echo "      --store-dir=<path>  set the geode store dir to <path> [$STORE_DIR]"
    echo "      --config=<path>     set the main system configuration path to <path> [$CONFIGURATION_FILE]"
    echo "      --disable-gcboehm   don't use gcboehm on host libshard"
    echo
}

function clean_build_files() {
    make -C $PROJECT_DIR clean
    [ -e amethyst.iso ] && rm -r amethyst.iso
}

function clean_store_build_files() {
    for dir in "$STORE_DIR"/*/; do
        if [ -f "${dir}Makefile" ]; then
            make -C "${dir}" clean
        fi
    done
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --build-dir=*)
            BUILD_DIR=$(realpath ${1#*=})
            ;;
        -j*)
            NJOBS=${1#*j}
            if [ $NJOBS == auto ]; then
                NJOBS=$(nproc)
            fi
            ;;
        --jobs=*)
            NJOBS=${1#*=}
            if [ $NJOBS == auto ]; then
                NJOBS=$(nproc)
            fi
            ;;
        --disable-gcboehm)
            USE_GCBOEHM=""
            ;;
        -c|--clean)
            clean_build_files
            exit 0
            ;;
        -C|--clean-all)
            clean_store_build_files
            clean_build_files
            exit 0
            ;;
        -r|--run)
            RUN_AFTERWARDS=1
            ;;
        -h|--help)
            show_help
            ;;
        (*=*)
            export $1
            ;;
        *)
            error "unknown option -- '$1'"
            ;;
    esac

    shift
done

HOST_BUILD_DIR="${BUILD_DIR}/host"
BOOTSTRAP_DIR="${BUILD_DIR}/bootstrap"
GEODE_BIN="${HOST_BUILD_DIR}/geode-host"

CFLAGS="${USE_GCBOEHM}" make -C ${PROJECT_DIR}/shard -j${NJOBS} ${GEODE_BIN} BUILD_DIR=${HOST_BUILD_DIR}

[ -e ${GEODE_BIN} ] || error "compiling host geode binary failed."

${GEODE_BIN} bootstrap --prefix=${BOOTSTRAP_DIR} --config=${CONFIGURATION_FILE} --store=${STORE_DIR} -j${NJOBS} --verbose

[ $? != 0 ] && error "boostrapping failed."

echo "bootstrapping done."

if [ $RUN_AFTERWARDS -ne 0 ]; then
    pushd $PROJECT_DIR
    tools/run.sh
    popd
fi

