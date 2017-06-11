#!/bin/bash

case "${TRAVIS_OS_NAME}" in
"linux")
    echo " * Travis/CI people, if you are seeing this, some systems require extended binary format support to work :/"
    docker run --rm --privileged multiarch/qemu-user-static:register
;;
"osx")
    brew install sdl2 cmake ninja openssl swig@3.04 ruby@2.3
    gem install github_api
    exit 0
;;
esac
