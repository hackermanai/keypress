
# Typing Latency

Minimal fixed-cell typing latency measurement tool for macOS.

Compile:
    xcrun clang -O2 tl.c -framework ApplicationServices -o tl

Run from Terminal:
    ./tl --pick --region-w 120 --region-h 80 --count 100 --out result.csv

macOS permissions required:

- System Settings -> Privacy & Security -> **Accessibility**
- System Settings -> Privacy & Security -> **Screen Recording**

