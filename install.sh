#!/bin/sh
set -eu

repo="hackermanai/typinglatency"
bin="tl"
install_dir="${TL_INSTALL_DIR:-$HOME/.local/bin}"

os="$(uname -s)"
arch="$(uname -m)"

case "$os:$arch" in
    Darwin:arm64)
        asset="tl-macos-arm64.tar.gz"
        package_dir="tl-macos-arm64"
        ;;
    Darwin:x86_64)
        asset="tl-macos-x86_64.tar.gz"
        package_dir="tl-macos-x86_64"
        ;;
    *)
        echo "Unsupported platform: $os $arch" >&2
        echo "Currently supported: macOS arm64, macOS x86_64" >&2
        exit 1
        ;;
esac

mkdir -p "$install_dir"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

url="https://github.com/${repo}/releases/latest/download/${asset}"

echo "Downloading $url"
curl -fL "$url" -o "$tmp/$asset"

tar -xzf "$tmp/$asset" -C "$tmp"

if [ ! -f "$tmp/$package_dir/$bin" ]; then
    echo "Downloaded package did not contain $bin" >&2
    exit 1
fi

install "$tmp/$package_dir/$bin" "$install_dir/$bin"

echo
echo "Installed: $install_dir/$bin"

if [ -x "$install_dir/$bin" ]; then
    "$install_dir/$bin" --version || true
fi

case ":$PATH:" in
    *":$install_dir:"*)
        ;;
    *)
        echo
        echo "Add this to your shell config if needed:"
        echo "  export PATH=\"$install_dir:\$PATH\""
        ;;
esac

echo
echo "macOS permissions required:"
echo " System Settings -> Privacy & Security -> Accessibility"
echo " System Settings -> Privacy & Security -> Screen Recording"
echo
echo "Example:"
echo " tl --pick --region-w 120 --region-h 80 --count 100 --out result.csv"
