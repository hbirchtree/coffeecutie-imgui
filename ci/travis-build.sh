#!/bin/bash

SOURCE_DIR="$PWD"
BUILD_DIR="$SOURCE_DIR/multi_build"

CI_DIR="$SOURCE_DIR/$MAKEFILE_DIR"

COFFEE_DIR="$BUILD_DIR/coffee_lib"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

GITHUBPY="$(dirname $0)/github_api.py"
QTHUB_DOCKER="hbirch/coffeecutie:qthub-client"
MAKEFILE="Makefile.standalone"

function die()
{
    echo " * " $@
    exit 1
}

function notify()
{
    echo " * " $@
}

function debug()
{
    echo $@ > /dev/stderr
}

function requires()
{
    for prog in $@; do
        local mute=$(which $prog)
        if [[ ! "$?" = "0" ]]; then
            die "Could not find program: $prog"
        fi
    done
}

#
# 1: Github repo slug
# 2: Sub-path
#
function github_curl()
{
    local output="$(curl \
        -s \
        -i \
        -o - \
        -X GET \
        -H "Accept: application/vnd.github.v3+json" \
        -H "Authorization: token $GITHUB_TOKEN" \
        https://api.github.com/repos/$1/$2)"

    debug $output

    local code=$(cat download-info.txt | grep HTTP | awk '{print $2}')

    #debug $output
    echo $(echo "$output" | grep body)
}

function github_filter_latest()
{
    local input=$(cat)
    #debug $input
    local tags=$(echo $input | jq -r '.[]?.tag_name')
    local string=""
    for tag in $tags; do
        echo "||$tag"
    done
}

function github_filter_asset()
{
    local input=$(cat)
    local IFS=$'\n'
    for rel in $(jq -c '.[]?'); do
        local rel_name=$(echo "$rel" | jq -r '.tag_name')
        if [[ "$?" = 0 ]] && [[ ! -z $(echo $rel_name | grep "$1") ]]; then
            local vars=$(echo "$rel" \
                | jq -c '.assets[] | {name: .name, url: .browser_download_url, id: .id}')

            for asset in $vars; do
                local ID=$(echo "$asset" | jq -r '.id')
                local NAME=$(echo "$asset" | jq -r '.name')
                local URL=$(echo "$asset" | jq -r '.url')
                echo "|$1|$ID||$NAME||$URL"
            done
        fi
    done
}

function github_curl_frontend()
{
    # It's a read-only application, so we only need these
    case "$1" in
    "list")
        case "$2" in
        "release")
            github_curl "$3" "releases" | github_filter_latest
        ;;
        "asset")
            local slug=$(echo $3 | cut -d':' -f 1)
            local release=$(echo $3 | cut -d':' -f 2)
            github_curl "$slug" "releases" | github_filter_asset "$release"
        ;;
        esac
    ;;
    "pull")
        case "$2" in
        "asset")
            local data=$(github_curl "$3" "release" | github_filter_asset)
            local filename=$(echo $data | cut -d'|' -f 5)
            local url=$(echo $data | cut -d'|' -f 7)

            wget "$url" -O "$filename"
        ;;
        esac
    ;;
    esac
}

function github_api()
{
    case "${TRAVIS_OS_NAME}" in
    *)
        # Python client
        "$GITHUBPY" --api-token "$GITHUB_TOKEN" $@
    ;;
    "linux")
        # Qt/C++ client
        docker run --rm -v "$PWD:/data" $QTHUB_DOCKER --api-token "$GITHUB_TOKEN" $@
    ;;
    *)
        # curl/wget client
        github_curl_frontend $@
    ;;
    esac
}

function download_libraries()
{
    notify "Downloading library ${1}:${BUILDVARIANT}"
    local LATEST_RELEASE="$(github_api list release ${1} | head -1 | cut -d'|' -f 3)"
    echo Release $LATEST_RELEASE
    local CURRENT_ASSET="$(github_api list asset ${1}:${LATEST_RELEASE} | grep $BUILDVARIANT)"
    echo Asset $CURRENT_ASSET

    [[ -z $CURRENT_ASSET ]] && die "Failed to find ${1} for $BUILDVARIANT"

    notify "Found assets: $CURRENT_ASSET (from $LATEST_RELEASE)"
    local ASSET_ID="$(echo $CURRENT_ASSET | cut -d'|' -f 3)"
    local ASSET_FN="$PWD/$(echo $CURRENT_ASSET | cut -d'|' -f 5)"

    github_api pull asset $1 $ASSET_ID

    local PREV_WD="$PWD"

    mkdir -p "$COFFEE_DIR"
    cd "$COFFEE_DIR"

    tar -xvf "$ASSET_FN"
    mv -f build/* .
    rm -f "$ASSET_FN"

    cd "$PREV_WD"
}

function build_standalone()
{
    OLD_IFS=$IFS
    IFS='\;'
    for dep in $DEPENDENCIES; do
        IFS=$OLD_IFS download_libraries "$dep"
    done

    make -f "$CI_DIR/$MAKEFILE" \
        -e SOURCE_DIR="$SOURCE_DIR" \
        -e COFFEE_DIR="$COFFEE_DIR" $@

    # We want to exit if the Make process fails horribly
    # Should also signify to Travis/CI that something went wrong
    EXIT_STAT=$?
    [[ ! "$EXIT_STAT" = 0 ]] && die "Make process failed"
}

function main()
{
    local LIB_ARCHIVE="$TRAVIS_BUILD_DIR/libraries_$BUILDVARIANT.tar.gz"

    case "${TRAVIS_OS_NAME}" in
    "linux")
        requires make docker tar python3
    ;;
    "osx")
        requires make tar python3
        MAKEFILE="Makefile.mac"
    ;;
    *)
        exit 1
    ;;
    esac

    build_standalone "$BUILDVARIANT"
    tar -zcvf "$LIB_ARCHIVE" -C ${BUILD_DIR} build/

    if [[ ! -z $MANUAL_DEPLOY ]]; then
        local SLUG=$(git -C "$SOURCE_DIR" remote show -n origin | grep 'Fetch URL' | sed -e 's/^.*github.com[:\/]//g' -e 's/\.git//g')
        [[ -z $SLUG ]] && die "Failed to get repo slug"

        local COMMIT_SHA=$(git -C "$SOURCE_DIR" rev-parse HEAD)
        [[ -z $COMMIT_SHA ]] && die "Failed to get commit SHA"

        local RELEASE="$(github_api list release $SLUG | head -1 | cut -d'|' -f 3)"
        [[ -z $RELEASE ]] && die "No releases to upload to"

        cd $(dirname $LIB_ARCHIVE)
        github_api push asset "$SLUG:$RELEASE" "$(basename $LIB_ARCHIVE)"
        github_api push status "$SLUG:$COMMIT_SHA" success "$BUILDVARIANT" \
                --gh-context "$MANUAL_CONTEXT"
    fi
}

main
