#!/bin/bash

SOURCE_DIR="$PWD"
BUILD_DIR="$SOURCE_DIR/multi_build"

CI_DIR="$SOURCE_DIR/ci"

COFFEE_DIR="$BUILD_DIR/coffee_lib"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

QTHUB_DOCKER="hbirch/coffeecutie:qthub-client"
COFFEE_SLUG="hbirchtree/coffeecutie"

function die()
{
    echo " * " $@
    exit 1
}

function notify()
{
    echo " * " $@
}

function github_api()
{
    docker run --rm $QTHUB_DOCKER --api-token "$GITHUB_TOKEN" $@
}

function download_libraries()
{
    notify "Downloading libraries for architecture: ${BUILDVARIANT}"
    local LATEST_RELEASE="$(github_api list release $COFFEE_SLUG | head -1 | cut -d'|' -f 3)"
    local CURRENT_ASSET="$(github_api list asset ${COFFEE_SLUG}:${LATEST_RELEASE} | grep $BUILDVARIANT)"
    if [[ -z $CURRENT_ASSET ]]; then
        die "Failed to find library release"
    fi
    notify "Found assets: $CURRENT_ASSET (from $LATEST_RELEASE)"
    local ASSET_ID="$(echo $CURRENT_ASSET | cut -d'|' -f 3)"
    local ASSET_FN="$(echo $CURRENT_ASSET | cut -d'|' -f 5)"

    github_api pull asset $COFFEE_SLUG $ASSET_ID

    tar -xvf "$ASSET_FN"
}

function get_opts()
{
    if [[ ! -z $COFFEE_LIBRARY_BUILD ]]; then
        echo "-DGENERATE_PROGRAMS=OFF"
    fi
}

function build_standalone()
{
    download_libraries

    make -f "$CI_DIR/Makefile.standalone" \
        -e SOURCE_DIR="$SOURCE_DIR" \
        -e COFFEE_DIR="$COFFEE_DIR" $@ \
        -e EXTRA_OPTIONS="$(get_opts)"

    # We want to exit if the Make process fails horribly
    # Should also signify to Travis/CI that something went wrong
    EXIT_STAT=$?
    if [[ ! "$EXIT_STAT" = 0 ]]; then
        die "Make process failed"
    fi
}

case "${TRAVIS_OS_NAME}" in
"linux")
    build_standalone "$BUILDVARIANT"

    tar -zcvf "libraries_$BUILDVARIANT.tar.gz" ${BUILD_DIR}/build
;;
"osx")

;;
esac
