#!/bin/sh
set -eu

rm -rf dist
mkdir -p dist/build dist/pkg

xcrun clang -O2 keypress.c \
    -framework ApplicationServices \
    -framework CoreFoundation \
    -target arm64-apple-macos11 \
    -o dist/build/keypress-arm64

xcrun clang -O2 keypress.c \
    -framework ApplicationServices \
    -framework CoreFoundation \
    -target x86_64-apple-macos10.13 \
    -o dist/build/keypress-x86_64

lipo -create \
    dist/build/keypress-arm64 \
    dist/build/keypress-x86_64 \
    -output dist/build/keypress

chmod +x dist/build/keypress-arm64
chmod +x dist/build/keypress-x86_64
chmod +x dist/build/keypress

mkdir -p dist/pkg/keypress-macos-arm64
cp dist/build/keypress-arm64 dist/pkg/keypress-macos-arm64/keypress
cp README.md LICENSE dist/pkg/keypress-macos-arm64/

mkdir -p dist/pkg/keypress-macos-x86_64
cp dist/build/keypress-x86_64 dist/pkg/keypress-macos-x86_64/keypress
cp README.md LICENSE dist/pkg/keypress-macos-x86_64/

mkdir -p dist/pkg/keypress-macos-universal
cp dist/build/keypress dist/pkg/keypress-macos-universal/keypress
cp README.md LICENSE dist/pkg/keypress-macos-universal/

cd dist/pkg

tar -czf ../keypress-macos-arm64.tar.gz keypress-macos-arm64
tar -czf ../keypress-macos-x86_64.tar.gz keypress-macos-x86_64
tar -czf ../keypress-macos-universal.tar.gz keypress-macos-universal

cd ../..

shasum -a 256 dist/*.tar.gz
