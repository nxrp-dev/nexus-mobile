#!/usr/bin/env bash -ex

mkdir libevent_ossl_build
cd libevent_ossl_build
ROOT="$(pwd)"

git clone git://git.openssl.org/openssl.git
cd openssl
git checkout OpenSSL_1_1_1h
curl https://github.com/openssl/openssl/commit/4907cf01f63cc966a40d67eb2e030c4d8eb1d528.patch | git apply -v --index
export MACOSX_DEPLOYMENT_TARGET=10.9
git clean -xfd
PREFIX="$ROOT/libs-x86"
./Configure darwin64-x86_64-cc --prefix="$PREFIX" no-shared  && make -j4 && make -j4 install
git clean -xfd
PREFIX="$ROOT/libs-arm64"
./Configure darwin64-arm64-cc --prefix="$PREFIX" no-shared  && make -j4 && make -j4 install

cd ..
curl -L https://github.com/libevent/libevent/releases/download/release-2.1.12-stable/libevent-2.1.12-stable.tar.gz -O

rm -rf libevent-2.1.12-stable
tar -zxvf libevent-2.1.12-stable.tar.gz
cd libevent-2.1.12-stable
PREFIX="$ROOT/libs-x86"
PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" ./configure CFLAGS=" -mmacosx-version-min=10.9  -I\"$PREFIX/include\" -target x86_64-apple-macos10.9  "  LDFLAGS="-L\"$PREFIX/lib\" "  --disable-shared  --enable-static --disable-debug-mode  --disable-clock-gettime  --prefix="$PREFIX" && make -j4 && make -j4 install
cd ..

rm -rf libevent-2.1.12-stable
tar -zxvf libevent-2.1.12-stable.tar.gz
cd libevent-2.1.12-stable
PREFIX="$ROOT/libs-arm64"
PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig" ./configure CFLAGS=" -mmacosx-version-min=10.9  -I\"$PREFIX/include\" -target arm64-apple-macos11  "  LDFLAGS="-L\"$PREFIX/lib\" "  --disable-shared  --enable-static --disable-debug-mode  --disable-clock-gettime  --prefix="$PREFIX" && make -j4 && make -j4 install
cd ..


mkdir -p libs/lib
cp -r libs-x86/include libs
for a in libs-x86/lib/*.a
do
	NAME=$(basename $a)
	lipo -create libs-x86/lib/$NAME libs-arm64/lib/$NAME -output libs/lib/$NAME
done
