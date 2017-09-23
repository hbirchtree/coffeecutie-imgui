$ErrorActionPreference = "Stop"

if($env:SAME_BUILD_DIR) {
    $BuildDir = $env:BUILD_DIR
}else{
    $BuildDir = "$env:BUILD_DIR\build_$env:BUILDVARIANT"
}

$DEPLOY_ASSET = "$env:APPVEYOR_BUILD_FOLDER\libraries_$env:BUILDVARIANT.zip"
$DEPLOY_ASSET_BIN = "$env:APPVEYOR_BUILD_FOLDER\binaries_$env:BUILDVARIANT.zip"

# First, compress the compiled files
$PrevWd = $Pwd
cd "$BuildDir\out"
7z a $DEPLOY_ASSET "*" -xr!bin -xr!packaged
7z a $DEPLOY_ASSET_BIN "*" -xr!share -xr!lib -xr!include
cd $PrevWd

# Next, we need to find the target tag and release
# We use Python to do most of the work...
$PYTHON="C:\Python36\python.exe"
$BUILDINFOPY = "$env:SOURCE_DIR\buildinfo.py"
$SCRIPT_DIR = (. $PYTHON $BUILDINFOPY --source-dir $env:SOURCE_DIR script_location )
$SCRIPT_DIR = "$env:SOURCE_DIR\$SCRIPT_DIR"

$GITHUBAPIPY = "$SCRIPT_DIR\github_api.py"

function github_api()
{
    . $PYTHON $GITHUBAPIPY --api-token "$env:GITHUB_TOKEN" $args
}

if ($env:APPVEYOR_PULL_REQUEST_NUMBER.Length -ne "0") {
    echo " * Won't deploy on pull request"
    exit
}

try{
    $TARGET_TAG = (github_api list tag $env:APPVEYOR_REPO_NAME "^$env:APPVEYOR_REPO_COMMIT$")
    echo " * Tag(s) found:"
    echo $TARGET_TAG
    $TARGET_TAG = $TARGET_TAG.Split("|")[1]

    if ($TARGET_TAG.Length -eq 0) {
        echo " * Could not find tag, will not deploy"
        exit
    }

    $TARGET_RELEASE = (github_api list release $env:APPVEYOR_REPO_NAME "^$TARGET_TAG$")
    echo " * Release(s) found:"
    echo $TARGET_RELEASE


    if ($TARGET_RELEASE.Length -eq 0) {
        echo " * Creating new release for tag"
        github_api push release $env:APPVEYOR_REPO_NAME $TARGET_TAG `
                        "Automatic Release" `
                        "Generated automatically"
	$TARGET_RELEASE = $TARGET_TAG
    }

    $TARGET_RELEASE = $TARGET_RELEASE.Split("|")[0]

    $DEPLOY_TARGET = $env:APPVEYOR_REPO_NAME+":"+$TARGET_TAG
    $FILEPATH = ([System.IO.Path]::GetDirectoryName($DEPLOY_ASSET))
    $FILENAME = ([System.IO.Path]::GetFileName($DEPLOY_ASSET))
	$FILENAME_BIN = ([System.IO.Path]::GetFileName($DEPLOY_ASSET_BIN))
    cd $FILEPATH
    echo " * Deploying $DEPLOY_ASSET to $TARGET_TAG"
    github_api push asset $DEPLOY_TARGET $FILENAME
	github_api push asset $DEPLOY_TARGET_BIN $FILENAME_BIN
}
catch {
    echo $_.Exception.Message
}
