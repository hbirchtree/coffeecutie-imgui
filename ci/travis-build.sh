#!/bin/bash

SOURCE_DIR="$PWD"
BUILD_DIR="$SOURCE_DIR/multi_build"

CI_DIR="$SOURCE_DIR/ci"

COFFEE_DIR="$BUILD_DIR/coffee_lib"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

function build_standalone()
{
    make -f "$CI_DIR/Makefile.choices" \
        -e SOURCE_DIR="$SOURCE_DIR" \
        -e COFFEE_DIR="$COFFEE_DIR" $@ \
        -e EXTRA_OPTIONS=""
}

case "${TRAVIS_OS_NAME}" in
"linux")
    build_standalone ubuntu.amd64

    tar -zcvf "binaries_All.tar.gz" ${BUILD_DIR}/build
;;
"osx")

;;
esac
