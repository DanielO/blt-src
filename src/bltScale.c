/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * bltScale.c --
 *
 *      This module implements a range scale for BLT.
 *
 * Copyright 2018 George A. Howlett. All rights reserved.  
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions are
 *   met:
 *
 *   1) Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2) Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the
 *      distribution.
 *   3) Neither the name of the authors nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *   4) Products derived from this software may not be called "BLT" nor may
 *      "BLT" appear in their names without specific prior written
 *      permission from the author.
 *
 *   THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY EXPRESS OR IMPLIED
 *   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* FIXME
 *      snap-to-tick
 *      show min/max arrow values.
 *      format for values?
 *      don't use -show to display value and grip in bindings. 
 */

#define BUILD_BLT_TK_PROCS 1

#include "bltInt.h"

#include "bltMath.h"

#ifdef HAVE_CTYPE_H
  #include <ctype.h>
#endif  /* HAVE_CTYPE_H */

#ifdef HAVE_STRING_H
  #include <string.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_FLOAT_H
  #include <float.h>
#endif /* HAVE_FLOAT_H */

#include <X11/Xutil.h>
#include "bltAlloc.h"
#include "bltBind.h"
#include "bltPaintBrush.h"
#include "bltPalette.h"
#include "bltPicture.h"
#include "bltBg.h"
#include "bltSwitch.h"

#include <time.h>
#include <sys/time.h>

#define MAXTICKS        10001

#define FCLAMP(x)       ((((x) < 0.0) ? 0.0 : ((x) > 1.0) ? 1.0 : (x)))
#define VAR_FLAGS (TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS)

/*
 * Round x in terms of units
 */
#define UROUND(x,u)     (round((x)/(u))*(u))
#define UCEIL(x,u)      (ceil((x)/(u))*(u))
#define UFLOOR(x,u)     (floor((x)/(u))*(u))

#define NUMDIGITS       15              /* Specifies the number of digits
                                         * of accuracy used when outputting
                                         * axis tick labels. */
#define AXIS_PAD_TITLE          2       /* Padding for axis title. */
#define TICK_PAD                2
#define COLORBAR_PAD            4
#define PADX                    4
#define PADY                    2

#define HORIZONTAL(s)   (((s)->flags & VERTICAL) == 0) 

#define SECONDS_SECOND        (1)
#define SECONDS_MINUTE        (60)
#define SECONDS_HOUR          (SECONDS_MINUTE * 60)
#define SECONDS_DAY           (SECONDS_HOUR * 24)
#define SECONDS_WEEK          (SECONDS_DAY * 7)
#define SECONDS_MONTH         (SECONDS_DAY * 30)
#define SECONDS_YEAR          (SECONDS_DAY * 365)

static const int numDaysMonth[2][13] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31}
} ;
static const int numDaysYear[2] = { 365, 366 };

#   define EPOCH           1970
#   define EPOCH_WDAY      4            /* Thursday. */

#define IsLeapYear(y) \
        ((((y) % 4) == 0) && ((((y) % 100) != 0) || (((y) % 400) == 0)))

#define REDRAW_PENDING  (1<<0)          /* Indicates the scale needs to be
                                         * redrawn at the next idle
                                         * point. */
#define LAYOUT_PENDING  (1<<1)          /* Indicates the scale coordinates
                                         * needs to be recomputed at the
                                         * next idle point. */
#define GEOMETRY        (1<<2)
#define VERTICAL        (1<<3)          /* Scale is oriented vertically. */
#define NORMAL          (1<<4)          /* Scale is drawn in its normal
                                         * foreground/background colors and
                                         * relief.*/
#define DISABLED        (1<<5)          /* Scale is drawn in its disabled
                                         * foreground/background colors. */
#define ACTIVE          (1<<6)          /* Scale is drawn in its active
                                         * foreground/background colors and
                                         * relief. */
#define STATE_MASK      (NORMAL|ACTIVE|DISABLED)              

#define TIGHT           (1<<7)          /* Axis line stops at designated
                                         * outer interval limits. Otherwise
                                         * the axis line will extend in
                                         * both directions to the next
                                         * major tick. */
#define DECREASING      (1<<8)          /* Axis is decreasing in order. */
#define EXTERIOR        (1<<9)          /* Ticks are exterior from colorbar. */
#define LABELOFFSET     (1<<10)
#define FOCUS           (1<<11)          /* Widget currently has focus. */

#define ACTIVE_MAXARROW (1<<12)         /* Draw max arrow with active
                                         * color. */
#define ACTIVE_MINARROW (1<<13)         /* Draw min arrow with active
                                         * color. */
#define ACTIVE_GRIP     (1<<14)         /* Draw grip with active background
                                         * and relief. */
#define ACTIVE_VALUE    (1<<15)         /* Draw value with active colors. */

#define AUTO_MAJOR      (1<<16)         /* Auto-generate major ticks. */
#define AUTO_MINOR      (1<<17)         /* Auto-generate minor ticks. */

#define SHOW_COLORBAR   (1<<20)
#define SHOW_MARK       (1<<21)
#define SHOW_GRIP       (1<<22)
#define SHOW_MAXARROW   (1<<23)
#define SHOW_MINARROW   (1<<24)
#define SHOW_TICKS      (1<<25)
#define SHOW_TICKLABELS (1<<26)
#define SHOW_TITLE      (1<<27)
#define SHOW_VALUE      (1<<28)
#define SHOW_ALL        (SHOW_COLORBAR|SHOW_MARK|SHOW_GRIP|SHOW_TICKS|\
                         SHOW_MAXARROW|SHOW_MINARROW|\
                         SHOW_TICKLABELS|SHOW_TITLE|SHOW_VALUE)
#define SHOW_ARROWS     (SHOW_MAXARROW|SHOW_MINARROW)


#define DEF_ACTIVE_BG           STD_ACTIVE_BACKGROUND
#define DEF_ACTIVE_FG           STD_ACTIVE_FOREGROUND
#define DEF_ACTIVE_GRIP_BG      STD_ACTIVE_BACKGROUND
#define DEF_ACTIVE_GRIP_FG      STD_ACTIVE_FOREGROUND
#define DEF_ACTIVE_VALUE_FG     STD_ACTIVE_FOREGROUND
#define DEF_ACTIVE_VALUE_BG     RGB_WHITE
#define DEF_ACTIVE_MAXARROW_COLOR RGB_BLUE
#define DEF_ACTIVE_MINARROW_COLOR RGB_RED
#define DEF_ACTIVE_RELIEF       "raised"
#define DEF_AXISLINE_COLOR      RGB_GREY30
#define DEF_AXISLINE_WIDTH      "0.03i"
#define DEF_BORDERWIDTH         "2"
#define DEF_COLORBAR_THICKNESS  "0.15i"
#define DEF_COMMAND             (char *)NULL
#define DEF_CURSOR              (char *)NULL
#define DEF_DECREASING          "0"
#define DEF_DISABLED_BG         STD_DISABLED_BACKGROUND
#define DEF_DISABLED_FG         STD_DISABLED_FOREGROUND
#define DEF_DIVISIONS           "10"
#define DEF_FORMAT_COMMAND      (char *)NULL
#define DEF_GRIP_BG             STD_NORMAL_BACKGROUND
#define DEF_GRIP_RELIEF         "raised"
#define DEF_GRIP_BORDERWIDTH    "2"
#define DEF_HIDE                ""
#define DEF_HIGHLIGHT_BACKGROUND STD_NORMAL_BACKGROUND
#define DEF_HIGHLIGHT_COLOR     RGB_BLACK
#define DEF_HIGHLIGHT_WIDTH     "0"
#define DEF_HEIGHT              "0"
#define DEF_LOOSE               "0"
#define DEF_MARK_COLOR          "0x80808080"
#define DEF_MARK_THICKNESS      "0.01i"
#define DEF_MAX                 "10.0"
#define DEF_MIN                 "1.0"
#define DEF_NORMAL_BG          STD_NORMAL_BACKGROUND
#define DEF_NORMAL_FG          RGB_BLACK
#define DEF_NORMAL_GRIP_BG      STD_NORMAL_BACKGROUND
#define DEF_NORMAL_GRIP_FG      STD_NORMAL_FORERGOUND
#define DEF_NORMAL_MAXARROW_COLOR "0x800000FF"
#define DEF_NORMAL_MINARROW_COLOR "0x80FF0000"
#define DEF_NORMAL_VALUE_COLOR  RGB_BLACK
#define DEF_ORIENT              "horizontal"
#define DEF_PALETTE             (char *)NULL
#define DEF_RANGE_COLOR         RGB_WHITE
#define DEF_RELIEF              "flat"
#define DEF_SCALE               "linear"
#define DEF_SHOW                "all"
#define DEF_STATE               "normal"
#define DEF_RESOLUTION          "0.0"
#define DEF_STEPSIZE            "0.0"
#define DEF_SUBDIVISIONS        "2"
#define DEF_TAGS                "all"
#define DEF_TAKE_FOCUS          ""
#define DEF_TICK_ANCHOR         "c"
#define DEF_TICK_ANGLE          "0.0"
#define DEF_TICK_COLOR          RGB_BLACK
#define DEF_TICK_DIRECTION      "out"
#define DEF_TICK_FONT           STD_FONT_NUMBERS
#define DEF_TICK_LABEL_COLOR    RGB_BLACK
#define DEF_TICK_LENGTH         "4"
#define DEF_TICK_LINEWIDTH      "1"
#define DEF_TITLE               (char *)NULL
#define DEF_TITLE_COLOR         RGB_BLACK
#define DEF_TITLE_FONT          STD_FONT_NORMAL
#define DEF_TITLE_JUSTIFY       "c"
#define DEF_UNITS               (char *)NULL
#define DEF_VALUE_FONT          "Math 6"
#define DEF_VALUE_ANGLE          "0.0"
#define DEF_WIDTH               "0"

typedef enum ScaleParts {
    PICK_NONE,
    PICK_AXIS,
    PICK_COLORBAR,
    PICK_GRIP,
    PICK_MARK,
    PICK_MAXARROW,
    PICK_MINARROW,
    PICK_TITLE,
    PICK_VALUE,
} ScalePart;

static const char *partNames[] = {
    "none", "axis", "colorbar", "grip", "mark", "maxarrow", "minarrow",
    "title", "value",
};

typedef enum _AxisScales {
    AXIS_LINEAR,
    AXIS_LOGARITHMIC,
    AXIS_TIME,
    AXIS_CUSTOM
} AxisScale;

typedef enum _TimeUnits {
    UNITS_YEARS=1,
    UNITS_MONTHS,
    UNITS_WEEKS,
    UNITS_DAYS,
    UNITS_HOURS,
    UNITS_MINUTES,
    UNITS_SECONDS,
    UNITS_SUBSECONDS,
} TimeUnits;

typedef enum _TimeFormats {
    FMT_SECONDS,
    FMT_YEARS1,
    FMT_YEARS5,
    FMT_YEARS10,
} TimeFormat;

/*
 *---------------------------------------------------------------------------
 *
 * AxisRange --
 *
 *      Designates a range of values by a minimum and maximum limit.
 *
 *---------------------------------------------------------------------------
 */
typedef struct {
    double min, max, range, scale;
} AxisRange;

/*
 *---------------------------------------------------------------------------
 *
 * TickLabel --
 *
 *      Structure containing the X-Y screen coordinates of the tick label
 *      (anchored at its center).
 *
 *---------------------------------------------------------------------------
 */
typedef struct {
    int x, y;
    unsigned int width, height;
    char string[1];
} TickLabel;

/*
 *---------------------------------------------------------------------------
 *
 * Tick --
 *
 *      Structure containing the X-Y screen coordinates of the tick label
 *      (anchored at its center).
 *
 *---------------------------------------------------------------------------
 */
typedef struct {
    int isValid;
    double value;
} Tick;

#define IsLogScale(scalePtr)    ((scalePtr)->scale == AXIS_LOGARITHMIC)
#define IsTimeScale(scalePtr)   ((scalePtr)->scale == AXIS_TIME)

/*
 *---------------------------------------------------------------------------
 *
 * Ticks --
 *
 *      Structure containing information where the ticks (major or minor)
 *      will be displayed on the scale.
 *
 *---------------------------------------------------------------------------
 */
typedef struct {
    double initial;                     /* Initial value */
    double step;                        /* Size of interval */
    double range;                       /* Range of entire sweep. */
    AxisScale axisScale;                /* Scale type. */
    time_t numDaysFromInitial;          /* # of days from the initial
                                         * tick. */
    int numSteps;                       /* Number of intervals. */
    int index;                          /* Current index of iterator. */
    int isLeapYear;                     /* Indicates if the major tick
                                         * value is a leap year. */
    TimeUnits timeUnits;                /* Indicates the time units of the
                                         * sweep. */
    int month;
    int year;
    TimeFormat timeFormat;
    const char *fmt;                    /* Default format for timescale
                                         * ticks. */
    double *values;                     /* Array of tick values
                                         * (malloc-ed). */
} Ticks;

/*
 * Colorbar --
 */
typedef struct {
    Blt_Palette palette;                /* Color palette for colorbar. */
    int thickness;
    int x, y, width, height;
    Blt_Picture picture;
} Colorbar;

/*
 *---------------------------------------------------------------------------
 *
 * Scale --
 *
 *      Structure contains options controlling how the scale will be
 *      displayed.
 *
 *---------------------------------------------------------------------------
 */
typedef struct _Scale {
    Display *display;                   /* Display containing widget;
                                         * needed, among other things, to
                                         * release resources after tkwin
                                         * has already gone away. */ 
    Tk_Window tkwin;                    /* Window that embodies the widget.
                                         * NULL means that the window has
                                         * been destroyed but the data
                                         * structures haven't yet been
                                         * cleaned up.*/
    Tcl_Interp *interp;                 /* Interpreter associated with
                                         * widget. */
    Tcl_Command cmdToken;               /* Token for widget's command. */
    unsigned int flags;                 /* For bitfield definitions, see
                                         * above. */
    int inset;                          /* Total width of all borders,
                                         * including traversal highlight
                                         * and 3-D border.  Indicates how
                                         * much interior stuff must be
                                         * offset from outside edges to
                                         * leave room for borders. */
    double outerLeft, outerRight;       /* Outer interval.  Ticks may be
                                         * drawn outside of the interval
                                         * (if -loose). */
    double innerLeft, innerRight;       /* Inner interval.  Arrows may be
                                         * drawn at both sites. */
    double reqInnerLeft;                /* User set minimum value for inner
                                         * interval. */
    double reqInnerRight;               /* User set maximum value for inner
                                         * internal. */
    int tickLineWidth;                  /* Width of the lines representing
                                         * major and minor ticks.  If zero,
                                         * then no ticks are drawn. */
    int axisLineWidth;                  /* Width of the axis line. If zero,
                                         * then no axis line is drawn. */
    int tickLength;                     /* Length of major ticks in
                                         * pixels. Minor ticks are 2/3 of
                                         * this value. */
    int arrowWidth, arrowHeight;        /* Dimension of min and max arrows
                                         * (unrotated). */
    AxisRange tickRange;                /* Smallest and largest major tick
                                         * values for the scale.  The tick
                                         * values lie outside the range of
                                         * data values.  This is used to
                                         * for "loose" mode. */
    AxisScale scale;                    /* Specifies how to scale the
                                         * scale: linear, logrithmic, or
                                         * time. */
    const char *takeFocus;              /* Not used in C code, indicates if
                                         * widget should be included in
                                         * focus traversal. */
    Tcl_Obj *fmtCmdObjPtr;              /* Specifies a TCL command, to be
                                         * invoked by the scale whenever it
                                         * has to generate tick labels. */
    Tcl_Obj *varNameObjPtr;             /* Name of TCL variable to be
                                         * updated when the current value
                                         * changes. */
    Tcl_Obj *cmdObjPtr;
    double min, max;                    /* The actual scale range. */
    double reqMin, reqMax;              /* Requested scale bounds. Consult
                                         * the scalePtr->flags field for
                                         * SCALE_CONFIG_MIN and
                                         * SCALE_CONFIG_MAX to see if the
                                         * requested bound have been set.
                                         * They override the computed range
                                         * of the scale (determined by
                                         * auto-scaling). */

    Blt_BindTable bindTable;            /* Scale binding information */
    Blt_Painter painter;
    Tk_Cursor cursor;                   /* X Cursor */

    double mark;                        /* Current value in the interval. */
    double resolution;

    double tickMin, tickMax;            /* Tick range in linear scale. */
    double prevMin, prevMax;
    double stepSize;
    double reqStep;                     /* If > 0.0, overrides the computed
                                         * major tick interval.  Otherwise
                                         * a stepsize is automatically
                                         * calculated, based upon the range
                                         * of elements mapped to the
                                         * scale. The default value is
                                         * 0.0. */
    Ticks minor, major;

    int reqNumMajorTicks;               /* Default number of ticks to be
                                         * displayed. */
    int reqNumMinorTicks;               /* If non-zero, represents the
                                         * requested the number of minor
                                         * ticks to be uniformally
                                         * displayed along each major
                                         * tick. */
    int reqWidth, reqHeight;

    int labelOffset;                    /* If non-zero, indicates that the
                                         * tick label should be offset to
                                         * sit in the middle of the next
                                         * interval. */
    int x1, y1, x2, y2;                 /* Location of axis (rectangle). */
    XSegment *tickSegments;             /* Array of line segments
                                         * representing the major and minor
                                         * ticks. */
    int numSegments;                    /* # of segments in the above
                                         * array. */
    Blt_Chain tickLabels;               /* List containing major tick label
                                         * strings and their offsets along
                                         * the axis. */
    short int width, height;            /* Extents of axis */
    short int maxTickLabelWidth;        /* Maximum (possibly rotated) width
                                         * of all ticks labels. */ 
    short int maxTickLabelHeight;       /* Maximum (possibly rotated)
                                         * height of all tick labels. */
    int borderWidth;                    /* Width of the 3D border around
                                         * the scale. */
    int relief;                         /* Normal relief of the outer
                                         * border around the scale
                                         * window. */
    int activeRelief;                   /* Relief of the outer border
                                         * around the scale window when the
                                         * scale is active. */
    int highlightWidth;                 /* Width in pixels of highlight to
                                         * draw around widget when it has
                                         * the focus.  <= 0 means don't
                                         * draw a highlight. */
    XColor *highlightBgColor;           /* Color for drawing traversal
                                         * highlight area when highlight is
                                         * off. */
    XColor *highlightColor;             /* Color for drawing traversal
                                         * highlight. */

    Blt_Bg normalBg;                    /* Normal background color of the
                                         * scale. */
    Blt_Bg activeBg;                    /* Active background color of the 
                                         * scale. */
    Blt_Bg disabledBg;                  /* Disabled background color of the
                                         * scale and its parts. */

    XColor *disabledFgColor;            /* Disabled foreground color of the
                                         * scale parts. */
    Blt_Bg normalGripBg;                /* Normal background color of the
                                         * grip. */
    Blt_Bg activeGripBg;                /* Active background color of the
                                         * grip. */
    /* There's no normalValueBg since it's transparent. */
    Blt_Bg activeValueBg;               /* Background color of the value
                                         * (rectangle) when it is
                                         * active. */
    XColor *activeValueFgColor;         /* Text color of the value when its
                                         * active. */
    XColor *normalValueFgColor;         /* Normal text color of the value. */

    /* The arrow and mark colors are Blt_Pixels so that they can specify
     * opacity. */
    Blt_Pixel normalMinArrowColor;      /* Normal color of min arrow. */
    Blt_Pixel normalMaxArrowColor;      /* Normal color of max arrow. */
    Blt_Pixel activeMinArrowColor;      /* Active color of min arrow. */
    Blt_Pixel activeMaxArrowColor;      /* Active color of max arrow. */
    Blt_Pixel markColor;                /* Color of mark line. */

    XColor *tickLabelColor;
    XColor *tickColor;
    XColor *axisLineColor;
    XColor *rangeColor;
    XColor *titleColor;

    int markWidth;                      /* Width of the mark (unrotated). */
    Blt_Font tickFont;                  /* Font for the tick numbers. */
    Blt_Font titleFont;                 /* Font for the scale's title. */
    Blt_Font valueFont;                 /* Font for the current value. */
    Blt_Font tickLabelFont;             /* Font for the tick label. */

    /* Arrows are cached as pictures (both arrows in each state). */
    Blt_Picture activeMaxArrow;
    Blt_Picture normalMaxArrow;
    Blt_Picture disabledMaxArrow;
    Blt_Picture activeMinArrow;
    Blt_Picture normalMinArrow;
    Blt_Picture disabledMinArrow;

    TickLabel *valuePtr;                /* Formatted label of the current
                                         * value. */
    float valueAngle;                   /* Angle in degrees to rotate the
                                         * current value number. */
    float tickAngle;                    /* Angle in degrees to rotate the
                                         * tick labels. */
    Tk_Anchor tickAnchor;               /* Anchor for the ticks. */

    GC disabledGC;                      /* Graphics context for disabled
                                         * scale axis line and text. */
    GC disabledTickGC;                  /* Graphics context for disabled
                                         * tick lines. */
    GC tickGC;                          /* Graphics context for axis and
                                         * tick labels */
    GC axisLineGC;                      /* Graphics context for the axis
                                         * (rectangle). */
    GC rangeGC;                         /* Graphics context for the range
                                         * line. */

    const char *units;                  /* Units. Displayed with the
                                         * value.  */
    const char *title;                  /* If non-NULL, this is the title
                                         * that is displayed at the top of
                                         * the scale. */
    int titleX, titleY;                 /* Position of the title. */
    int titleWidth, titleHeight;        /* Dimension of the title. */
    Tk_Anchor titleAnchor;              
    Tk_Justify titleJustify;
    
    int gripWidth, gripHeight;          /* Dimension of the grip
                                         * (unrotated). This includes its
                                         * borderwidth. */
    int gripBorderWidth;                /* Borderwidth of the grip. */
    int gripRelief;                     /* 3D relief of the grip. */

    double screenScale;
    Blt_Palette palette;

    Colorbar colorbar;
} Scale;


static double logTable[] = {
    0.301029995663981,                  /* 1 */
    0.477121254719662,                  /* 2 */
    0.602059991327962,                  /* 3 */
    0.698970004336019,                  /* 4 */
    0.778151250383644,                  /* 5 */
    0.845098040014257,                  /* 6 */
    0.903089986991944,                  /* 7 */
    0.954242509439325,                  /* 8 */
};


static Blt_OptionParseProc ObjToLimit;
static Blt_OptionPrintProc LimitToObj;
static Blt_CustomOption limitOption = {
    ObjToLimit, LimitToObj, NULL, (ClientData)0
};
static Blt_OptionParseProc ObjToTickDirection;
static Blt_OptionPrintProc TickDirectionToObj;
static Blt_CustomOption tickDirectionOption = {
    ObjToTickDirection, TickDirectionToObj, NULL, (ClientData)0
};
static Blt_OptionFreeProc  FreeTicks;
static Blt_OptionParseProc ObjToTicks;
static Blt_OptionPrintProc TicksToObj;
static Blt_CustomOption majorTicksOption = {
    ObjToTicks, TicksToObj, FreeTicks, (ClientData)AUTO_MAJOR,
};
static Blt_CustomOption minorTicksOption = {
    ObjToTicks, TicksToObj, FreeTicks, (ClientData)AUTO_MINOR,
};
static Blt_OptionParseProc ObjToScale;
static Blt_OptionPrintProc ScaleToObj;
static Blt_CustomOption scaleOption = {
    ObjToScale, ScaleToObj, NULL, (ClientData)0,
};
static Blt_OptionFreeProc FreePalette;
static Blt_OptionParseProc ObjToPalette;
static Blt_OptionPrintProc PaletteToObj;
static Blt_CustomOption paletteOption =
{
    ObjToPalette, PaletteToObj, FreePalette, (ClientData)0
};
static Blt_OptionParseProc ObjToShowFlags;
static Blt_OptionPrintProc ShowFlagsToObj;
Blt_CustomOption showFlagsOption = {
    ObjToShowFlags, ShowFlagsToObj, NULL, (ClientData)TRUE
};
Blt_CustomOption hideFlagsOption = {
    ObjToShowFlags, ShowFlagsToObj, NULL, (ClientData)FALSE
};
static Blt_OptionParseProc ObjToState;
static Blt_OptionPrintProc StateToObj;
static Blt_CustomOption stateOption = {
    ObjToState, StateToObj, NULL, (ClientData)0
};
static Blt_OptionParseProc ObjToOrient;
static Blt_OptionPrintProc OrientToObj;
static Blt_CustomOption orientOption = {
    ObjToOrient, OrientToObj, NULL, (ClientData)0
};
static Blt_OptionFreeProc FreeTraceVarProc;
static Blt_OptionParseProc ObjToTraceVar;
static Blt_OptionPrintProc TraceVarToObj;
static Blt_CustomOption traceVarOption = {
    ObjToTraceVar, TraceVarToObj, FreeTraceVarProc, (ClientData)0
};

static Blt_ConfigSpec configSpecs[] =
{
    {BLT_CONFIG_BACKGROUND, "-activebackground", "activeBackground", 
        "ActiveBackground", DEF_ACTIVE_BG, Blt_Offset(Scale, activeBg),
        0},
    {BLT_CONFIG_BACKGROUND, "-activegripbackground", "activeGripBackground",
        "ActiveGripBackground", DEF_ACTIVE_GRIP_BG, 
        Blt_Offset(Scale, activeGripBg), 0},
    {BLT_CONFIG_PIX32, "-activemaxarrowcolor", "activeMaxArrowColor", 
        "ActiveMaxArrowColor", DEF_ACTIVE_MAXARROW_COLOR,  
        Blt_Offset(Scale, activeMaxArrowColor), 0}, 
    {BLT_CONFIG_PIX32, "-activeminarrowcolor", "activeMinArrowColor", 
        "ActiveMinArrowColor", DEF_ACTIVE_MINARROW_COLOR,  
        Blt_Offset(Scale, activeMinArrowColor), 0}, 
    {BLT_CONFIG_RELIEF, "-activerelief", "activeRelief", "Relief",
        DEF_ACTIVE_RELIEF, Blt_Offset(Scale, activeRelief),
        BLT_CONFIG_DONT_SET_DEFAULT}, 
    {BLT_CONFIG_BACKGROUND, "-activevaluebackground", "activeValueBackground", 
        "ActiveValueBackground", DEF_ACTIVE_VALUE_BG,  
        Blt_Offset(Scale, activeValueBg), 0}, 
    {BLT_CONFIG_COLOR, "-activevalueforeground", "activeValueForeground", 
        "ActiveValueForeground", DEF_ACTIVE_VALUE_FG,  
        Blt_Offset(Scale, activeValueFgColor), 0}, 
    {BLT_CONFIG_COLOR, "-axislinecolor", "axisLineColor", "AxisLineColor", 
        DEF_AXISLINE_COLOR,  Blt_Offset(Scale, axisLineColor), 0}, 
    {BLT_CONFIG_PIXELS_NNEG, "-axislinewidth", "axisLineWidth", "AxisLineWidth",
        DEF_AXISLINE_WIDTH, Blt_Offset(Scale, axisLineWidth), 0},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
        DEF_NORMAL_BG, Blt_Offset(Scale, normalBg), 0},
    {BLT_CONFIG_SYNONYM, "-bg", "background"},
    {BLT_CONFIG_SYNONYM, "-bindtags", "tags"},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth"},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
        DEF_BORDERWIDTH, Blt_Offset(Scale, borderWidth),
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-colorbarthickness", "colorBarThickness", 
        "ColorBarThickness", DEF_COLORBAR_THICKNESS, 
        Blt_Offset(Scale, colorbar.thickness), 0},
    {BLT_CONFIG_OBJ, "-command", "command", "Command", DEF_COMMAND, 
        Blt_Offset(Scale, cmdObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
        DEF_CURSOR, Blt_Offset(Scale, cursor), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_BITMASK, "-decreasing", "decreasing", "Decreasing",
        DEF_DECREASING, Blt_Offset(Scale, flags),
        BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)DECREASING},
    {BLT_CONFIG_SYNONYM, "-descending", "decreasing"},
    {BLT_CONFIG_BACKGROUND, "-disabledbackground", "disabledBackground", 
        "DisabledBackground", DEF_DISABLED_BG, 
        Blt_Offset(Scale, disabledBg), 0},
    {BLT_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
        "DisabledForeground", DEF_DISABLED_FG,
        Blt_Offset(Scale, disabledFgColor), 0}, 
    {BLT_CONFIG_INT, "-divisions", "divisions", "Divisions", DEF_DIVISIONS, 
        Blt_Offset(Scale, reqNumMajorTicks), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_SYNONYM, "-fg", "color"},
    {BLT_CONFIG_SYNONYM, "-foreground", "color"},
    {BLT_CONFIG_OBJ, "-formatcommand", "formatCommand", "FormatCommand", 
        DEF_FORMAT_COMMAND, Blt_Offset(Scale, fmtCmdObjPtr), 
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_BACKGROUND, "-gripbackground", "gripBackground",
       "GripBackground", DEF_GRIP_BG, Blt_Offset(Scale, normalGripBg), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-gripborderwidth", "gripBorderWidth",
        "GripBorderWidth", DEF_GRIP_BORDERWIDTH,
        Blt_Offset(Scale, gripBorderWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_RELIEF, "-griprelief", "gripRelief", "GripRelief",
        DEF_GRIP_RELIEF, Blt_Offset(Scale, gripRelief),
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-height", "height", "Height", DEF_WIDTH, 
        Blt_Offset(Scale, reqHeight), 0},
    {BLT_CONFIG_CUSTOM, "-hide", "hide", "Hide", DEF_HIDE, 
        Blt_Offset(Scale, flags), BLT_CONFIG_DONT_SET_DEFAULT, 
        &hideFlagsOption},
    {BLT_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
        "HighlightBackground", DEF_HIGHLIGHT_BACKGROUND, 
        Blt_Offset(Scale, highlightBgColor), 0},
    {BLT_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
        DEF_HIGHLIGHT_COLOR, Blt_Offset(Scale, highlightColor), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-highlightthickness", "highlightThickness",
        "HighlightThickness", DEF_HIGHLIGHT_WIDTH, 
        Blt_Offset(Scale, highlightWidth), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BITMASK, "-labeloffset", "labelOffset", "LabelOffset",
        (char *)NULL, Blt_Offset(Scale, flags), 
        BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)LABELOFFSET},
    {BLT_CONFIG_BITMASK_INVERT, "-loose", "loose", "Loose", DEF_LOOSE,
         Blt_Offset(Scale, flags), BLT_CONFIG_DONT_SET_DEFAULT,
         (Blt_CustomOption *)TIGHT},
    {BLT_CONFIG_CUSTOM, "-majorticks", "majorTicks", "MajorTicks",
        (char *)NULL, Blt_Offset(Scale, major), BLT_CONFIG_NULL_OK,
        &majorTicksOption},
    {BLT_CONFIG_PIX32, "-maxarrowcolor", "maxArrowColor", "MaxArrowColor", 
        DEF_NORMAL_MAXARROW_COLOR,  Blt_Offset(Scale, normalMaxArrowColor), 0}, 
    {BLT_CONFIG_PIX32, "-minarrowcolor", "minArrowColor", "MinArrowColor", 
        DEF_NORMAL_MINARROW_COLOR,Blt_Offset(Scale, normalMinArrowColor), 0}, 
    {BLT_CONFIG_DOUBLE, "-max", "max", "Max", DEF_MAX,
        Blt_Offset(Scale, outerRight), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_DOUBLE, "-min", "min", "Min", DEF_MIN, 
        Blt_Offset(Scale, outerLeft), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-mark", "mark", "Mark", (char *)NULL, 
        Blt_Offset(Scale, mark), 0, &limitOption},
    {BLT_CONFIG_PIX32, "-markcolor", "markColor", "MarkColor", 
        DEF_MARK_COLOR,  Blt_Offset(Scale, markColor), 0}, 
    {BLT_CONFIG_PIXELS_NNEG, "-markthickness", "markThickness", "MarkThickness",
        DEF_MARK_THICKNESS, Blt_Offset(Scale, markWidth), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-minorticks", "minorTicks", "MinorTicks",
        (char *)NULL, Blt_Offset(Scale, minor), 
        BLT_CONFIG_NULL_OK, &minorTicksOption},
    {BLT_CONFIG_CUSTOM, "-orientation", "orientation", "Orientation", 
        DEF_ORIENT, Blt_Offset(Scale, flags), BLT_CONFIG_DONT_SET_DEFAULT,
        &orientOption},
    {BLT_CONFIG_CUSTOM, "-palette", "palette", "Palette", DEF_PALETTE, 
        Blt_Offset(Scale, palette), 0, &paletteOption},
    {BLT_CONFIG_COLOR, "-rangecolor", "rangeColor", "RangeColor", 
        DEF_RANGE_COLOR,  Blt_Offset(Scale, rangeColor), 0}, 
    {BLT_CONFIG_CUSTOM, "-rangemax", "rangeMax", "RangeMax", (char *)NULL, 
        Blt_Offset(Scale, reqInnerRight), 0, &limitOption},
    {BLT_CONFIG_CUSTOM, "-rangemin", "rangeMin", "RangeMin", (char *)NULL, 
        Blt_Offset(Scale, reqInnerLeft), 0, &limitOption},
    {BLT_CONFIG_RELIEF, "-relief", "relief", "Relief", DEF_RELIEF,
        Blt_Offset(Scale, relief), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_DOUBLE, "-resolution", "resolution", "Resolution", 
        DEF_RESOLUTION, Blt_Offset(Scale, resolution), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_SYNONYM, "-rmax", "rangeMax"},
    {BLT_CONFIG_SYNONYM, "-rmin", "rangeMin"},
    {BLT_CONFIG_CUSTOM, "-scale", "scale", "Scale", DEF_SCALE,
        Blt_Offset(Scale, scale),
        BLT_CONFIG_DONT_SET_DEFAULT, &scaleOption},
    {BLT_CONFIG_CUSTOM, "-show", "show", "Show",  DEF_SHOW,
        Blt_Offset(Scale, flags), BLT_CONFIG_DONT_SET_DEFAULT,
        &showFlagsOption},
    {BLT_CONFIG_CUSTOM, "-state", "state", "State", DEF_STATE, 
        Blt_Offset(Scale, flags), BLT_CONFIG_DONT_SET_DEFAULT, &stateOption},
    {BLT_CONFIG_DOUBLE, "-stepsize", "stepSize", "StepSize",
        DEF_STEPSIZE, Blt_Offset(Scale, reqStep),
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_INT, "-subdivisions", "subdivisions", "Subdivisions",
        DEF_SUBDIVISIONS, Blt_Offset(Scale, reqNumMinorTicks), 0},
    {BLT_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus", DEF_TAKE_FOCUS,
        Blt_Offset(Scale, takeFocus), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_FLOAT, "-tickangle", "tickAngle", "TickAngle", DEF_TICK_ANGLE,
        Blt_Offset(Scale, tickAngle), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_COLOR, "-tickcolor", "tickColor", "TickColor", 
        DEF_TICK_COLOR,  Blt_Offset(Scale, tickColor), 0}, 
    {BLT_CONFIG_CUSTOM, "-tickdirection", "tickDirection", "TickDirection",
        DEF_TICK_DIRECTION, Blt_Offset(Scale, flags),
        BLT_CONFIG_DONT_SET_DEFAULT, &tickDirectionOption},
    {BLT_CONFIG_FONT, "-tickfont", "tickFont", "Font",
        DEF_TICK_FONT, Blt_Offset(Scale, tickFont), 0},
    {BLT_CONFIG_COLOR, "-ticklabelcolor", "tickLabelColor", "TickLabelColor", 
        DEF_TICK_LABEL_COLOR,  Blt_Offset(Scale, tickLabelColor), 0}, 
    {BLT_CONFIG_PIXELS_NNEG, "-ticklength", "tickLength", "TickLength",
        DEF_TICK_LENGTH, Blt_Offset(Scale, tickLength), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_PIXELS_NNEG, "-ticklinewidth", "tickLineWidth", "TickLineWidth",
        DEF_TICK_LINEWIDTH, Blt_Offset(Scale, tickLineWidth),
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_STRING, "-title", "title", "Title", DEF_TITLE,
        Blt_Offset(Scale, title),
        BLT_CONFIG_DONT_SET_DEFAULT | BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-titlecolor", "titleColor", "Color", DEF_TITLE_COLOR, 
        Blt_Offset(Scale, titleColor), 0},
    {BLT_CONFIG_FONT, "-titlefont", "titleFont", "Font", DEF_TITLE_FONT, 
        Blt_Offset(Scale, titleFont), 0},
    {BLT_CONFIG_JUSTIFY, "-titlejustify", "titleJustify", "TitleJustify", 
        DEF_TITLE_JUSTIFY, Blt_Offset(Scale, titleJustify), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_STRING, "-units", "units", "Units", DEF_UNITS,
        Blt_Offset(Scale, units), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_FLOAT, "-valueangle", "valueAngle", "ValueAngle", 
        DEF_VALUE_ANGLE, Blt_Offset(Scale, valueAngle), 
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_COLOR, "-valuecolor", "valueColor", "ValueColor", 
        DEF_NORMAL_VALUE_COLOR,  Blt_Offset(Scale, normalValueFgColor), 0}, 
    {BLT_CONFIG_FONT, "-valuefont", "valueFont", "ValueFont",
        DEF_VALUE_FONT, Blt_Offset(Scale, valueFont), 0},
    {BLT_CONFIG_CUSTOM, "-variable", "variable", "Variable", (char *)NULL,
        Blt_Offset(Scale, varNameObjPtr), BLT_CONFIG_NULL_OK, &traceVarOption},
    {BLT_CONFIG_PIXELS_NNEG, "-width", "width", "Width", DEF_WIDTH, 
        Blt_Offset(Scale, reqWidth), 0},
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

typedef struct {
    unsigned int flags;
} BBoxSwitches;

#define BBOX_ROOT     (1<<0)
static Blt_SwitchSpec bboxSwitches[] = 
{
    {BLT_SWITCH_BITS_NOARG, "-root", "", (char *)NULL,
        Blt_Offset(BBoxSwitches, flags), 0, BBOX_ROOT},
    {BLT_SWITCH_END}
};

typedef struct {
    unsigned int flags;
} IdentifySwitches;

#define IDENTIFY_ROOT     (1<<0)

static Blt_SwitchSpec identifySwitches[] = 
{
    {BLT_SWITCH_BITS_NOARG, "-root", "", (char *)NULL,
        Blt_Offset(IdentifySwitches, flags), 0, IDENTIFY_ROOT},
    {BLT_SWITCH_END}
};

/* Forward declarations */
static Tcl_IdleProc DisplayProc;
static Tcl_FreeProc FreeScale;
static Blt_BindPickProc PickPartProc;
static Blt_BindAppendTagsProc AppendTagsProc;
static Tcl_VarTraceProc TraceVarProc;

static void
SetAxisRange(AxisRange *rangePtr, double min, double max)
{
    rangePtr->min = min;
    rangePtr->max = max;
    rangePtr->range = max - min;
    if (FABS(rangePtr->range) < DBL_EPSILON) {
        rangePtr->range = 1.0;
    }
    rangePtr->scale = 1.0 / rangePtr->range;
}

/*
 *---------------------------------------------------------------------------
 *
 * InRange --
 *
 *      Determines if a value lies within a given range.
 *
 *      The value is normalized and compared against the interval [0..1],
 *      where 0.0 is the minimum and 1.0 is the maximum.  DBL_EPSILON is
 *      the smallest number that can be represented on the host machine,
 *      such that (1.0 + epsilon) != 1.0.
 *
 *      Please note, *max* can't equal *min*.
 *
 * Results:
 *      If the value is within the interval [min..max], 1 is returned; 0
 *      otherwise.
 *
 *---------------------------------------------------------------------------
 */
INLINE static int
InRange(double x, AxisRange *rangePtr)
{
    if (rangePtr->range < DBL_EPSILON) {
        return (FABS(rangePtr->max - x) >= DBL_EPSILON);
    } else {
        double norm;

        norm = (x - rangePtr->min) * rangePtr->scale;
        return ((norm > -DBL_EPSILON) && ((norm - 1.0) <= DBL_EPSILON));
    }
}

INLINE static TickLabel *
GetFirstTickLabel(Scale *scalePtr)
{
    Blt_ChainLink link;

    link = Blt_Chain_FirstLink(scalePtr->tickLabels);
    if (link == NULL) {
        return NULL;
    }
    return Blt_Chain_GetValue(link);
}

INLINE static TickLabel *
GetLastTickLabel(Scale *scalePtr)
{
    Blt_ChainLink link;

    link = Blt_Chain_LastLink(scalePtr->tickLabels);
    if (link == NULL) {
        return NULL;
    }
    return Blt_Chain_GetValue(link);
}

/*
 *-----------------------------------------------------------------------------
 * Custom option parse and print procedures
 *-----------------------------------------------------------------------------
 */

/*
 *---------------------------------------------------------------------------
 *
 * EventuallyRedraw --
 *
 *      Queues a request to redraw the widget at the next idle point.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Information gets redisplayed.  Right now we don't do selective
 *      redisplays: the whole window will be redrawn.
 *
 *---------------------------------------------------------------------------
 */
static void
EventuallyRedraw(Scale *scalePtr)
{
    if ((scalePtr->tkwin != NULL) && !(scalePtr->flags & REDRAW_PENDING)) {
        scalePtr->flags |= REDRAW_PENDING;
        Tcl_DoWhenIdle(DisplayProc, scalePtr);
    }
}

static int
SetValue(Scale *scalePtr, double mark)
{
    /* If there's a resolution set, round to the nearest unit.  */
    if (scalePtr->resolution > 0.0) {
        mark = UROUND(mark, scalePtr->resolution);
    }
    /* Bound the new value to the inner interval. */
    if (mark < scalePtr->innerLeft) {
        mark = scalePtr->innerLeft;
    } else if (mark > scalePtr->innerRight) {
        mark = scalePtr->innerRight;
    }
    scalePtr->mark = mark;
    if (scalePtr->varNameObjPtr != NULL) {
        const char *varName;
        Tcl_Obj *objPtr;
        
        varName = Tcl_GetString(scalePtr->varNameObjPtr);
        objPtr = Tcl_NewDoubleObj(scalePtr->mark);
        Tcl_UntraceVar(scalePtr->interp, varName, VAR_FLAGS, TraceVarProc,
                        scalePtr);
        if (Tcl_SetVar2Ex(scalePtr->interp, varName, NULL, objPtr,
                          TCL_LEAVE_ERR_MSG | TCL_GLOBAL_ONLY) == NULL) {
            return TCL_ERROR;
        }
        Tcl_TraceVar(scalePtr->interp, varName, VAR_FLAGS, TraceVarProc,
                        scalePtr);
    }
    if (scalePtr->cmdObjPtr != NULL) {
        return Tcl_EvalObjEx(scalePtr->interp, scalePtr->cmdObjPtr,
                             TCL_EVAL_GLOBAL);
    }
   return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * BackgroundChangedProc
 *
 *      Stub for image change notifications.  Since we immediately draw the
 *      image into a pixmap, we don't really care about image changes.
 *
 *      It would be better if Tk checked for NULL proc pointers.
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
BackgroundChangedProc(ClientData clientData)
{
    Scale *scalePtr = clientData;

    if (scalePtr->tkwin != NULL) {
        EventuallyRedraw(scalePtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ScaleEventProc --
 *
 *      This procedure is invoked by the Tk dispatcher for various events on
 *      scale widgets.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      When the window gets deleted, internal structures get cleaned up.
 *      When it gets exposed, it is redisplayed.
 *
 *---------------------------------------------------------------------------
 */
static void
ScaleEventProc(ClientData clientData, XEvent *eventPtr)
{
    Scale *scalePtr = clientData;

    switch (eventPtr->type) {
    case Expose:
        if (eventPtr->xexpose.count == 0) {
            EventuallyRedraw(scalePtr);
        }
        break;
    case ConfigureNotify:
        scalePtr->flags |= GEOMETRY | LAYOUT_PENDING;
        EventuallyRedraw(scalePtr);
        break;
    case FocusIn:
    case FocusOut:
        if (eventPtr->xfocus.detail != NotifyInferior) {
            if (eventPtr->type == FocusIn) {
                scalePtr->flags |= FOCUS;
            } else {
                scalePtr->flags &= ~FOCUS;
            }
            EventuallyRedraw(scalePtr);
        }
        break;
    case DestroyNotify:
        if (scalePtr->tkwin != NULL) {
            scalePtr->tkwin = NULL;
            Tcl_DeleteCommandFromToken(scalePtr->interp, scalePtr->cmdToken);
        }
        if (scalePtr->flags & REDRAW_PENDING) {
            Tcl_CancelIdleCall(DisplayProc, scalePtr);
        }
        Tcl_EventuallyFree(scalePtr, FreeScale);
        break;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToScale --
 *
 *      Convert the string obj to indicate if the scale is scaled as time,
 *      log, or linear.
 *
 * Results:
 *      The return value is a standard TCL result.  
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToScale(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
           Tcl_Obj *objPtr, char *widgRec, int offset, int flags)
{
    Scale *scalePtr = (Scale *)(widgRec);
    char c;
    const char *string;
    int length;

    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[0];
    if ((c == 'l') && (length > 1) && 
        ((strncmp(string, "linear", length) == 0))) {
        scalePtr->scale = AXIS_LINEAR;
    } else if ((c == 'l') && (length > 1) && 
               ((strncmp(string, "logarithmic", length) == 0))) {
        scalePtr->scale = AXIS_LOGARITHMIC;
    } else if ((c == 't') && ((strncmp(string, "time", length) == 0))) {
        scalePtr->scale = AXIS_TIME;
    } else {
        Tcl_AppendResult(interp, "bad scale value \"", string, "\": should be"
                         " log, linear, or time", (char *)NULL);
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ScaleToObj --
 *
 *      Convert the scale to string obj.
 *
 * Results:
 *      The string representing if the axis scale.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ScaleToObj(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
           char *widgRec, int offset, int flags)
{
    Scale *scalePtr = (Scale *)(widgRec);
    Tcl_Obj *objPtr;
    
    switch (scalePtr->scale) {
    case AXIS_LOGARITHMIC:
        objPtr = Tcl_NewStringObj("log", 3);
        break;
    case AXIS_LINEAR:
        objPtr = Tcl_NewStringObj("linear", 6);
        break;
    case AXIS_TIME:
        objPtr = Tcl_NewStringObj("time", 4);
        break;
    default:
        objPtr = Tcl_NewStringObj("???", 3);
        break;
    }
    return objPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToLimit --
 *
 *      Convert the string representation of an scale limit into its numeric
 *      form.
 *
 * Results:
 *      The return value is a standard TCL result.  The symbol type is
 *      written into the widget record.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToLimit(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
           Tcl_Obj *objPtr, char *widgRec, int offset, int flags)
{
    double *limitPtr = (double *)(widgRec + offset);
    const char *string;

    string = Tcl_GetString(objPtr);
    if (string[0] == '\0') {
        *limitPtr = Blt_NaN();
    } else if (Blt_ExprDoubleFromObj(interp, objPtr, limitPtr) != TCL_OK) {
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * LimitToObj --
 *
 *      Convert the floating point scale limits into a string.
 *
 * Results:
 *      The string representation of the limits is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
LimitToObj(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
           char *widgRec, int offset, int flags)
{
    double limit = *(double *)(widgRec + offset);
    Tcl_Obj *objPtr;

    if (DEFINED(limit)) {
        objPtr = Tcl_NewDoubleObj(limit);
    } else {
        objPtr = Tcl_NewStringObj("", -1);
    }
    return objPtr;
}

/*ARGSUSED*/
static void
FreeTicks(ClientData clientData, Display *display, char *widgRec, int offset)
{
    Ticks *ticksPtr = (Ticks *)(widgRec + offset);

    if (ticksPtr->values != NULL) {
        Blt_Free(ticksPtr->values);
    }
    ticksPtr->values = NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToTicks --
 *
 *
 * Results:
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToTicks(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
           Tcl_Obj *objPtr, char *widgRec, int offset, int flags)
{
    Scale *scalePtr = (Scale *)widgRec;
    Tcl_Obj **objv;
    int i;
    Ticks *ticksPtr = (Ticks *)(widgRec + offset);
    double *values;
    int objc;
    size_t mask = (size_t)clientData;

    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
        return TCL_ERROR;
    }
    scalePtr->flags |= mask;
    values = NULL;
    if (objc == 0) {
        if (ticksPtr->values != NULL) {
            Blt_Free(ticksPtr->values);
        }
        ticksPtr->values = NULL;
        ticksPtr->numSteps = 0;
        return TCL_OK;
    }

    values = Blt_AssertMalloc(objc * sizeof(double));
    for (i = 0; i < objc; i++) {
        double value;
        
        if (Blt_ExprDoubleFromObj(interp, objv[i], &value) != TCL_OK) {
            Blt_Free(ticksPtr);
            return TCL_ERROR;
        }
        values[i] = value;
    }
    ticksPtr->axisScale = AXIS_CUSTOM;
    ticksPtr->values = values;
    scalePtr->flags &= ~mask;
    if (ticksPtr->values != NULL) {
        Blt_Free(ticksPtr->values);
    }
    ticksPtr->values = NULL;
    ticksPtr->numSteps = objc;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TicksToObj --
 *
 *      Convert array of tick coordinates to a list.
 *
 * Results:
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
TicksToObj(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
           char *widgRec, int offset, int flags)
{
    Scale *scalePtr = (Scale *)widgRec;
    Tcl_Obj *listObjPtr;
    Ticks *ticksPtr = (Ticks *)(widgRec + offset);
    size_t mask;

    mask = (size_t)clientData;
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if ((ticksPtr->values != NULL) && ((scalePtr->flags & mask) == 0)) {
        int i;

        for (i = 0; i < ticksPtr->numSteps; i++) {
            Tcl_Obj *objPtr;

            objPtr = Tcl_NewDoubleObj(ticksPtr->values[i]);
            Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
        }
    }
    return listObjPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToTickDirection --
 *
 *      Convert the string representation of a tick direction into its
 *      numeric form.
 *
 * Results:
 *      The return value is a standard TCL result.  The symbol type is
 *      written into the widget record.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToTickDirection(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
                   Tcl_Obj *objPtr, char *widgRec, int offset, int flags)
{
    int *flagsPtr = (int *)(widgRec + offset);
    const char *string;
    char c;

    string = Tcl_GetString(objPtr);
    c = string[0];
    if ((c == 'i') && (strcmp(string, "in") == 0)) {
        *flagsPtr &= ~EXTERIOR;
    } else if ((c == 'o') && (strcmp(string, "out") == 0)) {
        *flagsPtr |= EXTERIOR;
    } else {
        Tcl_AppendResult(interp, "unknown tick direction \"", string,
                "\": should be in or out.", (char *)NULL);
        return TCL_ERROR;        
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TickDirectionToObj --
 *
 *      Convert the flag for tick direction into a string.
 *
 * Results:
 *      The string representation of the tick direction is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
TickDirectionToObj(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
                   char *widgRec, int offset, int flags)
{
    int *flagsPtr = (int *)(widgRec + offset);
    const char *string;
    
    string = (*flagsPtr & EXTERIOR) ? "out" : "in";
    return Tcl_NewStringObj(string, -1);
}

/*
 *---------------------------------------------------------------------------
 *
 * PaletteChangedProc
 *
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
PaletteChangedProc(Blt_Palette palette, ClientData clientData, 
                   unsigned int flags)
{
    Scale *scalePtr = clientData;

    EventuallyRedraw(scalePtr);
}

/*ARGSUSED*/
static void
FreePalette(ClientData clientData, Display *display, char *widgRec, int offset)
{
    Blt_Palette *palPtr = (Blt_Palette *)(widgRec + offset);

    if (*palPtr != NULL) {
        Scale *scalePtr = (Scale *)widgRec;

        Blt_Palette_DeleteNotifier(*palPtr, PaletteChangedProc, scalePtr);
        Blt_Palette_Delete(*palPtr);
        *palPtr = NULL;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToPalette --
 *
 *      Convert the string representation of a palette into its token.
 *
 * Results:
 *      The return value is a standard TCL result.  The palette token is
 *      written into the widget record.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToPalette(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
             Tcl_Obj *objPtr, char *widgRec, int offset, int flags)
{
    Blt_Palette *palPtr = (Blt_Palette *)(widgRec + offset);
    Blt_Palette palette;
    Scale *scalePtr = (Scale *)widgRec;
    int length;

    Tcl_GetStringFromObj(objPtr, &length);
    palette = NULL;
    /* If the palette is the empty string (""), just remove the current
     * palette. */
    if (length > 0) {
        if (Blt_Palette_GetFromObj(interp, objPtr, &palette) != TCL_OK) {
            return TCL_ERROR;
        }
    }
    if (*palPtr != NULL) {
        /* Delete the old palette and its associated notifier. */
        Blt_Palette_DeleteNotifier(*palPtr, PaletteChangedProc, scalePtr);
        Blt_Palette_Delete(*palPtr);
    }
    /* Create a notifier to tell us when the palette changes or is
     * deleted. */
    if (palette != NULL) {
        Blt_Palette_CreateNotifier(palette, PaletteChangedProc, scalePtr);
    }
    *palPtr = palette;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * PaletteToObj --
 *
 *      Convert the palette token into a string.
 *
 * Results:
 *      The string representing the symbol type or line style is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
PaletteToObj(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
             char *widgRec, int offset, int flags)
{
    Blt_Palette palette = *(Blt_Palette *)(widgRec + offset);
    if (palette == NULL) {
        return Tcl_NewStringObj("", -1);
    } 
    return Tcl_NewStringObj(Blt_Palette_Name(palette), -1);
}


/*
 *---------------------------------------------------------------------------
 *
 * ObjToShowFlags --
 *
 *      Convert the string obj to show flags.
 *
 * Results:
 *      The return value is a standard TCL result.  
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToShowFlags(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
               Tcl_Obj *objPtr, char *widgRec, int offset, int flags)
{
    int *flagsPtr = (int *)(widgRec + offset);
    int objc;
    Tcl_Obj **objv;
    int showFlag = (int)(intptr_t)clientData;
    int i;

    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
        return TCL_ERROR;
    }
    for (i = 0; i < objc; i++) {
        char c;
        const char *string;
        unsigned int flag;
        int length;

        string = Tcl_GetStringFromObj(objv[i], &length);
        c = string[0];

        if ((c == 'a') && (length > 1) && 
            (strncmp(string, "all", length) == 0)) {
            flag = SHOW_ALL;
        } else if ((c == 'a') && (strncmp(string, "arrows", length) == 0)) {
            flag = SHOW_MAXARROW | SHOW_MINARROW;
        } else if ((c == 'm') && (length > 3) &&
            (strncmp(string, "minarrow", length) == 0)) {
            flag = SHOW_MINARROW;
        } else if ((c == 'm') && (length > 2) &&
                   (strncmp(string, "maxarrow", length) == 0)) {
            flag = SHOW_MAXARROW;
        } else if ((c == 'v') && ((strncmp(string, "value", length) == 0))) {
            flag = SHOW_VALUE;
        } else if ((c == 'g') && ((strncmp(string, "grip", length) == 0))) {
            flag = SHOW_GRIP;
        } else if ((c == 't') && ((strncmp(string, "title", length) == 0))) {
            flag = SHOW_TITLE;
        } else if ((c == 'c') && (strncmp(string, "colorbar", length) == 0)) {
            flag = SHOW_COLORBAR;
        } else if ((c == 'm') && (strncmp(string, "mark", length) == 0)) {
            flag = SHOW_MARK;
        } else if ((c == 't') && (length > 4) && 
                   (strncmp(string, "ticks", length) == 0)) {
            flag = SHOW_TICKS;
        } else if ((c == 't') && (length > 4) && 
                   (strncmp(string, "ticklabels", length) == 0)) {
            flag = SHOW_TICKLABELS;
        } else {
            Tcl_AppendResult(interp, "bad show flags value \"", string, 
                "\": should be arrows, minarrow, maxarrow, value, grip, title, "
                "colorbar, mark, ticks, ticklabels, or all", (char *)NULL);
            return TCL_ERROR;
        }
        if (showFlag) {
            *flagsPtr |= flag;
        } else {
            *flagsPtr &= ~flag;
        }
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ShowFlagsToObj --
 *
 *      Convert the show flags to string obj.
 *
 * Results:
 *      The string representing the show flags.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
ShowFlagsToObj(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
                char *widgRec, int offset, int flags)
{
    int showFlags = *(int *)(widgRec + offset);
    Tcl_Obj *listObjPtr, *objPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (showFlags & SHOW_COLORBAR) {
        objPtr = Tcl_NewStringObj("colorbar", 8);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_GRIP) {
        objPtr = Tcl_NewStringObj("grip", 4);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_MINARROW) {
        objPtr = Tcl_NewStringObj("minarrow", 8);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_MARK) {
        objPtr = Tcl_NewStringObj("mark", 4);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_MAXARROW) {
        objPtr = Tcl_NewStringObj("maxarrow", 8);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_TICKS) {
        objPtr = Tcl_NewStringObj("ticks", 5);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_TITLE) {
        objPtr = Tcl_NewStringObj("title", 5);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_VALUE) {
        objPtr = Tcl_NewStringObj("value", 5);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    return listObjPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToState --
 *
 *      Convert the string representation of a scale state into a flag.
 *
 * Results:
 *      The return value is a standard TCL result.  The state flags are
 *      updated.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToState(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
           Tcl_Obj *objPtr, char *widgRec, int offset, int flags)  
{
    unsigned int *flagsPtr = (unsigned int *)(widgRec + offset);
    const char *string;
    int length;
    char c;
    int flag;

    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[0];
    if ((c == 'a') && (strncmp(string, "active", length) == 0)) {
        flag = ACTIVE;
    } else if ((c == 'd') && (strncmp(string, "disabled", length) == 0)) {
        flag = DISABLED;
    } else if ((c == 'n') && (strncmp(string, "normal", length) == 0)) {
        flag = NORMAL;
    } else {
        Tcl_AppendResult(interp, "unknown state \"", string, 
                "\": should be active, disabled, or normal.", (char *)NULL);
        return TCL_ERROR;
    }
    if (*flagsPtr & flag) {
        return TCL_OK;                  /* State is already set to value. */
    }
    *flagsPtr &= ~STATE_MASK;
    *flagsPtr |= flag;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * StateToObj --
 *
 *      Return the name of the style.
 *
 * Results:
 *      The name representing the style is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
StateToObj(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
           char *widgRec, int offset, int flags)  
{
    unsigned int state = *(unsigned int *)(widgRec + offset);
    Tcl_Obj *objPtr;

    if (state & DISABLED) {
        objPtr = Tcl_NewStringObj("disabled", -1);
    } else if (state & ACTIVE) {
        objPtr = Tcl_NewStringObj("active", -1);
    } else {
        objPtr = Tcl_NewStringObj("normal", -1);
    }
    return objPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToOrient --
 *
 *      Converts the string representing a orientation into a bitflag.
 *
 * Results:
 *      The return value is a standard TCL result.  The state flags are
 *      updated.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToOrient(ClientData clientData, Tcl_Interp *interp, Tk_Window parent,
           Tcl_Obj *objPtr, char *widgRec, int offset, int flags)  
{
    char c;
    const char *string;
    int length;
    unsigned int *flagsPtr = (unsigned int *)(widgRec + offset);

    string = Tcl_GetStringFromObj(objPtr, &length);
    c = string[0];
    if ((c == 'v') && (strncmp(string, "vertical", length) == 0)) {
        *flagsPtr |= VERTICAL;
    } else if ((c == 'h') && (strncmp(string, "horizontal", length) == 0)) {
        *flagsPtr &= ~VERTICAL;
    } else {
        Tcl_AppendResult(interp, "bad orientation \"", string,
            "\": must be vertical or horizontal", (char *)NULL);
        return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * OrientToObj --
 *
 *      Returns the name of the current orientation as a Tcl_Obj.
 *
 * Results:
 *      The name representing the orientation is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
OrientToObj(ClientData clientData, Tcl_Interp *interp, Tk_Window parent,
            char *widgRec, int offset, int flags)  
{
    unsigned int orient = *(unsigned int *)(widgRec + offset);
    const char *string;

    if (orient & VERTICAL) {
        string = "vertical";    
    } else {
        string = "horizontal";  
    }
    return Tcl_NewStringObj(string, -1);
}


/*
 *---------------------------------------------------------------------------
 * 
 * TraceVarProc --
 *
 *      This procedure is invoked when someone changes the value variable
 *      associated with the scale. 
 *
 * Results:
 *      NULL is always returned.
 *
 * Side effects:
 *      The mark may change.
 *
 *---------------------------------------------------------------------------
 */
static char *
TraceVarProc(ClientData clientData, Tcl_Interp *interp, const char *name1,
                  const char *name2, int flags)
{
    Scale *scalePtr = clientData;
    double mark;
    Tcl_Obj *objPtr;
#define MAX_ERR_MSG     1023
    static char message[MAX_ERR_MSG + 1];

    assert(scalePtr->varNameObjPtr != NULL);
    if (flags & TCL_INTERP_DESTROYED) {
        return NULL;                    /* Interpreter is going away. */

    }
    /*
     * If the variable is being unset, then re-establish the trace.
     */
    if (flags & TCL_TRACE_UNSETS) {
        if (flags & TCL_TRACE_DESTROYED) {
            char *varName;

            varName = Tcl_GetString(scalePtr->varNameObjPtr);
            Tcl_TraceVar(interp, varName, VAR_FLAGS, TraceVarProc, 
                clientData);
        }
        EventuallyRedraw(scalePtr);
        return NULL;
    } 
    /*
     * Use the value of the variable to update the selected status of the
     * item.
     */
    objPtr = Tcl_ObjGetVar2(interp, scalePtr->varNameObjPtr, NULL, 
                            TCL_GLOBAL_ONLY);
    if (objPtr == NULL) {
        goto error;                   /* Can't get value of variable. */
    }
    if (Blt_GetDoubleFromObj(interp, objPtr, &mark) != TCL_OK) {
        goto error;
    }
    if (scalePtr->flags & DISABLED) {
        return NULL;
    }
    /* If there's a resolution set, round to the nearest unit.  */
    if (scalePtr->resolution > 0.0) {
        mark = UROUND(mark, scalePtr->resolution);
    }
    /* Bound the new value to the inner interval. */
    if (mark < scalePtr->innerLeft) {
        mark = scalePtr->innerLeft;
    } else if (mark > scalePtr->innerRight) {
        mark = scalePtr->innerRight;
    }
    scalePtr->mark = mark;
    EventuallyRedraw(scalePtr);
    return NULL;                        /* Done. */
 error: 
    strncpy(message, Tcl_GetStringResult(interp), MAX_ERR_MSG);
    message[MAX_ERR_MSG] = '\0';
    return message;
}

/*ARGSUSED*/
static void
FreeTraceVarProc(ClientData clientData, Display *display, char *widgRec, 
                 int offset)
{
    Scale *scalePtr = (Scale *)(widgRec);
    Tcl_Obj **varNameObjPtrPtr = (Tcl_Obj **)(widgRec + offset);

    if (*varNameObjPtrPtr != NULL) {
        const char *varName;

        varName = Tcl_GetString(*varNameObjPtrPtr);
        Tcl_UntraceVar(scalePtr->interp, varName, VAR_FLAGS, TraceVarProc, 
                scalePtr);
        Tcl_DecrRefCount(*varNameObjPtrPtr);
        *varNameObjPtrPtr = NULL;
    }
}

/*
 *---------------------------------------------------------------------------

 * ObjToTraceVar --
 *
 *      Convert the string representation of a color into a XColor pointer.
 *
 * Results:
 *      The return value is a standard TCL result.  The color pointer is
 *      written into the widget record.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToTraceVar(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
              Tcl_Obj *objPtr, char *widgRec, int offset, int flags)  
{
    Scale *scalePtr = (Scale *)(widgRec);
    Tcl_Obj **varNameObjPtrPtr = (Tcl_Obj **)(widgRec + offset);
    const char *varName;

    /* Remove the current trace on the variable. */
    if (*varNameObjPtrPtr != NULL) {
        varName = Tcl_GetString(*varNameObjPtrPtr);
        Tcl_UntraceVar(interp, varName, VAR_FLAGS, TraceVarProc, scalePtr);
        Tcl_DecrRefCount(*varNameObjPtrPtr);
        *varNameObjPtrPtr = NULL;
    }
    varName = Tcl_GetString(objPtr);
    if ((varName[0] == '\0') && (flags & BLT_CONFIG_NULL_OK)) {
        return TCL_OK;
    }
    *varNameObjPtrPtr = objPtr;
    Tcl_IncrRefCount(objPtr);
    Tcl_TraceVar(interp, varName, VAR_FLAGS, TraceVarProc, scalePtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TraceVarToObj --
 *
 *      Return the name of the value variable.
 *
 * Results:
 *      The variable name is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
TraceVarToObj(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
              char *widgRec, int offset, int flags)  
{
    Tcl_Obj *varNameObjPtr = *(Tcl_Obj **)(widgRec + offset);
    Tcl_Obj *objPtr;
    
    if (varNameObjPtr == NULL) {
        objPtr = Tcl_NewStringObj("", -1);
    } else {
        objPtr = varNameObjPtr;
    }
    return objPtr;
}


static void
FreeTickLabels(Blt_Chain chain)
{
    Blt_ChainLink link;

    for (link = Blt_Chain_FirstLink(chain); link != NULL; 
         link = Blt_Chain_NextLink(link)) {
        TickLabel *labelPtr;

        labelPtr = Blt_Chain_GetValue(link);
        Blt_Free(labelPtr);
    }
    Blt_Chain_Reset(chain);
}

/*
 *---------------------------------------------------------------------------
 *
 * MakeLabel --
 *
 *      Converts a floating point tick value to a string to be used as its
 *      label.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Returns a new label in the string character buffer.  The formatted
 *      tick label will be displayed on the scale.
 *
 * -------------------------------------------------------------------------- 
 */
static TickLabel *
MakeLabel(Scale *scalePtr, double value, const char *units)
{
#define TICK_LABEL_SIZE         200
    char buffer[TICK_LABEL_SIZE + 1];
    const char *string;
    TickLabel *labelPtr;
    Tcl_DString ds;
    int length;

    if (units == NULL) {
        units = "";
    }
    string = NULL;
    Tcl_DStringInit(&ds);
    if (scalePtr->fmtCmdObjPtr != NULL) {
        Tcl_Obj *cmdObjPtr, *objPtr;
        int result;

        /*
         * A TCL proc was designated to format tick labels. Append the path
         * name of the widget and the default tick label as arguments when
         * invoking it. Copy and save the new label from interp->result.
         */
        cmdObjPtr = Tcl_DuplicateObj(scalePtr->fmtCmdObjPtr);
        objPtr = Tcl_NewStringObj(Tk_PathName(scalePtr->tkwin), -1);
        Tcl_ListObjAppendElement(scalePtr->interp, cmdObjPtr, objPtr);
        objPtr = Tcl_NewDoubleObj(value);
        Tcl_ResetResult(scalePtr->interp);
        Tcl_IncrRefCount(cmdObjPtr);
        Tcl_ListObjAppendElement(scalePtr->interp, cmdObjPtr, objPtr);
        result = Tcl_EvalObjEx(scalePtr->interp, cmdObjPtr, TCL_EVAL_GLOBAL);
        Tcl_DecrRefCount(cmdObjPtr);
        if (result != TCL_OK) {
            Tcl_BackgroundError(scalePtr->interp);
        } 
        string = Tcl_GetStringFromObj(Tcl_GetObjResult(scalePtr->interp), 
                                      &length);
    } else if (IsLogScale(scalePtr)) {
        if (units != NULL) {
            length = Blt_FmtString(buffer, TICK_LABEL_SIZE, "%1E%d %s", 
                ROUND(value), units);
        } else {
            length = Blt_FmtString(buffer, TICK_LABEL_SIZE, "1E%d", 
                ROUND(value));
        }
        string = buffer;
    } else if ((IsTimeScale(scalePtr)) && (scalePtr->major.fmt != NULL)) {
        Blt_DateTime date;

        Blt_SecondsToDate(value, &date);
        Blt_FormatDate(&date, scalePtr->major.fmt, &ds);
        string = Tcl_DStringValue(&ds);
        length = Tcl_DStringLength(&ds);
    } else {
        if ((IsTimeScale(scalePtr)) &&
            (scalePtr->major.timeUnits == UNITS_SUBSECONDS)) {
            value = fmod(value, 60.0);
            value = UROUND(value, scalePtr->major.step);
        }
        if (units != NULL) {
            length = Blt_FmtString(buffer, TICK_LABEL_SIZE, "%.*G %s", 
                NUMDIGITS, value, units);
        } else {
            length = Blt_FmtString(buffer, TICK_LABEL_SIZE, "%.*G", 
                NUMDIGITS, value);
        }
        string = buffer;
    }
    labelPtr = Blt_AssertMalloc(sizeof(TickLabel) + length);
    strcpy(labelPtr->string, string);
    labelPtr->x = labelPtr->y = -1000;
    Tcl_DStringFree(&ds);
    return labelPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * InvHMap --
 *
 *      Maps the given screen coordinate back to an axis coordinate.  
 *
 * Results:
 *      Returns the axis coordinate value at the given window
 *      y-coordinate.
 *
 *---------------------------------------------------------------------------
 */
static double
InvHMap(Scale *scalePtr, double x)
{
    double value;

    x = (double)(x - scalePtr->x1) * scalePtr->screenScale;
    if (scalePtr->flags & DECREASING) {
        x = 1.0 - x;
    }
    value = (x * scalePtr->tickRange.range) + scalePtr->tickRange.min;
    if (IsLogScale(scalePtr)) {
        if (scalePtr->min > 0.0) {
            value = EXP10(value);
        } else {
            value = EXP10(value) + scalePtr->min - 1.0;
        }
    }
    return value;
}

/*
 *---------------------------------------------------------------------------
 *
 * InvVMap --
 *
 *      Maps the given screen y-coordinate back to the axis coordinate
 *      value. 
 *
 * Results:
 *      Returns the axis coordinate value for the given screen
 *      coordinate.
 *
 *---------------------------------------------------------------------------
 */
static double
InvVMap(Scale *scalePtr, double y)      /* Screen coordinate */
{
    double value;

    y = (double)(y - scalePtr->y1) * scalePtr->screenScale;
    if (scalePtr->flags & DECREASING) {
        y = 1.0 - y;
    }
    value = ((1.0 - y) * scalePtr->tickRange.range) + scalePtr->tickRange.min;
    if (IsLogScale(scalePtr)) {
        if (scalePtr->min > 0.0) {
            value = EXP10(value);
        } else {
            value = EXP10(value) + scalePtr->min - 1.0;
        }
    }
    return value;
}

/*
 *---------------------------------------------------------------------------
 *
 * ConvertToScreenX --
 *
 *      Maps the given scale value to its axis, returning a screen
 *      x-coordinate.
 *
 * Results:
 *      Returns a double precision number representing the screen coordinate
 *      of the value on the given axis.
 *
 *---------------------------------------------------------------------------
 */
INLINE static double
ConvertToScreenX(Scale *scalePtr, double x)
{
    /* Map axis coordinate to normalized coordinates [0..1] */
    x = (x - scalePtr->tickRange.min) * scalePtr->tickRange.scale;
    if (scalePtr->flags & DECREASING) {
        x = 1.0 - x;
    }
    return ((x * (scalePtr->x2 - scalePtr->x1)) + scalePtr->x1);
}

/*
 *---------------------------------------------------------------------------
 *
 * ConvertToScreenY --
 *
 *      Map the given scale value to its scale, returning a screen
 *      y-coordinate.
 *
 * Results:
 *      Returns a double precision number representing the screen coordinate
 *      of the value on the given scale.
 *
 *---------------------------------------------------------------------------
 */
INLINE static double
ConvertToScreenY(Scale *scalePtr, double y)
{
    /* Map scale coordinate to normalized coordinates [0..1] */
    y = (y - scalePtr->tickRange.min) * scalePtr->tickRange.scale;
    if (scalePtr->flags & DECREASING) {
        y = 1.0 - y;
    }
    return (((1.0 - y) * (scalePtr->y2 - scalePtr->y1)) + scalePtr->y1);
}

/*
 *---------------------------------------------------------------------------
 *
 * HMap --
 *
 *      Map the given scale coordinate value to its axis, returning a
 *      window position.  Automatically converts the value to log scale if
 *      the axis is in log scale.
 *
 * Results:
 *      Returns a double precision number representing the window coordinate
 *      position on the given axis.
 *
 *---------------------------------------------------------------------------
 */
static double
HMap(Scale *scalePtr, double x)
{
    if (IsLogScale(scalePtr)) {
        if (scalePtr->min > 0.0) {
            x = log10(x);
        } else {
            x = log10(x - scalePtr->min + 1.0);
        }
    }
    return ConvertToScreenX(scalePtr, x);
}

/*
 *---------------------------------------------------------------------------
 *
 * VMap --
 *
 *      Map the given scale coordinate value to its axis, returning a
 *      window position. Automatically converts the value to log scale if
 *      the axis is in log scale.
 *
 * Results:
 *      Returns a double precision number representing the window coordinate
 *      position on the given axis.
 *
 *---------------------------------------------------------------------------
 */
static double
VMap(Scale *scalePtr, double y)
{
    if (IsLogScale(scalePtr)) {
        if (scalePtr->min > 0.0) {
            y = log10(y);
        } else {
            y = log10(y - scalePtr->min + 1.0);
        }
    }
    return ConvertToScreenY(scalePtr, y);
}

/*
 *---------------------------------------------------------------------------
 *
 * NiceNum --
 *
 *      Reference: Paul Heckbert, "Nice Numbers for Graph Labels",
 *                 Graphics Gems, pp 61-63.  
 *
 *      Finds a "nice" number approximately equal to x.
 *
 *---------------------------------------------------------------------------
 */
static double
NiceNum(double x, int round)            /* If non-zero, round. Otherwise
                                         * take ceiling of value. */
{
    double expt;                        /* Exponent of x */
    double frac;                        /* Fractional part of x */
    double nice;                        /* Nice, rounded fraction */

    expt = floor(log10(x));
    frac = x / EXP10(expt);             /* between 1 and 10 */
    if (round) {
        if (frac < 1.5) {
            nice = 1.0;
        } else if (frac < 3.0) {
            nice = 2.0;
        } else if (frac < 7.0) {
            nice = 5.0;
        } else {
            nice = 10.0;
        }
    } else {
        if (frac <= 1.0) {
            nice = 1.0;
        } else if (frac <= 2.0) {
            nice = 2.0;
        } else if (frac <= 5.0) {
            nice = 5.0;
        } else {
            nice = 10.0;
        }
    }
    return nice * EXP10(expt);
}

/*
 *---------------------------------------------------------------------------
 *
 * LogScaleAxis --
 *
 *      Determine the range and units of a log scaled axis.
 *
 *      Unless the axis limits are specified, the axis is scaled
 *      automatically, where the smallest and largest major ticks encompass
 *      the range of actual data values.  When an axis limit is specified,
 *      that value represents the smallest(min)/largest(max) value in the
 *      displayed range of values.
 *
 *      Both manual and automatic scaling are affected by the step used.  By
 *      default, the step is the largest power of ten to divide the range in
 *      more than one piece.
 *
 *      Automatic scaling:
 *      Find the smallest number of units which contain the range of
 *      values.  The minimum and maximum major tick values will be
 *      represent the range of values for the axis. This greatest number of
 *      major ticks possible is 10.
 *
 *      Manual scaling:
 *      Make the minimum and maximum data values the represent the range of
 *      the values for the axis.  The minimum and maximum major ticks will
 *      be inclusive of this range.  This provides the largest area for
 *      plotting and the expected results when the axis min and max values
 *      have be set by the user (.e.g zooming).  The maximum number of
 *      major ticks is 20.
 *
 *      For log scale, there's the possibility that the minimum and maximum
 *      data values are the same magnitude.  To represent the points
 *      properly, at least one full decade should be shown.  However, if
 *      you zoom a log scale plot, the results should be
 *      predictable. Therefore, in that case, show only minor ticks.
 *      Lastly, there should be an appropriate way to handle numbers <=0.
 *
 *          maxY
 *            |    units = magnitude (of least significant digit)
 *            |    high  = largest unit tick < max axis value
 *      high _|    low   = smallest unit tick > min axis value
 *            |
 *            |    range = high - low
 *            |    # ticks = greatest factor of range/units
 *           _|
 *        U   |
 *        n   |
 *        i   |
 *        t  _|
 *            |
 *            |
 *            |
 *       low _|
 *            |
 *            |_minX________________maxX__
 *            |   |       |      |       |
 *     minY  low                        high
 *           minY
 *
 *
 *      numTicks = Number of ticks
 *      min = Minimum value of axis
 *      max = Maximum value of axis
 *      range    = Range of values (max - min)
 *
 *      If the number of decades is greater than ten, it is assumed
 *      that the full set of log-style ticks can't be drawn properly.
 *
 * Results:
 *      None
 *
 * -------------------------------------------------------------------------- 
 */
static void
LogScaleAxis(Scale *scalePtr)
{
    double tickMin, tickMax;
    double majorStep, minorStep;
    int numMajor, numMinor;

    numMajor = numMinor = 0;
    /* Suppress compiler warnings. */
    majorStep = minorStep = 0.0;
    tickMin = tickMax = Blt_NaN();
    if (scalePtr->outerLeft < scalePtr->outerRight) {
        double amin, amax;
        double range;
        
        if (scalePtr->outerLeft > 0.0) {
            amin = log10(scalePtr->outerLeft);
            amax = log10(scalePtr->outerRight);
        } else {
            amin = 0.0;
            amax = log10(scalePtr->outerRight - scalePtr->outerLeft + 1.0);
        }
        tickMin = floor(amin);
        tickMax = ceil(amax);
        range = tickMax - tickMin;
        
        if (range > 10) {
            /* There are too many decades to display a major tick at every
             * decade.  Instead, treat the axis as a linear scale.  */
            range = NiceNum(range, 0);
            majorStep = NiceNum(range / scalePtr->reqNumMajorTicks, 1);
            tickMin = UFLOOR(tickMin, majorStep);
            tickMax = UCEIL(tickMax, majorStep);
            numMajor = (int)((tickMax - tickMin) / majorStep) + 1;
            minorStep = EXP10(floor(log10(majorStep)));
            if (minorStep == majorStep) {
                numMinor = 4, minorStep = 0.2;
            } else {
                numMinor = ROUND(majorStep / minorStep) - 1;
            }
        } else {
            if (tickMin == tickMax) {
                tickMax++;
            }
            majorStep = 1.0;
            numMajor = (int)(tickMax - tickMin) + 1; /* FIXME: Check this. */
            
            minorStep = 0.0;            /* This is a special hack to pass
                                         * information to the GenerateTicks
                                         * routine. An interval of 0.0
                                         * tells 1) this is a minor sweep
                                         * and 2) the axis is log scale. */
            numMinor = 10;
        }
        if (scalePtr->flags & TIGHT) {
            tickMin = amin;
            tickMax = amax;
            numMajor++;
        }
    }
    scalePtr->major.axisScale = AXIS_LOGARITHMIC;
    scalePtr->major.step = majorStep;
    scalePtr->major.initial = floor(tickMin);
    scalePtr->major.numSteps = numMajor;
    scalePtr->minor.initial = scalePtr->minor.step = minorStep;
    scalePtr->major.range = tickMax - tickMin;
    scalePtr->minor.numSteps = numMinor;
    scalePtr->minor.axisScale = AXIS_LOGARITHMIC;
    SetAxisRange(&scalePtr->tickRange, tickMin, tickMax);
    if (scalePtr->outerLeft > 0.0) {
        scalePtr->tickMin = EXP10(tickMin);
        scalePtr->tickMax = EXP10(tickMax);
    } else {
        scalePtr->tickMin = EXP10(tickMin) + scalePtr->outerLeft - 1.0;
        scalePtr->tickMax = EXP10(tickMax) + scalePtr->outerLeft - 1.0;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * LinearScaleAxis --
 *
 *      Determine the units of a linear scaled axis.
 *
 *      The axis limits are either the range of the data values mapped
 *      to the axis (autoscaled), or the values specified by the -min
 *      and -max options (manual).
 *
 *      If autoscaled, the smallest and largest major ticks will
 *      encompass the range of data values.  If the -loose option is
 *      selected, the next outer ticks are choosen.  If tight, the
 *      ticks are at or inside of the data limits are used.
 *
 *      If manually set, the ticks are at or inside the data limits
 *      are used.  This makes sense for zooming.  You want the
 *      selected range to represent the next limit, not something a
 *      bit bigger.
 *
 *          maxY
 *            |    units = magnitude (of least significant digit)
 *            |    high  = largest unit tick < max axis value
 *      high _|    low   = smallest unit tick > min axis value
 *            |
 *            |    range = high - low
 *            |    # ticks = greatest factor of range/units
 *           _|
 *        U   |
 *        n   |
 *        i   |
 *        t  _|
 *            |
 *            |
 *            |
 *       low _|
 *            |
 *            |_minX________________maxX__
 *            |   |       |      |       |
 *     minY  low                        high
 *           minY
 *
 *      numTicks = Number of ticks
 *      min = Minimum value of axis
 *      max = Maximum value of axis
 *      range    = Range of values (max - min)
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      The axis tick information is set.  The actual tick values will
 *      be generated later.
 *
 *---------------------------------------------------------------------------
 */
static void
LinearScaleAxis(Scale *scalePtr)
{
    double step;
    double tickMin, tickMax;
    double axisMin, axisMax;
    unsigned int numTicks;

    numTicks = 0;
    step = 1.0;
    /* Suppress compiler warning. */
    axisMin = axisMax = tickMin = tickMax = Blt_NaN();
    if (scalePtr->outerLeft < scalePtr->outerRight) {
        double range;

        range = scalePtr->outerRight - scalePtr->outerLeft;
        /* Calculate the major tick stepping. */
        if (scalePtr->reqStep > 0.0) {
            /* An interval was designated by the user.  Keep scaling it
             * until it fits comfortably within the current range of the
             * axis.  */
            step = scalePtr->reqStep;
            while ((2 * step) >= range) {
                step *= 0.5;
            }
        } else {
            range = NiceNum(range, 0);
            step = NiceNum(range / scalePtr->reqNumMajorTicks, 1);
        }
        
        /* Find the outer tick values. Add 0.0 to prevent getting -0.0. */
        axisMin = tickMin = floor(scalePtr->outerLeft / step) * step + 0.0;
        axisMax = tickMax = ceil(scalePtr->outerRight / step) * step + 0.0;
        
        numTicks = ROUND((tickMax - tickMin) / step) + 1;
    } 
    /*
     * The limits of the axis are either the range of the data ("tight") or
     * at the next outer tick interval ("loose").  The looseness or
     * tightness has to do with how the axis fits the range of data values.
     * This option is overridden when the user sets an axis limit (by
     * either -min or -max option).  The axis limit is always at the
     * selected limit (otherwise we assume that user would have picked a
     * different number).
     */
    if (scalePtr->flags & TIGHT) {
        axisMin = scalePtr->outerLeft;
        axisMax = scalePtr->outerRight;
    }
    SetAxisRange(&scalePtr->tickRange, axisMin, axisMax);
    scalePtr->tickMin = axisMin;
    scalePtr->tickMax = axisMax;

    scalePtr->major.step = step;
    scalePtr->major.initial = tickMin;
    scalePtr->major.numSteps = numTicks;
    scalePtr->major.axisScale = AXIS_LINEAR;

    /* Now calculate the minor tick step and number. */

    if ((scalePtr->reqNumMinorTicks > 0) && (scalePtr->flags & AUTO_MAJOR)) {
        step = 1.0 / (double)scalePtr->reqNumMinorTicks;
        numTicks = scalePtr->reqNumMinorTicks - 1;
    } else {
        numTicks = 0;                   /* No minor ticks. */
        /* Don't set the minor tick interval to 0.0. It makes the
         * GenerateTicks routine create minor log-scale tick marks.  */
        step = 0.5;
    }

    scalePtr->minor.step = step;
    scalePtr->minor.numSteps = numTicks;
    scalePtr->minor.axisScale = AXIS_LINEAR;
    /* Never generate minor ticks. */
}

static int 
GetMajorTimeUnits(double min, double max)
{
    double range;

    range = max - min;
    if (range <= 0.0) {
        return -1;
    }
    if (range > (SECONDS_YEAR * 1.5)) {
        return UNITS_YEARS;
    } 
    if (range > (SECONDS_MONTH * 2.1)) {
        return UNITS_MONTHS;
    } 
    if (range > (SECONDS_WEEK * 2)) {
        return UNITS_WEEKS;
    } 
    if (range > (SECONDS_DAY * 2)) {
        return UNITS_DAYS;
    }
    if (range > (SECONDS_HOUR * 2)) {
        return UNITS_HOURS;
    }
    if (range > (SECONDS_MINUTE * 2)) {
        return UNITS_MINUTES;
    }
    if (range > (SECONDS_SECOND * 2)) {
        return UNITS_SECONDS;
    }
    return UNITS_SUBSECONDS;
}

static double
TimeFloor(Scale *scalePtr, double min, TimeUnits units, Blt_DateTime *datePtr)
{
    double seconds;
    int mday;

    seconds = floor(min);
    Blt_SecondsToDate(seconds, datePtr);
    mday = 0;                           /* Suppress compiler warning. */
    switch (units) {
    case UNITS_YEARS:
        datePtr->mon = 0;               /* 0-11 */
        /* fallthrough */
    case UNITS_MONTHS:
    case UNITS_WEEKS:
        mday = datePtr->mday;
        datePtr->mday = 1;              /* 1-31, 0 is last day of preceding
                                         * month. */
        /* fallthrough */
    case UNITS_DAYS:
        datePtr->hour = 0;              /* 0-23 */
        /* fallthrough */
    case UNITS_HOURS:
        datePtr->min = 0;               /* 0-59 */
        /* fallthrough */
    case UNITS_MINUTES:
        datePtr->sec = 0;               /* 0-60 */
        /* fallthrough */
    default:
    case UNITS_SECONDS:
        break;
    }
    if (units == UNITS_WEEKS) {
        mday -= datePtr->wday;
        datePtr->wday = 0;
        if (mday < 1) {
            datePtr->mon--;
            if (datePtr->mon < 0) {
                datePtr->mon = 11;      /* 0-11 */
                datePtr->year--;
            }
            mday += 
                numDaysMonth[IsLeapYear(datePtr->year)][datePtr->mon];
        }
        datePtr->mday = mday;
    }
    datePtr->isdst = 0;
    Blt_DateToSeconds(datePtr, &seconds);
    return seconds;
}

static double
TimeCeil(Scale *scalePtr, double max, TimeUnits units, Blt_DateTime *datePtr)
{
    double seconds;

    seconds = ceil(max);
    switch (units) {
    case UNITS_YEARS:
        seconds += SECONDS_YEAR - 1;
        break;
    case UNITS_MONTHS:
        seconds += SECONDS_MONTH - 1;
        break;
    case UNITS_WEEKS:
        seconds += SECONDS_WEEK - 1;
        break;
    case UNITS_DAYS:
        seconds += SECONDS_DAY - 1;
        break;
    case UNITS_HOURS:
        seconds += SECONDS_HOUR - 1;
        break;
    case UNITS_MINUTES:
        seconds += SECONDS_MINUTE - 1;
        break;
    case UNITS_SECONDS:
        seconds += 1.0;
        break;
    default:
        break;
    }
    return TimeFloor(scalePtr, seconds, units, datePtr);
}


static long
NumberDaysFromEpoch(int year)
{
    int y;
    long numDays;

    numDays = 0;
    if (year >= EPOCH) {
        for (y = EPOCH; y < year; y++) {
            numDays += numDaysYear[IsLeapYear(y)];
        }
    } else {
        for (y = year; y < EPOCH; y++)
            numDays -= numDaysYear[IsLeapYear(y)];
    }
    return numDays;
}

static void
YearTicks(Scale *scalePtr)
{
    Blt_DateTime date1, date2;
    double step;
    int numTicks;
    double tickMin, tickMax;            /* Years. */
    double axisMin, axisMax;            /* Seconds. */
    double numYears;

    scalePtr->major.axisScale = scalePtr->minor.axisScale = AXIS_TIME;
    tickMin = TimeFloor(scalePtr, scalePtr->outerLeft, UNITS_YEARS, &date1);
    tickMax = TimeCeil(scalePtr, scalePtr->outerRight, UNITS_YEARS, &date2);
    step = 1.0;
    numYears = date2.year - date1.year;
    if (numYears > 10) {
        long minDays, maxDays;
        double range;

        scalePtr->major.timeFormat = FMT_YEARS10;
        range = numYears;
        range = NiceNum(range, 0);
        step = NiceNum(range / scalePtr->reqNumMajorTicks, 1);
        tickMin = UFLOOR((double)date1.year, step);
        tickMax = UCEIL((double)date2.year, step);
        range = tickMax - tickMin;
        numTicks = (int)(range / step) + 1;
    
        minDays = NumberDaysFromEpoch((int)tickMin);
        maxDays  = NumberDaysFromEpoch((int)tickMax);
        tickMin = minDays * SECONDS_DAY;
        tickMax = maxDays * SECONDS_DAY;
        scalePtr->major.year = tickMin;
        scalePtr->minor.timeUnits = UNITS_YEARS;
        if (step > 5) {
            scalePtr->minor.numSteps = 1;
            scalePtr->minor.step = step / 2;
        } else {
            scalePtr->minor.step = 1; /* Years */
            scalePtr->minor.numSteps = step - 1;
        }
    } else {
        numTicks = numYears + 1;
        step = 0;                       /* Number of days in the year */

        tickMin = NumberDaysFromEpoch(date1.year) * SECONDS_DAY;
        tickMax = NumberDaysFromEpoch(date2.year) * SECONDS_DAY;
        
        scalePtr->major.year = date1.year;
        if (numYears > 5) {
            scalePtr->major.timeFormat = FMT_YEARS5;

            scalePtr->minor.step = (SECONDS_YEAR+1) / 2; /* 1/2 year */
            scalePtr->minor.numSteps = 1;  /* 3 - 2 */
            scalePtr->minor.timeUnits = UNITS_YEARS;
        } else {
            scalePtr->major.timeFormat = FMT_YEARS1;
     
            scalePtr->minor.step = 0; /* Months */
            scalePtr->minor.numSteps = 11;  /* 12 - 1 */
            scalePtr->minor.timeUnits = UNITS_MONTHS;
            scalePtr->minor.month = date1.year;
        } 
    }

    axisMin = tickMin;
    axisMax = tickMax;
    if (scalePtr->flags & TIGHT) {
        axisMin = scalePtr->outerLeft;
        axisMax = scalePtr->outerRight;
    }
    SetAxisRange(&scalePtr->tickRange, axisMin, axisMax);
    scalePtr->tickMin = axisMin;
    scalePtr->tickMax = axisMax;
    scalePtr->major.timeUnits = UNITS_YEARS;
    scalePtr->major.fmt = "%Y";
    scalePtr->major.axisScale = AXIS_TIME;
    scalePtr->major.range = tickMax - tickMin;
    scalePtr->major.initial = tickMin;
    scalePtr->major.numSteps = numTicks;
    scalePtr->major.step = step;
}

static void
MonthTicks(Scale *scalePtr)
{
    Blt_DateTime left, right;
    int numMonths;
    double step;
    int numTicks;
    double tickMin, tickMax;            /* months. */
    double axisMin, axisMax;            /* seconds. */
    
    tickMin = axisMin = TimeFloor(scalePtr, scalePtr->outerLeft, UNITS_MONTHS, 
                                  &left);
    tickMax = axisMax = TimeCeil(scalePtr, scalePtr->outerRight, UNITS_MONTHS, 
        &right);
    if (right.year > left.year) {
        right.mon += (right.year - left.year) * 12;
    }
    numMonths = right.mon - left.mon;
    numTicks = numMonths + 1;
    step = 1;
    if (scalePtr->flags & TIGHT) {
        axisMin = scalePtr->outerLeft;
        axisMax = scalePtr->outerRight;
    }
    SetAxisRange(&scalePtr->tickRange, axisMin, axisMax);
    scalePtr->tickMin = axisMin;
    scalePtr->tickMax = axisMax;

    scalePtr->major.initial = tickMin;
    scalePtr->major.numSteps = numTicks;
    scalePtr->major.step = step;
    scalePtr->major.range = tickMax - tickMin;
    scalePtr->major.isLeapYear = left.isLeapYear;
    scalePtr->major.fmt = "%h\n%Y";
    scalePtr->major.month = left.mon;
    scalePtr->major.year = left.year;
    scalePtr->major.timeUnits = UNITS_MONTHS;
    scalePtr->major.axisScale = AXIS_TIME;
    
    scalePtr->minor.numSteps = 5;
    scalePtr->minor.step = SECONDS_WEEK;
    scalePtr->minor.month = left.mon;
    scalePtr->minor.year = left.year;
    scalePtr->minor.timeUnits = UNITS_WEEKS;
    scalePtr->minor.axisScale = AXIS_TIME;

}

/* 
 *---------------------------------------------------------------------------
 *
 * WeekTicks --
 *
 *    Calculate the ticks for a major axis divided into weeks.  The step for
 *    week ticks is 1 week if the number of week is less than 6.  Otherwise
 *    we compute the linear version of 
 *
 *---------------------------------------------------------------------------
 */
static void
WeekTicks(Scale *scalePtr)
{
    Blt_DateTime left, right;
    int numWeeks;
    double step;
    int numTicks;
    double tickMin, tickMax;            /* days. */
    double axisMin, axisMax;            /* seconds. */

    tickMin = axisMin = TimeFloor(scalePtr, scalePtr->outerLeft, UNITS_WEEKS, &left);
    tickMax = axisMax = TimeCeil(scalePtr, scalePtr->outerRight, UNITS_WEEKS, &right);
    numWeeks = (tickMax - tickMin) / SECONDS_WEEK;
    if (numWeeks > 10) {
        double range;

        fprintf(stderr, "Number of weeks > 10\n");
        range = numWeeks;
        range = NiceNum(range, 0);
        step = NiceNum(range / scalePtr->reqNumMajorTicks, 1);
        numTicks = (int)(range / step) + 1;
        step *= SECONDS_WEEK;
        tickMin = UFLOOR(tickMin, step);
        tickMax = UCEIL(tickMax, step);
        tickMin += (7 - EPOCH_WDAY)*SECONDS_DAY;
        tickMax -= EPOCH_WDAY*SECONDS_DAY;
        axisMin = tickMin;
        axisMax = tickMax;
    } else {
        numTicks = numWeeks + 1;
        step = SECONDS_WEEK;
    }

    if (scalePtr->flags & TIGHT) {
        axisMin = scalePtr->outerLeft;
        axisMax = scalePtr->outerRight;
    }
    SetAxisRange(&scalePtr->tickRange, axisMin, axisMax);
    scalePtr->tickMin = axisMin;
    scalePtr->tickMax = axisMax;
    
    scalePtr->major.step = step;
    scalePtr->major.initial = tickMin;
    scalePtr->major.numSteps = numTicks;
    scalePtr->major.timeUnits = UNITS_WEEKS;
    scalePtr->major.range = tickMax - tickMin;
    scalePtr->major.fmt = "%h %d";
    scalePtr->major.axisScale = AXIS_TIME;
    scalePtr->minor.step = SECONDS_DAY;
    scalePtr->minor.numSteps = 6;  
    scalePtr->minor.timeUnits = UNITS_DAYS;
    scalePtr->minor.axisScale = AXIS_TIME;
}


/* 
 *---------------------------------------------------------------------------
 *
 * DayTicks --
 *
 *    Calculate the ticks for a major axis divided into days.  The step for
 *    day ticks is always 1 day.  There is no multiple of days that fits 
 *    evenly into a week or month.
 *
 *---------------------------------------------------------------------------
 */
static void
DayTicks(Scale *scalePtr)
{
    Blt_DateTime left, right;
    int numDays, numTicks;
    double step;
    double tickMin, tickMax;            /* days. */
    double axisMin, axisMax;            /* seconds. */

    tickMin = axisMin = TimeFloor(scalePtr, scalePtr->outerLeft, UNITS_DAYS, &left);
    tickMax = axisMax = TimeCeil(scalePtr, scalePtr->outerRight, UNITS_DAYS, &right);
    numDays = (tickMax - tickMin) / SECONDS_DAY;
    numTicks = numDays + 1;
    step = SECONDS_DAY;

    if (scalePtr->flags & TIGHT) {
        axisMin = scalePtr->outerLeft;
        axisMax = scalePtr->outerRight;
    }
    SetAxisRange(&scalePtr->tickRange, axisMin, axisMax);
    scalePtr->tickMin = axisMin;
    scalePtr->tickMax = axisMax;

    scalePtr->major.step = step;
    scalePtr->major.initial = tickMin;
    scalePtr->major.numSteps = numTicks;
    scalePtr->major.timeUnits = UNITS_DAYS;
    scalePtr->major.range = tickMax - tickMin;
    scalePtr->major.axisScale = AXIS_TIME;
    scalePtr->major.fmt = "%h %d";
    scalePtr->minor.step = SECONDS_HOUR * 6;
    scalePtr->minor.initial = 0;
    scalePtr->minor.numSteps = 2;  /* 6 - 2 */
    scalePtr->minor.timeUnits = UNITS_HOURS;
    scalePtr->minor.axisScale = AXIS_TIME;
}

/* 
 *---------------------------------------------------------------------------
 *
 * HourTicks --
 *
 *    Calculate the ticks for a major axis divided in hours.  The hour step
 *    should evenly divide into 24 hours, so we select the step based on the 
 *    number of hours in the range.
 *
 *---------------------------------------------------------------------------
 */
static void
HourTicks(Scale *scalePtr)
{
    Blt_DateTime left, right;
    double axisMin, axisMax;            
    double tickMin, tickMax;            
    int numTicks;
    int numHours;

    double step;

    tickMin = axisMin = TimeFloor(scalePtr, scalePtr->outerLeft, UNITS_HOURS, &left);
    tickMax = axisMax = TimeCeil(scalePtr, scalePtr->outerRight, UNITS_HOURS, &right);
    numHours = (tickMax - tickMin) / SECONDS_HOUR;
    if (numHours < 7) {                 /* 3-6 hours */
        step = SECONDS_HOUR;            
    } else if (numHours < 13) {         /* 7-12 hours */
        step = SECONDS_HOUR * 2;
    } else if (numHours < 25) {         /* 13-24 hours */
        step = SECONDS_HOUR * 4;
    } else if (numHours < 36) {         /* 23-35 hours */
        step = SECONDS_HOUR * 6;
    } else {                            /* 33-48 hours */
        step = SECONDS_HOUR * 8;
    }

    axisMin = tickMin = UFLOOR(tickMin, step);
    axisMax = tickMax = UCEIL(tickMax, step);
    numTicks = ((tickMax - tickMin) / (long)step) + 1;

    if (scalePtr->flags & TIGHT) {
        axisMin = scalePtr->outerLeft;
        axisMax = scalePtr->outerRight;
    }
    SetAxisRange(&scalePtr->tickRange, axisMin, axisMax);
    scalePtr->tickMin = axisMin;
    scalePtr->tickMax = axisMax;

    scalePtr->major.step = step;
    scalePtr->major.initial = tickMin;
    scalePtr->major.numSteps = numTicks;
    scalePtr->major.range = tickMax - tickMin;
    scalePtr->major.fmt = "%H:%M\n%h %d";
    scalePtr->major.timeUnits = UNITS_HOURS;
    scalePtr->major.axisScale = AXIS_TIME;

    scalePtr->minor.step = step / 4;
    scalePtr->minor.numSteps = 4;  /* 6 - 2 */
    scalePtr->minor.timeUnits = UNITS_MINUTES;
    scalePtr->minor.axisScale = AXIS_TIME;
}

/* 
 *---------------------------------------------------------------------------
 *
 * MinuteTicks --
 *
 *    Calculate the ticks for a major axis divided in minutes.  Minutes can
 *    be in steps of 5 or 10 so we can use the standard tick selecting
 *    procedures.
 *
 *---------------------------------------------------------------------------
 */
static void
MinuteTicks(Scale *scalePtr)
{
    Blt_DateTime left, right;
    int numMinutes, numTicks;
    double step, range;
    double tickMin, tickMax;            /* minutes. */
    double axisMin, axisMax;            /* seconds. */

    tickMin = axisMin = TimeFloor(scalePtr, scalePtr->outerLeft, UNITS_MINUTES, &left);
    tickMax = axisMax = TimeCeil(scalePtr, scalePtr->outerRight, UNITS_MINUTES, &right);
    numMinutes = (tickMax - tickMin) / SECONDS_MINUTE;

    range = numMinutes;
    range = NiceNum(range, 0);
    step = NiceNum(range / scalePtr->reqNumMajorTicks, 1);
    numTicks = (int)(range / step) + 1;
    step *= SECONDS_MINUTE;
    axisMin = tickMin = UFLOOR(tickMin, step);
    axisMax = tickMax = UCEIL(tickMax, step);

    if (scalePtr->flags & TIGHT) {
        axisMin = scalePtr->outerLeft;
        axisMax = scalePtr->outerRight;
    }
    SetAxisRange(&scalePtr->tickRange, axisMin, axisMax);
    scalePtr->tickMin = axisMin;
    scalePtr->tickMax = axisMax;

    scalePtr->major.step = step;
    scalePtr->major.initial = tickMin;
    scalePtr->major.numSteps = numTicks;
    scalePtr->major.timeUnits = UNITS_MINUTES;
    scalePtr->major.axisScale = AXIS_TIME;
    scalePtr->major.fmt = "%H:%M";
    scalePtr->major.range = tickMax - tickMin;

    scalePtr->minor.step = step / (scalePtr->reqNumMinorTicks - 1);
    scalePtr->minor.numSteps = scalePtr->reqNumMinorTicks;
    scalePtr->minor.timeUnits = UNITS_MINUTES;
    scalePtr->minor.axisScale = AXIS_TIME;
}

/* 
 *---------------------------------------------------------------------------
 *
 * SecondTicks --
 *
 *    Calculate the ticks for a major axis divided into seconds.  Seconds
 *    can be in steps of 5 or 10 so we can use the standard tick selecting
 *    procedures.
 *
 *---------------------------------------------------------------------------
 */
static void
SecondTicks(Scale *scalePtr)
{
    double step, range;
    int numTicks;
    double tickMin, tickMax;            /* minutes. */
    double axisMin, axisMax;            /* seconds. */
    long numSeconds;

    numSeconds = (long)(scalePtr->outerRight - scalePtr->outerLeft);
    step = 1.0;
    range = numSeconds;
    if (scalePtr->reqStep > 0.0) {
        /* An interval was designated by the user.  Keep scaling it until
         * it fits comfortably within the current range of the axis.  */
        step = scalePtr->reqStep;
        while ((2 * step) >= range) {
            step *= 0.5;
        }
    } else {
        range = NiceNum(range, 0);
        step = NiceNum(range / scalePtr->reqNumMajorTicks, 1);
    }
    /* Find the outer tick values. Add 0.0 to prevent getting -0.0. */
    axisMin = tickMin = UFLOOR(scalePtr->outerLeft, step);
    axisMax = tickMax = UCEIL(scalePtr->outerRight, step);
    numTicks = ROUND((tickMax - tickMin) / step) + 1;
    if (scalePtr->flags & TIGHT) {
        axisMin = scalePtr->outerLeft;
        axisMax = scalePtr->outerRight;
    }
    SetAxisRange(&scalePtr->tickRange, axisMin, axisMax);
    scalePtr->tickMin = axisMin;
    scalePtr->tickMax = axisMax;

    scalePtr->tickMin = axisMin;
    scalePtr->tickMax = axisMax;
    scalePtr->major.initial = tickMin;
    scalePtr->major.numSteps = numTicks;
    scalePtr->major.step = step;
    scalePtr->major.fmt = "%H:%M:%S%f";
    scalePtr->major.timeUnits = UNITS_SECONDS;
    scalePtr->major.axisScale = AXIS_TIME;
    scalePtr->major.range = tickMax - tickMin;

    scalePtr->minor.step = 1.0 / (double)scalePtr->reqNumMinorTicks;
    scalePtr->minor.numSteps = scalePtr->reqNumMinorTicks - 1;
    scalePtr->minor.axisScale = AXIS_LINEAR;
}

/*
 *---------------------------------------------------------------------------
 *
 * TimeScaleAxis --
 *
 *      Determine the units of a linear scaled axis.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      The tick values are generated.
 *
 *---------------------------------------------------------------------------
 */
static void
TimeScaleAxis(Scale *scalePtr)
{
    int units;

    units = GetMajorTimeUnits(scalePtr->outerLeft, scalePtr->outerRight);
    if (units == -1) {
        return;
    }
    switch (units) {
    case UNITS_YEARS:
        YearTicks(scalePtr);
        break;
    case UNITS_MONTHS:
        MonthTicks(scalePtr);
        break;
    case UNITS_WEEKS:
        WeekTicks(scalePtr);
        break;
    case UNITS_DAYS:
        DayTicks(scalePtr);
        break;
    case UNITS_HOURS:
        HourTicks(scalePtr);
        break;
    case UNITS_MINUTES:
        MinuteTicks(scalePtr);
        break;
    case UNITS_SECONDS:
        SecondTicks(scalePtr);
        break;
    case UNITS_SUBSECONDS:
        LinearScaleAxis(scalePtr);
        scalePtr->major.axisScale = AXIS_TIME;
        scalePtr->major.timeUnits = units;
        break;
    default:
        Blt_Panic("unknown time units");
    }
}


static Tick
FirstMajorTick(Scale *scalePtr)
{
    Ticks *ticksPtr;
    Tick tick;

    ticksPtr = &scalePtr->major;
    ticksPtr->index = 0;
    ticksPtr->numDaysFromInitial = 0;
    tick.isValid = FALSE;
    tick.value = Blt_NaN();
#ifdef notdef
    fprintf(stderr, "axisScale is %d, timeUnits=%d\n", ticksPtr->axisScale,
            ticksPtr->timeUnits);
#endif
    switch (ticksPtr->axisScale) {
    case AXIS_CUSTOM:                  /* User defined minor ticks */
        tick.value = ticksPtr->values[0];
        break;
    case AXIS_TIME:
        switch (ticksPtr->timeUnits) {
        case UNITS_YEARS:
            {
                Blt_DateTime date;

                Blt_SecondsToDate(ticksPtr->initial, &date);
                ticksPtr->isLeapYear = date.isLeapYear;
                ticksPtr->year = date.year;
            }
            break;
        case UNITS_MONTHS:
            if (ticksPtr->numSteps <= 3) {
                scalePtr->minor.numSteps = 
                    numDaysMonth[ticksPtr->isLeapYear][ticksPtr->month];  
                scalePtr->minor.step = SECONDS_DAY;
            } 
            break;
        default:
            break;
        }
        tick.value = ticksPtr->initial;
        break;
    case AXIS_LOGARITHMIC:
    case AXIS_LINEAR:
    default:
        /* The number of major ticks and the step has been computed in
         * LinearScale. */
        tick.value = ticksPtr->initial;
        break;
    }
    if (ticksPtr->index >= ticksPtr->numSteps) {
        return tick;
    }
#ifdef notdef
    fprintf(stderr, "FirstMajorTick: tick.value=%.15g\n", tick.value);
#endif
    tick.isValid = TRUE;
    return tick;
}

static Tick
NextMajorTick(Scale *scalePtr)
{
    double d;                           /* Delta from initial to major
                                         * tick. */
    Ticks *ticksPtr;
    Tick tick;

    ticksPtr = &scalePtr->major;
    ticksPtr->index++;
    tick.isValid = FALSE;
    tick.value = Blt_NaN();
    if (ticksPtr->index >= ticksPtr->numSteps) {
        return tick;
    }
    d = ticksPtr->initial; 
    switch (ticksPtr->axisScale) {
    case AXIS_LINEAR:
    default:
        d += ticksPtr->index * ticksPtr->step;
        d = UROUND(d, ticksPtr->step) + 0.0;
        break;

    case AXIS_LOGARITHMIC:
        d += ticksPtr->index * ticksPtr->step;
        d = UROUND(d, ticksPtr->step) + 0.0;
#ifdef notdef
        d += ticksPtr->range * logTable[ticksPtr->index];
#endif
        break;

    case AXIS_CUSTOM:                   /* User defined minor ticks */
        tick.value = ticksPtr->values[ticksPtr->index];
        tick.isValid = TRUE;
        return tick;

    case AXIS_TIME:
        switch (ticksPtr->timeUnits) {
        case UNITS_YEARS:
            switch(ticksPtr->timeFormat) {
            case FMT_YEARS10:
                {
                    int i;
                    
                    for (i = 0; i < ticksPtr->step; i++) {
                        int year, numDays;
                        
                        year = ticksPtr->year++;
                        numDays = numDaysYear[IsLeapYear(year)]; 
                        ticksPtr->numDaysFromInitial += numDays;
                    }
                    d += ticksPtr->numDaysFromInitial * SECONDS_DAY;
                }
                break;
            case FMT_YEARS5:
            case FMT_YEARS1:
                {
                    int i;
                    
                    for (i = 0; i < ticksPtr->index; i++) {
                        int year, numDays;
                        
                        year = ticksPtr->year + i;
                        numDays = numDaysYear[IsLeapYear(year)]; 
                        d += numDays * SECONDS_DAY;
                    }
                }
                break;
            case FMT_SECONDS:
            default:
                break;
            }
            break;
        case UNITS_MONTHS:
            {
                long numDays;
                int mon, year;
                int i;

                numDays = 0;
                mon = ticksPtr->month, year = ticksPtr->year;
                for (i = 0; i < ticksPtr->index; i++, mon++) {
                    if (mon > 11) {
                        mon = 0;
                        year++;
                    }
                    numDays += numDaysMonth[IsLeapYear(year)][mon];
                }
                d += numDays * SECONDS_DAY;
            }
            break;
        case UNITS_WEEKS:
        case UNITS_HOURS:
        case UNITS_MINUTES:
            d += ticksPtr->index * ticksPtr->step;
            break;
        case UNITS_DAYS:
            d += ticksPtr->index * ticksPtr->step;
            break;
        case UNITS_SECONDS:
        case UNITS_SUBSECONDS:
            d += ticksPtr->index * ticksPtr->step;
            d = UROUND(d, ticksPtr->step);
            break;
        }
        break;

    }
    tick.value = d;
    tick.isValid = TRUE;
#ifdef notdef
    fprintf(stderr, "NextMajorTick: tick.value=%.15g\n", tick.value);
#endif
    return tick;
}

static Tick
FirstMinorTick(Scale *scalePtr)
{
    double d;                           /* Delta from major to minor
                                         * tick. */
    Ticks *ticksPtr;
    Tick tick;

    ticksPtr = &scalePtr->minor;
    ticksPtr->numDaysFromInitial = 0;
    ticksPtr->index = 0;
    tick.isValid = FALSE;
    tick.value = Blt_NaN();
    d = 0.0;                            /* Suppress compiler warning. */
    switch (ticksPtr->axisScale) {
    case AXIS_LINEAR:
    default:
        d = ticksPtr->step * ticksPtr->range;
        break;

    case AXIS_CUSTOM:                   /* User defined minor ticks */
        d = ticksPtr->values[0] * ticksPtr->range;
        break;

    case AXIS_LOGARITHMIC:
        d = logTable[0] * ticksPtr->range;
        break;

    case AXIS_TIME:
        switch (ticksPtr->timeUnits) {
        case UNITS_YEARS:
            {
                Blt_DateTime date;
                int i;

                Blt_SecondsToDate(ticksPtr->initial, &date);
                ticksPtr->isLeapYear = date.isLeapYear;
                ticksPtr->year = date.year;
                for (i = 0; i < ticksPtr->step; i++) {
                    int year, numDays;

                    year = ticksPtr->year++;
                    numDays = numDaysYear[IsLeapYear(year)];
                    ticksPtr->numDaysFromInitial += numDays;
                }
                d = ticksPtr->numDaysFromInitial * SECONDS_DAY;
            }
            break;
        case UNITS_MONTHS:
            {
                Blt_DateTime date;
                int numDays;

                Blt_SecondsToDate(ticksPtr->initial, &date);
                ticksPtr->isLeapYear = date.isLeapYear;
                numDays = numDaysMonth[date.isLeapYear][date.mon];
                ticksPtr->month = date.mon;
                ticksPtr->year = date.year;
                d = numDays * SECONDS_DAY;
            }
            break;
        case UNITS_DAYS:
            if (ticksPtr->numSteps == 1) {
                ticksPtr->step = ticksPtr->range * 0.5;
            } 
            d = ticksPtr->step;
            break;
        case UNITS_WEEKS:
            {
                Blt_DateTime date;

                Blt_SecondsToDate(ticksPtr->initial, &date);
                ticksPtr->numDaysFromInitial = (7 - date.wday);
                d = ticksPtr->numDaysFromInitial * SECONDS_DAY;
            }
            break;
        case UNITS_HOURS:
        case UNITS_MINUTES:
            ticksPtr->step = ticksPtr->range / ticksPtr->numSteps;
            d = ticksPtr->step;
            break;
        case UNITS_SECONDS:
        case UNITS_SUBSECONDS:
            /* The number of minor ticks has been computed in
             * LinearScaleAxis. */
            d = ticksPtr->step;
            d = UROUND(d, ticksPtr->step);
            break;
        }
        break;
    }
    if (ticksPtr->index >= ticksPtr->numSteps) {
        return tick;
    }
    tick.isValid = TRUE;
    tick.value = ticksPtr->initial + d;
    return tick;
}

static Tick
NextMinorTick(Scale *scalePtr)
{
    double d;                           /* Delta from major to minor
                                         * tick. */
    Ticks *ticksPtr;
    Tick tick;

    ticksPtr = &scalePtr->minor;
    ticksPtr->index++;
    tick.isValid = FALSE;
    tick.value = Blt_NaN();
    if (ticksPtr->index >= ticksPtr->numSteps) {
        return tick;
    }
    d = ticksPtr->initial;              
    switch (ticksPtr->axisScale) {
    case AXIS_LINEAR:
    default:
        d += ticksPtr->range * (ticksPtr->index + 1) * ticksPtr->step;
        break;

    case AXIS_CUSTOM:                   /* User defined minor ticks */
        d += ticksPtr->range * ticksPtr->values[ticksPtr->index];
        break;

    case AXIS_LOGARITHMIC:
        d += ticksPtr->range * logTable[ticksPtr->index];
        break;

    case AXIS_TIME:
        switch (ticksPtr->timeUnits) {
        case UNITS_YEARS:
            {
                int i;

                for (i = 0; i < ticksPtr->step; i++) {
                    int year, numDays;

                    year = ticksPtr->year++;
                    numDays = numDaysYear[IsLeapYear(year)];
                    ticksPtr->numDaysFromInitial += numDays;
                }
                d += ticksPtr->numDaysFromInitial * SECONDS_DAY;
            }
            break;
        case UNITS_MONTHS:
            {
                int mon, year;
                long numDays;
                int i;

                mon = ticksPtr->month + 1;
                year = ticksPtr->year;
                numDays = 0;
                for (i = 0; i <= ticksPtr->index; i++, mon++) {
                    if (mon > 11) {
                        mon = 0;
                        year++;
                    }
                    numDays += numDaysMonth[IsLeapYear(year)][mon];
                }
                d += numDays * SECONDS_DAY;
            }
            break;
        case UNITS_WEEKS:
            ticksPtr->numDaysFromInitial += 7;
            d += ticksPtr->numDaysFromInitial * SECONDS_DAY;
            break;
        case UNITS_DAYS:
        case UNITS_HOURS:
        case UNITS_MINUTES:
            d += (ticksPtr->index + 1) * ticksPtr->step;
            break;
        case UNITS_SECONDS:
        case UNITS_SUBSECONDS:
            d += (ticksPtr->range * ticksPtr->step * ticksPtr->index);
            break;
        }
        break;
    }
    tick.isValid = TRUE;
    tick.value = d;
    return tick;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetAxisGeometry --
 *
 * Results:
 *      None.
 *
 *      |h|     label     value
 *           spectrum.
 *                                  V
 *           ------------------------
 *                       x
 *           ------------------------
 *           ^   |   |   |   |   |   |
 *           |   |   |   |   |   |   |
 *           0  10  20  30  40  50  60
 *
 *
 * Exterior axis:
 *                    l       r
 *  |a|b|c|d|e|f|g|h|i|   j   |i|h|g|f|e|d|c|d|a|
 *
 * Interior axis: 
 *                  l           r
 *  |a|b|c|d|h|g|f|e|     j     |e|f|g|h|d|c|b|a|
 *               i..             ..i 
 * a = highlight thickness
 * b = scale borderwidth
 * c = axis title
 * d = tick label 
 * e = tick 
 * f = axis line
 * g = 1 pixel pad
 * h = plot borderwidth
 * i = plot pad
 * j = plot area 
 *---------------------------------------------------------------------------
 */
static void
GetAxisGeometry(Scale *scalePtr)
{
    int numTicks;
    Tick left, right;

    FreeTickLabels(scalePtr->tickLabels);
    
    scalePtr->maxTickLabelHeight = scalePtr->maxTickLabelWidth = 0;
    
    /* Compute the tick labels. */
    numTicks = scalePtr->major.numSteps;
    assert(numTicks <= MAXTICKS);
    for (left = FirstMajorTick(scalePtr); left.isValid; left = right) {
        TickLabel *labelPtr;
        double mid;
        int lw, lh;                 /* Label width and height. */
        
        right = NextMajorTick(scalePtr);
        mid = left.value;
        if ((scalePtr->flags & LABELOFFSET) && (right.isValid)) {
            mid = (right.value - left.value) * 0.5;
        }
        if (!InRange(mid, &scalePtr->tickRange)) {
            continue;
        }
        labelPtr = MakeLabel(scalePtr, left.value, NULL);
        Blt_Chain_Append(scalePtr->tickLabels, labelPtr);
        /* 
         * Get the dimensions of each tick label.  Remember tick labels
         * can be multi-lined and/or rotated.
         */
        Blt_GetTextExtents(scalePtr->tickFont, 0, labelPtr->string, -1, 
                           &labelPtr->width, &labelPtr->height);
        
        if (scalePtr->tickAngle != 0.0f) {
            double rlw, rlh;        /* Rotated label width and height. */

            Blt_GetBoundingBox((double)labelPtr->width, 
                               (double)labelPtr->height, 
                               scalePtr->tickAngle, &rlw, &rlh, NULL);
            lw = ROUND(rlw), lh = ROUND(rlh);
        } else {
            lw = labelPtr->width;
            lh = labelPtr->height;
        }
        if (scalePtr->maxTickLabelWidth < lw) {
            scalePtr->maxTickLabelWidth = lw;
        }
        if (scalePtr->maxTickLabelHeight < lh) {
            scalePtr->maxTickLabelHeight = lh;
        }
    }
    assert(Blt_Chain_GetLength(scalePtr->tickLabels) <= numTicks);
}

/*
 *---------------------------------------------------------------------------
 *
 * ComputeHorizontalGeometry --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
ComputeHorizontalGeometry(Scale *scalePtr) 
{
    int y, x1, x2;
    TickLabel *minLabelPtr, *maxLabelPtr;
    int leftPad, rightPad;

    if (scalePtr->flags & DECREASING) {
        minLabelPtr = GetLastTickLabel(scalePtr);
        maxLabelPtr = GetFirstTickLabel(scalePtr);
    } else {
        maxLabelPtr = GetLastTickLabel(scalePtr);
        minLabelPtr = GetFirstTickLabel(scalePtr);
    }
    leftPad = MAX(minLabelPtr->width / 2, scalePtr->arrowWidth / 2);
    rightPad = MAX(maxLabelPtr->width / 2, scalePtr->arrowWidth / 2);

    x1 = scalePtr->inset + leftPad + PADX;    
    x2 = Tk_Width(scalePtr->tkwin) - (scalePtr->inset + rightPad + PADX);

    y = scalePtr->inset + 2 * PADY;
    if (scalePtr->flags & SHOW_TITLE) {
        y += scalePtr->titleHeight + PADY;
    }
    if (scalePtr->flags & SHOW_COLORBAR) {
        scalePtr->colorbar.height = (scalePtr->flags & EXTERIOR) ?
            MAX(scalePtr->colorbar.thickness, scalePtr->tickLength) :
            scalePtr->colorbar.thickness;
        y += scalePtr->colorbar.height + PADY;
    } else {
        y += scalePtr->arrowHeight;
    }
    scalePtr->x1 = x1;
    scalePtr->x2 = x2;
#ifdef notdef
    scalePtr->y1 = y;
    scalePtr->y2 = y + scalePtr->axisLineWidth;
    fprintf(stderr, "setting y2 to %d\n", scalePtr->y2);
#endif
    scalePtr->screenScale = 1.0 / (x2 - x1);

    y += scalePtr->axisLineWidth;
    if (scalePtr->flags & SHOW_TICKS) {
        y += PADY;
        if (scalePtr->flags & EXTERIOR) {
            y += scalePtr->tickLength;
        } 
    }
    if (scalePtr->flags & SHOW_TICKLABELS) {
        y += scalePtr->maxTickLabelHeight + PADY;
    }
    y += PADY + scalePtr->inset;
    scalePtr->height = y;
    scalePtr->width = Tk_Width(scalePtr->tkwin);
}    

/*
 *---------------------------------------------------------------------------
 *
 * ComputeVerticalGeometry --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
ComputeVerticalGeometry(Scale *scalePtr) 
{
    int x, y1, y2;
    TickLabel *minLabelPtr, *maxLabelPtr;
    int topPad, bottomPad;

    if (scalePtr->flags & DECREASING) {
        minLabelPtr = GetLastTickLabel(scalePtr);
        maxLabelPtr = GetFirstTickLabel(scalePtr);
    } else {
        maxLabelPtr = GetLastTickLabel(scalePtr);
        minLabelPtr = GetFirstTickLabel(scalePtr);
    }
    bottomPad = MAX(minLabelPtr->height / 2, scalePtr->arrowWidth / 2);
    topPad = MAX(maxLabelPtr->height / 2, scalePtr->arrowWidth / 2);

    y1 = scalePtr->inset + topPad;    
    y2 = Tk_Height(scalePtr->tkwin) - (scalePtr->inset + bottomPad);

    x = scalePtr->inset + PADX;
    if (scalePtr->flags & SHOW_TITLE) {
        y1 += scalePtr->titleHeight + PADX;
    }
    if (scalePtr->flags & SHOW_COLORBAR) {
        scalePtr->colorbar.width = (scalePtr->flags & EXTERIOR) ?
            MAX(scalePtr->colorbar.thickness, scalePtr->tickLength) :
            scalePtr->colorbar.thickness;
        x += scalePtr->colorbar.width + PADX;
    } else {
        x += scalePtr->arrowHeight;
    }
    scalePtr->x1 = x;
    scalePtr->x2 = x + scalePtr->axisLineWidth;
    scalePtr->y1 = y1;
    scalePtr->y2 = y2;
    scalePtr->screenScale = 1.0 / (y2 - y1);

    x += scalePtr->axisLineWidth;
    if (scalePtr->flags & SHOW_TICKS) {
        x += PADX;
        if (scalePtr->flags & EXTERIOR) {
            x += scalePtr->tickLength;
        } 
    }
    if (scalePtr->flags & SHOW_TICKLABELS) {
        x += scalePtr->maxTickLabelHeight + PADX;
    }
    x += PADX + scalePtr->inset;
    scalePtr->width = x;
    scalePtr->height = Tk_Height(scalePtr->tkwin);
}    

/*
 *---------------------------------------------------------------------------
 *
 * ComputeGeometry --
 *
 *           Window Width
 *      ___________________________________________________________
 *      |          |                               |               |
 *      |          |   TOP  height of title        |               |
 *      |          |                               |               |
 *      |          |           x2 title            |               |
 *      |          |                               |               |
 *      |          |        height of x2-axis      |               |
 *      |__________|_______________________________|_______________|  W
 *      |          | -plotpady                     |               |  i
 *      |__________|_______________________________|_______________|  n
 *      |          | top                   right   |               |  d
 *      |          |                               |               |  o
 *      |   LEFT   |                               |     RIGHT     |  w
 *      |          |                               |               |
 *      | y        |     Free area = 104%          |      y2       |  H
 *      |          |     Plotting surface = 100%   |               |  e
 *      | t        |     Tick length = 2 + 2%      |      t        |  i
 *      | i        |                               |      i        |  g
 *      | t        |                               |      t  legend|  h
 *      | l        |                               |      l   width|  t
 *      | e        |                               |      e        |
 *      |    height|                               |height         |
 *      |       of |                               | of            |
 *      |    y-axis|                               |y2-axis        |
 *      |          |                               |               |
 *      |          |origin 0,0                     |               |
 *      |__________|_left_________________bottom___|_______________|
 *      |          |-plotpady                      |               |
 *      |__________|_______________________________|_______________|
 *      |          | (xoffset, yoffset)            |               |
 *      |          |                               |               |
 *      |          |       height of x-axis        |               |
 *      |          |                               |               |
 *      |          |   BOTTOM   x title            |               |
 *      |__________|_______________________________|_______________|
 *
 * 3) The TOP margin is the area from the top window border to the top
 *    of the graph. It composes the border width, twice the height of
 *    the title font (if one is given) and some padding between the
 *    title.
 *
 * 4) The BOTTOM margin is area from the bottom window border to the
 *    X axis (not including ticks). It composes the border width, the height
 *    an optional X axis label and its padding, the height of the font
 *    of the tick labels.
 *
 * The plotting area is between the margins which includes the X and Y axes
 * including the ticks but not the tick numeric labels. The length of the
 * ticks and its padding is 5% of the entire plotting area.  Hence the
 * entire plotting area is scaled as 105% of the width and height of the
 * area.
 *
 * The axis labels, ticks labels, title, and legend may or may not be
 * displayed which must be taken into account.
 *
 * if reqWidth > 0 : set outer size
 * if reqPlotWidth > 0 : set plot size
 *---------------------------------------------------------------------------
 */
static void
ComputeGeometry(Scale *scalePtr)
{
    scalePtr->flags &= ~GEOMETRY;
    GetAxisGeometry(scalePtr);
    if (HORIZONTAL(scalePtr)) {
        ComputeHorizontalGeometry(scalePtr);
    } else {
        ComputeVerticalGeometry(scalePtr);
    }
    if (scalePtr->reqHeight > 0) {
        scalePtr->height = scalePtr->reqHeight;
    }
    if (scalePtr->reqWidth > 0) {
        scalePtr->width = scalePtr->reqWidth;
    }
    if ((scalePtr->width != Tk_ReqWidth(scalePtr->tkwin)) ||
        (scalePtr->height != Tk_ReqHeight(scalePtr->tkwin))) {
        Tk_GeometryRequest(scalePtr->tkwin, scalePtr->width, scalePtr->height);
    }
    scalePtr->flags |= LAYOUT_PENDING;
}

/*
 *---------------------------------------------------------------------------
 *
 * ResetGCs --
 *
 *      Configures axis attributes (font, line width, label, etc) and
 *      allocates a new (possibly shared) graphics context.  Line cap style
 *      is projecting.  This is for the problem of when a tick sits
 *      directly at the end point of the axis.
 *
 * Results:
 *      The return value is a standard TCL result.
 *
 * Side Effects:
 *      Axis resources are allocated (GC, font). Axis layout is deferred
 *      until the height and width of the window are known.
 *
 *---------------------------------------------------------------------------
 */
static void
ResetGCs(Scale *scalePtr)
{
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;
    
    /* Disabled GC  */
    gcMask = GCForeground;
    gcValues.foreground = scalePtr->disabledFgColor->pixel;
    newGC = Tk_GetGC(scalePtr->tkwin, gcMask, &gcValues);
    if (scalePtr->disabledGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->disabledGC);
    }
    scalePtr->disabledGC = newGC;

    /* Tick GC  */
    gcMask = (GCForeground | GCLineWidth | GCCapStyle);
    gcValues.foreground = scalePtr->tickColor->pixel;
    gcValues.line_width = scalePtr->tickLineWidth;
    gcValues.cap_style = CapProjecting;
    newGC = Tk_GetGC(scalePtr->tkwin, gcMask, &gcValues);
    if (scalePtr->tickGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->tickGC);
    }
    scalePtr->tickGC = newGC;

    /* Disabled Tick GC  */
    gcMask = (GCForeground | GCLineWidth | GCCapStyle);
    gcValues.foreground = scalePtr->disabledFgColor->pixel;
    gcValues.font = Blt_Font_Id(scalePtr->tickFont);
    gcValues.line_width = scalePtr->tickLineWidth;
    gcValues.cap_style = CapProjecting;
    newGC = Tk_GetGC(scalePtr->tkwin, gcMask, &gcValues);
    if (scalePtr->disabledTickGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->disabledTickGC);
    }
    scalePtr->disabledTickGC = newGC;

    /* Axis Line GC  */
    gcMask = GCForeground;
    gcValues.foreground = scalePtr->axisLineColor->pixel;
    newGC = Tk_GetGC(scalePtr->tkwin, gcMask, &gcValues);
    if (scalePtr->axisLineGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->axisLineGC);
    }
    scalePtr->axisLineGC = newGC;

    /* Interval GC  */
    gcMask = GCForeground;
    gcValues.foreground = scalePtr->rangeColor->pixel;
    newGC = Tk_GetGC(scalePtr->tkwin, gcMask, &gcValues);
    if (scalePtr->rangeGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->rangeGC);
    }
    scalePtr->rangeGC = newGC;
}

/*
 *---------------------------------------------------------------------------
 *
 * DestroyScale --
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Resources (font, color, gc, labels, etc.) associated with the scale
 *      are deallocated.
 *
 *---------------------------------------------------------------------------
 */
static void
DestroyScale(Scale *scalePtr)
{
    Blt_FreeOptions(configSpecs, (char *)scalePtr, scalePtr->display, 0);
    Blt_DestroyBindingTable(scalePtr->bindTable);

    if (scalePtr->tickGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->tickGC);
    }
    if (scalePtr->disabledTickGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->disabledTickGC);
    }
    if (scalePtr->disabledGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->disabledGC);
    }
    if (scalePtr->axisLineGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->axisLineGC);
    }
    if (scalePtr->rangeGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->rangeGC);
    }
    FreeTickLabels(scalePtr->tickLabels);
    Blt_Chain_Destroy(scalePtr->tickLabels);

    if (scalePtr->tickSegments != NULL) {
        Blt_Free(scalePtr->tickSegments);
    }
    if (scalePtr->colorbar.picture != NULL) {
        Blt_FreePicture(scalePtr->colorbar.picture);
    }
    if (scalePtr->activeMaxArrow != NULL) {
        Blt_FreePicture(scalePtr->activeMaxArrow);
    }
    if (scalePtr->activeMinArrow != NULL) {
        Blt_FreePicture(scalePtr->activeMinArrow);
    }
    if (scalePtr->normalMaxArrow != NULL) {
        Blt_FreePicture(scalePtr->normalMaxArrow);
    }
    if (scalePtr->normalMinArrow != NULL) {
        Blt_FreePicture(scalePtr->normalMinArrow);
    }
    if (scalePtr->disabledMaxArrow != NULL) {
        Blt_FreePicture(scalePtr->disabledMaxArrow);
    }
    if (scalePtr->disabledMinArrow != NULL) {
        Blt_FreePicture(scalePtr->disabledMinArrow);
    }
    Blt_Free(scalePtr);
}

static void
FreeScale(DestroyData data)
{
    Scale *scalePtr = (Scale *)data;
    DestroyScale(scalePtr);
}


static ScalePart
IdentifyHorizontalPart(Scale *scalePtr, int sx, int sy)
{
    if ((sx >= scalePtr->titleX) && (sy >= scalePtr->titleY) &&
        (sx < (scalePtr->titleX + scalePtr->titleWidth)) &&
        (sy < (scalePtr->titleY + scalePtr->titleHeight))) {
        return PICK_TITLE;
    }
    /* Grip */
    if ((scalePtr->flags & (ACTIVE|SHOW_GRIP)) == (ACTIVE|SHOW_GRIP)) {
        int x, y;

        x = HMap(scalePtr, scalePtr->mark);
        y = ((scalePtr->y2 + scalePtr->y1) / 2) - (scalePtr->gripHeight / 2);
        if ((sx >= (x - scalePtr->gripWidth / 2)) && 
            (sy >= y) && 
            (sx < (x + scalePtr->gripWidth / 2)) && 
            (sy < (y + scalePtr->gripHeight))) {
            return PICK_GRIP;
        }
    }
    /* Value */
    if ((scalePtr->flags & (ACTIVE|SHOW_VALUE)) == (ACTIVE|SHOW_VALUE)) {
        if ((sx >= scalePtr->valuePtr->x) && 
            (sy >= scalePtr->valuePtr->y) && 
            (sx <= (scalePtr->valuePtr->x + scalePtr->valuePtr->width)) && 
            (sy <= (scalePtr->valuePtr->y + scalePtr->valuePtr->height))) {
            return PICK_VALUE;
        }
    }
    /* Axis line. */
    if ((sx >= scalePtr->x1) && (sy >= scalePtr->y1) &&
        (sx < scalePtr->x2) && (sy < scalePtr->y2)) {
        return PICK_AXIS;
    }
    /* Max arrow. */
    if (scalePtr->flags & SHOW_MAXARROW) {
        int x;

        x = HMap(scalePtr, scalePtr->innerRight);
        if ((sx >= (x - scalePtr->arrowWidth / 2)) && 
            (sy >= (scalePtr->y1 - scalePtr->arrowHeight)) &&
            (sx < (x + scalePtr->arrowWidth / 2)) &&
            (sy < scalePtr->y1)) {
            return PICK_MAXARROW;
        }
    }
    /* Min arrow. */
    if (scalePtr->flags & SHOW_MINARROW) {
        int x;

        x = HMap(scalePtr, scalePtr->innerLeft);
        if ((sx >= (x - scalePtr->arrowWidth / 2)) && 
            (sy > scalePtr->y2) &&
            (sx < (x + scalePtr->arrowWidth / 2)) &&
            (sy < (scalePtr->y2 + scalePtr->arrowHeight))) {
            return PICK_MINARROW;
        }
    }
    /* Current value line. */
    if (scalePtr->flags & SHOW_MARK) {
        int x;

        x = HMap(scalePtr, scalePtr->mark);
        if ((sx >= (x - scalePtr->markWidth / 2)) && 
            (sy > (scalePtr->inset + PADY)) &&
            (sx <= (x + scalePtr->markWidth / 2)) &&
            (sy < (Tk_Height(scalePtr->tkwin) - scalePtr->inset - PADY))) {
            return PICK_MARK;
        }
    }
    /* Colorbar. */
    if (scalePtr->flags & SHOW_COLORBAR) {
        if ((sx >= scalePtr->colorbar.x) && 
            (sy >= scalePtr->colorbar.y) &&
            (sx < (scalePtr->colorbar.x + scalePtr->colorbar.width)) &&
            (sy < (scalePtr->colorbar.y + scalePtr->colorbar.height))) {
            return PICK_COLORBAR;
        }
    }
    return PICK_NONE;
}

static ScalePart
IdentifyVerticalPart(Scale *scalePtr, int sx, int sy)
{
    if ((sx >= scalePtr->titleX) && (sy >= scalePtr->titleY) &&
        (sx < (scalePtr->titleX + scalePtr->titleWidth)) &&
        (sy < (scalePtr->titleY + scalePtr->titleHeight))) {
        return PICK_TITLE;
    }
    /* Grip */
    if ((scalePtr->flags & (ACTIVE|SHOW_GRIP)) == (ACTIVE|SHOW_GRIP)) {
        int x, y;

        y = VMap(scalePtr, scalePtr->mark);
        x = ((scalePtr->x2 + scalePtr->x1) / 2) - (scalePtr->gripHeight / 2);
        if ((sy >= (y - scalePtr->gripWidth / 2)) && 
            (sx >= x) && 
            (sy < (y + scalePtr->gripWidth / 2)) && 
            (sx < (x + scalePtr->gripHeight))) {
            return PICK_GRIP;
        }
    }
    /* Value */
    if ((scalePtr->flags & (ACTIVE|SHOW_VALUE)) == (ACTIVE|SHOW_VALUE)) {
        if ((sx >= scalePtr->valuePtr->x) && 
            (sy >= scalePtr->valuePtr->y) && 
            (sx < (scalePtr->valuePtr->x + scalePtr->valuePtr->width)) && 
            (sy < (scalePtr->valuePtr->y + scalePtr->valuePtr->height))) {
            return PICK_VALUE;
        }
    }
    /* Axis line. */
    if ((sx >= scalePtr->x1) && (sy >= scalePtr->y1) &&
        (sx < scalePtr->x2) && (sy < scalePtr->y2)) {
        return PICK_AXIS;
    }
    /* Max arrow. */
    if (scalePtr->flags & SHOW_MAXARROW) {
        int y;

        y = VMap(scalePtr, scalePtr->innerRight);
        if ((sy >= (y - scalePtr->arrowWidth / 2)) && 
            (sx >= (scalePtr->x1 - scalePtr->arrowHeight)) &&
            (sy < (y + scalePtr->arrowWidth / 2)) &&
            (sx < scalePtr->x1)) {
            return PICK_MAXARROW;
        }
    }
    /* Min arrow. */
    if (scalePtr->flags & SHOW_MINARROW) {
        int y;

        y = VMap(scalePtr, scalePtr->innerLeft);
        if ((sy >= (y - scalePtr->arrowWidth / 2)) && 
            (sx > scalePtr->x2) &&
            (sy < (y + scalePtr->arrowWidth / 2)) &&
            (sx < (scalePtr->x2 + scalePtr->arrowHeight))) {
            return PICK_MINARROW;
        }
    }
    /* Current value line. */
    if (scalePtr->flags & SHOW_MARK) {
        int y;

        y = VMap(scalePtr, scalePtr->mark);
        if ((sy >= (y - scalePtr->markWidth / 2)) && 
            (sx > (scalePtr->inset + PADX)) &&
            (sy <= (y + scalePtr->markWidth / 2)) &&
            (sx < (Tk_Width(scalePtr->tkwin) - scalePtr->inset - PADX))) {
            return PICK_MARK;
        }
    }
    /* Colorbar. */
    if (scalePtr->flags & SHOW_COLORBAR) {
        if ((sx >= scalePtr->colorbar.x) && 
            (sy >= scalePtr->colorbar.y) &&
            (sx < (scalePtr->colorbar.x + scalePtr->colorbar.width)) &&
            (sy < (scalePtr->colorbar.y + scalePtr->colorbar.height))) {
            return PICK_COLORBAR;
        }
    }
    return PICK_NONE;
}

/*ARGSUSED*/
static void
AppendTagsProc(Blt_BindTable table, ClientData object, ClientData context, 
               Blt_Chain tags)
{
    Blt_Chain_Append(tags, object);
}

/*
 *---------------------------------------------------------------------------
 *
 * PickPartProc --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static ClientData
PickPartProc(ClientData clientData, int sx, int sy, ClientData *contextPtr)
{
    Scale *scalePtr = clientData;
    ScalePart id;

    if (HORIZONTAL(scalePtr)) {
        id = IdentifyHorizontalPart(scalePtr, sx, sy);
    } else {
        id = IdentifyVerticalPart(scalePtr, sx, sy);
    }
    return (ClientData)(intptr_t)id;
}

/*
 *---------------------------------------------------------------------------
 *
 * NewScale --
 *
 *      Create and initialize a structure containing information to display
 *      a scale.
 *
 * Results:
 *      The return value is a pointer to an Scale structure.
 *
 *---------------------------------------------------------------------------
 */
static Scale *
NewScale(Tcl_Interp *interp, Tk_Window tkwin)
{
    Scale *scalePtr;

    scalePtr = Blt_Calloc(1, sizeof(Scale));
    if (scalePtr == NULL) {
        Tcl_AppendResult(interp, "can't allocate memory for scale \"", 
                Tk_PathName(tkwin), "\"", (char *)NULL);
        return NULL;
    }
    Tk_SetClass(tkwin, "BltScale");
    scalePtr->display = Tk_Display(tkwin);
    scalePtr->reqInnerLeft = scalePtr->reqInnerRight = Blt_NaN();
    scalePtr->interp = interp;
    scalePtr->tkwin = tkwin;
    scalePtr->relief = TK_RELIEF_FLAT;
    scalePtr->activeRelief = TK_RELIEF_RAISED;
    scalePtr->gripRelief = TK_RELIEF_RAISED;
    scalePtr->titleJustify = TK_JUSTIFY_CENTER;
    scalePtr->gripBorderWidth = 2;
    scalePtr->borderWidth = 2;
    scalePtr->reqNumMinorTicks = 2;
    scalePtr->reqNumMajorTicks = 10;
    scalePtr->tickLength = 8;
    scalePtr->outerLeft = 0.1;
    scalePtr->outerRight = 1.0;
    scalePtr->flags = (GEOMETRY | LAYOUT_PENDING | SHOW_ALL | AUTO_MAJOR| AUTO_MINOR | TIGHT | EXTERIOR);
    scalePtr->tickLabels = Blt_Chain_Create();
    scalePtr->tickLineWidth = 1;
    Blt_SetWindowInstanceData(tkwin, scalePtr);
    scalePtr->mark = 0.53;
    scalePtr->markWidth = 1;
    scalePtr->bindTable = Blt_CreateBindingTable(interp, tkwin, scalePtr, 
        PickPartProc, AppendTagsProc);
    return scalePtr;
}


/*
 *---------------------------------------------------------------------------
 *
 * ConfigureScale --
 *
 *      Configures axis attributes (font, line width, label, etc).
 *
 * Results:
 *      The return value is a standard TCL result.
 *
 * Side Effects:
 *      Axis layout is deferred until the height and width of the window are
 *      known.
 *
 *---------------------------------------------------------------------------
 */
static int
ConfigureScale(
    Tcl_Interp *interp,                 /* Interpreter to report errors. */
    Scale *scalePtr,                    /* Information about widget; may or
                                         * may not already have values for
                                         * some fields. */
    int objc,
    Tcl_Obj *const *objv,
    int flags)
{
    float angle;
    Blt_FontMetrics fm;
    double oldMark;
    
    oldMark = scalePtr->mark;
    if (Blt_ConfigureWidgetFromObj(interp, scalePtr->tkwin, configSpecs, 
           objc, objv, (char *)scalePtr, flags) != TCL_OK) {
        return TCL_ERROR;
    }

    Blt_Font_GetMetrics(scalePtr->tickFont, &fm);
    scalePtr->tickLength = fm.linespace * 60 / 100;
    scalePtr->arrowHeight = fm.linespace;
    scalePtr->arrowWidth = fm.linespace;
    scalePtr->gripHeight = fm.linespace;
    scalePtr->gripWidth = fm.linespace * 60 / 100;

    if (scalePtr->painter == NULL) {
        scalePtr->painter = Blt_GetPainter(scalePtr->tkwin, 1.0);
    }
    /* Check that the designate interval represents a valid range. Swap
     * values and if min is greater than max. */
    if (scalePtr->outerLeft >= scalePtr->outerRight) {
        double tmp;

        /* Just reverse the values. */
        tmp = scalePtr->outerLeft;
        scalePtr->outerLeft = scalePtr->outerRight;
        scalePtr->outerRight = tmp;
    }
    /* Log scale limits must be positive. Move the limits to the positive
     * range. */
    if ((IsLogScale(scalePtr)) && (scalePtr->outerLeft <= 0.0)) {
        double delta;
        
        delta = 1.0 - scalePtr->outerLeft;
        scalePtr->outerLeft = 1.0;
        scalePtr->outerRight = scalePtr->outerRight + delta;
    }
    if (scalePtr->outerLeft == scalePtr->outerRight) {
        scalePtr->outerRight = scalePtr->outerLeft + 1.0;
    }
    /* If no range min and max is set, use the data range values. */
    scalePtr->innerLeft = (DEFINED(scalePtr->reqInnerLeft))
        ? scalePtr->reqInnerLeft : scalePtr->outerLeft;
    scalePtr->innerRight = (DEFINED(scalePtr->reqInnerRight))
        ? scalePtr->reqInnerRight : scalePtr->outerRight;

    /* Subrange min and max must be within the scale's range. */
    if (scalePtr->innerLeft < scalePtr->outerLeft) {
        scalePtr->innerLeft = scalePtr->outerLeft;
    }
    if (scalePtr->innerRight > scalePtr->outerRight) {
        scalePtr->innerRight = scalePtr->outerRight;
    }
    if ((Blt_ConfigModified(configSpecs, "-rangemin", (char *)NULL)) &&
        (scalePtr->innerLeft > scalePtr->innerRight)) {
        scalePtr->innerLeft = scalePtr->innerRight;
    }
    if ((Blt_ConfigModified(configSpecs, "-rangemax", (char *)NULL)) &&
        (scalePtr->innerRight < scalePtr->innerLeft)) {
        scalePtr->innerRight = scalePtr->innerLeft;
    }
    if (scalePtr->innerLeft > scalePtr->innerRight) {
        double tmp;

        /* Just reverse the values. */
        tmp = scalePtr->innerLeft;
        scalePtr->innerLeft = scalePtr->innerRight;
        scalePtr->innerRight = tmp;
    }
    /* If there's a resolution set, adjust the range accordingly. */
    if (scalePtr->resolution > 0.0) {
        double left, right;

        left = UROUND(scalePtr->innerLeft, scalePtr->resolution);
        if (left < scalePtr->outerLeft) {
            left += scalePtr->resolution;
        }
        right = UROUND(scalePtr->innerRight, scalePtr->resolution);
        if (right > scalePtr->outerRight) {
            right -= scalePtr->resolution;
        }
        scalePtr->innerLeft = left;
        scalePtr->innerRight = right;
    }
    if (!DEFINED(scalePtr->mark)) {
        scalePtr->mark = scalePtr->innerLeft;
    }        
    if (scalePtr->mark < scalePtr->innerLeft) {
        scalePtr->mark = scalePtr->innerLeft;
    }
    if (scalePtr->mark > scalePtr->innerRight) {
        scalePtr->mark = scalePtr->innerRight;
    }
    if (scalePtr->resolution > 0.0) {
        double mark;

        mark = UROUND(scalePtr->mark, scalePtr->resolution);
        if (mark < scalePtr->innerLeft) {
            mark += scalePtr->resolution;
        }
        if (mark > scalePtr->innerRight) {
            mark -= scalePtr->resolution;
        }
        scalePtr->mark = mark;
    }
    if (scalePtr->mark != oldMark) {
        SetValue(scalePtr, scalePtr->mark);
    }
    if (IsLogScale(scalePtr)) {
        LogScaleAxis(scalePtr);
    } else if (IsTimeScale(scalePtr)) {
        TimeScaleAxis(scalePtr);
    } else {
        LinearScaleAxis(scalePtr);
    }

    ComputeGeometry(scalePtr);
    angle = FMOD(scalePtr->tickAngle, 360.0f);
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    scalePtr->tickAngle = angle;
    if (scalePtr->normalBg != NULL) {
        Blt_Bg_SetChangedProc(scalePtr->normalBg, BackgroundChangedProc, 
                scalePtr);
    }
    ResetGCs(scalePtr);
    if (Blt_ConfigModified(configSpecs, "-palette", "-orient", (char *)NULL)) {
        if (scalePtr->colorbar.picture != NULL) {
            Blt_FreePicture(scalePtr->colorbar.picture);
            scalePtr->colorbar.picture = NULL;
        }
    }
    scalePtr->titleWidth = scalePtr->titleHeight = 0;
    if (scalePtr->title != NULL) {
        unsigned int w, h;

        Blt_GetTextExtents(scalePtr->titleFont, 0, scalePtr->title, -1, &w, &h);
        scalePtr->titleWidth = w;
        scalePtr->titleHeight = h;
    }
    scalePtr->flags |= LAYOUT_PENDING;
    EventuallyRedraw(scalePtr);
    return TCL_OK;
}


static void
MapHorizontalTicks(Scale *scalePtr, int y)
{
    int arraySize;
    int numMajorTicks, numMinorTicks;
    XSegment *segments, *segPtr;
    Blt_ChainLink link;
    double labelPos;
    Tick left, right;
    int minorTickLength, majorTickLength;

    scalePtr->tickAnchor = TK_ANCHOR_N;
    minorTickLength = (scalePtr->tickLength * 10) / 15;
    majorTickLength = scalePtr->tickLength;
    if ((scalePtr->flags & EXTERIOR) == 0) {
        y -= scalePtr->axisLineWidth;
        minorTickLength = -minorTickLength;
        majorTickLength = -majorTickLength;
    }
    numMajorTicks = scalePtr->major.numSteps;
    numMinorTicks = scalePtr->minor.numSteps;
    arraySize = 1 + numMajorTicks + (numMajorTicks * (numMinorTicks + 1));
    segments = Blt_AssertMalloc(arraySize * sizeof(XSegment));
    segPtr = segments;

    link = Blt_Chain_FirstLink(scalePtr->tickLabels);
    labelPos = y + scalePtr->tickLength + PADY;
    for (left = FirstMajorTick(scalePtr); left.isValid; left = right) {
        right = NextMajorTick(scalePtr);
        if (right.isValid) {
            Tick minor;
            
            /* If this isn't the last major tick, add minor ticks. */
            scalePtr->minor.range = right.value - left.value;
            scalePtr->minor.initial = left.value;
            for (minor = FirstMinorTick(scalePtr); minor.isValid; 
                 minor = NextMinorTick(scalePtr)) {
                if (InRange(minor.value, &scalePtr->tickRange)) {
                    /* Add minor tick. */
                    segPtr->x1 = segPtr->x2 = 
                        ConvertToScreenX(scalePtr, minor.value);
                    segPtr->y1 = y;
                    segPtr->y2 = y + minorTickLength;
                    segPtr++;
                }
            }        
        }
        if (InRange(left.value, &scalePtr->tickRange)) {
            double mid;
            
            /* Add major tick. This could be the last major tick. */
            segPtr->x1 = segPtr->x2 = ConvertToScreenX(scalePtr, left.value);
            segPtr->y1 = y;
            segPtr->y2 = y + majorTickLength;
            
            mid = left.value;
            if ((scalePtr->flags & LABELOFFSET) && (right.isValid)) {
                mid = (right.value - left.value) * 0.5;
            }
            if (InRange(mid, &scalePtr->tickRange)) {
                TickLabel *labelPtr;
                
                labelPtr = Blt_Chain_GetValue(link);
                link = Blt_Chain_NextLink(link);
                
                labelPtr->x = segPtr->x1;
                labelPtr->y = labelPos;
            }
            segPtr++;
        }
    }
    scalePtr->tickSegments = segments;
    scalePtr->numSegments = segPtr - segments;
    assert(scalePtr->numSegments <= arraySize);
}

static void
MapVerticalTicks(Scale *scalePtr, int x)
{
    int arraySize;
    int numMajorTicks, numMinorTicks;
    XSegment *segments, *segPtr;
    Blt_ChainLink link;
    double labelPos;
    Tick left, right;
    int minorTickLength, majorTickLength;

    scalePtr->tickAnchor = TK_ANCHOR_W;
    minorTickLength = (scalePtr->tickLength * 10) / 15;
    majorTickLength = scalePtr->tickLength;
    if ((scalePtr->flags & EXTERIOR) == 0) {
        x -= scalePtr->axisLineWidth;
        minorTickLength = -minorTickLength;
        majorTickLength = -majorTickLength;
    }
    numMajorTicks = scalePtr->major.numSteps;
    numMinorTicks = scalePtr->minor.numSteps;
    arraySize = 1 + numMajorTicks + (numMajorTicks * (numMinorTicks + 1));
    segments = Blt_AssertMalloc(arraySize * sizeof(XSegment));
    segPtr = segments;

    link = Blt_Chain_FirstLink(scalePtr->tickLabels);
    labelPos = x + scalePtr->tickLength + PADX;
    for (left = FirstMajorTick(scalePtr); left.isValid; left = right) {
        right = NextMajorTick(scalePtr);
        if (right.isValid) {
            Tick minor;
            
            /* If this isn't the last major tick, add minor ticks. */
            scalePtr->minor.range = right.value - left.value;
            scalePtr->minor.initial = left.value;
            for (minor = FirstMinorTick(scalePtr); minor.isValid; 
                 minor = NextMinorTick(scalePtr)) {
                if (InRange(minor.value, &scalePtr->tickRange)) {
                    /* Add minor tick. */
                    segPtr->y1 = segPtr->y2 = 
                        ConvertToScreenY(scalePtr, minor.value);
                    segPtr->x1 = x;
                    segPtr->x2 = x + minorTickLength;
                    segPtr++;
                }
            }        
        }
        if (InRange(left.value, &scalePtr->tickRange)) {
            double mid;
            
            /* Add major tick. This could be the last major tick. */
            segPtr->y1 = segPtr->y2 = ConvertToScreenY(scalePtr, left.value);
            segPtr->x1 = x;
            segPtr->x2 = x + majorTickLength;

            mid = left.value;
            if ((scalePtr->flags & LABELOFFSET) && (right.isValid)) {
                mid = (right.value - left.value) * 0.5;
            }
            if (InRange(mid, &scalePtr->tickRange)) {
                TickLabel *labelPtr;
                
                labelPtr = Blt_Chain_GetValue(link);
                link = Blt_Chain_NextLink(link);
                
                labelPtr->x = labelPos;
                labelPtr->y = segPtr->y1;
            }
            segPtr++;
        }
    }
    scalePtr->tickSegments = segments;
    scalePtr->numSegments = segPtr - segments;
    assert(scalePtr->numSegments <= arraySize);
}

/*
 *---------------------------------------------------------------------------
 *
 * MapHorizontalScale --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
MapHorizontalScale(Scale *scalePtr) 
{
    int y, ytop;

    y = scalePtr->inset + 2 * PADY;
    if (scalePtr->flags & SHOW_TITLE) {
        scalePtr->titleY = y;
        switch (scalePtr->titleJustify) {
        case TK_JUSTIFY_LEFT:
            scalePtr->titleX = scalePtr->x1;
            scalePtr->titleAnchor = TK_ANCHOR_NW;
            break;
        case TK_JUSTIFY_CENTER:
            scalePtr->titleX = (scalePtr->x1 + scalePtr->x2) / 2;
            scalePtr->titleAnchor = TK_ANCHOR_N;
            break;
        case TK_JUSTIFY_RIGHT:
            scalePtr->titleX = scalePtr->x2;
            scalePtr->titleAnchor = TK_ANCHOR_NE;
            break;
        }
        y += scalePtr->titleHeight + PADY;
    }
    ytop = y;

    /* Compute the location of the axis from the bottom of the widget. */
    y = Tk_Height(scalePtr->tkwin) - scalePtr->inset - PADY;
    if (scalePtr->flags & SHOW_TICKLABELS) {
        y -= scalePtr->maxTickLabelHeight + PADY;
    }
    if (scalePtr->flags & SHOW_TICKS) {
        if (scalePtr->flags & EXTERIOR) {
            y -= scalePtr->tickLength;
        } 
    }
    scalePtr->y2 = y;
    y -= scalePtr->axisLineWidth;
    scalePtr->y1 = y;
    
    if (scalePtr->tickSegments != NULL) {
        Blt_Free(scalePtr->tickSegments);
        scalePtr->tickSegments = NULL;
    }
    if (scalePtr->flags & (SHOW_TICKS|SHOW_TICKLABELS)) {
        MapHorizontalTicks(scalePtr, scalePtr->y2);
    }
    if (scalePtr->flags & SHOW_COLORBAR) {
        int x1, x2;
        
        if (scalePtr->flags & DECREASING) {
            x2 = HMap(scalePtr, scalePtr->outerLeft);
            x1 = HMap(scalePtr, scalePtr->outerRight);
        } else {
            x1 = HMap(scalePtr, scalePtr->outerLeft);
            x2 = HMap(scalePtr, scalePtr->outerRight);
        }
        scalePtr->colorbar.height = y - ytop;
        y -= scalePtr->colorbar.height;
        scalePtr->colorbar.x = MIN(x1, x2);
        scalePtr->colorbar.y = y;
        scalePtr->colorbar.width = ABS(x2 - x1) + 1;
    }
    y += PADY + scalePtr->inset;
    /* Value */
    if (scalePtr->flags & SHOW_VALUE) {
        TickLabel *labelPtr;
        int x;
        
        labelPtr = MakeLabel(scalePtr, scalePtr->mark, scalePtr->units);
        Blt_GetTextExtents(scalePtr->valueFont, 0, labelPtr->string, -1, 
                           &labelPtr->width, &labelPtr->height);
        if (scalePtr->valueAngle != 0.0f) {
            double rlw, rlh;        /* Rotated label width and height. */

            Blt_GetBoundingBox((double)labelPtr->width,
                               (double)labelPtr->height, 
                               scalePtr->valueAngle, &rlw, &rlh, NULL);
            labelPtr->width = ROUND(rlw), labelPtr->height = ROUND(rlh);
        }
        labelPtr->width += 4;
        labelPtr->height += 4;
        x = HMap(scalePtr, scalePtr->mark);
        if (x > (scalePtr->x2 - labelPtr->width - scalePtr->arrowWidth)) {
            x -= scalePtr->gripWidth + labelPtr->width;
        } else {
            x += scalePtr->gripWidth;
        }
        labelPtr->x = x;
        labelPtr->y = scalePtr->colorbar.y +
            (scalePtr->colorbar.height - labelPtr->height) / 2;
        if (scalePtr->valuePtr != NULL) {
            Blt_Free(scalePtr->valuePtr);
        }
        scalePtr->valuePtr = labelPtr;
    }
}    

/*
 *---------------------------------------------------------------------------
 *
 * MapVerticalScale --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
MapVerticalScale(Scale *scalePtr) 
{
    int x, y, xleft;

    x = scalePtr->inset + PADX;
    y = scalePtr->inset + PADY;
    if (scalePtr->flags & SHOW_TITLE) {
        scalePtr->titleY = scalePtr->inset + PADY;
        y += scalePtr->inset + PADY;
        switch (scalePtr->titleJustify) {
        case TK_JUSTIFY_LEFT:
            scalePtr->titleX = scalePtr->inset + PADX;
            scalePtr->titleAnchor = TK_ANCHOR_NW;
            break;
        case TK_JUSTIFY_CENTER:
            scalePtr->titleX = Tk_Width(scalePtr->tkwin) / 2;
            scalePtr->titleAnchor = TK_ANCHOR_N;
            break;
        case TK_JUSTIFY_RIGHT:
            scalePtr->titleX = Tk_Width(scalePtr->tkwin) - scalePtr->inset -
                PADX;
            scalePtr->titleAnchor = TK_ANCHOR_NE;
            break;
        }
        y += scalePtr->titleHeight + PADX;
    }
    xleft = x;

    /* Compute the location of the axis from the right side of the widget. */
    x = Tk_Width(scalePtr->tkwin) - scalePtr->inset - PADX;
    if (scalePtr->flags & SHOW_TICKLABELS) {
        x -= scalePtr->maxTickLabelWidth + PADX;
    }
    if (scalePtr->flags & SHOW_TICKS) {
        if (scalePtr->flags & EXTERIOR) {
            x -= scalePtr->tickLength;
        } 
    }
    scalePtr->x2 = x;
    x -= scalePtr->axisLineWidth;
    scalePtr->x1 = x;
    
    if (scalePtr->tickSegments != NULL) {
        Blt_Free(scalePtr->tickSegments);
        scalePtr->tickSegments = NULL;
    }
    if (scalePtr->flags & (SHOW_TICKS|SHOW_TICKLABELS)) {
        MapVerticalTicks(scalePtr, scalePtr->x2);
    }
    if (scalePtr->flags & SHOW_COLORBAR) {
        int y1, y2;
        
        if (scalePtr->flags & DECREASING) {
            y2 = VMap(scalePtr, scalePtr->outerRight);
            y1 = VMap(scalePtr, scalePtr->outerLeft);
        } else {
            y1 = VMap(scalePtr, scalePtr->outerRight);
            y2 = VMap(scalePtr, scalePtr->outerLeft);
        }
        scalePtr->colorbar.width = x - xleft;
        x -= scalePtr->colorbar.width;
        scalePtr->colorbar.y = y1;
        scalePtr->colorbar.x = x;
        scalePtr->colorbar.height = ABS(y2 - y1);
    }
    x += PADX + scalePtr->inset;
    if (scalePtr->flags & SHOW_VALUE) {
        TickLabel *labelPtr;
        int y;
        
        labelPtr = MakeLabel(scalePtr, scalePtr->mark, scalePtr->units);
        Blt_GetTextExtents(scalePtr->valueFont, 0, labelPtr->string, -1, 
                           &labelPtr->width, &labelPtr->height);
        if (scalePtr->valueAngle != 0.0f) {
            double rlw, rlh;        /* Rotated label width and height. */

            Blt_GetBoundingBox((double)labelPtr->width,
                               (double)labelPtr->height, 
                               scalePtr->valueAngle, &rlw, &rlh, NULL);
            labelPtr->width = ROUND(rlw), labelPtr->height = ROUND(rlh);
        }
        labelPtr->width += 4;
        labelPtr->height += 4;
        y = VMap(scalePtr, scalePtr->mark);
        if (y < (scalePtr->y1 + labelPtr->height + scalePtr->arrowHeight)) {
            y += scalePtr->gripWidth;
        } else {
            y -= scalePtr->gripWidth + labelPtr->height;
        }
        labelPtr->y = y;
        labelPtr->x = scalePtr->colorbar.x +
            (scalePtr->colorbar.width - labelPtr->width) / 2;
        if (scalePtr->valuePtr != NULL) {
            Blt_Free(scalePtr->valuePtr);
        }
        scalePtr->valuePtr = labelPtr;
    }
}    

static int
GradientCalcProc(ClientData clientData, int x, int y, double *valuePtr)
{
    Scale *scalePtr = clientData;
    double t;

    if (HORIZONTAL(scalePtr)) {
        t = (double)x * scalePtr->screenScale;
        if (scalePtr->flags & DECREASING) {
            t = 1.0 - t;
        }
    } else {
        t = (double)y * scalePtr->screenScale;
        if ((scalePtr->flags & DECREASING) == 0) {
            t = 1.0 - t;
        }
    }
    *valuePtr = t;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ColorbarToPicture --
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
static Blt_Picture
ColorbarToPicture(Scale *scalePtr, int w, int h)
{
    Blt_Picture picture;

    if (scalePtr->palette == NULL) {
        return NULL;                    /* No palette defined. */
    }
    picture = Blt_CreatePicture(w, h);
    if (picture != NULL) {
        Blt_PaintBrush brush;

        brush = Blt_NewLinearGradientBrush();
        Blt_SetLinearGradientBrushPalette(brush, scalePtr->palette);
        Blt_SetLinearGradientBrushCalcProc(brush, GradientCalcProc, scalePtr);
        Blt_PaintRectangle(picture, 0, 0, w, h, 0, 0, brush, FALSE);
        Blt_FreeBrush(brush);
        return picture;
    }
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawColorbar --
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawColorbar(Scale *scalePtr, Drawable drawable)
{
    Colorbar *c;
    
    if (scalePtr->palette == NULL) {
        return;                         /* No palette defined. */
    }
    c = &scalePtr->colorbar;
    if ((c->width > 0) && (c->height > 0)) {
        if ((c->picture == NULL) ||
            (Blt_Picture_Width(c->picture) != c->width) ||
            (Blt_Picture_Height(c->picture) != c->height)) {
            Blt_Picture picture;

            picture = ColorbarToPicture(scalePtr, c->width, c->height);
            if (picture == NULL) {
                return;                 /* Background is obscured. */
            }
            if (c->picture != NULL) {
                Blt_FreePicture(c->picture);
            }
            c->picture = picture;
        }
        Blt_PaintPicture(scalePtr->painter, drawable, c->picture, 0, 0, 
              c->width, c->height, c->x, c->y, 0);
    } else {
        fprintf(stderr, "cbw=%d cbh=%d w=%d h=%d rw=%d rh=%d\n",
                c->width, c->height,
                Tk_Width(scalePtr->tkwin), Tk_Height(scalePtr->tkwin),
                Tk_ReqWidth(scalePtr->tkwin), Tk_ReqHeight(scalePtr->tkwin));
    }
}

static Blt_Picture
GetMaxArrowPicture(Scale *scalePtr, int w, int h, int direction)
{
    Blt_Picture *pictPtr;
    Blt_Pixel pixel;

    if (scalePtr->flags & ACTIVE_MAXARROW) {
        pixel.u32 = scalePtr->activeMaxArrowColor.u32;
        pictPtr = &scalePtr->activeMaxArrow;
    } else if (scalePtr->flags & DISABLED) {
        pixel.u32 = Blt_XColorToPixel(scalePtr->disabledFgColor);
        pictPtr = &scalePtr->disabledMaxArrow;
    } else {
        pixel.u32 = scalePtr->normalMaxArrowColor.u32;
        pictPtr = &scalePtr->normalMaxArrow;
    }
    if (((*pictPtr) == NULL) ||
        (Blt_Picture_Width(*pictPtr) != w) ||
        (Blt_Picture_Height(*pictPtr) != h)) {
        Blt_Picture picture;
        int ix, iy, ih, iw;

        if (*pictPtr != NULL) {
            Blt_FreePicture(*pictPtr);
        }
        ih = h;
        iw = w;
        picture = Blt_CreatePicture(w, h);
        Blt_BlankPicture(picture, 0x0);
        iy = (h - ih) / 2;
        ix = (w - iw) / 2;
        Blt_PaintArrowHead(picture, ix, iy, iw, ih, 
                           pixel.u32, direction);
        *pictPtr = picture;
    }
    return *pictPtr;
}

static Blt_Picture
GetMinArrowPicture(Scale *scalePtr, int w, int h, int direction)
{
    Blt_Picture *pictPtr;
    Blt_Pixel pixel;

    if (scalePtr->flags & ACTIVE_MINARROW) {
        pixel.u32 = scalePtr->activeMinArrowColor.u32;
        pictPtr = &scalePtr->activeMinArrow;
    } else if (scalePtr->flags & DISABLED) {
        pixel.u32 = Blt_XColorToPixel(scalePtr->disabledFgColor);
        pictPtr = &scalePtr->disabledMinArrow;
    } else {
        pixel.u32 = scalePtr->normalMinArrowColor.u32;
        pictPtr = &scalePtr->normalMinArrow;
    }
    if (((*pictPtr) == NULL) ||
        (Blt_Picture_Width(*pictPtr) != w) ||
        (Blt_Picture_Height(*pictPtr) != h)) {
        Blt_Picture picture;
        int ix, iy, ih, iw;
        
        if (*pictPtr != NULL) {
            Blt_FreePicture(*pictPtr);
        }
        ih = h;
        iw = w;
        picture = Blt_CreatePicture(w, h);
        Blt_BlankPicture(picture, 0x0);
        iy = (h - ih) / 2;
        ix = (w - iw) / 2;
        Blt_PaintArrowHead(picture, ix, iy, iw, ih, 
                           pixel.u32, direction);
        *pictPtr = picture;
    }
    return *pictPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawScale --
 *
 *      Draws the scale.
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawScale(Scale *scalePtr, Drawable drawable)
{
    Blt_Bg bg;
    XColor *titleFg;
    GC gc;
    int relief;

    if (scalePtr->flags & ACTIVE) {
        bg = scalePtr->activeBg;
        titleFg = scalePtr->titleColor;
        relief = scalePtr->activeRelief;
        gc = scalePtr->tickGC;
    } else if (scalePtr->flags & DISABLED) {
        bg = scalePtr->disabledBg;
        titleFg = scalePtr->disabledFgColor;
        relief = scalePtr->relief;
        gc = scalePtr->disabledGC;
    } else {
        bg = scalePtr->normalBg;
        relief = scalePtr->relief;
        titleFg = scalePtr->titleColor;
        gc = scalePtr->tickGC;
    }        
    Blt_Bg_FillRectangle(scalePtr->tkwin, drawable, bg, 
        scalePtr->highlightWidth, scalePtr->highlightWidth, 
        Tk_Width(scalePtr->tkwin) - 2 * scalePtr->highlightWidth, 
        Tk_Height(scalePtr->tkwin) - 2 * scalePtr->highlightWidth, 
        scalePtr->borderWidth, relief);

    if (scalePtr->flags & SHOW_COLORBAR) {
        DrawColorbar(scalePtr, drawable);
    }
    if ((scalePtr->flags & SHOW_TITLE) && (scalePtr->title != NULL)) {
        TextStyle ts;

        Blt_Ts_InitStyle(ts);
        Blt_Ts_SetFont(ts, scalePtr->titleFont);
        Blt_Ts_SetPadding(ts, 1, 2, 0, 0);
        Blt_Ts_SetAnchor(ts, scalePtr->titleAnchor);
        Blt_Ts_SetForeground(ts, titleFg);
        Blt_Ts_SetMaxLength(ts, scalePtr->width - 2 * scalePtr->inset);
        Blt_Ts_DrawText(scalePtr->tkwin, drawable, scalePtr->title, -1, &ts, 
                scalePtr->titleX, scalePtr->titleY);
    }
    /* Axis line */
    XFillRectangle(scalePtr->display, drawable, scalePtr->axisLineGC, 
                   scalePtr->x1, scalePtr->y1,
                   scalePtr->x2 - scalePtr->x1 + 1, 
                   scalePtr->y2 - scalePtr->y1 + 1);
    /* Draw inner interval. */
    if (HORIZONTAL(scalePtr)) {
        int x1, x2, y;

        x1 = HMap(scalePtr, scalePtr->innerLeft);
        x2 = HMap(scalePtr, scalePtr->innerRight);
        y = (scalePtr->y2 + scalePtr->y1) / 2;
        XDrawLine(scalePtr->display, drawable, scalePtr->rangeGC, 
                  x1, y, x2, y);
    } else {
        int y1, y2, x;

        y1 = VMap(scalePtr, scalePtr->innerLeft);
        y2 = VMap(scalePtr, scalePtr->innerRight);
        x = (scalePtr->x2 + scalePtr->x1) / 2;
        XDrawLine(scalePtr->display, drawable, scalePtr->rangeGC, 
                  x, y1, x, y2);
    }
    /* Major and minor ticks. */
    if ((scalePtr->flags & SHOW_TICKS) && (scalePtr->numSegments > 0) && 
        (scalePtr->tickLineWidth > 0)) {       
        XDrawSegments(scalePtr->display, drawable, gc, scalePtr->tickSegments, 
                scalePtr->numSegments);
    }
    /* Tick labels. */
    if (scalePtr->flags & SHOW_TICKLABELS) {
        Blt_ChainLink link;
        TextStyle ts;

        Blt_Ts_InitStyle(ts);
        Blt_Ts_SetAngle(ts, scalePtr->tickAngle);
        Blt_Ts_SetFont(ts, scalePtr->tickFont);
        Blt_Ts_SetPadding(ts, 2, 0, 0, 0);
        Blt_Ts_SetAnchor(ts, scalePtr->tickAnchor);
        if (scalePtr->flags & DISABLED) {
            Blt_Ts_SetForeground(ts, scalePtr->disabledFgColor);
        } else {
           Blt_Ts_SetForeground(ts, scalePtr->tickLabelColor);
        }
        for (link = Blt_Chain_FirstLink(scalePtr->tickLabels); link != NULL;
            link = Blt_Chain_NextLink(link)) {  
            TickLabel *labelPtr;

            labelPtr = Blt_Chain_GetValue(link);
            /* Draw major tick labels */
            Blt_DrawText(scalePtr->tkwin, drawable, labelPtr->string, &ts, 
                labelPtr->x, labelPtr->y);
        }
    }
    /* Max arrow. */
    if (scalePtr->flags & SHOW_MAXARROW) {
        Blt_Picture picture;

        if (HORIZONTAL(scalePtr)) {
            int x;

            x = HMap(scalePtr, scalePtr->innerRight);
            picture = GetMaxArrowPicture(scalePtr, scalePtr->arrowWidth, 
                                  scalePtr->arrowHeight, ARROW_DOWN);
            Blt_PaintPicture(scalePtr->painter, drawable, picture, 0, 0, 
                scalePtr->arrowWidth, scalePtr->arrowHeight, 
                x - scalePtr->arrowWidth / 2,
                scalePtr->y1 - scalePtr->arrowHeight,
                0);
        } else {
            int y;

            y = VMap(scalePtr, scalePtr->innerRight);
            picture = GetMaxArrowPicture(scalePtr, scalePtr->arrowHeight, 
                                  scalePtr->arrowWidth, ARROW_RIGHT);
            Blt_PaintPicture(scalePtr->painter, drawable, picture, 0, 0, 
                scalePtr->arrowHeight, scalePtr->arrowWidth, 
                scalePtr->x1 - scalePtr->arrowHeight, 
                y - scalePtr->arrowWidth / 2, 0);
        }
   }
    /* Min arrow. */
    if (scalePtr->flags & SHOW_MINARROW) {
        Blt_Picture picture;

        if (HORIZONTAL(scalePtr)) {
            int x;

            x = HMap(scalePtr, scalePtr->innerLeft);
            picture = GetMinArrowPicture(scalePtr, scalePtr->arrowWidth, 
                                  scalePtr->arrowHeight, ARROW_UP);
            Blt_PaintPicture(scalePtr->painter, drawable, picture, 0, 0, 
                scalePtr->arrowWidth, scalePtr->arrowHeight, 
                x - scalePtr->arrowWidth / 2,
                             scalePtr->y2,
                0);
        } else {
            int y;

            y = VMap(scalePtr, scalePtr->innerLeft);
            picture = GetMinArrowPicture(scalePtr, scalePtr->arrowHeight, 
                                  scalePtr->arrowWidth, ARROW_LEFT);
            Blt_PaintPicture(scalePtr->painter, drawable, picture, 0, 0, 
                scalePtr->arrowHeight, scalePtr->arrowWidth, 
                             scalePtr->x2, 
                             y - scalePtr->arrowWidth /2,
                0);
        }
    }
    if (scalePtr->flags & (SHOW_MARK|SHOW_GRIP)) {
        Blt_Bg bg;

        if (scalePtr->flags & DISABLED) {
            bg = scalePtr->disabledBg;
        } else if (scalePtr->flags & ACTIVE_GRIP) {
            bg = scalePtr->activeGripBg;
        } else {
            bg = scalePtr->normalGripBg;
        }
        if (HORIZONTAL(scalePtr)) {
            int x;

            x = HMap(scalePtr, scalePtr->mark);
            /* Current value line. */
            if (scalePtr->flags & SHOW_MARK) {
                int w, h;

                w = scalePtr->markWidth;
                h = Tk_Height(scalePtr->tkwin) - 2 * (scalePtr->inset + PADY);
                if ((w > 0) && (h > 0)) {
                    Blt_Picture picture;
                    Blt_PaintBrush brush;

                    brush = Blt_NewColorBrush(scalePtr->markColor.u32);
                    picture = Blt_CreatePicture(w, h);
                    Blt_SetBrushArea(brush, 0, 0, w, h);
                    Blt_PaintRectangle(picture, 0, 0, w, h, 0, 0, brush, 0);
                    Blt_PaintPicture(scalePtr->painter, drawable, picture, 0, 0,
                         w, h, x - w / 2, scalePtr->inset + PADY, TRUE);
                    Blt_FreePicture(picture);
                    Blt_FreeBrush(brush);
                }
            }
            if ((scalePtr->flags & (ACTIVE|SHOW_GRIP)) == (ACTIVE|SHOW_GRIP)) {
                Blt_Bg_FillRectangle(scalePtr->tkwin, drawable, bg, 
                        x - scalePtr->gripWidth / 2,
                (scalePtr->y2 + scalePtr->y1) / 2 - scalePtr->gripHeight / 2,
                        scalePtr->gripWidth, scalePtr->gripHeight,
                        scalePtr->gripBorderWidth, scalePtr->gripRelief);
            } 
        } else {
            int y;

            y = VMap(scalePtr, scalePtr->mark);
            /* Current value line. */
            if (scalePtr->flags & SHOW_MARK) {
                Blt_Picture picture;
                Blt_PaintBrush brush;
                int w, h;

                brush = Blt_NewColorBrush(scalePtr->markColor.u32);
                h = scalePtr->markWidth;
                w = Tk_Width(scalePtr->tkwin) - 2 * scalePtr->inset;
                picture = Blt_CreatePicture(w, h);
                Blt_SetBrushArea(brush, 0, 0, w, h);
                Blt_PaintRectangle(picture, 0, 0, w, h, 0, 0, brush, 0);
                Blt_PaintPicture(scalePtr->painter, drawable, picture, 0, 0, 
                                 w, h, scalePtr->inset, y - h / 2, TRUE);
                Blt_FreePicture(picture);
                Blt_FreeBrush(brush);
            }
            if ((scalePtr->flags & (ACTIVE|SHOW_GRIP)) == (ACTIVE|SHOW_GRIP)) {
                Blt_Bg_FillRectangle(scalePtr->tkwin, drawable, bg, 
         (scalePtr->x2 + scalePtr->x1) / 2 - scalePtr->gripHeight / 2,
                        y - scalePtr->gripWidth / 2,
                        scalePtr->gripHeight, scalePtr->gripWidth,
                        scalePtr->gripBorderWidth, scalePtr->gripRelief);
            } 
        }
    }
    if ((scalePtr->flags & (ACTIVE|SHOW_VALUE)) == (ACTIVE|SHOW_VALUE)) {
        TextStyle ts;

        Blt_Ts_InitStyle(ts);
        Blt_Ts_SetAngle(ts, scalePtr->valueAngle);
        Blt_Ts_SetFont(ts, scalePtr->valueFont);
        Blt_Ts_SetPadding(ts, 2, 2, 2, 2);
        if (scalePtr->flags & ACTIVE_VALUE) {
            Blt_Bg_FillRectangle(scalePtr->tkwin, drawable, 
                scalePtr->activeValueBg, scalePtr->valuePtr->x, 
                scalePtr->valuePtr->y, 
                scalePtr->valuePtr->width, scalePtr->valuePtr->height, 
                0, TK_RELIEF_FLAT);
            Blt_Ts_SetForeground(ts, scalePtr->activeValueFgColor);
        } else {
            Blt_Ts_SetForeground(ts, scalePtr->normalValueFgColor);
        }
        Blt_Ts_SetMaxLength(ts, scalePtr->width - 2 * scalePtr->inset);
        if (HORIZONTAL(scalePtr)) {
            Blt_Ts_DrawText(scalePtr->tkwin, drawable,
                            scalePtr->valuePtr->string, -1, &ts,
                            scalePtr->valuePtr->x, scalePtr->valuePtr->y);
        } else {
            Blt_Ts_DrawText(scalePtr->tkwin, drawable,
                            scalePtr->valuePtr->string, -1, &ts,
                            scalePtr->valuePtr->x, scalePtr->valuePtr->y);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * DisplayProc --
 *
 *      This procedure is invoked to display a scale widget.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Commands are output to X to display the scale in its current mode.
 *
 *---------------------------------------------------------------------------
 */
static void
DisplayProc(ClientData clientData)
{
    Scale *scalePtr = clientData;
    Pixmap pixmap;
    int w, h;
    
    scalePtr->flags &= ~REDRAW_PENDING;
    if (scalePtr->tkwin == NULL) {
        return;                         /* Window has been destroyed (we
                                         * should not get here) */
    }
#ifdef notdef
    fprintf(stderr, "Calling DisplayProc(%s)\n", Tk_PathName(scalePtr->tkwin));
#endif
    if (scalePtr->flags & GEOMETRY) {
        ComputeGeometry(scalePtr);
    }
    if ((Tk_Width(scalePtr->tkwin) <= 1) || (Tk_Height(scalePtr->tkwin) <= 1)) {
        /* Don't bother computing the layout until the size of the window
         * is something reasonable. */
        return;
    }
    scalePtr->width = Tk_Width(scalePtr->tkwin);
    scalePtr->height = Tk_Height(scalePtr->tkwin);
    if (scalePtr->flags & LAYOUT_PENDING) {
        if (HORIZONTAL(scalePtr)) {
            MapHorizontalScale(scalePtr);
        } else {
            MapVerticalScale(scalePtr);
        }
    }
    if (!Tk_IsMapped(scalePtr->tkwin)) {
        /* The scale's window isn't displayed, so don't bother drawing
         * anything.  By getting this far, we've at least computed the
         * coordinates of the scale's new layout.  */
        return;
    }
    /* Create a pixmap the size of the window for double buffering. */
    pixmap = Blt_GetPixmap(scalePtr->display, Tk_WindowId(scalePtr->tkwin), 
          scalePtr->width, scalePtr->height, Tk_Depth(scalePtr->tkwin));
#ifdef WIN32
    assert(pixmap != None);
#endif
    DrawScale(scalePtr, pixmap);
    w = scalePtr->width  - 2 * scalePtr->highlightWidth;
    h = scalePtr->height - 2 * scalePtr->highlightWidth;
    /* Draw 3D border just inside of the focus highlight ring. */
    if ((w > 0) && (h > 0) && (scalePtr->borderWidth > 0) &&
        (scalePtr->relief != TK_RELIEF_FLAT)) {
        Blt_Bg_DrawRectangle(scalePtr->tkwin, pixmap, scalePtr->normalBg,
                scalePtr->highlightWidth, scalePtr->highlightWidth, w, h,
                scalePtr->borderWidth, scalePtr->relief);
    }
    /* Draw focus highlight ring. */
    if ((scalePtr->highlightWidth > 0) && (scalePtr->flags & FOCUS)) {
        GC gc;

        gc = Tk_GCForColor(scalePtr->highlightColor, pixmap);
        Tk_DrawFocusHighlight(scalePtr->tkwin, gc, scalePtr->highlightWidth,
            pixmap);
    }
    XCopyArea(scalePtr->display, pixmap, Tk_WindowId(scalePtr->tkwin),
        scalePtr->tickGC, 0, 0, scalePtr->width, scalePtr->height, 0, 0);
    Tk_FreePixmap(scalePtr->display, pixmap);
}

/*
 *---------------------------------------------------------------------------
 *
 * ActivateOp --
 *
 *      Activates the axis, drawing the axis with its -activeforeground,
 *      -activebackgound, -activerelief attributes including the min, max
 *	and current indicators.
 *
 * Results:
 *      A standard TCL result.
 *
 * Side Effects:
 *      Scale will be redrawn to reflect the new axis attributes.
 *
 *	pathName activate min/max/grip
 *	pathName deactivate min/max/grip/value
 *
 *---------------------------------------------------------------------------
 */
static int
ActivateOp(ClientData clientData, Tcl_Interp *interp, int objc,
           Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;
    char c;
    const char *string;
    int activate;
    int flag;
    int length;

    string = Tcl_GetString(objv[1]);
    activate = (string[0] == 'a');

    string = Tcl_GetStringFromObj(objv[2], &length);
    c = string[0];
    if ((c == 'm') && (strncmp(string, "minarrow", length) == 0)) {
        flag = ACTIVE_MINARROW;
    } else if ((c == 'm') && (strncmp(string, "maxarrow", length) == 0)) {
        flag = ACTIVE_MAXARROW;
    } else if ((c == 'g') && (strncmp(string, "grip", length) == 0)) {
        flag = ACTIVE_GRIP;
    } else if ((c == 'v') && (strncmp(string, "value", length) == 0)) {
        flag = ACTIVE_VALUE;
    } else {
        Tcl_AppendResult(interp, "unknown scale part \"", string, 
                "\": should be grip, maxarrow, minarrow, or value.", 
                (char *)NULL);
        return TCL_ERROR;
    }
    if (scalePtr->flags & DISABLED) {
        return TCL_OK;
    }
    if (activate) {
        scalePtr->flags |= flag;
    } else {
        scalePtr->flags &= ~flag;
    }
    scalePtr->flags |=  LAYOUT_PENDING;
    EventuallyRedraw(scalePtr);
    return TCL_OK;
}
/*
 *---------------------------------------------------------------------------
 *
 * BboxOp --
 *
 *      Returns the bounding box of the given part.  The accepted parts are
 *      "colorbar", "grip", "maxarrow", "minarrow", "title" and "value".
 *
 * Results:
 *      A standard TCL result.  The interpreter result will contain 4
 *      number representing the bounding box of the part.
 *
 *      pathName bbox partName ?switches?
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
BboxOp(ClientData clientData, Tcl_Interp *interp, int objc,
       Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;
    Tcl_Obj *listObjPtr;
    char c;
    const char *string;
    int length;
    int x1, y1, x2, y2;
    BBoxSwitches switches;

    string = Tcl_GetStringFromObj(objv[2], &length);
    c = string[0];
    x1 = y1 = x2 = y2 = 0;
    if ((c == 'v') && (strncmp(string, "value", length) == 0)) {
        x1 = scalePtr->valuePtr->x;
        y1 = scalePtr->valuePtr->y;
        x2 = x1 + scalePtr->valuePtr->width;
        y2 = y1 + scalePtr->valuePtr->height;
    } else if ((c == 'g') && (strncmp(string, "grip", length) == 0)) {
        if (HORIZONTAL(scalePtr)) {
            int x, y;

            x = HMap(scalePtr, scalePtr->mark);
            y = (scalePtr->y2 + scalePtr->y1) / 2;
            x1 = x - scalePtr->gripWidth / 2;
            y1 = y - scalePtr->gripHeight / 2;
            x2 = x + scalePtr->gripWidth / 2;
            y2 = y + scalePtr->gripHeight / 2;
            return PICK_GRIP;
        }  else {
            int x, y;

            y = VMap(scalePtr, scalePtr->mark);
            x = (scalePtr->x2 + scalePtr->x1) / 2;
            y1 = y - scalePtr->gripWidth / 2;
            x1 = x - scalePtr->gripHeight / 2;
            y2 = y + scalePtr->gripWidth / 2;
            x2 = x + scalePtr->gripHeight / 2;
        }
    } else if ((c == 'm') && (length > 2) &&
               (strncmp(string, "maxarrow", length) == 0)) {
        if (HORIZONTAL(scalePtr)) {
            int x;
            
            x = HMap(scalePtr, scalePtr->innerRight);
            x1 = x - scalePtr->arrowWidth / 2;
            y1 = scalePtr->y1 - scalePtr->arrowHeight;
            x2 = x + scalePtr->arrowWidth / 2;
            y2 = scalePtr->y1;
        } else {
            int y;
            
            y = VMap(scalePtr, scalePtr->innerRight);
            y1 = y - scalePtr->arrowWidth / 2;
            x1 = scalePtr->x1 - scalePtr->arrowHeight;
            y2 = y + scalePtr->arrowWidth / 2;
            x2 = scalePtr->x1;
        }
    } else if ((c == 'r') && (length > 2) &&
               (strncmp(string, "minarrow", length) == 0)) {
        if (HORIZONTAL(scalePtr)) {
            int x;
            
            x = HMap(scalePtr, scalePtr->innerLeft);
            x1 = x - scalePtr->arrowWidth / 2;
            y1 = scalePtr->y2;
            x2 = x + scalePtr->arrowWidth / 2;
            y2 = scalePtr->y2 + scalePtr->arrowHeight;
        } else {
            int y;
            
            y = VMap(scalePtr, scalePtr->innerLeft);
            y1 = y - scalePtr->arrowWidth / 2;
            x1 = scalePtr->x2;
            y2 = y + scalePtr->arrowWidth / 2;
            x2 = scalePtr->x2 + scalePtr->arrowHeight;
        }
    } else if ((c == 't') && (strncmp(string, "title", length) == 0)) {
        x1 = scalePtr->titleX;
        y1 = scalePtr->titleY;
        x2 = scalePtr->titleX + scalePtr->titleWidth;
        y2 = scalePtr->titleY + scalePtr->titleHeight;
    } else if ((c == 'c') && (strncmp(string, "colorbar", length) == 0)) {
        x1 = scalePtr->colorbar.x;
        y2 = scalePtr->colorbar.y;
        x2 = scalePtr->colorbar.x + scalePtr->colorbar.width;
        y2 = scalePtr->colorbar.y + scalePtr->colorbar.height;
    } else {
        Tcl_AppendResult(interp, "unknown scale part \"", string, 
             "\": should be colorbar, maxarrow, minarrow, title, or value.", 
                         (char *)NULL);
        return TCL_ERROR;
    }
    memset(&switches, 0, sizeof(switches));
    if (Blt_ParseSwitches(interp, bboxSwitches, objc - 3, objv + 3, 
        &switches, BLT_SWITCH_DEFAULTS) < 0) {
        return TCL_ERROR;
    }
    if (switches.flags & BBOX_ROOT) {
        int rootX, rootY;
        
        Tk_GetRootCoords(scalePtr->tkwin, &rootX, &rootY);
        x1 += rootX, y1 += rootY;
        x2 += rootX, y2 += rootY;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewIntObj(x1));
    Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewIntObj(y1));
    Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewIntObj(x2));
    Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewIntObj(y2));
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*---------------------------------------------------------------------------
 *
 * BindOp --
 *
 *    pathName bind what sequence command
 *    pathName bind min|max|grip sequence command
 *
 *---------------------------------------------------------------------------
 */
static int
BindOp(ClientData clientData, Tcl_Interp *interp, int objc,
       Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;
    const char *string;
    char c;
    int length;
    ScalePart id;

    string = Tcl_GetStringFromObj(objv[2], &length);
    c = string[0];
    if ((c == 'a') && (strncmp(string, "axis", length) == 0)) {
        id = PICK_AXIS;
    } else if ((c == 'm') && (strncmp(string, "minarrow", length) == 0)) {
        id = PICK_MINARROW;
    } else if ((c == 'm') && (strncmp(string, "maxarrow", length) == 0)) {
        id = PICK_MAXARROW;
    } else if ((c == 'g') && (strncmp(string, "grip", length) == 0)) {
        id = PICK_GRIP;
    } else if ((c == 't') && (strncmp(string, "title", length) == 0)) {
        id = PICK_MAXARROW;
    } else if ((c == 'c') && (strncmp(string, "colorbar", length) == 0)) {
        id = PICK_COLORBAR;
    } else if ((c == 'm') && (strncmp(string, "mark", length) == 0)) {
        id = PICK_MARK;
    } else if ((c == 'v') && (strncmp(string, "value", length) == 0)) {
        id = PICK_VALUE;
    } else {
        Tcl_AppendResult(interp, "unknown scale part \"", string, 
                         "\": should be axis, colorbar, grip, mark, maxarrow, "
                         "minarrow, or title.", (char *)NULL);
        return TCL_ERROR;
    }
    return Blt_ConfigureBindingsFromObj(interp, scalePtr->bindTable,
             (ClientData)(intptr_t)id, objc - 3, objv + 3);
}
          
/*
 *---------------------------------------------------------------------------
 *
 * CgetOp --
 *
 *      Queries scale attributes (font, line width, label, etc).
 *
 * Results:
 *      Return value is a standard TCL result.  If querying configuration
 *      values, interp->result will contain the results.
 *
 *    pathName cget option
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
CgetOp(ClientData clientData, Tcl_Interp *interp, int objc,
       Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;

    return Blt_ConfigureValueFromObj(interp, scalePtr->tkwin, configSpecs,
        (char *)scalePtr, objv[2], 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 *      Queries or resets scale attributes (font, line width, label, etc).
 *
 * Results:
 *      Return value is a standard TCL result.  If querying configuration
 *      values, interp->result will contain the results.
 *
 * Side Effects:
 *      Scale resources are possibly allocated (GC, font). Scale layout is
 *      deferred until the height and width of the window are known.
 *
 *    pathName configure ?option value ...?
 *
 *---------------------------------------------------------------------------
 */
static int
ConfigureOp(ClientData clientData, Tcl_Interp *interp, int objc,
            Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;
    int flags;

    flags = BLT_CONFIG_OBJV_ONLY;
    if (objc == 2) {
        return Blt_ConfigureInfoFromObj(interp, scalePtr->tkwin, configSpecs,
            (char *)scalePtr, (Tcl_Obj *)NULL, flags);
    } else if (objc == 3) {
        return Blt_ConfigureInfoFromObj(interp, scalePtr->tkwin, configSpecs,
            (char *)scalePtr, objv[2], flags);
    }
    if (ConfigureScale(interp, scalePtr, objc - 2, objv + 2, flags) != TCL_OK) {
        return TCL_ERROR;
    }
    EventuallyRedraw(scalePtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * LimitsOp --
 *
 *      This procedure returns a string representing the scale limits
 *      of the axis.  The format of the string is { left top right bottom}.
 *
 * Results:
 *      Always returns TCL_OK.  The interp->result field is
 *      a list of the axis limits.
 *
 *	pathName limits 
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
LimitsOp(ClientData clientData, Tcl_Interp *interp, int objc,
         Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;
    Tcl_Obj *listObjPtr, *objPtr;
    double min, max;
    
    min = (scalePtr->flags & TIGHT) ? scalePtr->outerLeft :
        scalePtr->tickMin;
    max = (scalePtr->flags & TIGHT) ? scalePtr->outerRight : 
        scalePtr->tickMax;
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    objPtr = Tcl_NewDoubleObj(min);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    objPtr = Tcl_NewDoubleObj(max);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * InvTransformOp --
 *
 *      Maps the given window coordinate into an axis value.
 *
 * Results:
 *      Returns a standard TCL result.  interp->result contains
 *      the axis value. If an error occurred, TCL_ERROR is returned
 *      and interp->result will contain an error message.
 *
 *      pathName invtransform x y 
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InvTransformOp(ClientData clientData, Tcl_Interp *interp, int objc, 
               Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;
    double r;                           /* Resulting axis coordinate */
    int sx, sy;                         /* Integer window coordinates */

    if ((Tcl_GetIntFromObj(interp, objv[2], &sx) != TCL_OK) ||
        (Tcl_GetIntFromObj(interp, objv[3], &sy) != TCL_OK)) {
        return TCL_ERROR;
    }
    if (HORIZONTAL(scalePtr)) {
        r = InvHMap(scalePtr, (double)sx);
    } else {
        r = InvVMap(scalePtr, (double)sy);
    }
    Tcl_SetDoubleObj(Tcl_GetObjResult(interp), r);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TransformOp --
 *
 *      Maps the given scale coordinate into a window/screen coordinate.
 *
 * Results:
 *      Returns a standard TCL result.  interp->result contains the window
 *      coordinate. If an error occurred, TCL_ERROR is returned and
 *      interp->result will contain an error message.
 *
 *	pathName transform x y
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TransformOp(ClientData clientData, Tcl_Interp *interp, int objc,
            Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;
    double x;

    if (Blt_ExprDoubleFromObj(interp, objv[0], &x) != TCL_OK) {
        return TCL_ERROR;
    }
    if (HORIZONTAL(scalePtr)) {
        x = HMap(scalePtr, x);
    } else {
        x = VMap(scalePtr, x);
    }
    Tcl_SetIntObj(Tcl_GetObjResult(interp), (int)x);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * IdentifyOp --
 *
 * Results:
 *      Return value is a standard TCL result.  If querying configuration
 *      values, interp->result will contain the results.
 *
 *    pathName identify x y
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
IdentifyOp(ClientData clientData, Tcl_Interp *interp, int objc,
       Tcl_Obj *const *objv)
{
    IdentifySwitches switches;
    Scale *scalePtr = clientData;
    ScalePart id;
    int sx, sy;

    if ((Tk_GetPixelsFromObj(interp, scalePtr->tkwin, objv[2], &sx) != TCL_OK)||
        (Tk_GetPixelsFromObj(interp, scalePtr->tkwin, objv[3], &sy) != TCL_OK)){
        return TCL_ERROR;
    }
    memset(&switches, 0, sizeof(switches));
    if (Blt_ParseSwitches(interp, identifySwitches, objc - 4, objv + 4, 
        &switches, BLT_SWITCH_DEFAULTS) < 0) {
        return TCL_ERROR;
    }
    if (switches.flags & IDENTIFY_ROOT) {
        int rootX, rootY;
        
        Tk_GetRootCoords(scalePtr->tkwin, &rootX, &rootY);
        sx -= rootX;
        sy -= rootY;
    }        
    if (HORIZONTAL(scalePtr)) {
        id = IdentifyHorizontalPart(scalePtr, sx, sy);
    } else {
        id = IdentifyVerticalPart(scalePtr, sx, sy);
    }
    Tcl_SetStringObj(Tcl_GetObjResult(interp), partNames[id], -1);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetOp --
 *
 *    Returns the name of the picked axis (using the axis bind operation).
 *    Right now, the only name accepted is "current".
 *
 * Results:
 *    A standard TCL result.  The interpreter result will contain the name of
 *    the axis.
 *
 *      pathName get min|max|mark|rmin|rmax|
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
GetOp(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;
    const char *string;
    double value;
    char c;
    int length;

    string = Tcl_GetStringFromObj(objv[2], &length);
    c = string[0];
    if ((c == 'm') && (length > 1) &&
        (strncmp(string, "min", length) == 0)) {
        value = scalePtr->outerLeft;
    } else if ((c == 'm') && (length > 2) &&
               (strncmp(string, "max", length) == 0)) {
        value = scalePtr->outerRight;
    } else if ((c == 'r') && (length > 2) &&
               (strncmp(string, "rmin", length) == 0)) {
        value = scalePtr->innerLeft;
    } else if ((c == 'r') && (length > 2) &&
               (strncmp(string, "rmax", length) == 0)) {
        value = scalePtr->innerRight;
    } else if ((c == 'm') && (length > 2) &&
               (strncmp(string, "mark", length) == 0)) {
        value = scalePtr->mark;
    } else {
        Tcl_AppendResult(interp, "unknown scale part \"", string, 
                         "\": should be max, min, rmin, rmax, or mark.", 
                         (char *)NULL);
        return TCL_ERROR;
    }
    Tcl_SetDoubleObj(Tcl_GetObjResult(interp), value);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * SetOp --
 *
 *    Returns the name of the picked axis (using the axis bind operation).
 *    Right now, the only name accepted is "current".
 *
 * Results:
 *    A standard TCL result.  The interpreter result will contain the name of
 *    the axis.
 *
 *      pathName set value
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SetOp(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;
    double mark;
    const char *string;
    int length;
    char c;

    string = Tcl_GetStringFromObj(objv[2], &length);
    c = string[0];
    if ((c == 'r') && (length > 2) && (strncmp(string, "rmin", length) == 0)) {
        mark = scalePtr->innerLeft;
    } else if ((c == 'r') && (length > 2) &&
               (strncmp(string, "rmax", length) == 0)) {
        mark = scalePtr->innerRight;
    } else if ((c == 'm') && (length > 1) &&
               (strncmp(string, "min", length) == 0)) {
        mark = scalePtr->outerLeft;
    } else if ((c == 'm') && (length > 1) &&
               (strncmp(string, "max", length) == 0)) {
        mark = scalePtr->outerRight;
    } else if (Blt_GetDoubleFromObj(interp, objv[2], &mark) != TCL_OK) {
        return TCL_ERROR;
    }
    if (scalePtr->flags & DISABLED) {
        return TCL_OK;
    }
    if (SetValue(scalePtr, mark) != TCL_OK) {
        return TCL_ERROR;
    }
    EventuallyRedraw(scalePtr);
    return TCL_OK;
}

static Blt_OpSpec scaleOps[] = {
    {"activate",     1, ActivateOp,     3, 3, "part"},
    {"bbox",         2, BboxOp,         3, 0, "part ?switches?"},
    {"bind",         2, BindOp,         2, 5, "sequence command"},
    {"cget",         2, CgetOp,         3, 3, "option"},
    {"configure",    2, ConfigureOp,    2, 0, "?option value?..."},
    {"deactivate",   1, ActivateOp,     3, 3, "part"},
    {"get",          1, GetOp,          3, 3, "what"},
    {"identify",     2, IdentifyOp,     4, 4, "x y"},
    {"invtransform", 2, InvTransformOp, 4, 4, "x y"},
    {"limits",       1, LimitsOp,       2, 2, ""},
    {"set",          1, SetOp,          3, 3, "value"},
    {"transform",    1, TransformOp,    3, 3, "value"},
};

static int numScaleOps = sizeof(scaleOps) / sizeof(Blt_OpSpec);

static int
ScaleInstCmd(
    ClientData clientData,              /* Information about the widget. */
    Tcl_Interp *interp,                 /* Interpreter to report errors. */
    int objc,                           /* # of arguments. */
    Tcl_Obj *const *objv)               /* Vector of argument strings. */
{
    Scale *scalePtr = clientData;
    Tcl_ObjCmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, numScaleOps, scaleOps, BLT_OP_ARG1, 
        objc, objv, 0);
    if (proc == NULL) {
        return TCL_ERROR;
    }
    Tcl_Preserve(scalePtr);
    result = (*proc) (clientData, interp, objc, objv);
    Tcl_Release(scalePtr);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * ScaleInstDeletedCmd --
 *
 *      This procedure can be called if the window was destroyed (tkwin
 *      will be NULL) and the command was deleted automatically.  In this
 *      case, we need to do nothing.
 *
 *      Otherwise this routine was called because the command was deleted.
 *      Then we need to clean-up and destroy the widget.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      The widget is destroyed.
 *
 *---------------------------------------------------------------------------
 */
static void
ScaleInstDeletedCmd(ClientData clientData)
{
    Scale *scalePtr = clientData;

    if (scalePtr->tkwin != NULL) {
        Tk_Window tkwin;

        tkwin = scalePtr->tkwin;
        scalePtr->tkwin = NULL;
        Tk_DestroyWindow(tkwin);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ScaleCmd --
 *
 *      This procedure is invoked to process the TCL command that
 *      corresponds to a widget managed by this module. See the user
 *      documentation for details on what it does.
 *
 * Results:
 *      A standard TCL result.
 *
 * Side Effects:
 *      See the user documentation.
 *
 *
 *      blt::scale pathName ?option value ...?
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
ScaleCmd(
    ClientData clientData,              /* Main window associated with
                                         * interpreter. */
    Tcl_Interp *interp,                 /* Current interpreter. */
    int objc,                           /* Number of arguments. */
    Tcl_Obj *const *objv)               /* Argument strings. */
{
    Scale *scalePtr;
    Tk_Window tkwin;
    unsigned int mask;
    const char *pathName;

    if (objc < 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", 
                Tcl_GetString(objv[0]), " pathName ?option value ...?\"", 
                (char *)NULL);
        return TCL_ERROR;
    }
    pathName = Tcl_GetString(objv[1]);
    tkwin = Tk_CreateWindowFromPath(interp, Tk_MainWindow(interp), pathName,
        (char *)NULL);
    if (tkwin == NULL) {
        return TCL_ERROR;
    }
    /*
     * Try to invoke a procedure to initialize various bindings on tabs.
     * Source the file containing the procedure now if the procedure isn't
     * currently defined.  We deferred this to now so that the user could
     * set the variable "blt_library" within the script.
     */
    if (!Blt_CommandExists(interp, "::blt::Scale::Init")) {
        static char initCmd[] =
            "source [file join $blt_library bltScale.tcl]";
 
        if (Tcl_GlobalEval(interp, initCmd) != TCL_OK) {
            char info[200];

            Blt_FmtString(info, 200, "\n\t(while loading bindings for %s)",
                          Tcl_GetString(objv[0]));
            Tcl_AddErrorInfo(interp, info);
            Tk_DestroyWindow(tkwin);
            return TCL_ERROR;
        }
    }
    scalePtr = NewScale(interp, tkwin);
    if (ConfigureScale(interp, scalePtr, objc - 2, objv + 2, 0) != TCL_OK) {
        Tk_DestroyWindow(scalePtr->tkwin);
        return TCL_ERROR;
    }
    mask = (ExposureMask | StructureNotifyMask | FocusChangeMask);
    Tk_CreateEventHandler(tkwin, mask, ScaleEventProc, scalePtr);
    scalePtr->cmdToken = Tcl_CreateObjCommand(interp, pathName, ScaleInstCmd, 
        scalePtr, ScaleInstDeletedCmd);
        
    if (Tcl_VarEval(interp, "::blt::Scale::Init ", 
                Tk_PathName(scalePtr->tkwin), (char *)NULL) != TCL_OK) {
        Tk_DestroyWindow(scalePtr->tkwin);
        return TCL_ERROR;
    }
    Tcl_SetStringObj(Tcl_GetObjResult(interp), pathName, -1);
    return TCL_OK;
}

/*LINTLIBRARY*/
int
Blt_ScaleCmdInitProc(Tcl_Interp *interp)
{
    static Blt_CmdSpec cmdSpec = {
        "scale", ScaleCmd 
    };
    return Blt_InitCmd(interp, "::blt", &cmdSpec);
}
