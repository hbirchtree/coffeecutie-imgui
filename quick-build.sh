#!/bin/bash

function build_info()
{
    $(dirname $0)/buildinfo.py $@ 2>/dev/null
}

TRAVIS_OS_NAME=linux
BUILDVARIANT=$1
DEPENDENCIES=$(build_info dependencies)
MAKEFILE_DIR=$(build_info makefile_location)
SCRIPT_DIR=

[ -z $MAKEFILE_DIR ] && echo "No Makefile directory found" && exit 1

export TRAVIS_OS_NAME
export BUILDVARIANT
export DEPENDENCIES
export MAKEFILE_DIR

$(build_info script_location)/travis-build.sh
