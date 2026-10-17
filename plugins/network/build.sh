#!/bin/bash

cd android
echo "Building for Android ..."
./build.plugin.sh || exit 1
cd ..

cd ios
echo "Building for iOS ..."
./build.sh || exit 1
cd ..

cd mac
echo "Building for Mac OS X ..."
./build.sh || exit 1
cd ..

cd win32
echo
echo "`basename $0`: You'll need to build for Win32 on a Windows host"
cd ..

echo
echo "Remember to do 'hg commit --subrepos'"
