#!/bin/csh -f

echo Distributing version $argv
echo Run this script from support/scripts!

cd ../..

echo Now in
pwd

mkdir impact_"$argv"

cd impact_"$argv"

echo Now in
pwd


echo Copying source files...
mkdir src
cp ../*.cpp src
cp ../*.h src
cp ../*.txt src
cp ../*.TXT src
rm src/TODO.TXT
cp ../*.c src
cp ../*.cfg src

mkdir src/support
mkdir src/support/enummaker
cp ../support/enummaker/Makefile src/support/enummaker
cp ../support/enummaker/enummaker.cpp src/support/enummaker
cp ../support/enummaker/enummaker.sln src/support/enummaker
cp ../support/enummaker/enummaker.vcproj src/support/enummaker
cp ../support/enummaker/enummaker.py src/support/enummaker
mkdir src/support/scripts
cp ../support/scripts/builddist.sh src/support/scripts
mkdir src/support/fontbuilder
cp ../support/fontbuilder/*.png src/support/fontbuilder
cp ../support/fontbuilder/*.ttf src/support/fontbuilder
cp ../support/fontbuilder/*.txt src/support/fontbuilder

mkdir src/lib
mkdir src/lib/windows
cp -R ../lib/* src/lib/windows
mkdir src/rooms
mkdir src/save
cp ../rooms/*.map src/rooms
mkdir src/windows
cp ../windows/*.sln src/windows
cp ../windows/*.vcproj src/windows
cp ../windows/*.rc src/windows
cp ../windows/*.ico src/windows
cp ../windows/*.bmp src/windows
cp ../windows/*.png src/windows

mkdir src/linux
cp ../linux/Makefile src/linux
cp ../linux/*.TXT src/linux
cp ../linux/*.sh src/linux
cp ../linux/*.png src/linux

echo Copying Windows Files

cp ../text.txt .
cp ../names.txt .
cp ../README.TXT .
cp ../7drlchanges.* .
mkdir save
mkdir rooms
cp ../rooms/*.map rooms
mkdir windows
cp ../windows/Release/*.exe windows
cp ../windows/*.dll windows
cp ../windows/*.lib windows
cp ../windows/*.png windows

# cp "c:/Program Files (x86)/Microsoft Visual Studio 14.0/VC/redist/x86/Microsoft.VC140.CRT"/* windows
cp "e:/MSVC/2019/Community/VC/Redist/MSVC/14.24.28127/x64/Microsoft.VC142.CRT"/* windows


cp ../impact.cfg .

mkdir music

echo All done!  Zipping.

cd ..
rm -f impact_"$argv".zip
7za a impact_"$argv".zip impact_"$argv"

