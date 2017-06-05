#!/bin/bash

SOURCE_DIR="$PWD"
BUILD_DIR="$SOURCE_DIR/multi_build"

CI_DIR="$SOURCE_DIR/ci"

COFFEE_DIR="$BUILD_DIR/coffee_lib"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

function build_standalone()
{
    make -f "$CI_DIR/Makefile.standalone" \
        -e SOURCE_DIR="$SOURCE_DIR" \
        -e COFFEE_DIR="$COFFEE_DIR" $@ \
        -e EXTRA_OPTIONS=""

    # We want to exit if the Make process fails horribly
    # Should also signify to Travis/CI that something went wrong
    EXIT_STAT=$?
    if [[ ! "$EXIT_STAT" = 0 ]]; then
        exit 1
    fi
}

case "${TRAVIS_OS_NAME}" in
"linux")
    build_standalone "$BUILDVARIANT"

    tar -zcvf "binaries_All.tar.gz" ${BUILD_DIR}/build
;;
"osx")

;;
esac
