
# Typing Latency

Minimal fixed-cell typing latency tester for macOS.
It sends a synthetic key event, watches a small screen region, and records the time until visible pixels change.

**This is a practical editor responsiveness test, not a lab-grade hardware/input-lag measurement.**

## Install

```
curl -fsSL https://raw.githubusercontent.com/hackermanai/typinglatency/main/install.sh | sh
```

Installs to:

```
~/.local/bin/tl
```

Add to PATH if needed:

```
export PATH="$HOME/.local/bin:$PATH"
```

## Permissions

Required on macOS:

- System Settings -> Privacy & Security -> **Accessibility**
- System Settings -> Privacy & Security -> **Screen Recording**

Restart Terminal after granting permissions if needed.

## Run

Pick the target region interactively:

```
tl --pick --region-w 120 --region-h 80 --count 100 --out result.csv
```

Or use fixed coordinates:

```
tl --x 930 --y 420 --region-w 120 --region-h 80 --count 100 --out result.csv
```

Pick a point near the caret/glyph position where the typed character appears.

```
michael@Michaels-Mac-mini ~ % tl --pick --count 10
Click the target caret/glyph position in the editor...
Picked x=1955 y=1111
Typing Latency fixed-cell test
Picked/using x=1955 y=1111
Watch region: (1935,1081 40x60)
Threshold: 10, min changed pixels: 1
Running...
  0 appear      29.720 ms  best=666 changed=122  ok
  0 disappear    9.131 ms  best=666 changed=122  ok
  1 appear      10.860 ms  best=666 changed=122  ok
  1 disappear   11.232 ms  best=666 changed=122  ok
  2 appear      13.478 ms  best=666 changed=122  ok
  2 disappear   11.464 ms  best=666 changed=122  ok
  3 appear      16.842 ms  best=666 changed=122  ok
  3 disappear   14.025 ms  best=666 changed=122  ok
  4 appear      11.434 ms  best=666 changed=122  ok
  4 disappear   11.471 ms  best=666 changed=122  ok
  5 appear      11.003 ms  best=666 changed=122  ok
  5 disappear   11.319 ms  best=666 changed=122  ok
  6 appear      13.355 ms  best=666 changed=122  ok
  6 disappear   11.202 ms  best=666 changed=122  ok
  7 appear      10.763 ms  best=666 changed=122  ok
  7 disappear   11.012 ms  best=666 changed=122  ok
  8 appear      10.886 ms  best=666 changed=122  ok
  8 disappear   10.380 ms  best=666 changed=122  ok
  9 appear      11.125 ms  best=666 changed=122  ok
  9 disappear   10.114 ms  best=666 changed=122  ok
Saved: latency.csv
```

## Output

CSV columns:

```
index,phase,watch_x,watch_y,watch_w,watch_h,latency_ms,best_dist,changed_pixels,status
```

Each iteration records:

- appear -> type `.`
- disappear —> press `Backspace`

`latency_ms` is the time from posting the key event until the watched region changes.

## Options

| Command / option | Description |
|---|---|
| `tl --pick [options]` | Run with interactive target selection |
| `tl --x N --y N [options]` | Run with fixed target coordinates |
| `--help`, `-h` | Show help |
| `--version` | Show version |
| `--pick` | Choose target by clicking |
| `--x N` | Watch center x coordinate |
| `--y N` | Watch center y coordinate |
| `--count N` | Default: `100` |
| `--period-ms N` | Default: `100` |
| `--timeout-ms N` | Default: `1000` |
| `--region-w N` | Default: `40` |
| `--region-h N` | Default: `60` |
| `--threshold N` | Default: `10` |
| `--min-changed-pixels N` | Default: `1` |
| `--out file.csv` | Default: `latency.csv` |

For example:

```
tl --pick --region-w 120 --region-h 80 --count 100 --out result.csv
```

## Build

Compile directly:

```
xcrun clang -O2 tl.c -framework ApplicationServices -framework CoreFoundation -o tl
```

## Notes

Results include OS input dispatch, editor processing, rendering, compositor behavior, and capture polling.
For comparable runs, keep the editor still, use the same region/count, and compare medians over multiple runs.
A timeout usually means the watched region did not change enough: increase the region size, lower the threshold, or pick closer to the inserted glyph.

