#!/bin/sh
set -eu

version="${1:-0.1.0}"

rm -rf dist
mkdir -p dist/build dist/pkg

xcrun clang -O2 tl.c \
  -framework ApplicationServices \
  -framework CoreFoundation \
  -target arm64-apple-macos11 \
  -o dist/build/tl-arm64

xcrun clang -O2 tl.c \
  -framework ApplicationServices \
  -framework CoreFoundation \
  -target x86_64-apple-macos10.13 \
  -o dist/build/tl-x86_64

lipo -create \
  dist/build/tl-arm64 \
  dist/build/tl-x86_64 \
  -output dist/build/tl

chmod +x dist/build/tl-arm64
chmod +x dist/build/tl-x86_64
chmod +x dist/build/tl

mkdir -p dist/pkg/tl-macos-arm64
cp dist/build/tl-arm64 dist/pkg/tl-macos-arm64/tl
cp README.md LICENSE dist/pkg/tl-macos-arm64/

mkdir -p dist/pkg/tl-macos-x86_64
cp dist/build/tl-x86_64 dist/pkg/tl-macos-x86_64/tl
cp README.md LICENSE dist/pkg/tl-macos-x86_64/

mkdir -p dist/pkg/tl-macos-universal
cp dist/build/tl dist/pkg/tl-macos-universal/tl
cp README.md LICENSE dist/pkg/tl-macos-universal/

cd dist/pkg

tar -czf ../tl-macos-arm64.tar.gz tl-macos-arm64
tar -czf ../tl-macos-x86_64.tar.gz tl-macos-x86_64
tar -czf ../tl-macos-universal.tar.gz tl-macos-universal

cd ../..

shasum -a 256 dist/*.tar.gz
