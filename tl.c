
//////////////////////////////////////////////////////
//
// Typing Latency
//  Minimal fixed-cell typing latency measurement tool for macOS.
//
// Compile:
//  xcrun clang -O2 tl.c -framework ApplicationServices -framework CoreFoundation -o tl
//
// Run from Terminal:
//  ./tl --pick --region-w 120 --region-h 80 --count 100 --out result.csv
//
// macOS permissions required:
//  System Settings -> Privacy & Security -> Accessibility
//  System Settings -> Privacy & Security -> Screen Recording
//
//////////////////////////////////////////////////////

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int x;
    int y;
    bool pick;
    int count;
    int period_ms;
    int timeout_ms;
    int region_w;
    int region_h;
    int threshold;
    int min_changed_pixels;
    const char *out_path;
} Config;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} Pixel;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    Pixel *pixels;
} Region;

static uint64_t g_timebase_numer = 0;
static uint64_t g_timebase_denom = 0;

static void init_timebase(void) {
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    g_timebase_numer = info.numer;
    g_timebase_denom = info.denom;
}

static double now_ms(void) {
    uint64_t t = mach_absolute_time();
    return (double)t * (double)g_timebase_numer / (double)g_timebase_denom / 1000000.0;
}

static void sleep_ms(int ms) {
    if (ms <= 0) {
        return;
    }
    usleep((useconds_t)ms * 1000);
}

static int color_distance(Pixel a, Pixel b) {
    int dr = (int)a.r - (int)b.r;
    int dg = (int)a.g - (int)b.g;
    int db = (int)a.b - (int)b.b;
    return abs(dr) + abs(dg) + abs(db);
}

static void free_region(Region *r) {
    if (!r) {
        return;
    }
    free(r->pixels);
    r->pixels = NULL;
    r->x = 0;
    r->y = 0;
    r->w = 0;
    r->h = 0;
}

static bool capture_region(int x, int y, int w, int h, Region *out) {
    if (!out || w <= 0 || h <= 0) {
        return false;
    }

    memset(out, 0, sizeof(*out));

    out->x = x;
    out->y = y;
    out->w = w;
    out->h = h;
    out->pixels = calloc((size_t)w * (size_t)h, sizeof(Pixel));

    if (!out->pixels) {
        return false;
    }

    CGRect rect = CGRectMake((CGFloat)x, (CGFloat)y, (CGFloat)w, (CGFloat)h);
    CGImageRef image = CGDisplayCreateImageForRect(CGMainDisplayID(), rect);

    if (!image) {
        free_region(out);
        return false;
    }

    uint8_t *data = calloc((size_t)w * (size_t)h * 4, 1);

    if (!data) {
        CGImageRelease(image);
        free_region(out);
        return false;
    }

    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        data,
        (size_t)w,
        (size_t)h,
        8,
        (size_t)w * 4,
        color_space,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );

    CGColorSpaceRelease(color_space);

    if (!ctx) {
        free(data);
        CGImageRelease(image);
        free_region(out);
        return false;
    }

    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), image);

    for (int i = 0; i < w * h; i++) {
        out->pixels[i].r = data[i * 4 + 0];
        out->pixels[i].g = data[i * 4 + 1];
        out->pixels[i].b = data[i * 4 + 2];
        out->pixels[i].a = data[i * 4 + 3];
    }

    CGContextRelease(ctx);
    CGImageRelease(image);
    free(data);

    return true;
}

static int region_distance(
    const Region *a,
    const Region *b,
    int threshold,
    int *changed_pixels
) {
    if (!a || !b || !a->pixels || !b->pixels) {
        return 0;
    }

    if (a->w != b->w || a->h != b->h) {
        return 0;
    }

    int best = 0;
    int changed = 0;
    int n = a->w * a->h;

    for (int i = 0; i < n; i++) {
        int dist = color_distance(a->pixels[i], b->pixels[i]);

        if (dist > best) {
            best = dist;
        }

        if (dist >= threshold) {
            changed++;
        }
    }

    if (changed_pixels) {
        *changed_pixels = changed;
    }

    return best;
}

static bool post_key_unicode(CGKeyCode keycode, const UniChar *chars, int char_count) {
    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

    if (!source) {
        return false;
    }

    CGEventRef down = CGEventCreateKeyboardEvent(source, keycode, true);
    CGEventRef up = CGEventCreateKeyboardEvent(source, keycode, false);

    if (!down || !up) {
        if (down) {
            CFRelease(down);
        }

        if (up) {
            CFRelease(up);
        }

        CFRelease(source);
        return false;
    }

    if (chars && char_count > 0) {
        CGEventKeyboardSetUnicodeString(down, char_count, chars);
        CGEventKeyboardSetUnicodeString(up, char_count, chars);
    }

    CGEventSetIntegerValueField(down, kCGKeyboardEventAutorepeat, 0);
    CGEventSetIntegerValueField(up, kCGKeyboardEventAutorepeat, 0);

    CGEventPost(kCGSessionEventTap, down);
    usleep(2000);
    CGEventPost(kCGSessionEventTap, up);

    CFRelease(down);
    CFRelease(up);
    CFRelease(source);

    return true;
}

static bool post_period_key(void) {
    UniChar ch = '.';
    return post_key_unicode((CGKeyCode)47, &ch, 1);
}

static bool post_backspace_key(void) {
    return post_key_unicode((CGKeyCode)51, NULL, 0);
}

static bool wait_until_region_differs_since(
    const Region *baseline,
    double start_ms,
    int threshold,
    int min_changed_pixels,
    int timeout_ms,
    double *latency_ms,
    int *out_best_dist,
    int *out_changed_pixels
) {
    int max_best_dist = 0;
    int max_changed_pixels = 0;

    for (;;) {
        Region current = {0};

        if (!capture_region(
                baseline->x,
                baseline->y,
                baseline->w,
                baseline->h,
                &current
        )) {
            if (latency_ms) {
                *latency_ms = -1.0;
            }

            return false;
        }

        int changed_pixels = 0;
        int best_dist = region_distance(
            baseline,
            &current,
            threshold,
            &changed_pixels
        );

        free_region(&current);

        if (best_dist > max_best_dist) {
            max_best_dist = best_dist;
        }

        if (changed_pixels > max_changed_pixels) {
            max_changed_pixels = changed_pixels;
        }

        if (best_dist >= threshold && changed_pixels >= min_changed_pixels) {
            if (latency_ms) {
                *latency_ms = now_ms() - start_ms;
            }

            if (out_best_dist) {
                *out_best_dist = best_dist;
            }

            if (out_changed_pixels) {
                *out_changed_pixels = changed_pixels;
            }

            return true;
        }

        if ((now_ms() - start_ms) > (double)timeout_ms) {
            if (latency_ms) {
                *latency_ms = -1.0;
            }

            if (out_best_dist) {
                *out_best_dist = max_best_dist;
            }

            if (out_changed_pixels) {
                *out_changed_pixels = max_changed_pixels;
            }

            return false;
        }

        usleep(500);
    }
}

static bool measure_one_phase(
    FILE *out,
    int index,
    const char *phase,
    int rx,
    int ry,
    int rw,
    int rh,
    int threshold,
    int min_changed_pixels,
    int timeout_ms,
    bool (*post_key)(void)
) {
    Region baseline = {0};

    if (!capture_region(rx, ry, rw, rh, &baseline)) {
        fprintf(
            out,
            "%d,%s,%d,%d,%d,%d,-1,-1,-1,capture_failed\n",
            index,
            phase,
            rx,
            ry,
            rw,
            rh
        );

        fprintf(stderr, "%s %d: capture failed\n", phase, index);
        return false;
    }

    double start_ms = now_ms();

    if (!post_key()) {
        free_region(&baseline);

        fprintf(
            out,
            "%d,%s,%d,%d,%d,%d,-1,-1,-1,key_failed\n",
            index,
            phase,
            rx,
            ry,
            rw,
            rh
        );

        fprintf(stderr, "%s %d: key post failed\n", phase, index);
        return false;
    }

    double latency_ms = -1.0;
    int best_dist = 0;
    int changed_pixels = 0;

    bool ok = wait_until_region_differs_since(
        &baseline,
        start_ms,
        threshold,
        min_changed_pixels,
        timeout_ms,
        &latency_ms,
        &best_dist,
        &changed_pixels
    );

    free_region(&baseline);

    fprintf(
        out,
        "%d,%s,%d,%d,%d,%d,%.3f,%d,%d,%s\n",
        index,
        phase,
        rx,
        ry,
        rw,
        rh,
        latency_ms,
        best_dist,
        changed_pixels,
        ok ? "ok" : "timeout"
    );

    fflush(out);

    printf(
        "%3d %-9s %8.3f ms  best=%d changed=%d  %s\n",
        index,
        phase,
        latency_ms,
        best_dist,
        changed_pixels,
        ok ? "ok" : "timeout"
    );

    fflush(stdout);

    return ok;
}

static bool pick_mouse_position(int *out_x, int *out_y) {
    printf("Click the target caret/glyph position in the editor...\n");
    fflush(stdout);

    // Wait until mouse is not already pressed.
    while (CGEventSourceButtonState(
        kCGEventSourceStateCombinedSessionState,
        kCGMouseButtonLeft
    )) {
        usleep(10000);
    }

    // Wait for left click.
    while (!CGEventSourceButtonState(
        kCGEventSourceStateCombinedSessionState,
        kCGMouseButtonLeft
    )) {
        usleep(10000);
    }

    CGEventRef event = CGEventCreate(NULL);

    if (!event) {
        fprintf(stderr, "Could not read mouse position.\n");
        return false;
    }

    CGPoint p = CGEventGetLocation(event);
    CFRelease(event);

    if (out_x) {
        *out_x = (int)p.x;
    }

    if (out_y) {
        *out_y = (int)p.y;
    }

    printf("Picked x=%d y=%d\n", (int)p.x, (int)p.y);
    fflush(stdout);

    // Wait for mouse release so the following key events do not race the click.
    while (CGEventSourceButtonState(
        kCGEventSourceStateCombinedSessionState,
        kCGMouseButtonLeft
    )) {
        usleep(10000);
    }

    // Let the click focus/place caret.
    sleep_ms(300);

    return true;
}

static void usage(const char *argv0) {
    fprintf(
        stderr,
        "Usage:\n"
        "  %s --pick [options]\n"
        "  %s --x N --y N [options]\n\n"
        "Options:\n"
        "  --help, -h                show this help\n"
        "  --version                 show version\n"
        "  --pick                    click target position interactively\n"
        "  --count N                 default: 100\n"
        "  --period-ms N             default: 100\n"
        "  --timeout-ms N            default: 1000\n"
        "  --region-w N              default: 40\n"
        "  --region-h N              default: 60\n"
        "  --threshold N             default: 10\n"
        "  --min-changed-pixels N    default: 1\n"
        "  --out file.csv            default: latency.csv\n\n"
        "Examples:\n"
        "  %s --pick --region-w 120 --region-h 80 --count 50 --out result.csv\n"
        "  %s --x 930 --y 420 --region-w 120 --region-h 80 --count 50 --out result.csv\n",
        argv0,
        argv0,
        argv0,
        argv0
    );
}

static bool parse_args(int argc, char **argv, Config *cfg) {
    cfg->x = -1;
    cfg->y = -1;
    cfg->pick = false;
    cfg->count = 100;
    cfg->period_ms = 100;
    cfg->timeout_ms = 1000;
    cfg->region_w = 40;
    cfg->region_h = 60;
    cfg->threshold = 10;
    cfg->min_changed_pixels = 1;
    cfg->out_path = "latency.csv";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pick") == 0) {
            cfg->pick = true;
        } else if (strcmp(argv[i], "--x") == 0 && i + 1 < argc) {
            cfg->x = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--y") == 0 && i + 1 < argc) {
            cfg->y = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            cfg->count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--period-ms") == 0 && i + 1 < argc) {
            cfg->period_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc) {
            cfg->timeout_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--region-w") == 0 && i + 1 < argc) {
            cfg->region_w = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--region-h") == 0 && i + 1 < argc) {
            cfg->region_h = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--threshold") == 0 && i + 1 < argc) {
            cfg->threshold = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--min-changed-pixels") == 0 && i + 1 < argc) {
            cfg->min_changed_pixels = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            cfg->out_path = argv[++i];
        } else {
            return false;
        }
    }

    return (cfg->pick || (cfg->x >= 0 && cfg->y >= 0)) &&
           cfg->count > 0 &&
           cfg->period_ms >= 0 &&
           cfg->timeout_ms > 0 &&
           cfg->region_w > 0 &&
           cfg->region_h > 0 &&
           cfg->threshold >= 0 &&
           cfg->min_changed_pixels > 0;
}

#define TL_VERSION "0.1.0"

int main(int argc, char **argv) {
    Config cfg;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("Typing Latency %s\n", TL_VERSION);
            return 0;
        }
    
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    if (!parse_args(argc, argv, &cfg)) {
        usage(argv[0]);
        return 1;
    }

    init_timebase();

    if (cfg.pick) {
        if (!pick_mouse_position(&cfg.x, &cfg.y)) {
            return 1;
        }

        // Let the click focus the editor and place the caret.
        sleep_ms(350);
    }

    FILE *out = fopen(cfg.out_path, "w");

    if (!out) {
        perror("fopen");
        return 1;
    }

    fprintf(
        out,
        "index,phase,watch_x,watch_y,watch_w,watch_h,latency_ms,best_dist,changed_pixels,status\n"
    );

    int rx = cfg.x - cfg.region_w / 2;
    int ry = cfg.y - cfg.region_h / 2;
    int rw = cfg.region_w;
    int rh = cfg.region_h;

    printf("Typing Latency fixed-cell test\n");
    printf("Picked/using x=%d y=%d\n", cfg.x, cfg.y);
    printf("Watch region: (%d,%d %dx%d)\n", rx, ry, rw, rh);
    printf("Threshold: %d, min changed pixels: %d\n", cfg.threshold, cfg.min_changed_pixels);
    printf("Running...\n");
    fflush(stdout);

    for (int i = 0; i < cfg.count; i++) {
        bool appear_ok = measure_one_phase(
            out,
            i,
            "appear",
            rx,
            ry,
            rw,
            rh,
            cfg.threshold,
            cfg.min_changed_pixels,
            cfg.timeout_ms,
            post_period_key
        );

        sleep_ms(cfg.period_ms);

        bool disappear_ok = measure_one_phase(
            out,
            i,
            "disappear",
            rx,
            ry,
            rw,
            rh,
            cfg.threshold,
            cfg.min_changed_pixels,
            cfg.timeout_ms,
            post_backspace_key
        );

        sleep_ms(cfg.period_ms);

        (void)appear_ok;
        (void)disappear_ok;
    }

    fclose(out);

    printf("Saved: %s\n", cfg.out_path);
    return 0;
}

