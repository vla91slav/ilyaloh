#!/bin/bash

# This script requires depot_tools to be on path.

print_usage () {
  echo "Usage: create_cipd_united_package.sh <VERSION_TAG> [PATH_TO_SDK_DIR]"
  echo "  where:"
  echo "    - VERSION_TAG is the tag of the cipd packages, e.g. 28r6 or 31v1"
  echo "    - PATH_TO_SDK_DIR is the path to the sdk folder. If omitted, this defaults to"
  echo "                      your ANDROID_SDK_ROOT environment variable."
  echo ""
  echo "This script downloads the packages specified in packages.txt and uploads"
  echo "them to CIPD for linux, mac, and windows."
  echo ""
  echo "Manage the packages to download in 'packages.txt'. You can use"
  echo "'sdkmanager --list --include_obsolete' in cmdline-tools to list all available packages."
  echo "Packages should be listed in the format of <package-name>:<directory-to-upload>."
  echo "For example, build-tools;31.0.0:build-tools"
  echo "Multiple directories to upload can be specified by delimiting by additional ':'"
  echo ""
  echo "This script expects the cmdline-tools to be installed in your specified PATH_TO_SDK_DIR"
  echo "and should only be run on linux or macos hosts."
}

# Validate version is provided
if [[ $1 == "" ]]; then
  print_usage
  exit 1
fi

# Validate path contains depot_tools
if [[ `which cipd` == "" ]]; then
  echo "'cipd' command not found. depot_tools should be on the path."
  exit 1
fi

sdk_path=${2:-$ANDROID_SDK_ROOT}
version_tag=$1

# Validate directory contains all SDK packages
if [[ ! -d "$sdk_path" ]]; then
  echo "Android SDK at '$sdk_path' not found."
  print_usage
  exit 1
fi
if [[ ! -d "$sdk_path/cmdline-tools" ]]; then
  echo "SDK directory does not contain $sdk_path/cmdline-tools."
  print_usage
  exit 1
fi

platforms=("linux" "macosx" "windows")
package_file_name="packages.txt"

# Find the sdkmanager in cmdline-tools. We default to using latest if available.
sdkmanager_path="$sdk_path/cmdline-tools/latest/bin/sdkmanager"
find_results=()
while IFS= read -r line; do
  find_results+=("$line")
done < <(find "$sdk_path/cmdline-tools" -name sdkmanager)
i=0
while [ ! -f "$sdkmanager_path" ]; do
  if [ $i -ge ${#find_results[@]} ]; then
    echo "Unable to find sdkmanager in the SDK directory. Please ensure cmdline-tools is installed."
    exit 1
  fi
  sdkmanager_path="${find_results[$i]}"
  echo $sdkmanager_path
  ((i++))
done

# We create a new temporary SDK directory because the default working directory
# tends to not update/re-download packages if they are being used. This guarantees
# a clean install of Android SDK.
temp_dir=`mktemp -d -t android_sdk`

for platform in "${platforms[@]}"; do
  sdk_root="$temp_dir/sdk_$platform"
  upload_dir="$temp_dir/upload_$platform"
  echo "Creating temporary working directory for $platform: $sdk_root"
  mkdir $sdk_root
  mkdir $upload_dir
  mkdir $upload_dir/sdk
  export REPO_OS_OVERRIDE=$platform

  # Download all the packages with sdkmanager.
  for package in $(cat $package_file_name); do
    echo $package
    split=(${package//:/ })
    echo "Installing ${split[0]}"
    yes "y" | $sdkmanager_path --sdk_root=$sdk_root ${split[0]}

    # We copy only the relevant directories to a temporary dir
    # for upload. sdkmanager creates extra files that we don't need.
    array_length=${#split[@]}
    for (( i=1; i<${array_length}; i++ )); do
      cp -r "$sdk_root/${split[$i]}" "$upload_dir/sdk"
    done
  done

  # Special treatment for NDK to move to expected directory.
  mv $upload_dir/sdk/ndk-bundle $upload_dir
  mv $upload_dir/ndk-bundle $upload_dir/ndk

  # Accept all licenses to ensure they are generated and uploaded.
  yes "y" | $sdkmanager_path --licenses --sdk_root=$sdk_root
  cp -r "$sdk_root/licenses" "$upload_dir/sdk"

  # Mac uses a different sdkmanager name than the platform name used in gn.
  cipd_name="$platform-amd64"
  if [[ $platform == "macosx" ]]; then
    cipd_name="mac-amd64"
  fi
  echo "Uploading $cipd_name to CIPD"
  cipd create -in $upload_dir -name "flutter/android/sdk/all/$cipd_name" -install-mode copy -tag version:$version_tag

  rm -rf $sdk_root
  rm -rf $upload_dir
done
rm -rf $temp_dir
