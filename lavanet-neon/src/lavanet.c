/*
 * lavanet.c
 *
 *      Author: Robert 'Bobby' Zenz
 *
 */

#include <X11/Xlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "vroot.h"

#define ulong unsigned long
#define uchar unsigned char
#define FALSE 0
#define TRUE 1
#define BLACK 0x000000
#define WHITE 0xFFFFFF
#define THRESHOLD 0

struct vector {
    float x;
    float y;
};

struct line {
    float startX;
    float startY;
    float endX;
    float endY;
    double value;
};

typedef struct {
    double red, blue, green;
} Color;

// Global, sorry.
int debug = FALSE;
int pointCount = 200;
float minimumDistance = 100;
int targetFps = 60;
float topChange = 0.02f;
float topSpeed = 0.1f;
double hue = 0.0;

float get_random() { return ((float)rand() / (float)RAND_MAX - 0.5f) * 2; }

ulong make_color(uchar red, uchar green, uchar blue)
{
    ulong color = red;
    color = color << 8;

    color |= green;
    color = color << 8;

    color |= blue;

    return color;
}

float sign(float x)
{
    float val = x > 0;
    return val - (x < 0);
}

int float_to_color_value(double in)
{
    return (in >= 1.0 ? 255 : (in <= 0.0 ? 0 : (int)floor(in * 255.0)));
}


// ######################################################################
// T. Nathan Mundhenk
// mundhenk@usc.edu
// C/C++ Macro HSV to RGB

Color hsv_to_rgb(double h, double s, double v)
{
    if (v == 0) {
        return (Color){ .red = 0, .blue = 0, .green = 0 };
    }
    else if (s == 0) {
        return (Color){ .red = v, .blue = v, .green = v };
    }

    const double hf = h / 60.0;
    const int i = (int)floor(hf);
    const double f = hf - i;
    const double pv = v * (1 - s);
    const double qv = v * (1 - s * f);
    const double tv = v * (1 - s * (1 - f));

    switch (i) {
        case 0:
            return (Color){ .red = v, .green = tv, .blue = pv };
        case 1:
            return (Color){ .red = qv, .green = v, .blue = pv };
        case 2:
            return (Color){ .red = pv, .green = v, .blue = tv };
        case 3:
            return (Color){ .red = pv, .green = qv, .blue = v };
        case 4:
            return (Color){ .red = tv, .green = pv, .blue = v };
        case 5:
            return (Color){ .red = v, .green = pv, .blue = qv };
        case 6:
            return (Color){ .red = v, .green = tv, .blue = pv };
        case -1:
            return (Color){ .red = v, .green = pv, .blue = qv };
    }
    printf("i Value error in Pixel conversion, Value is %d!!", i);
    return (Color){ .red = 1, .green = 1, .blue = 1 };
}

void draw_lines(struct line *lines, int lineCount, Display *dpy, GC g,
                Pixmap pixmap, double hue)
{
    int idx;
    for (idx = 0; idx < lineCount; idx++) {
        struct line *line = &lines[idx];

        Color targetColor = hsv_to_rgb(hue, .9, line->value);
        int targetRed = targetColor.red * 255.0;
        int targetGreen = targetColor.green * 255.0;
        int targetBlue = targetColor.blue * 255.0;

        XSetForeground(dpy, g, make_color(targetRed, targetGreen, targetBlue));

        XDrawLine(dpy, pixmap, g, line->startX, line->startY, line->endX,
                  line->endY);
    }
}

int gather_lines(struct vector *points, struct line **lines)
{
    int counter = 0;
    int idx;
    for (idx = 0; idx < pointCount; idx++) {
        struct vector *pointA = &points[idx];
        int idx2 = 0;
        for (idx2 = idx + 1; idx2 < pointCount; idx2++) {
            struct vector *pointB = &points[idx2];

            // Check distance between points
            int distanceX = (int)fabs(pointA->x - pointB->x);
            int distanceY = (int)fabs(pointA->y - pointB->y);

            double distance = sqrt(pow(distanceX, 2) + pow(distanceY, 2));

            if (distance < minimumDistance) {
                counter++;

                *lines = realloc(*lines, counter * sizeof(struct line));
                struct line *newLine = &(*lines)[counter - 1];
                newLine->startX = pointA->x;
                newLine->startY = pointA->y;
                newLine->endX = pointB->x;
                newLine->endY = pointB->y;
                /* distance / minimumDistance by itself gives us zero constantly
                 * multiplying by 100 fixes this! but gives us 100
                 * HSV conversion takes a 1. - 0. value, so we divide again lmao
                 *
                 * fun times!
                 */
                newLine->value = floor(distance / minimumDistance * 100) / 100;
            }
        }
    }

    return counter;
}

int sort_lines(const void *a, const void *b)
{
    struct line *lineA = (struct line *)a;
    struct line *lineB = (struct line *)b;
    if (lineA->value == lineB->value) {
        return 0;
    } else if (lineA->value > lineB->value) {
        return -1;
    }

    return 1;
}

void move_points(struct vector *points, struct vector *velocities,
                 XWindowAttributes wa)
{
    int idx;
    for (idx = 0; idx < pointCount; idx++) {
        struct vector *velocity = &velocities[idx];

        velocity->x += get_random() * topChange;
        velocity->y += get_random() * topChange;

        if ((int)fabs(velocity->x) > topSpeed) {
            velocity->x = topSpeed * sign(velocity->x);
        }

        if ((int)fabs(velocity->y) > topSpeed) {
            velocity->y = topSpeed * sign(velocity->y);
        }

        struct vector *point = &points[idx];
        point->x += velocity->x;
        point->y += velocity->y;

        if (point->x < 0) {
            point->x = wa.width;
        }
        if (point->x > wa.width) {
            point->x = 0;
        }
        if (point->y < 0) {
            point->y = wa.height;
        }
        if (point->y > wa.height) {
            point->y = 0;
        }
    }
}

void parse_arguments(int argc, char *argv[])
{
    int idx;
    for (idx = 1; idx < argc; idx++) {
        if (strcasecmp(argv[idx], "--debug") == 0) {
            debug = TRUE;
        }
    }
}

int main(int argc, char *argv[])
{
    parse_arguments(argc, argv);

    // Some stuff
    int sleepFor = (int)1000 / targetFps * 1000;

    // Create our display
    Display *dpy = XOpenDisplay(getenv("DISPLAY"));

    char *xwin = getenv("XSCREENSAVER_WINDOW");

    int root_window_id = 0;

    if (xwin) {
        root_window_id = strtol(xwin, NULL, 0);
    }

    // Get the root window
    Window root;
    if (debug == FALSE) {
        // Get the root window
        // root = DefaultRootWindow(dpy);
        if (root_window_id == 0) {
            // root = DefaultRootWindow(dpy);
            printf("usage as standalone app: %s --debug\n", argv[0]);
            return EXIT_FAILURE;
        } else {
            root = root_window_id;
        }
    } else {
        // Let's create our own window.
        int screen = DefaultScreen(dpy);
        root = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 24, 48,
                                   1180, 700, 1, BlackPixel(dpy, screen),
                                   WhitePixel(dpy, screen));
        XMapWindow(dpy, root);
    }

    XSelectInput(dpy, root, ExposureMask | StructureNotifyMask);

    // Get the window attributes
    XWindowAttributes wa;
    XGetWindowAttributes(dpy, root, &wa);

    // Create the buffer
    Pixmap double_buffer =
        XCreatePixmap(dpy, root, wa.width, wa.height, wa.depth);

    // And new create our graphics context to draw on
    GC g = XCreateGC(dpy, root, 0, NULL);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand(tv.tv_sec);

    struct vector points[pointCount];
    struct vector velocities[pointCount];
    int counter = 0;
    for (counter = 0; counter < pointCount; counter++) {
        points[counter].x = rand() % wa.width;
        points[counter].y = rand() % wa.height;

        velocities[counter].x = get_random() * topSpeed;
        velocities[counter].y = get_random() * topSpeed;
    }

    // this is to terminate nicely:
    Atom wmDeleteMessage = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, root, &wmDeleteMessage, 1);

    while (TRUE) {
        XEvent event;
        if (XCheckWindowEvent(dpy, root, StructureNotifyMask, &event) ||
            XCheckTypedWindowEvent(dpy, root, ClientMessage,
                                   &event)) // needed to catch ClientMessage
        {
            if (event.type == ConfigureNotify) {
                XConfigureEvent xce = event.xconfigure;

                // This event type is generated for a variety of
                // happenings, so check whether the window has been
                // resized.

                if (xce.width != wa.width || xce.height != wa.height) {
                    wa.width = xce.width;
                    wa.height = xce.height;

                    XFreePixmap(dpy, double_buffer);
                    double_buffer =
                        XCreatePixmap(dpy, root, wa.width, wa.height, wa.depth);

                    continue;
                }
            } else if (event.type == ClientMessage) {
                if (event.xclient.data.l[0] == wmDeleteMessage) {
                    break;
                }
            }
        }

        hue = hue > 360. ? 0 : hue + .1;

        // Clear the pixmap used for double buffering
        XSetBackground(dpy, g, BLACK);
        XSetForeground(dpy, g, BLACK);
        XFillRectangle(dpy, double_buffer, g, 0, 0, wa.width, wa.height);

        // Move the points around
        move_points(points, velocities, wa);

        // Gather the lines and draw them
        struct line *lines = malloc(sizeof(struct line));
        int lineCount = gather_lines(points, &lines);
        qsort(lines, lineCount, sizeof(struct line), sort_lines);
        draw_lines(lines, lineCount, dpy, g, double_buffer, hue);
        free(lines);

        XCopyArea(dpy, double_buffer, root, g, 0, 0, wa.width, wa.height, 0, 0);
        XFlush(dpy);

        usleep(sleepFor);
    }

    // cleanup
    XFreePixmap(dpy, double_buffer);
    XFreeGC(dpy, g);
    XDestroyWindow(dpy, root);
    XCloseDisplay(dpy);

    return EXIT_SUCCESS;
}
