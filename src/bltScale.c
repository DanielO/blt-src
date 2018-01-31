/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * bltScale.c --
 *
 *      This module implements coordinate axes for the BLT graph widget.
 *
 * Copyright 2015 George A. Howlett. All rights reserved.  
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
#include "bltPs.h"
#include "bltPaintBrush.h"
#include "bltPalette.h"
#include "bltPicture.h"
#include "bltBg.h"
#include "bltSwitch.h"
#include "bltTags.h"
#include "bltGraph.h"
#include "bltGrAxis.h"
#include "bltGrLegd.h"
#include "bltGrElem.h"

#define IsLeapYear(y) \
        ((((y) % 4) == 0) && ((((y) % 100) != 0) || (((y) % 400) == 0)))

#define SHOW_COLORBAR   (1<<0)
#define SHOW_CURRENT    (1<<1)
#define SHOW_GRIP       (1<<2)
#define SHOW_MAJORTICKS (1<<3)
#define SHOW_MAXARROW   (1<<4)
#define SHOW_MINARROW   (1<<5)
#define SHOW_MINORTICKS (1<<6)
#define SHOW_TICKLABELS (1<<7)
#define SHOW_TICKS      (1<<8)
#define SHOW_TITLE      (1<<9)
#define SHOW_VALUE      (1<<10)
#define SHOW_ALL        (SHOW_COLORBAR|SHOW_CURRENT|SHOW_GRIP|SHOW_MAJORTICKS|\
                         SHOW_MAXARROW|SHOW_MINARROW|SHOW_MINORTICKS|\
                         SHOW_TICKLABELS|SHOW_TICKS|SHOW_TITLE|SHOW_VALUE)

#define NORMAL          (1<<11)         /* Scale is drawn in its normal
                                         * foreground/background colors and
                                         * relief.*/
#define DISABLED        (1<<12)         /* Scale is drawn in its disabled
                                         * foreground/background colors. */
#define ACTIVE          (1<<13)         /* Scale is drawn in its active
                                         * foreground/background colors and
                                         * relief. */
#define TIGHT           (1<<14)         /* Axis line stops at designated
                                         * outer interval limits. Otherwise
                                         * the axis line will extend in
                                         * both directions to the next
                                         * major tick. */
#define VERTICAL        (1<<15)         /* Scale is oriented vertically. */
#define AUTO_MAJOR      (1<<16)         /* Auto-generate major ticks. */
#define AUTO_MINOR      (1<<17)         /* Auto-generate minor ticks. */
#define EXTERIOR        (1<<22)         /* Ticks are exterior colorbar. */
#define CHECK_LIMITS    (1<<23)         /* Validate user-defined axis
                                         * limits. */
#define DECREASING      (1<<25)         /* Axis is decreasing in order. */
#define LABELOFFSET     (1<<28)

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
    Point2d anchorPos;
    unsigned int width, height;
    char string[1];
} TickLabel;

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
 *      will be displayed on the graph.
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
    XRectangle rect;                    /* Location of colorbar. */
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
struct _Scale {
    Tk_Window tkwin;                    /* Window that embodies the widget.
                                         * NULL means that the window has been
                                         * destroyed but the data structures
                                         * haven't yet been cleaned up.*/
    Display *display;                   /* Display containing widget; needed,
                                         * among other things, to release
                                         * resources after tkwin has already
                                         * gone away. */
    Tcl_Interp *interp;                 /* Interpreter associated with
                                         * widget. */
    Tcl_Command cmdToken;               /* Token for widget's command. */
    unsigned int flags;                 /* For bitfield definitions, see
                                         * below */

    int inset;                          /* Total width of all borders,
                                         * including traversal highlight and
                                         * 3-D border.  Indicates how much
                                         * interior stuff must be offset from
                                         * outside edges to leave room for
                                         * borders. */
    int relief;
    int borderWidth;
    int activeRelief;

    int highlightWidth;                 /* Width in pixels of highlight to
                                         * draw around widget when it has
                                         * the focus.  <= 0 means don't
                                         * draw a highlight. */
    XColor *highlightBgColor;           /* Color for drawing traversal
                                         * highlight area when highlight is
                                         * off. */
    XColor *highlightColor;             /* Color for drawing traversal
                                         * highlight. */

    Blt_Bg normalBg;                    /* Nomal background color. */
    Blt_Bg activeBg;                    /* Active background color. */
    Blt_Bg disabledBg;                  /* Disabled background color. */

    Blt_Bg normalGripBg;
    Blt_Bg activeGripBg;
    XColor *activeGripFgColor;
    XColor *normalGripFgColor;

    XColor *minArrowFgColor;
    XColor *maxArrowFgColor;
    XColor *activeMinArrowFgColor;
    XColor *activeMaxArrowFgColor;

    double outerLeft, outerRight;       /* Outer interval.  Ticks may be
                                         * drawn outside of the interval
                                         * (if -loose). */
    double innerLeft, innerRight;       /* Inner interval.  Arrows may be
                                         * drawn at both sites. */
    double reqInnerLeft;                /* User set minimum value for inner
                                         * interval. */
    double reqInnerRight;               /* User set maximum value for inner
                                         * internal. */
    double nom;                         /* Currently selected value of
                                         * scale. */

    int tickLineWidth;                  /* Width of tick lines.  If zero,
                                         * then no ticks are drawn. */
    int axisLineWidth;                  /* Width of axis line. If zero,
                                         * then no axis line is drawn. */
    int tickLength;                     /* Length of major ticks in
                                         * pixels. Minor ticks are 2/3 of
                                         * this value. */
    int arrowWidth, arrowHeight;

    AxisRange tickRange;                /* Smallest and largest major tick
                                         * values for the scale.  The tick
                                         * values lie outside the range of
                                         * data values.  This is used to
                                         * for "loose" mode. */
    const char *detail;
    AxisScale scale;                    /* How to scale the scale: linear,
                                         * logrithmic, or time. */
    
    double windowSize;                  /* Size of a sliding window of
                                         * values used to scale the scale
                                         * automatically as new data values
                                         * are added. The scale will always
                                         * display the latest values in
                                         * this range. */
    double shiftBy;                     /* Shift maximum by this
                                         * interval. */
    Tcl_Obj *fmtCmdObjPtr;              /* Specifies a TCL command, to be
                                         * invoked by the scale whenever it
                                         * has to generate tick labels. */
    Tcl_Obj *scrollCmdObjPtr;
    int scrollUnits;

    double min, max;                    /* The actual scale range. */
    double reqMin, reqMax;              /* Requested scale bounds. Consult
                                         * the scalePtr->flags field for
                                         * SCALE_CONFIG_MIN and
                                         * SCALE_CONFIG_MAX to see if the
                                         * requested bound have been set.
                                         * They override the computed range
                                         * of the scale (determined by
                                         * auto-scaling). */

    double reqScrollMin, reqScrollMax;
    double scrollMin, scrollMax;        /* Defines the scrolling reqion of
                                         * the scale.  Normally the region
                                         * is determined from the data
                                         * limits. If specified, these
                                         * values override the
                                         * data-range. */
    double tickMin, tickMax;            /* Tick range in linear scale. */
    double prevMin, prevMax;
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
    int labelOffset;                    /* If non-zero, indicates that the
                                         * tick label should be offset to
                                         * sit in the middle of the next
                                         * interval. */

    /* The following fields are specific to logical axes */

    Blt_Chain chain;
    Segment2d *tickSegments;            /* Array of line segments
                                         * representing the major and minor
                                         * ticks. The segment coordinates
                                         * are relative to the axis. */
    int numSegments;                    /* # of segments in the above
                                         * array. */
    Blt_Chain tickLabels;               /* Contains major tick label
                                         * strings and their offsets along
                                         * the axis. */
    short int left, right, top, bottom; /* Region occupied by the of axis. */
    short int width, height;            /* Extents of axis */
    short int maxLabelWidth;            /* Maximum width of all ticks
                                         * labels. */
    short int maxLabelHeight;           /* Maximum height of all tick
                                         * labels. */

    float tickAngle;    
    Blt_Font tickFont;
    Tk_Anchor tickAnchor;
    Tk_Anchor reqTickAnchor;
    XColor *tickColor;
    GC tickGC;                          /* Graphics context for axis and
                                         * tick labels */
    GC activeTickGC;
    GC disabledTickGC;

    const char *title;                  /* Title of the scale. */
    Point2d titlePos;                   /* Position of the title */
    int titleWidth, titleHeight;  
    Blt_Font titleFont;
    Tk_Anchor titleAnchor;
    Tk_Justify titleJustify;
    XColor *titleColor;
    
    double screenScale;
    int screenMin, screenRange;
    Blt_Palette palette;
    float weight;

    Colorbar colorbar;
};

static void GetAxisGeometry(Scale *scalePtr);

#define MAXTICKS        10001

#define FCLAMP(x)       ((((x) < 0.0) ? 0.0 : ((x) > 1.0) ? 1.0 : (x)))

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

#define HORIZONTAL(s)   (((s)->flags & VERTICAL) == 0) 


#define DEF_ACTIVEBACKGROUND    STD_ACTIVE_BACKGROUND
#define DEF_ACTIVEFOREGROUND    STD_ACTIVE_FOREGROUND
#define DEF_ACTIVERELIEF        "flat"
#define DEF_ANGLE               "0.0"
#define DEF_BACKGROUND          (char *)NULL
#define DEF_BORDERWIDTH         "0"
#define DEF_CHECKLIMITS         "0"
#define DEF_COLORBAR_THICKNESS  "0"
#define DEF_COMMAND             (char *)NULL
#define DEF_DECREASING          "0"
#define DEF_DIVISIONS           "10"
#define DEF_FOREGROUND          RGB_BLACK
#define DEF_HIDE                "0"
#define DEF_JUSTIFY             "c"
#define DEF_LIMITS_FONT         STD_FONT_NUMBERS
#define DEF_LIMITS_FORMAT       (char *)NULL
#define DEF_LINEWIDTH           "1"
#define DEF_LOGSCALE            "0"
#define DEF_LOOSE               "0"
#define DEF_PALETTE             (char *)NULL
#define DEF_RANGE               "0.0"
#define DEF_RELIEF              "flat"
#define DEF_SCALE               "linear"
#define DEF_SCROLL_INCREMENT    "10"
#define DEF_SHIFTBY             "0.0"
#define DEF_SHOWTICKLABELS           "1"
#define DEF_STEP                "0.0"
#define DEF_SUBDIVISIONS        "2"
#define DEF_TAGS                "all"
#define DEF_TICKFONT_BARCHART   STD_FONT_SMALL
#define DEF_TICKFONT_GRAPH      STD_FONT_NUMBERS
#define DEF_TICKLENGTH          "4"
#define DEF_TICK_ANCHOR         "c"
#define DEF_TICK_DIRECTION      "out"
#define DEF_TIMESCALE           "0"
#define DEF_TITLE_ALTERNATE     "0"
#define DEF_TITLE_FG            RGB_BLACK
#define DEF_TITLE_FONT          STD_FONT_NORMAL
#define DEF_WEIGHT              "1.0"

/* Indicates how to rotate axis title for each margin. */
static float titleAngle[4] = {
    0.0, 90.0, 0.0, 270.0
};

typedef struct {
    int axisLength;                     /* Length of the axis. */
    int t1;                             /* Length of a major tick (in
                                         * pixels). */
    int t2;                             /* Length of a minor tick (in
                                         * pixels). */
    int label;                          /* Distance from axis to tick
                                         * label. */
    int colorbar;
} AxisInfo;

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
static Blt_OptionFreeProc  FreeAxis;
static Blt_OptionPrintProc AxisToObj;
static Blt_OptionParseProc ObjToAxis;
Blt_CustomOption bltXAxisOption = {
    ObjToAxis, AxisToObj, FreeAxis, (ClientData)CID_AXIS_X
};
Blt_CustomOption bltYAxisOption = {
    ObjToAxis, AxisToObj, FreeAxis, (ClientData)CID_AXIS_Y
};
Blt_CustomOption bltZAxisOption = {
    ObjToAxis, AxisToObj, FreeAxis, (ClientData)CID_AXIS_Z
};

Blt_CustomOption bltAxisOption = {
    ObjToAxis, AxisToObj, FreeAxis, (ClientData)CID_NONE
};

static Blt_SwitchParseProc XAxisSwitch;
static Blt_SwitchParseProc YAxisSwitch;
static Blt_SwitchFreeProc FreeAxisSwitch;
Blt_SwitchCustom bltXAxisSwitch =
{
    XAxisSwitch, NULL, FreeAxisSwitch, (ClientData)0
};
Blt_SwitchCustom bltYAxisSwitch =
{
    YAxisSwitch, NULL, FreeAxisSwitch, (ClientData)0
};

static Blt_OptionParseProc ObjToScale;
static Blt_OptionPrintProc ScaleToObj;
static Blt_CustomOption scaleOption = {
    ObjToScale, ScaleToObj, NULL, (ClientData)0,
};

static Blt_OptionParseProc ObjToTimeScale;
static Blt_OptionPrintProc TimeScaleToObj;
static Blt_CustomOption timeScaleOption = {
    ObjToTimeScale, TimeScaleToObj, NULL, (ClientData)0,
};

static Blt_OptionParseProc ObjToLogScale;
static Blt_OptionPrintProc LogScaleToObj;
static Blt_CustomOption logScaleOption = {
    ObjToLogScale, LogScaleToObj, NULL, (ClientData)0,
};

static Blt_OptionFreeProc  FreeFormat;
static Blt_OptionParseProc ObjToFormat;
static Blt_OptionPrintProc FormatToObj;
static Blt_CustomOption formatOption = {
    ObjToFormat, FormatToObj, FreeFormat, (ClientData)0,
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

static Blt_ConfigSpec configSpecs[] =
{
    {BLT_CONFIG_BACKGROUND, "-activebackground", "activeBackground", 
        "ActiveBackground", DEF_ACTIVEBACKGROUND, Blt_Offset(Scale, activeBg),
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-activeforeground", "activeForeground",
        "ActiveForeground", DEF_ACTIVEFOREGROUND,
        Blt_Offset(Scale, activeFgColor), 0}, 
    {BLT_CONFIG_BACKGROUND, "-activegripbackground", "activeGripBackground",
        "ActiveGripBackground", DEF_ACTIVEGRIPBG, 
        Blt_Offset(Scale, activeGripBg), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-activegripforeground", "activeGripForeground",
        "ActiveGripForeground", DEF_ACTIVEGRIPFOREGROUND,
        Blt_Offset(Scale, activeGripFgColor), 0}, 
    {BLT_CONFIG_COLOR, "-activemaxarrowbackground", "activeMaxArrowBackground", 
        "ActiveMaxArrowBackground", DEF_ACTIVEMAXARROWBG,  
        Blt_Offset(Scale, activeMaxArrowColor), 0}, 
    {BLT_CONFIG_COLOR, "-activeminarrowbackground", "activeMinArrowBackground", 
        "ActiveMinArrowBackground", DEF_ACTIVEMINARROWCOLOR,  
        Blt_Offset(Scale, activeMinArrowColor), 0}, 
    {BLT_CONFIG_RELIEF, "-activerelief", "activeRelief", "Relief",
        DEF_ACTIVERELIEF, Blt_Offset(Scale, activeRelief),
        BLT_CONFIG_DONT_SET_DEFAULT}, 
    {BLT_CONFIG_DOUBLE, "-autorange", "autoRange", "AutoRange",
        DEF_RANGE, Blt_Offset(Scale, windowSize),
        BLT_CONFIG_DONT_SET_DEFAULT}, 
    {BLT_CONFIG_PIXELS_NNEG, "-axislinewidth", "axisLineWidth", "AxisLineWidth",
        DEF_LINEWIDTH, Blt_Offset(Scale, axisLineWidth),
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BACKGROUND, "-background", "background", "Background",
        DEF_BACKGROUND, Blt_Offset(Scale, normalBg),
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_SYNONYM, "-bg", "background"},
    {BLT_CONFIG_SYNONYM, "-bindtags", "tags"},
    {BLT_CONFIG_SYNONYM, "-bd", "borderWidth"},
    {BLT_CONFIG_PIXELS_NNEG, "-borderwidth", "borderWidth", "BorderWidth",
        DEF_BORDERWIDTH, Blt_Offset(Scale, borderWidth),
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_COLOR, "-color", "color", "Color", DEF_FOREGROUND,
        Blt_Offset(Scale, tickColor), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-colorbarthickness", "colorBarThickness", 
        "ColorBarThickness", DEF_COLORBAR_THICKNESS, 
        Blt_Offset(Scale, colorbar.thickness), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_OBJ, "-command", "command", "Command", DEF_COMMAND, 
        Blt_Offset(Scale, fmtCmdObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_BITMASK, "-decreasing", "decreasing", "Decreasing",
        DEF_DECREASING, Blt_Offset(Scale, flags),
        BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)DECREASING},
    {BLT_CONFIG_SYNONYM, "-descending", "decreasing"},
    {BLT_CONFIG_BACKGROUND, "-disabledbackground", "disabledBackground", 
        "DisabledBackground", DEF_DISABLEDBACKGROUND, 
        Blt_Offset(Scale, disabledBg), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
        "DisabledForeground", DEF_ACTIVEFOREGROUND,
        Blt_Offset(Scale, disabledFgColor), 0}, 
    {BLT_CONFIG_INT, "-divisions", "division", "Divisions", DEF_DIVISIONS,
        Blt_Offset(Scale, reqNumMajorTicks), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_SYNONYM, "-fg", "color"},
    {BLT_CONFIG_SYNONYM, "-foreground", "color"},
    {BLT_CONFIG_CUSTOM, "-hide", "hide", "Hide", DEF_HIDE, 
        Blt_Offset(Scale, showFlags), BLT_CONFIG_DONT_SET_DEFAULT, 
        &hideFlagsOption},
    {BLT_CONFIG_BITMASK, "-labeloffset", "labelOffset", "LabelOffset",
        (char *)NULL, Blt_Offset(Scale, flags), 
        BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)LABELOFFSET},
    {BLT_CONFIG_PIXELS_NNEG, "-ticklinewidth", "tickLineWidth", "TickLineWidth",
        DEF_TICKLINEWIDTH, Blt_Offset(Scale, tickLineWidth),
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_BITMASK_INVERT, "-loose", "loose", "Loose", DEF_LOOSE, 0, 
        BLT_CONFIG_DONT_SET_DEFAULT, (Blt_CustomOption *)TIGHT},
    {BLT_CONFIG_CUSTOM, "-majorticks", "majorTicks", "MajorTicks",
        (char *)NULL, Blt_Offset(Scale, major), BLT_CONFIG_NULL_OK,
        &majorTicksOption},
    {BLT_CONFIG_COLOR, "-maxarrowcolor", "maxArrowColor", "MaxArrowColor", 
        DEF_MAXARROWCOLOR,  Blt_Offset(Scale, maxArrowColor), 0}, 
    {BLT_CONFIG_COLOR, "-minarrowcolor", "minArrowColor", "MinArrowColor", 
        DEF_MINARROWCOLOR,Blt_Offset(Scale, minArrowColor), 0}, 
    {BLT_CONFIG_DOUBLE, "-max", "max", "Max", DEF_MAX,
        Blt_Offset(Scale, outerRight), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_DOUBLE, "-min", "min", "Min", DEF_MIN, 
        Blt_Offset(Scale, outerLeft), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-nom", "nom", "Nom", (char *)NULL, 
        Blt_Offset(Scale, nom), 0, &limitOption},
    {BLT_CONFIG_OBJ, "-nomvariable", "nomVariable", "NomVariable", (char *)NULL,
        Blt_Offset(Scale, nomVarObjPtr), BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_CUSTOM, "-rangemax", "reqOuterRight", "RangeMax", (char *)NULL, 
        Blt_Offset(Scale, reqInnerRight), 0, &limitOption},
    {BLT_CONFIG_CUSTOM, "-rangemin", "reqInnerLeft", "RangeMin", (char *)NULL, 
        Blt_Offset(Scale, reqInnerLeft), 0, &limitOption},
    {BLT_CONFIG_CUSTOM, "-minorticks", "minorTicks", "MinorTicks",
        (char *)NULL, Blt_Offset(Scale, minor), 
        BLT_CONFIG_NULL_OK, &minorTicksOption},
    {BLT_CONFIG_CUSTOM, "-orient", "orient", "Orient", DEF_ORIENT, 
        Blt_Offset(Scale, flags), BLT_CONFIG_DONT_SET_DEFAULT,
        &orientOption},
    {BLT_CONFIG_CUSTOM, "-palette", "palette", "Palette", DEF_PALETTE, 
        Blt_Offset(Scale, palette), 0, &paletteOption},
    {BLT_CONFIG_RELIEF, "-relief", "relief", "Relief", DEF_RELIEF,
        Blt_Offset(Scale, relief), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-scale", "scale", "Scale", DEF_SCALE, 0,
        BLT_CONFIG_DONT_SET_DEFAULT, &scaleOption},
    {BLT_CONFIG_OBJ, "-scrollcommand", "scrollCommand", "ScrollCommand",
        (char *)NULL, Blt_Offset(Scale, scrollCmdObjPtr),
        BLT_CONFIG_NULL_OK},
    {BLT_CONFIG_PIXELS_POS, "-scrollincrement", "scrollIncrement", 
        "ScrollIncrement", DEF_SCROLL_INCREMENT, 
        Blt_Offset(Scale, scrollUnits), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-scrollmax", "scrollMax", "ScrollMax", (char *)NULL, 
        Blt_Offset(Scale, reqScrollMax),  0, &limitOption},
    {BLT_CONFIG_CUSTOM, "-scrollmin", "scrollMin", "ScrollMin", (char *)NULL, 
        Blt_Offset(Scale, reqScrollMin), 0, &limitOption},
    {BLT_CONFIG_DOUBLE, "-shiftby", "shiftBy", "ShiftBy", DEF_SHIFTBY,
        Blt_Offset(Scale, shiftBy), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-show", "show", "Show",
        DEF_SHOW, Blt_Offset(Scale, showFlags),
        BLT_CONFIG_DONT_SET_DEFAULT, &showFlagsOption},
    {BLT_CONFIG_CUSTOM, "-state", "state", "State", DEF_STATE, 
        Blt_Offset(Scale, flags), BLT_CONFIG_DONT_SET_DEFAULT, &stateOption},
    {BLT_CONFIG_DOUBLE, "-stepsize", "stepSize", "StepSize",
        DEF_STEPSIZE, Blt_Offset(Scale, reqStep),
        BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_INT, "-subdivisions", "subdivisions", "Subdivisions",
        DEF_SUBDIVISIONS, Blt_Offset(Scale, reqNumMinorTicks), 0},
    {BLT_CONFIG_ANCHOR, "-tickanchor", "tickAnchor", "Anchor",
        DEF_TICK_ANCHOR, Blt_Offset(Scale, reqTickAnchor), 0},
    {BLT_CONFIG_FLOAT, "-tickangle", "tickAngle", "TickAngle", DEF_TICK_ANGLE,
        Blt_Offset(Scale, tickAngle), BLT_CONFIG_DONT_SET_DEFAULT},
    {BLT_CONFIG_CUSTOM, "-tickdirection", "tickDirection", "TickDirection",
        DEF_TICK_DIRECTION, Blt_Offset(Scale, flags),
        BLT_CONFIG_DONT_SET_DEFAULT, &tickDirectionOption},
    {BLT_CONFIG_FONT, "-tickfont", "tickFont", "Font",
        DEF_TICK_FONT, Blt_Offset(Scale, tickFont), 0},
    {BLT_CONFIG_PIXELS_NNEG, "-ticklength", "tickLength", "TickLength",
        DEF_TICK_LENGTH, Blt_Offset(Scale, tickLength), 
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
    {BLT_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/* Forward declarations */
static void DestroyScale(Scale *scalePtr);
static Tcl_FreeProc FreeScaleProc;
static void TimeScaleAxis(Axis *scalePtr);
static Tick FirstMajorTick(Scale *scalePtr);
static Tick NextMajorTick(Scale *scalePtr);
static Tick FirstMinorTick(Scale *scalePtr);
static Tick NextMinorTick(Scale *scalePtr);

static void
FreeScaleProc(DestroyData data)
{
    Blt_Free(data);
}

INLINE static double
Clamp(double x) 
{
    return (x < 0.0) ? 0.0 : (x > 1.0) ? 1.0 : x;
}

static void
SetScaleRange(AxisRange *rangePtr, double min, double max)
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

INLINE static int
ScaleIsHorizontal(Scale *scalePtr)
{
    if (scalePtr->obj.graphPtr->flags & INVERTED) {
        return (scalePtr->obj.classId == CID_SCALE_Y);
    } else {
        return (scalePtr->obj.classId == CID_SCALE_X);
    }
    return FALSE;
}

static void
ReleaseScale(Scale *scalePtr)
{
    if (scalePtr != NULL) {
        scalePtr->refCount--;
        assert(scalePtr->refCount >= 0);
        if (scalePtr->refCount == 0) {
            DestroyScale(scalePtr);
        }
    }
}

static INLINE *TickLabel
GetFirstTickLabel(scalePtr)
{
    link = Blt_Chain_FirstLink(scalePtr->chain);
    if (link == NULL) {
        return NULL;
    }
    return Blt_Chain_GetValue(link);
}

static INLINE *TickLabel
GetLastTickLabel(scalePtr)
{
    link = Blt_Chain_LastLink(scalePtr->chain);
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

/*ARGSUSED*/
static void
FreeFormat(ClientData clientData, Display *display, char *widgRec, int offset)
{
    Scale *scalePtr = (Scale *)(widgRec);

    if (scalePtr->limitsFmtsObjPtr != NULL) {
        Tcl_DecrRefCount(scalePtr->limitsFmtsObjPtr);
        scalePtr->limitsFmtsObjPtr = NULL;
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * ObjToFormat --
 *
 *      Converts the obj to a format list obj.  Checks if there
 *      are 1 or 2 elements in the list.
 *
 * Results:
 *      The return value is a standard TCL result.  The scale flags are
 *      written into the widget record.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToFormat(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
            Tcl_Obj *objPtr, char *widgRec, int offset, int flags)
{
    Tcl_Obj **objPtrPtr = (Tcl_Obj **)(widgRec + offset);
    Tcl_Obj **objv;
    int objc;

    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
        return TCL_ERROR;
    }
    if (objc > 2) {
        Tcl_AppendResult(interp, "too many elements in limits format list \"",
                Tcl_GetString(objPtr), "\"", (char *)NULL);
        return TCL_ERROR;
    }
    if (objc == 0) {
        objPtr = NULL;                  /* An empty string is the same as
                                         * no formats. */
    } else {
        Tcl_IncrRefCount(objPtr);
    }
    if (*objPtrPtr != NULL) {
        Tcl_DecrRefCount(*objPtrPtr);
    }
    *objPtrPtr = objPtr;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * FormatToObj --
 *
 *      Convert the window coordinates into a string.
 *
 * Results:
 *      The string representing the coordinate position is returned.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static Tcl_Obj *
FormatToObj(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
            char *widgRec, int offset, int flags)
{
    Scale *scalePtr = (Scale *)(widgRec);
    Tcl_Obj *objPtr;

    if (scalePtr->limitsFmtsObjPtr == NULL) {
        objPtr = Tcl_NewStringObj("", -1);
    } else {
        Tcl_IncrRefCount(scalePtr->limitsFmtsObjPtr);
        objPtr = scalePtr->limitsFmtsObjPtr;
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
    Scale *scalePtr = (Scale *)widgRec;
    TickGrid *ptr = (TickGrid *)(widgRec + offset);
    Ticks *ticksPtr;
    size_t mask = (size_t)clientData;

    ticksPtr = &ptr->ticks;
    if (ticksPtr->values != NULL) {
        Blt_Free(ticksPtr->values);
    }
    ticksPtr->values = NULL;
    scalePtr->flags |= mask;             
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
    TickGrid *ptr = (TickGrid *)(widgRec + offset);
    Ticks *ticksPtr;
    double *values;
    int objc;
    size_t mask = (size_t)clientData;

    ticksPtr = &ptr->ticks;
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
    Ticks *ticksPtr;
    size_t mask;
    TickGrid *ptr = (TickGrid *)(widgRec + offset);

    ticksPtr = &ptr->ticks;
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

    scalePtr->flags |= MAP_ITEM;
    scalePtr->obj.graphPtr->flags |= CACHE_DIRTY;
    Blt_EventuallyRedrawGraph(scalePtr->obj.graphPtr);
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
    int showFlag = clientData;

    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
        return TCL_ERROR;
    }
    for (i = 0; i < objc; i++) {
        char c;
        const char *string;
        unsigned int flag;
        int length;

        string = Tcl_GetStringFromObj(objPtr, &length);
        c = string[0];

        if ((c == 'a') && (strncmp(string, "all", length) == 0)) {
            flag = SHOW_ALL;
        } else if ((c == 'm') && (length > 3) &&
            (strncmp(string, "minarrow", length) == 0)) {
            flag = SHOW_MINARROW;
        } else if ((c == 'm') && (length > 2) &&
                   (strncmp(string, "maxarrow", length) == 0)) {
            flag = SHOW_MAXARROW;
        } else if ((c == 'm') && (length > 2) && 
                   (strncmp(string, "majorticks", length) == 0)) {
            flag = SHOW_MAJORTICKS;
        } else if ((c == 'm') && (length > 3) && 
                   (strncmp(string, "minorticks", length) == 0)) {
            flag = SHOW_MINORTICKS;
        } else if ((c == 'v') && ((strncmp(string, "value", length) == 0))) {
            flag = SHOW_VALUE;
        } else if ((c == 'g') && ((strncmp(string, "grip", length) == 0))) {
            flag = SHOW_GRIP;
        } else if ((c == 't') && ((strncmp(string, "title", length) == 0))) {
            flag = SHOW_TITLE;
        } else if ((c == 'c') && (strcmp(string, "colorbar", length) == 0)) {
            flag = SHOW_COLORBAR;
        } else if ((c == 'c') && (strcmp(string, "current", length) == 0)) {
            flag = SHOW_CURRENT;
        } else if ((c == 't') && (length > 4) && 
                   (strcmp(string, "ticks", length) == 0)) {
            flag = SHOW_TICKS;
        } else if ((c == 't') && (length > 4) && 
                   (strcmp(string, "ticklabels", length) == 0)) {
            flag = SHOW_TICKLABELS;
        } else {
            Tcl_AppendResult(interp, "bad scale value \"", string, "\": should be"
                             " log, linear, or time", (char *)NULL);
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
ScaleToObj(ClientData clientData, Tcl_Interp *interp, Tk_Window tkwin,
           char *widgRec, int offset, int flags)
{
    int showFlags = *(int *)(widgRec + offset);
    Tcl_Obj *listObjPtr, *objPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    if (showFlags & SHOW_COLORBAR) {
        objPtr = Tcl_NewStringObj("colorbar", 8);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_MINARROW) {
        objPtr = Tcl_NewStringObj("minarrow", 8);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_MAXARROW) {
        objPtr = Tcl_NewStringObj("maxarrow", 8);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_MINORTICKS) {
        objPtr = Tcl_NewStringObj("minorticks", 10);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_MAJORTICKS) {
        objPtr = Tcl_NewStringObj("majorticks", 10);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_GRIP) {
        objPtr = Tcl_NewStringObj("grip", 4);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_TITLE) {
        objPtr = Tcl_NewStringObj("title", 5);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (showFlags & SHOW_CURRENT) {
        objPtr = Tcl_NewStringObj("current", 7);
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
 *      tick label will be displayed on the graph.
 *
 * -------------------------------------------------------------------------- 
 */
static TickLabel *
MakeLabel(Scale *scalePtr, double value)
{
#define TICK_LABEL_SIZE         200
    char buffer[TICK_LABEL_SIZE + 1];
    const char *string;
    TickLabel *labelPtr;
    Tcl_DString ds;

    string = NULL;
    Tcl_DStringInit(&ds);
    if (scalePtr->fmtCmdObjPtr != NULL) {
        Graph *graphPtr;
        Tcl_Interp *interp;
        Tcl_Obj *cmdObjPtr, *objPtr;
        Tk_Window tkwin;
        int result;

        graphPtr = scalePtr->obj.graphPtr;
        interp = graphPtr->interp;
        tkwin = graphPtr->tkwin;
        /*
         * A TCL proc was designated to format tick labels. Append the path
         * name of the widget and the default tick label as arguments when
         * invoking it. Copy and save the new label from interp->result.
         */
        cmdObjPtr = Tcl_DuplicateObj(scalePtr->fmtCmdObjPtr);
        objPtr = Tcl_NewStringObj(Tk_PathName(tkwin), -1);
        Tcl_ListObjAppendElement(interp, cmdObjPtr, objPtr);
        objPtr = Tcl_NewDoubleObj(value);
        Tcl_ResetResult(interp);
        Tcl_IncrRefCount(cmdObjPtr);
        Tcl_ListObjAppendElement(interp, cmdObjPtr, objPtr);
        result = Tcl_EvalObjEx(interp, cmdObjPtr, TCL_EVAL_GLOBAL);
        Tcl_DecrRefCount(cmdObjPtr);
        if (result != TCL_OK) {
            Tcl_BackgroundError(interp);
        } 
        Tcl_DStringGetResult(interp, &ds);
        string = Tcl_DStringValue(&ds);
    } else if (IsLogScale(scalePtr)) {
        Blt_FmtString(buffer, TICK_LABEL_SIZE, "1E%d", ROUND(value));
        string = buffer;
    } else if ((IsTimeScale(scalePtr)) && (scalePtr->major.ticks.fmt != NULL)) {
        Blt_DateTime date;

        Blt_SecondsToDate(value, &date);
        Blt_FormatDate(&date, scalePtr->major.ticks.fmt, &ds);
        string = Tcl_DStringValue(&ds);
    } else {
        if ((IsTimeScale(scalePtr)) &&
            (scalePtr->major.ticks.timeUnits == UNITS_SUBSECONDS)) {
            value = fmod(value, 60.0);
            value = UROUND(value, scalePtr->major.ticks.step);
        }
        Blt_FmtString(buffer, TICK_LABEL_SIZE, "%.*G", NUMDIGITS, value);
        string = buffer;
    }
    labelPtr = Blt_AssertMalloc(sizeof(TickLabel) + strlen(string));
    strcpy(labelPtr->string, string);
    labelPtr->anchorPos.x = labelPtr->anchorPos.y = -1000;
    Tcl_DStringFree(&ds);
    return labelPtr;
}

/*
 *---------------------------------------------------------------------------
 *
 * InvHMap --
 *
 *      Maps the given screen coordinate back to a graph coordinate.  Called
 *      by the graph locater routine.
 *
 * Results:
 *      Returns the graph coordinate value at the given window
 *      y-coordinate.
 *
 *---------------------------------------------------------------------------
 */
static double
InvHMap(Scale *scalePtr, double x)
{
    double value;

    x = (double)(x - scalePtr->screenMin) * scalePtr->screenScale;
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
 *      Maps the given screen y-coordinate back to the graph coordinate
 *      value. Called by the graph locater routine.
 *
 * Results:
 *      Returns the graph coordinate value for the given screen
 *      coordinate.
 *
 *---------------------------------------------------------------------------
 */
static double
InvVMap(Scale *scalePtr, double y)      /* Screen coordinate */
{
    double value;

    y = (double)(y - scalePtr->screenMin) * scalePtr->screenScale;
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
 *      Map the given graph value to its axis, returning a screen
 *      x-coordinate.
 *
 * Results:
 *      Returns a double precision number representing the screen coordinate
 *      of the value on the given axis.
 *
 *---------------------------------------------------------------------------
 */
static INLINE double
ConvertToScreenX(Scale *scalePtr, double x)
{
    /* Map axis coordinate to normalized coordinates [0..1] */
    x = (x - scalePtr->tickRange.min) * scalePtr->tickRange.scale;
    if (scalePtr->flags & DECREASING) {
        x = 1.0 - x;
    }
    return (x * scalePtr->screenRange + scalePtr->screenMin);
}

/*
 *---------------------------------------------------------------------------
 *
 * ConvertToScreenY --
 *
 *      Map the given graph value to its scale, returning a screen
 *      y-coordinate.
 *
 * Results:
 *      Returns a double precision number representing the screen coordinate
 *      of the value on the given scale.
 *
 *---------------------------------------------------------------------------
 */
static INLINE double
ConvertToScreenY(Scale *scalePtr, double y)
{
    /* Map graph coordinate to normalized coordinates [0..1] */
    y = (y - scalePtr->tickRange.min) * scalePtr->tickRange.scale;
    if (scalePtr->flags & DECREASING) {
        y = 1.0 - y;
    }
    return ((1.0 - y) * scalePtr->screenRange + scalePtr->screenMin);
}

/*
 *---------------------------------------------------------------------------
 *
 * HMap --
 *
 *      Map the given graph coordinate value to its axis, returning a
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
 *      Map the given graph coordinate value to its axis, returning a
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

static void
FixAxisRange(Axis *scalePtr)
{
    double min, max;

    /*
     * When auto-scaling, the axis limits are the bounds of the element data.
     * If no data exists, set arbitrary limits (wrt to log/linear scale).
     */
    min = scalePtr->outerLeft;
    max = scalePtr->outerRight;

    /* Check the requested axis limits. Can't allow -min to be greater
     * than -max, or have undefined log scale limits.  */
    if (((DEFINED(scalePtr->reqMin)) && (DEFINED(scalePtr->reqMax))) &&
        (scalePtr->reqMin >= scalePtr->reqMax)) {
        scalePtr->reqMin = scalePtr->reqMax = Blt_NaN();
    }
    if (IsLogScale(scalePtr)) {
        if ((DEFINED(scalePtr->reqMin)) && (scalePtr->reqMin <= 0.0)) {
            scalePtr->reqMin = Blt_NaN();
        }
        if ((DEFINED(scalePtr->reqMax)) && (scalePtr->reqMax <= 0.0)) {
            scalePtr->reqMax = Blt_NaN();
        }
    }

    if (min == DBL_MAX) {
        if (DEFINED(scalePtr->reqMin)) {
            min = scalePtr->reqMin;
        } else {
            min = (IsLogScale(scalePtr)) ? 0.001 : 0.0;
        }
    }
    if (max == -DBL_MAX) {
        if (DEFINED(scalePtr->reqMax)) {
            max = scalePtr->reqMax;
        } else {
            max = 1.0;
        }
    }
    if (min > max) {
        /*
         * There is no range of data (i.e. min is not less than max), so
         * manufacture one.
         */
        if (min == 0.0) {
            max = 1.0;
        } else {
            max = min + (FABS(min) * 0.1);
        }
    } else if (min == max) {
        /*
         * There is no range of data (i.e. min is not less than max), so
         * manufacture one.
         */
        max = min + 0.5;
        min = min - 0.5;
    }

    /*   
     * The axis limits are either the current data range or overridden by the
     * values selected by the user with the -min or -max options.
     */
    scalePtr->min = min;
    scalePtr->max = max;
    if (DEFINED(scalePtr->reqMin)) {
        scalePtr->min = scalePtr->reqMin;
    }
    if (DEFINED(scalePtr->reqMax)) { 
        scalePtr->max = scalePtr->reqMax;
    }
    if (scalePtr->max < scalePtr->min) {
        /*   
         * If the limits still don't make sense, it's because one limit
         * configuration option (-min or -max) was set and the other default
         * (based upon the data) is too small or large.  Remedy this by making
         * up a new min or max from the user-defined limit.
         */
        if (!DEFINED(scalePtr->reqMin)) {
            scalePtr->min = scalePtr->max - (FABS(scalePtr->max) * 0.1);
        }
        if (!DEFINED(scalePtr->reqMax)) {
            scalePtr->max = scalePtr->min + (FABS(scalePtr->max) * 0.1);
        }
    }
    /* 
     * If a window size is defined, handle auto ranging by shifting the axis
     * limits.
     */
    if ((scalePtr->windowSize > 0.0) && 
        (!DEFINED(scalePtr->reqMin)) && (!DEFINED(scalePtr->reqMax))) {
        if (scalePtr->shiftBy < 0.0) {
            scalePtr->shiftBy = 0.0;
        }
        max = scalePtr->min + scalePtr->windowSize;
        if (scalePtr->max >= max) {
            if (scalePtr->shiftBy > 0.0) {
                max = UCEIL(scalePtr->max, scalePtr->shiftBy);
            }
            scalePtr->min = max - scalePtr->windowSize;
        }
        scalePtr->max = max;
    }
    if ((scalePtr->max != scalePtr->prevMax) || 
        (scalePtr->min != scalePtr->prevMin)) {
        /* Indicate if the axis limits have changed */
        scalePtr->flags |= DIRTY;
        /* and save the previous minimum and maximum values */
        scalePtr->prevMin = scalePtr->min;
        scalePtr->prevMax = scalePtr->max;
    }
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
    scalePtr->major.ticks.axisScale = AXIS_LOGARITHMIC;
    scalePtr->major.ticks.step = majorStep;
    scalePtr->major.ticks.initial = floor(tickMin);
    scalePtr->major.ticks.numSteps = numMajor;
    scalePtr->minor.ticks.initial = scalePtr->minor.ticks.step = minorStep;
    scalePtr->major.ticks.range = tickMax - tickMin;
    scalePtr->minor.ticks.numSteps = numMinor;
    scalePtr->minor.ticks.axisScale = AXIS_LOGARITHMIC;
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
        axisMin = tickMin = floor(min / step) * step + 0.0;
        axisMax = tickMax = ceil(max / step) * step + 0.0;
        
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

    scalePtr->major.ticks.step = step;
    scalePtr->major.ticks.initial = tickMin;
    scalePtr->major.ticks.numSteps = numTicks;
    scalePtr->major.ticks.axisScale = AXIS_LINEAR;

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

    scalePtr->minor.ticks.step = step;
    scalePtr->minor.ticks.numSteps = numTicks;
    scalePtr->minor.ticks.axisScale = AXIS_LINEAR;
    /* Never generate minor ticks. */
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_ResetAxes --
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_ResetAxes(Graph *graphPtr)
{
    Blt_ChainLink link;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;

    /* FIXME: This should be called whenever the display list of elements
     *        change. Maybe yet another flag INIT_STACKS to indicate that
     *        the element display list has changed.  Needs to be done
     *        before the axis limits are set.
     */
    Blt_InitBarGroups(graphPtr);
    /*
     * Step 1:  Reset all axes. Initialize the data limits of the axis to
     *          impossible values.
     */
    for (hPtr = Blt_FirstHashEntry(&graphPtr->axes.nameTable, &cursor);
        hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
        Scale *scalePtr;

        scalePtr = Blt_GetHashValue(hPtr);
        scalePtr->min = scalePtr->outerLeft = DBL_MAX;
        scalePtr->max = scalePtr->outerRight = -DBL_MAX;
    }

    /*
     * Step 2:  For each element that's to be displayed, get the smallest
     *          and largest data values mapped to each X and Y-axis.  This
     *          will be the axis limits if the user doesn't override them 
     *          with -min and -max options.
     */
    for (link = Blt_Chain_FirstLink(graphPtr->elements.displayList);
         link != NULL; link = Blt_Chain_NextLink(link)) {
        Element *elemPtr;

        elemPtr = Blt_Chain_GetValue(link);
        if ((graphPtr->flags & MAP_VISIBLE) && (elemPtr->flags & HIDDEN)) {
            continue;
        }
        (*elemPtr->procsPtr->extentsProc) (elemPtr);
    }
    /*
     * Step 3:  Now that we know the range of data values for each axis,
     *          set axis limits and compute a sweep to generate tick values.
     */
    for (hPtr = Blt_FirstHashEntry(&graphPtr->axes.nameTable, &cursor);
         hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
        Scale *scalePtr;
        double min, max;

        scalePtr = Blt_GetHashValue(hPtr);
        FixAxisRange(scalePtr);

        /* Calculate min/max tick (major/minor) layouts */
        min = scalePtr->min;
        max = scalePtr->max;
        if ((DEFINED(scalePtr->scrollMin)) && (min < scalePtr->scrollMin)) {
            min = scalePtr->scrollMin;
        }
        if ((DEFINED(scalePtr->scrollMax)) && (max > scalePtr->scrollMax)) {
            max = scalePtr->scrollMax;
        }
        if (IsLogScale(scalePtr)) {
            LogScaleAxis(scalePtr);
        } else if (IsTimeScale(scalePtr)) {
            TimeScaleAxis(scalePtr);
        } else {
            LinearScaleAxis(scalePtr);
        }
        if ((scalePtr->marginPtr != NULL) && (scalePtr->flags & DIRTY)) {
            graphPtr->flags |= CACHE_DIRTY;
        }
    }

    graphPtr->flags &= ~RESET_AXES;
    
    /*
     * When any axis changes, we need to layout the entire graph.
     */
    graphPtr->flags |= (GET_AXIS_GEOMETRY | LAYOUT_NEEDED | 
                        MAP_ALL | REDRAW_WORLD);
}

/*
 *---------------------------------------------------------------------------
 *
 * ResetTextStyles --
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
ResetTextStyles(Scale *scalePtr)
{
    GC newGC;
    XGCValues gcValues;
    unsigned long gcMask;
    
    gcMask = (GCForeground | GCLineWidth | GCCapStyle);
    gcValues.foreground = scalePtr->tickColor->pixel;
    gcValues.font = Blt_Font_Id(scalePtr->tickFont);
    gcValues.line_width = LineWidth(scalePtr->tickLineWidth);
    gcValues.cap_style = CapProjecting;

    newGC = Tk_GetGC(scalePtr->tkwin, gcMask, &gcValues);
    if (scalePtr->tickGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->tickGC);
    }
    scalePtr->tickGC = newGC;

    /* Assuming settings from above GC */
    gcValues.foreground = scalePtr->activeFgColor->pixel;
    newGC = Tk_GetGC(scalePtr->tkwin, gcMask, &gcValues);
    if (scalePtr->activeTickGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->activeTickGC);
    }
    scalePtr->activeTickGC = newGC;
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
    if (scalePtr->bindTable != NULL) {
        Blt_DeleteBindings(scalePtr->bindTable, scalePtr);
    }

    Blt_Ts_FreeStyle(scalePtr->display, &scalePtr->limitsTextStyle);

    if (scalePtr->tickGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->tickGC);
    }
    if (scalePtr->activeTickGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->activeTickGC);
    }
    if (scalePtr->disabledTickGC != NULL) {
        Tk_FreeGC(scalePtr->display, scalePtr->disabledTickGC);
    }
    FreeTickLabels(scalePtr->tickLabels);
    Blt_Chain_Destroy(scalePtr->tickLabels);
    if (scalePtr->tickSegments != NULL) {
        Blt_Free(scalePtr->tickSegments);
    }
    Blt_Free(scalePtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * AxisOffsets --
 *
 *      Determines the sides of the axis, major and minor ticks, and title
 *      of the axis.
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
static void
AxisOffsets(Axis *scalePtr, AxisInfo *infoPtr)
{
    Graph *graphPtr = scalePtr->obj.graphPtr;
    int pad;                            /* Offset of axis from interior
                                         * region. This includes a possible
                                         * border and the axis line
                                         * width. */
    int axisLine;
    int t1, t2, labelOffset;
    int tickLabel, axisPad;
    int inset, mark;
    int x, y;
    float fangle;

    scalePtr->titleAngle = 0;
    tickLabel = axisLine = t1 = t2 = 0;
    labelOffset = AXIS_PAD_TITLE;
    if (scalePtr->tickLineWidth > 0) {
        if (scalePtr->flags & TICKLABELS) {
            t1 = scalePtr->tickLength;
            t2 = (t1 * 10) / 15;
        }
        labelOffset = t1 + AXIS_PAD_TITLE;
        if (scalePtr->flags & EXTERIOR) {
            labelOffset += scalePtr->tickLineWidth;
        }
    }
    axisPad = 0;
    /* Adjust offset for the interior border width and the line width */
    pad = 1;
    pad = 0;                            /* FIXME: test */
    /*
     * Pre-calculate the x-coordinate positions of the axis, tick labels,
     * and the individual major and minor ticks.
     */
    inset = pad + scalePtr->tickLineWidth / 2;
    switch (scalePtr->side) {
    case SIDE_TOP:
        axisLine = graphPtr->y1 - scalePtr->marginPtr->nextLayerOffset;
        if (scalePtr->colorbar.thickness > 0) {
            axisLine -= scalePtr->colorbar.thickness + COLORBAR_PAD;
        }
        if (scalePtr->flags & EXTERIOR) {
            axisLine -= axisPad + scalePtr->tickLineWidth / 2;
            tickLabel = axisLine - 2;
            if (scalePtr->tickLineWidth > 0) {
                tickLabel -= scalePtr->tickLength;
            }
        } else {
            axisLine -= axisPad + scalePtr->tickLineWidth / 2;
            tickLabel = graphPtr->y1 + 2;
        }
        mark = graphPtr->y1 - pad;
        scalePtr->tickAnchor = TK_ANCHOR_S;
        scalePtr->left = scalePtr->screenMin - inset - 2;
        scalePtr->right = scalePtr->screenMin + scalePtr->screenRange +
            inset - 1;
        if (graphPtr->flags & STACK_AXES) {
            scalePtr->top = mark;
        } else {
            scalePtr->top = mark - scalePtr->height;
        }
        scalePtr->bottom = mark;
        x = (scalePtr->right + scalePtr->left) / 2;
        if (graphPtr->flags & STACK_AXES) {
            y = mark - AXIS_PAD_TITLE;
        } else {
            y = mark - scalePtr->height + AXIS_PAD_TITLE;
        }
        scalePtr->titleAnchor = TK_ANCHOR_N;
        scalePtr->titlePos.x = x;
        scalePtr->titlePos.y = y;
        infoPtr->colorbar = axisLine - scalePtr->colorbar.thickness;
        break;

    case SIDE_BOTTOM:
        /*
         *  ----------- bottom + plot borderwidth
         *      mark --------------------------------------------
         *          ===================== axisLine (linewidth)
         *                   tick
         *                  title
         *
         *          ===================== axisLine (linewidth)
         *  ----------- bottom + plot borderwidth
         *      mark --------------------------------------------
         *                   tick
         *                  title
         */
        axisLine = graphPtr->y2 + scalePtr->marginPtr->nextLayerOffset;
        if (scalePtr->colorbar.thickness > 0) {
            axisLine += scalePtr->colorbar.thickness + COLORBAR_PAD;
        }
        if (scalePtr->flags & EXTERIOR) {
            axisLine += graphPtr->plotBorderWidth + axisPad +
                scalePtr->tickLineWidth / 2;
            if (graphPtr->plotRelief == TK_RELIEF_SOLID) {
                axisLine--;
            } else {
                axisLine += 2;
            }
            tickLabel = axisLine + 2;
            if (scalePtr->tickLineWidth > 0) {
                tickLabel += scalePtr->tickLength;
            }
        } else {
            axisLine -= axisPad + scalePtr->tickLineWidth / 2;
            tickLabel = graphPtr->y2 +  graphPtr->plotBorderWidth + 2;
            if (graphPtr->plotRelief != TK_RELIEF_SOLID) {
                axisLine--;
            }
        }
        fangle = FMOD(scalePtr->tickAngle, 90.0f);
        if (fangle == 0.0) {
            scalePtr->tickAnchor = TK_ANCHOR_N;
        } else {
            int quadrant;

            quadrant = (int)(scalePtr->tickAngle / 90.0);
            if ((quadrant == 0) || (quadrant == 2)) {
                scalePtr->tickAnchor = TK_ANCHOR_NE;
            } else {
                scalePtr->tickAnchor = TK_ANCHOR_NW;
            }
        }
        scalePtr->left = scalePtr->screenMin - inset - 2;
        scalePtr->right = scalePtr->screenMin + scalePtr->screenRange + inset - 1;
        scalePtr->top = graphPtr->y2 + labelOffset - t1;
        mark = graphPtr->y2 + graphPtr->plotBorderWidth +
            scalePtr->marginPtr->nextLayerOffset;
        if (graphPtr->flags & STACK_AXES) {
            scalePtr->bottom = mark + scalePtr->marginPtr->axesOffset - 1;
        } else {
            scalePtr->bottom = mark + scalePtr->height - 1;
        }
        if (scalePtr->titleAlternate) {
            x = graphPtr->x2 + AXIS_PAD_TITLE;
            y = mark + (scalePtr->height / 2);
            scalePtr->titleAnchor = TK_ANCHOR_W; 
        } else {
            x = (scalePtr->right + scalePtr->left) / 2;
            if (graphPtr->flags & STACK_AXES) {
                y = mark + scalePtr->marginPtr->axesOffset - AXIS_PAD_TITLE;
            } else {
                y = mark + scalePtr->height - AXIS_PAD_TITLE;
            }
            scalePtr->titleAnchor = TK_ANCHOR_S; 
        }
        scalePtr->titlePos.x = x;
        scalePtr->titlePos.y = y;
        infoPtr->colorbar = axisLine - scalePtr->colorbar.thickness;
        break;

    case MARGIN_LEFT:
        /*
         *                    mark
         *                  |  : 
         *                  |  :      
         *                  |  : 
         *                  |  :
         *                  |  : 
         *     axisLine
         */
        /* 
         * Exterior axis 
         *     + plotarea right
         *     |A|B|C|D|E|F|G|H
         *           |right
         * A = plot pad 
         * B = plot border width
         * C = axis pad
         * D = axis line
         * E = tick length
         * F = tick label 
         * G = graph border width
         * H = highlight thickness
         */
        /* 
         * Interior axis 
         *     + plotarea right
         *     |A|B|C|D|E|F|G|H
         *           |right
         * A = plot pad 
         * B = tick length
         * C = axis line width
         * D = axis pad
         * E = plot border width
         * F = tick label 
         * G = graph border width
         * H = highlight thickness
         */
        axisLine = graphPtr->x1 - scalePtr->marginPtr->nextLayerOffset;
        if (scalePtr->colorbar.thickness > 0) {
            axisLine -= scalePtr->colorbar.thickness + COLORBAR_PAD;
        }
        if (scalePtr->flags & EXTERIOR) {
            axisLine -= graphPtr->plotBorderWidth + axisPad +
                scalePtr->tickLineWidth / 2;
            if (graphPtr->plotRelief == TK_RELIEF_SOLID) {
#ifdef notdef
                axisLine++;
#endif
            } else {
                axisLine -= 3;
            }
            tickLabel = axisLine - 2;
            if (scalePtr->tickLineWidth > 0) {
                tickLabel -= scalePtr->tickLength;
            }
        } else {
            if (graphPtr->plotRelief == TK_RELIEF_SOLID) {
                axisLine--;
            }
            axisLine += axisPad + scalePtr->tickLineWidth / 2;
            tickLabel = graphPtr->x1 - graphPtr->plotBorderWidth - 2;
        }
        scalePtr->tickAnchor = TK_ANCHOR_E;
        mark = graphPtr->x1 - graphPtr->plotBorderWidth -
            scalePtr->marginPtr->nextLayerOffset;
        if (graphPtr->flags & STACK_AXES) {
            scalePtr->left = mark - scalePtr->marginPtr->axesOffset;
        } else {
            scalePtr->left = mark - scalePtr->width;
        }
        scalePtr->right = mark - 3;
        scalePtr->top = scalePtr->screenMin - inset - 2;
        scalePtr->bottom = scalePtr->screenMin + scalePtr->screenRange + inset - 1;
        if (scalePtr->titleAlternate) {
            x = mark - (scalePtr->width / 2);
            y = graphPtr->y1 - AXIS_PAD_TITLE;
            scalePtr->titleAnchor = TK_ANCHOR_SW; 
        } else {
            if (graphPtr->flags & STACK_AXES) {
                x = mark - scalePtr->marginPtr->axesOffset;
            } else {
                x = mark - scalePtr->width + AXIS_PAD_TITLE;
            }
            y = (scalePtr->bottom + scalePtr->top) / 2;
            scalePtr->titleAnchor = TK_ANCHOR_W; 
        } 
        scalePtr->titlePos.x = x;
        scalePtr->titlePos.y = y;
        infoPtr->colorbar = axisLine;
        break;

    case MARGIN_RIGHT:
        axisLine = graphPtr->x2 + scalePtr->marginPtr->nextLayerOffset;
        if (scalePtr->colorbar.thickness > 0) {
            axisLine += scalePtr->colorbar.thickness + COLORBAR_PAD;
        }
        if (scalePtr->flags & EXTERIOR) {
            axisLine += graphPtr->plotBorderWidth + axisPad + scalePtr->tickLineWidth / 2;
            tickLabel = axisLine + 2;
            if (scalePtr->tickLineWidth > 0) {
                tickLabel += scalePtr->tickLength;
            }
            if (graphPtr->plotRelief == TK_RELIEF_SOLID) {
                axisLine--;
            } else {
                axisLine++;
            }
        } else {
            if (graphPtr->plotRelief == TK_RELIEF_SOLID) {

#ifdef notdef
                axisLine--;                 /* Draw axis line within solid
                                             * plot border. */
#endif
            } 
            axisLine -= axisPad + scalePtr->tickLineWidth / 2;
            tickLabel = axisLine + 2;
        }
        mark = graphPtr->x2 + scalePtr->marginPtr->nextLayerOffset + pad;
        scalePtr->tickAnchor = TK_ANCHOR_W;
        scalePtr->left = mark;
        if (graphPtr->flags & STACK_AXES) {
            scalePtr->right = mark + scalePtr->marginPtr->axesOffset - 1;
        } else {
            scalePtr->right = mark + scalePtr->width - 1;
        }
        scalePtr->top = scalePtr->screenMin - inset - 2;
        scalePtr->bottom = scalePtr->screenMin + scalePtr->screenRange + inset-1;
        if (scalePtr->titleAlternate) {
            x = mark + (scalePtr->width / 2);
            y = graphPtr->y1 - AXIS_PAD_TITLE;
            scalePtr->titleAnchor = TK_ANCHOR_SE; 
        } else {
            if (graphPtr->flags & STACK_AXES) {
                x = mark + scalePtr->marginPtr->axesOffset - AXIS_PAD_TITLE;
            } else {
                x = mark + scalePtr->width - AXIS_PAD_TITLE;
            }
            y = (scalePtr->bottom + scalePtr->top) / 2;
            scalePtr->titleAnchor = TK_ANCHOR_E;
        }
        scalePtr->titlePos.x = x;
        scalePtr->titlePos.y = y;
        infoPtr->colorbar = axisLine - scalePtr->colorbar.thickness;
        break;

    case MARGIN_NONE:
        axisLine = 0;
        break;
    }
    if ((scalePtr->side == MARGIN_LEFT) ||
        (scalePtr->side == MARGIN_TOP)) {
        t1 = -t1, t2 = -t2;
        labelOffset = -labelOffset;
    }
    infoPtr->axisLength = axisLine;
    infoPtr->t1 = axisLine + t1;
    infoPtr->t2 = axisLine + t2;
    if (tickLabel > 0) {
        infoPtr->label = tickLabel;
    } else {
        infoPtr->label = axisLine + labelOffset;
    }
    if ((scalePtr->flags & EXTERIOR) == 0) {
        /*infoPtr->label = axisLine + labelOffset - t1; */
        infoPtr->t1 = axisLine - t1;
        infoPtr->t2 = axisLine - t2;
    } 
}

static void
MakeColorbar(Scale *scalePtr, AxisInfo *infoPtr)
{
    double min, max;

    min = scalePtr->tickMin;
    max = scalePtr->tickMax;
    if (HORIZONTAL(scalePtr)) {
        int x1, x2;

        x2 = HMap(scalePtr, min);
        x1 = HMap(scalePtr, max);
        scalePtr->colorbar.rect.x = MIN(x1, x2);
        scalePtr->colorbar.rect.y = infoPtr->colorbar;
        scalePtr->colorbar.rect.width = ABS(x1 - x2) + 1;
        scalePtr->colorbar.rect.height = scalePtr->colorbar.thickness;
    } else {
        int y1, y2;

        y2 = VMap(scalePtr, min);
        y1 = VMap(scalePtr, max);
        scalePtr->colorbar.rect.x = infoPtr->colorbar;
        scalePtr->colorbar.rect.y = MIN(y1,y2);
        scalePtr->colorbar.rect.height = ABS(y1 - y2) + 1;
        scalePtr->colorbar.rect.width = scalePtr->colorbar.thickness;
    }
}

static void
MakeAxisLine(Scale *scalePtr, int line, Segment2d *s)
{
    double min, max;

    min = scalePtr->tickMin;
    max = scalePtr->tickMax;
    if (HORIZONTAL(scalePtr)) {
        segPtr->p.x = HMap(scalePtr, min);
        segPtr->q.x = HMap(scalePtr, max);
        segPtr->p.y = segPtr->q.y = line;
    } else {
        segPtr->q.x = segPtr->p.x = line;
        segPtr->p.y = VMap(scalePtr, min);
        segPtr->q.y = VMap(scalePtr, max);
    }
}


/* 
 * If the axis is in log scale, the value is also in log scale.  we must
 * convert it back to linear, because the HMap and VMap routines
 * will convert it back to log before computing a scene value.
 */
static void
MakeTick(Axis *scalePtr, double value, int tick, int line, Segment2d *s)
{
    if (HORIZONTAL(scalePtr)) {
        segPtr->p.x = segPtr->q.x = ConvertToScreenX(scalePtr, value);
        segPtr->p.y = line;
        segPtr->q.y = tick;
    } else {
        segPtr->p.x = line;
        segPtr->p.y = segPtr->q.y = ConvertToScreenY(scalePtr, value);
        segPtr->q.x = tick;
    }
}

static void
MakeSegments(Axis *scalePtr, AxisInfo *infoPtr)
{
    int arraySize;
    int numMajorTicks, numMinorTicks;
    Segment2d *segments;
    Segment2d *s;

    if (scalePtr->segments != NULL) {
        Blt_Free(scalePtr->segments);
    }
    numMajorTicks = scalePtr->major.ticks.numSteps;
    numMinorTicks = scalePtr->minor.ticks.numSteps;
    arraySize = 1 + numMajorTicks + (numMajorTicks * (numMinorTicks + 1));
    segments = Blt_AssertMalloc(arraySize * sizeof(Segment2d));
    s = segments;
    if (scalePtr->tickLineWidth > 0) {
        /* Axis baseline */
        MakeAxisLine(scalePtr, infoPtr->axisLength, s);
        s++;
    }
    if (scalePtr->flags & TICKLABELS) {
        Blt_ChainLink link;
        double labelPos;
        Tick left, right;

        link = Blt_Chain_FirstLink(scalePtr->tickLabels);
        labelPos = (double)infoPtr->label;
        for (left = FirstMajorTick(scalePtr); left.isValid; left = right) {
            right = NextMajorTick(scalePtr);
            if (right.isValid) {
                Tick minor;

                /* If this isn't the last major tick, add minor ticks. */
                scalePtr->minor.ticks.range = right.value - left.value;
                scalePtr->minor.ticks.initial = left.value;
                for (minor = FirstMinorTick(scalePtr); minor.isValid; 
                     minor = NextMinorTick(scalePtr)) {
                    if (InRange(minor.value, &scalePtr->tickRange)) {
                        /* Add minor tick. */
                        MakeTick(scalePtr, minor.value, infoPtr->t2, 
                                infoPtr->axisLength, s);
                        s++;
                    }
                }        
            }
            if (InRange(left.value, &scalePtr->tickRange)) {
                double mid;

                /* Add major tick. This could be the last major tick. */
                MakeTick(scalePtr, left.value, infoPtr->t1,
                        infoPtr->axisLength, s);
                mid = left.value;
                if ((scalePtr->flags & LABELOFFSET) && (right.isValid)) {
                    mid = (right.value - left.value) * 0.5;
                }
                if (InRange(mid, &scalePtr->tickRange)) {
                    TickLabel *labelPtr;

                    labelPtr = Blt_Chain_GetValue(link);
                    link = Blt_Chain_NextLink(link);
                    
                    /* Set the position of the tick label. */
                    if (HORIZONTAL(scalePtr)) {
                        labelPtr->anchorPos.x = segPtr->p.x;
                        labelPtr->anchorPos.y = labelPos;
                    } else {
                        labelPtr->anchorPos.x = labelPos;
                        labelPtr->anchorPos.y = segPtr->p.y;
                    }
                }
                s++;
            }
        }
    }
    scalePtr->segments = segments;
    scalePtr->numSegments = s - segments;
#ifdef notdef
    fprintf(stderr,
            "axis=%s numSegments=%d, arraySize=%d numMajor=%d numMinor=%d\n",
            scalePtr->obj.name, scalePtr->numSegments, arraySize,
            numMajorTicks, numMinorTicks);
#endif
    assert(scalePtr->numSegments <= arraySize);
}

/*
 *---------------------------------------------------------------------------
 *
 * MapAxis --
 *
 *      Pre-calculates positions of the axis, ticks, and labels (to be used
 *      later when displaying the axis).  Calculates the values for each
 *      major and minor tick and checks to see if they are in range (the
 *      outer ticks may be outside of the range of plotted values).
 *
 *      Line segments for the minor and major ticks are saved into one
 *      XSegment array so that they can be drawn by a single XDrawSegments
 *      call. The positions of the tick labels are also computed and saved.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Line segments and tick labels are saved and used later to draw the
 *      axis.
 *
 *---------------------------------------------------------------------------
 */
static void
MapAxis(Axis *scalePtr)
{
    AxisInfo info;
    
    AxisOffsets(scalePtr, &info);
    MakeSegments(scalePtr, &info);
    if (scalePtr->colorbar.thickness > 0) {
        MakeColorbar(scalePtr, &info);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * AdjustViewport --
 *
 *      Adjusts the offsets of the viewport according to the scroll mode.
 *      This is to accommodate both "listbox" and "canvas" style scrolling.
 *
 *      "canvas"        The viewport scrolls within the range of world
 *                      coordinates.  This way the viewport always displays
 *                      a full page of the world.  If the world is smaller
 *                      than the viewport, then (bizarrely) the world and
 *                      viewport are inverted so that the world moves up
 *                      and down within the viewport.
 *
 *      "listbox"       The viewport can scroll beyond the range of world
 *                      coordinates.  Every entry can be displayed at the
 *                      top of the viewport.  This also means that the
 *                      scrollbar thumb weirdly shrinks as the last entry
 *                      is scrolled upward.
 *
 * Results:
 *      The corrected offset is returned.
 *
 *---------------------------------------------------------------------------
 */
static double
AdjustViewport(double offset, double windowSize)
{
    /*
     * Canvas-style scrolling allows the world to be scrolled within the
     * window.
     */
    if (windowSize > 1.0) {
        if (windowSize < (1.0 - offset)) {
            offset = 1.0 - windowSize;
        }
        if (offset > 0.0) {
            offset = 0.0;
        }
    } else {
        if ((offset + windowSize) > 1.0) {
            offset = 1.0 - windowSize;
        }
        if (offset < 0.0) {
            offset = 0.0;
        }
    }
    return offset;
}

static int
GetAxisScrollInfo(Tcl_Interp *interp, int objc, Tcl_Obj *const *objv,
                  double *offsetPtr, double windowSize, double scrollUnits,
                  double scale)
{
    const char *string;
    char c;
    double offset;
    int length;

    offset = *offsetPtr;
    string = Tcl_GetStringFromObj(objv[0], &length);
    c = string[0];
    scrollUnits *= scale;
    if ((c == 's') && (strncmp(string, "scroll", length) == 0)) {
        int count;
        double fract;

        assert(objc == 3);
        /* Scroll number unit/page */
        if (Tcl_GetIntFromObj(interp, objv[1], &count) != TCL_OK) {
            return TCL_ERROR;
        }
        string = Tcl_GetStringFromObj(objv[2], &length);
        c = string[0];
        if ((c == 'u') && (strncmp(string, "units", length) == 0)) {
            fract = count * scrollUnits;
        } else if ((c == 'p') && (strncmp(string, "pages", length) == 0)) {
            /* A page is 90% of the view-able window. */
            fract = (int)(count * windowSize * 0.9 + 0.5);
        } else if ((c == 'p') && (strncmp(string, "pixels", length) == 0)) {
            fract = count * scale;
        } else {
            Tcl_AppendResult(interp, "unknown \"scroll\" units \"", string,
                "\"", (char *)NULL);
            return TCL_ERROR;
        }
        offset += fract;
    } else if ((c == 'm') && (strncmp(string, "moveto", length) == 0)) {
        double fract;

        assert(objc == 2);
        /* moveto fraction */
        if (Tcl_GetDoubleFromObj(interp, objv[1], &fract) != TCL_OK) {
            return TCL_ERROR;
        }
        offset = fract;
    } else {
        int count;
        double fract;

        /* Treat like "scroll units" */
        if (Tcl_GetIntFromObj(interp, objv[0], &count) != TCL_OK) {
            return TCL_ERROR;
        }
        fract = (double)count * scrollUnits;
        offset += fract;
        /* CHECK THIS: return TCL_OK; */
    }
    *offsetPtr = AdjustViewport(offset, windowSize);
    return TCL_OK;
}

static int
GradientCalcProc(ClientData clientData, int x, int y, double *valuePtr)
{
    Axis *scalePtr = clientData;
    double t;

    if (HORIZONTAL(scalePtr)) {
        t = (double)(x - scalePtr->screenMin) * scalePtr->screenScale;
    } else {
        t = (double)(y - scalePtr->screenMin) * scalePtr->screenScale;
    } else {
        return TCL_ERROR;
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
ColorbarToPicture(Axis *scalePtr, int w, int h)
{
    Graph *graphPtr;
    Blt_Picture picture;

    if (scalePtr->palette == NULL) {
        return NULL;                    /* No palette defined. */
    }
    graphPtr = scalePtr->obj.graphPtr;
    picture = Blt_CreatePicture(w, h);
    if (picture != NULL) {
        Blt_PaintBrush brush;

        Blt_BlankPicture(picture, Blt_Bg_GetColor(graphPtr->normalBg));
        brush = Blt_NewLinearGradientBrush();
        Blt_SetLinearGradientBrushPalette(brush, scalePtr->palette);
        Blt_SetLinearGradientBrushCalcProc(brush, GradientCalcProc, scalePtr);
        Blt_PaintRectangle(picture, 0, 0, w, h, 0, 0, brush, TRUE);
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
DrawColorbar(Axis *scalePtr, Drawable drawable)
{
    Blt_Painter painter;
    Blt_Picture picture;
    Graph *graphPtr;
    XRectangle *rectPtr;

    if (scalePtr->palette == NULL) {
        return;                         /* No palette defined. */
    }
    graphPtr = scalePtr->obj.graphPtr;
    rectPtr = &scalePtr->colorbar.rect;
    picture = ColorbarToPicture(scalePtr, rectPtr->width, rectPtr->height);
    if (picture == NULL) {
        return;                         /* Background is obscured. */
    }
    painter = Blt_GetPainter(graphPtr->tkwin, 1.0);
    Blt_PaintPicture(painter, drawable, picture, 0, 0, rectPtr->width, 
                     rectPtr->height, rectPtr->x, rectPtr->y, 0);
    Blt_FreePicture(picture);
}

/*
 *---------------------------------------------------------------------------
 *
 * DrawAxis --
 *
 *      Draws the axis, ticks, and labels onto the canvas.
 *
 *      Initializes and passes text attribute information through TextStyle
 *      structure.
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
static void
DrawAxis(Scale *scalePtr, Drawable drawable)
{
#ifdef notdef
    fprintf(stderr, "scale=%s scale=%d tmin=%g tmax=%g vmin=%g vmax=%g\n",
            scalePtr->obj.name, scalePtr->scale, scalePtr->tickRange.min,
            scalePtr->tickRange.max, scalePtr->outerLeft,
            scalePtr->outerRight);
#endif
    if (scalePtr->normalBg != NULL) {
        Blt_Bg_FillRectangle(graphPtr->tkwin, drawable, 
                scalePtr->normalBg, 
                scalePtr->left, scalePtr->top, 
                scalePtr->right - scalePtr->left, 
                scalePtr->bottom - scalePtr->top, scalePtr->borderWidth, 
                scalePtr->relief);
    }
    if (scalePtr->colorbar.thickness > 0) {
        DrawColorbar(scalePtr, drawable);
    }
    if (scalePtr->title != NULL) {
        TextStyle ts;

        Blt_Ts_InitStyle(ts);
        Blt_Ts_SetAngle(ts, scalePtr->titleAngle);
        Blt_Ts_SetFont(ts, scalePtr->titleFont);
        Blt_Ts_SetPadding(ts, 1, 2, 0, 0);
        Blt_Ts_SetAnchor(ts, scalePtr->titleAnchor);
        Blt_Ts_SetJustify(ts, scalePtr->titleJustify);
        if (scalePtr->flags & ACTIVE) {
            Blt_Ts_SetForeground(ts, scalePtr->activeFgColor);
        } else {
            Blt_Ts_SetForeground(ts, scalePtr->titleColor);
        }
        Blt_Ts_SetForeground(ts, scalePtr->titleColor);
        if ((scalePtr->titleAngle == 90.0) || (scalePtr->titleAngle == 270.0)) {
            Blt_Ts_SetMaxLength(ts, scalePtr->height);
        } else {
            Blt_Ts_SetMaxLength(ts, scalePtr->width);
        }
        Blt_Ts_DrawText(graphPtr->tkwin, drawable, scalePtr->title, -1, &ts, 
                (int)scalePtr->titlePos.x, (int)scalePtr->titlePos.y);
    }
    if (scalePtr->scrollCmdObjPtr != NULL) {
        double viewWidth, viewMin, viewMax;
        double worldWidth, worldMin, worldMax;
        double fract;
        int isHoriz;

        worldMin = scalePtr->outerLeft;
        worldMax = scalePtr->outerRight;
        if (DEFINED(scalePtr->scrollMin)) {
            worldMin = scalePtr->scrollMin;
        }
        if (DEFINED(scalePtr->scrollMax)) {
            worldMax = scalePtr->scrollMax;
        }
        viewMin = scalePtr->min;
        viewMax = scalePtr->max;
        if (viewMin < worldMin) {
            viewMin = worldMin;
        }
        if (viewMax > worldMax) {
            viewMax = worldMax;
        }
        if (IsLogScale(scalePtr)) {
            worldMin = log10(worldMin);
            worldMax = log10(worldMax);
            viewMin = log10(viewMin);
            viewMax = log10(viewMax);
        }
        worldWidth = worldMax - worldMin;       
        viewWidth = viewMax - viewMin;
        isHoriz = HORIZONTAL(scalePtr);

        if (isHoriz != ((scalePtr->flags & DECREASING) != 0)) {
            fract = (viewMin - worldMin) / worldWidth;
        } else {
            fract = (worldMax - viewMax) / worldWidth;
        }
        fract = AdjustViewport(fract, viewWidth / worldWidth);

        if (isHoriz != ((scalePtr->flags & DECREASING) != 0)) {
            viewMin = (fract * worldWidth);
            scalePtr->min = viewMin + worldMin;
            scalePtr->max = scalePtr->min + viewWidth;
            viewMax = viewMin + viewWidth;
            if (IsLogScale(scalePtr)) {
                if (scalePtr->min > 0.0) {
                    scalePtr->min = EXP10(scalePtr->min);
                    scalePtr->max = EXP10(scalePtr->max);
                } else {
                    scalePtr->min = EXP10(scalePtr->min) + scalePtr->min - 1.0;
                    scalePtr->max = EXP10(scalePtr->max) + scalePtr->min - 1.0;
                }
            }
            Blt_UpdateScrollbar(graphPtr->interp, scalePtr->scrollCmdObjPtr,
                (int)viewMin, (int)viewMax, (int)worldWidth);
        } else {
            viewMax = (fract * worldWidth);
            scalePtr->max = worldMax - viewMax;
            scalePtr->min = scalePtr->max - viewWidth;
            viewMin = viewMax + viewWidth;
            if (IsLogScale(scalePtr)) {
                if (scalePtr->min > 0.0) {
                    scalePtr->min = EXP10(scalePtr->min);
                    scalePtr->max = EXP10(scalePtr->max);
                } else {
                    scalePtr->min = EXP10(scalePtr->min) + scalePtr->min - 1.0;
                    scalePtr->max = EXP10(scalePtr->max) + scalePtr->min - 1.0;
                }
            }
            Blt_UpdateScrollbar(graphPtr->interp, scalePtr->scrollCmdObjPtr,
                (int)viewMax, (int)viewMin, (int)worldWidth);
        }
    }
    if (scalePtr->flags & TICKLABELS) {
        Blt_ChainLink link;
        TextStyle ts;

        Blt_Ts_InitStyle(ts);
        Blt_Ts_SetAngle(ts, scalePtr->tickAngle);
        Blt_Ts_SetFont(ts, scalePtr->tickFont);
        Blt_Ts_SetPadding(ts, 2, 0, 0, 0);
        Blt_Ts_SetAnchor(ts, scalePtr->tickAnchor);
        if (scalePtr->flags & ACTIVE) {
            Blt_Ts_SetForeground(ts, scalePtr->activeFgColor);
        } else {
            Blt_Ts_SetForeground(ts, scalePtr->tickColor);
        }
        for (link = Blt_Chain_FirstLink(scalePtr->tickLabels); link != NULL;
            link = Blt_Chain_NextLink(link)) {  
            TickLabel *labelPtr;

            labelPtr = Blt_Chain_GetValue(link);
            /* Draw major tick labels */
            Blt_DrawText(graphPtr->tkwin, drawable, labelPtr->string, &ts, 
                (int)labelPtr->anchorPos.x, (int)labelPtr->anchorPos.y);
        }
    }
    if ((scalePtr->numSegments > 0) && (scalePtr->tickLineWidth > 0)) {       
        GC gc;

        gc = (scalePtr->flags & ACTIVE) ? scalePtr->activeTickGC :
            scalePtr->tickGC;
        /* Draw the tick marks and scale line. */
        Blt_DrawSegments2d(graphPtr->display, drawable, gc, scalePtr->segments, 
                scalePtr->numSegments);
    }
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
 * b = graph borderwidth
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
    int h;
    int pad, numTicks;
    Tick left, right;
    
    FreeTickLabels(scalePtr->tickLabels);
    h = 0;
    
    scalePtr->maxLabelHeight = scalePtr->maxLabelWidth = 0;
    
    /* Compute the tick labels. */
    numTicks = scalePtr->major.ticks.numSteps;
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
        labelPtr = MakeLabel(scalePtr, left.value);
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
        if (scalePtr->maxLabelWidth < lw) {
            scalePtr->maxLabelWidth = lw;
        }
        if (scalePtr->maxLabelHeight < lh) {
            scalePtr->maxLabelHeight = lh;
        }
    }
    assert(Blt_Chain_GetLength(scalePtr->tickLabels) <= numTicks);
    
    Blt_GetTextExtents(scalePtr->titleFont, 0, scalePtr->title, -1, &tw, &th);
    
    pad = 0;
    if (scalePtr->flags & EXTERIOR) {
        /* Because the axis cap style is "CapProjecting", we need to
         * account for an extra 1.5 linewidth at the end of each line.  */
        pad = ((scalePtr->tickLineWidth * 12) / 8);
    }
    if (HORIZONTAL(scalePtr)) {
        h += scalePtr->maxLabelHeight + pad;
    } else {
        h += scalePtr->maxLabelWidth + pad;
        if (scalePtr->maxLabelWidth > 0) {
            h += 5;                 /* Pad either size of label. */
        }  
    }
    h += 2 * SCALE_PAD_TITLE;
    if ((scalePtr->tickLineWidth > 0) && (scalePtr->flags & EXTERIOR)) {
        /* Distance from axis line to tick label. */
        h += scalePtr->tickLength;
    }

    h += h;
    
    /* Correct for orientation of the axis. */
    if (HORIZONTAL(scalePtr)) {
        scalePtr->height = h;
    } else {
        scalePtr->width = h;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * GetMarginGeometry --
 *
 *      Examines all the axes in the given margin and determines the area
 *      required to display them.
 *
 *      Note: For multiple axes, the titles are displayed in another
 *            margin. So we must keep track of the widest title.
 *      
 * Results:
 *      Returns the width or height of the margin, depending if it runs
 *      horizontally along the graph or vertically.
 *
 * Side Effects:
 *      The area width and height set in the margin.  Note again that this
 *      may be corrected later (mulitple axes) to adjust for the longest
 *      title in another margin.
 *
 *---------------------------------------------------------------------------
 */
static int
GetMarginGeometry(Graph *graphPtr, Margin *marginPtr)
{
    Axis *scalePtr;
    int l, w, h;                        /* Length, width, and height. */
    int isHoriz;
    unsigned int numVisible;
    
    isHoriz = HORIZONTAL(marginPtr);

    /* Count the visible axes. */
    numVisible = 0;
    l = w = h = 0;
    /* Need to track the widest and tallest tick labels in the margin. */
    marginPtr->maxAxisLabelWidth = marginPtr->maxAxisLabelHeight = 0;
    if (graphPtr->flags & STACK_AXES) {
        for (scalePtr = FirstAxis(marginPtr); scalePtr != NULL;
             scalePtr = NextAxis(scalePtr)) {
            if (scalePtr->flags & DELETED) {
                continue;
            }
            if (graphPtr->flags & GET_AXIS_GEOMETRY) {
                Blt_GetAxisGeometry(graphPtr, scalePtr);
            }
            if (scalePtr->flags & HIDDEN) {
                continue;
            }
            numVisible++;
            if (isHoriz) {
                if (h < scalePtr->height) {
                    h = scalePtr->height;
                }
            } else {
                if (w < scalePtr->width) {
                    w = scalePtr->width;
                }
            }
            if (scalePtr->maxLabelWidth > marginPtr->maxAxisLabelWidth) {
                marginPtr->maxAxisLabelWidth = scalePtr->maxLabelWidth;
            }
            if (scalePtr->maxLabelHeight > marginPtr->maxAxisLabelHeight) {
                marginPtr->maxAxisLabelHeight = scalePtr->maxLabelHeight;
            }
        }
    } else {
        for (scalePtr = FirstAxis(marginPtr); scalePtr != NULL;
             scalePtr = NextAxis(scalePtr)) {
            if (scalePtr->flags & DELETED) {
                continue;
            }
            if (graphPtr->flags & GET_AXIS_GEOMETRY) {
                Blt_GetAxisGeometry(graphPtr, scalePtr);
            }
            if (scalePtr->flags & HIDDEN) {
                continue;
            }
            numVisible++;
            if ((scalePtr->titleAlternate) && (l < scalePtr->titleWidth)) {
                l = scalePtr->titleWidth;
            }
            if (isHoriz) {
                h += scalePtr->height;
            } else {
                w += scalePtr->width;
            }
            if (scalePtr->maxLabelWidth > marginPtr->maxAxisLabelWidth) {
                marginPtr->maxAxisLabelWidth = scalePtr->maxLabelWidth;
            }
            if (scalePtr->maxLabelHeight > marginPtr->maxAxisLabelHeight) {
                marginPtr->maxAxisLabelHeight = scalePtr->maxLabelHeight;
            }
        }
    }
    /* Enforce a minimum size for margins. */
    if (w < 3) {
        w = 3;
    }
    if (h < 3) {
        h = 3;
    }
    marginPtr->numVisibleAxes = numVisible;
    marginPtr->axesTitleLength = l;
    marginPtr->width = w;
    marginPtr->height = h;
    marginPtr->axesOffset = (isHoriz) ? h : w;
    return marginPtr->axesOffset;
}

/*
 *---------------------------------------------------------------------------
 *
 * MapScale --
 *
 *      Calculate the layout of the graph.  Based upon the data, axis limits,
 *      X and Y titles, and title height, determine the cavity left which is
 *      the plotting surface.  The first step get the data and axis limits for
 *      calculating the space needed for the top, bottom, left, and right
 *      margins.
 *
 *      1) The LEFT margin is the area from the left border to the Y axis 
 *         (not including ticks). It composes the border width, the width an 
 *         optional Y axis label and its padding, and the tick numeric labels. 
 *         The Y axis label is rotated 90 degrees so that the width is the 
 *         font height.
 *
 *      2) The RIGHT margin is the area from the end of the graph
 *         to the right window border. It composes the border width,
 *         some padding, the font height (this may be dubious. It
 *         appears to provide a more even border), the max of the
 *         legend width and 1/2 max X tick number. This last part is
 *         so that the last tick label is not clipped.
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
    int left, right, top, bottom;
    int plotWidth, plotHeight;
    int inset, inset2;
    int width, height;
    int pad;

    width = graphPtr->width;
    height = graphPtr->height;

    GetAxisGeometry(scalePtr);
    if (HORIZONATAL(scalePtr)) {
        pad = scalePtr->maxAxisLabelWidth;
        pad = pad / 2 + 3;
        if (right < pad) {
            right = pad;
        }
        if (left < pad) {
            left = pad;
        }
    } else {
        pad = scalePtr->maxAxisLabelHeight;
        if (pad < graphPtr->rightPtr->maxAxisLabelHeight) {
            pad = graphPtr->rightPtr->maxAxisLabelHeight;
        }
        pad = pad / 2;
        if (top < pad) {
            top = pad;
        }
        if (bottom < pad) {
            bottom = pad;
        }
    }

    {
        Blt_FontMetrics fm;

        Blt_Font_GetMetrics(scalePtr->tickFont, &fm);
        scalePtr->arrowHeight = scalePtr->arrowWidth = fm.linespace;
    }

    /* 
     * Step 3: Estimate the size of the plot area from the remaining
     *         space.  This may be overridden by the -plotwidth and
     *         -plotheight graph options.  We use this to compute the
     *         size of the legend. 
     */
    if (width == 0) {
        width = 400;
    }
    if (height == 0) {
        height = 400;
    }
    plotWidth  = (graphPtr->reqPlotWidth > 0) ? graphPtr->reqPlotWidth :
        width - (inset2 + left + right); /* Plot width. */
    plotHeight = (graphPtr->reqPlotHeight > 0) ? graphPtr->reqPlotHeight : 
        height - (inset2 + top + bottom); /* Plot height. */

    /* 
     * Step 4:  Add the legend to the appropiate margin. 
     */
    if (!Blt_Legend_IsHidden(graphPtr)) {
        switch (Blt_Legend_Site(graphPtr)) {
        case LEGEND_RIGHT:
            right += Blt_Legend_Width(graphPtr) + 2;
            break;
        case LEGEND_LEFT:
            left += Blt_Legend_Width(graphPtr) + 2;
            break;
        case LEGEND_TOP:
            top += Blt_Legend_Height(graphPtr) + 2;
            break;
        case LEGEND_BOTTOM:
            bottom += Blt_Legend_Height(graphPtr) + 2;
            break;
        case LEGEND_XY:
        case LEGEND_PLOT:
        case LEGEND_WINDOW:
            /* Do nothing. */
            break;
        }
    }
    /* 
     * Recompute the plotarea or graph size, now accounting for the legend. 
     */
    if (graphPtr->reqPlotWidth == 0) {
        plotWidth = width  - (inset2 + left + right);
        if (plotWidth < 1) {
            plotWidth = 1;
        }
    }
    if (graphPtr->reqPlotHeight == 0) {
        plotHeight = height - (inset2 + top + bottom);
        if (plotHeight < 1) {
            plotHeight = 1;
        }
    }
    /*
     * Step 5: If necessary, correct for the requested plot area aspect
     *         ratio.
     */
    if ((graphPtr->reqPlotWidth == 0) && (graphPtr->reqPlotHeight == 0) && 
        (graphPtr->aspect > 0.0f)) {
        float ratio;

        /* 
         * Shrink one dimension of the plotarea to fit the requested
         * width/height aspect ratio.
         */
        ratio = (float)plotWidth / (float)plotHeight;
        if (ratio > graphPtr->aspect) {
            int sw;

            /* Shrink the width. */
            sw = (int)(plotHeight * graphPtr->aspect);
            if (sw < 1) {
                sw = 1;
            }
            /* Add the difference to the right margin. */
            /* CHECK THIS: w = sw; */
            right += (plotWidth - sw);
        } else {
            int sh;

            /* Shrink the height. */
            sh = (int)(plotWidth / graphPtr->aspect);
            if (sh < 1) {
                sh = 1;
            }
            /* Add the difference to the top margin. */
            /* CHECK THIS: h = sh; */
            top += (plotHeight - sh); 
        }
    }

    /* 
     * Step 6: If there are multiple axes in a margin, the axis titles will be
     *         displayed in the adjoining margins.  Make sure there's room 
     *         for the longest axis titles.
     */
    if (top < graphPtr->leftPtr->axesTitleLength) {
        top = graphPtr->leftPtr->axesTitleLength;
    }
    if (right < graphPtr->bottomPtr->axesTitleLength) {
        right = graphPtr->bottomPtr->axesTitleLength;
    }
    if (top < graphPtr->rightPtr->axesTitleLength) {
        top = graphPtr->rightPtr->axesTitleLength;
    }
    if (right < graphPtr->topPtr->axesTitleLength) {
        right = graphPtr->topPtr->axesTitleLength;
    }

    /* 
     * Step 7: Override calculated values with requested margin sizes.
     */
    if (graphPtr->reqLeftMarginSize > 0) {
        left = graphPtr->reqLeftMarginSize;
    }
    if (graphPtr->reqRightMarginSize > 0) {
        right = graphPtr->reqRightMarginSize;
    }
    if (graphPtr->reqTopMarginSize > 0) {
        top = graphPtr->reqTopMarginSize;
    }
    if (graphPtr->reqBottomMarginSize > 0) {
        bottom = graphPtr->reqBottomMarginSize;
    }
    if (graphPtr->reqPlotWidth > 0) {   
        int w;

        /* 
         * Width of plotarea is constained.  If there's extra space, add it
         * to th left and/or right margins.  If there's too little, grow
         * the graph width to accomodate it.
         */
        w = plotWidth + inset2 + left + right;
        if (width > w) {                /* Extra space in window. */
            int extra;

            extra = (width - w) / 2;
            if (graphPtr->reqLeftMarginSize == 0) { 
                left += extra;
                if (graphPtr->reqRightMarginSize == 0) { 
                    right += extra;
                } else {
                    left += extra;
                }
            } else if (graphPtr->reqRightMarginSize == 0) {
                right += extra + extra;
            }
        } else if (width < w) {
            width = w;
        }
    } 
    if (graphPtr->reqPlotHeight > 0) {  /* Constrain the plotarea height. */
        int h;

        /* 
         * Height of plotarea is constained.  If there's extra space, add
         * it to th top and/or bottom margins.  If there's too little, grow
         * the graph height to accomodate it.
         */
        h = plotHeight + inset2 + top + bottom;
        if (height > h) {               /* Extra space in window. */
            int extra;

            extra = (height - h) / 2;
            if (graphPtr->reqTopMarginSize == 0) { 
                top += extra;
                if (graphPtr->reqBottomMarginSize == 0) { 
                    bottom += extra;
                } else {
                    top += extra;
                }
            } else if (graphPtr->reqBottomMarginSize == 0) {
                bottom += extra + extra;
            }
        } else if (height < h) {
            height = h;
        }
    }   
    graphPtr->width  = width;
    graphPtr->height = height;
    graphPtr->x1 = left + inset;
    graphPtr->y1 = top  + inset;
    graphPtr->x2 = width  - right  - inset;
    graphPtr->y2 = height - bottom - inset;
    if (graphPtr->plotRelief == TK_RELIEF_SOLID) {
        graphPtr->x1--;
        graphPtr->y1--;
    }
    graphPtr->leftPtr->width    = left   + graphPtr->inset;
    graphPtr->rightPtr->width   = right  + graphPtr->inset;
    graphPtr->topPtr->height    = top    + graphPtr->inset;
    graphPtr->bottomPtr->height = bottom + graphPtr->inset;
            
    graphPtr->vOffset = graphPtr->y1 + graphPtr->padTop;
    graphPtr->vRange  = plotHeight - PADDING(graphPtr->padY);
    graphPtr->hOffset = graphPtr->x1 + graphPtr->padLeft;
    graphPtr->hRange  = plotWidth  - PADDING(graphPtr->padX);

    if (graphPtr->vRange < 1) {
        graphPtr->vRange = 1;
    }
    if (graphPtr->hRange < 1) {
        graphPtr->hRange = 1;
    }
    graphPtr->hScale = 1.0f / (float)graphPtr->hRange;
    graphPtr->vScale = 1.0f / (float)graphPtr->vRange;

    /*
     * Calculate the placement of the graph title so it is centered within the
     * space provided for it in the top margin
     */
    graphPtr->titleY = 3 + graphPtr->inset;
    graphPtr->titleX = (graphPtr->x2 + graphPtr->x1) / 2;
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
ConfigureScale(Scale *scalePtr)
{
    float angle;

    /* Check that the designate interval represents a valid range. Swap
     * values and if min is greater than max. */
    if (scalePtr->outerLeft >= scalePtr->outerRight) {
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
    /* If no range min and max is set, use the data range values. */
    scalePtr->innerLeft = (DEFINED(scalePtr->reqInnerLeft))
        ? scalePtr->outerLeft : scalePtr->reqInnerLeft;
    scalePtr->innerRight = (DEFINED(scalePtr->reqInnerRight))
        ? scalePtr->outerRight : scalePtr->reqInnerRight;

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
        /* Just reverse the values. */
        tmp = scalePtr->innerLeft;
        scalePtr->innerLeft = scalePtr->innerRight;
        scalePtr->innerRight = tmp;
    }
    if (!DEFINED(scalePtr->nom)) {
        scalePtr->nom = scalePtr->innerLeft;
    }        
    if (scalePtr->nom < scalePtr->innerLeft) {
        scalePtr->nom = scalePtr->innerLeft;
    }
    if (scalePtr->nom > scalePtr->innerRight) {
        scalePtr->nom = scalePtr->innerRight;
    }
    if (IsLogScale(scalePtr)) {
        LogScaleAxis(scalePtr);
    } else if (IsTimeScale(scalePtr)) {
        TimeScaleAxis(scalePtr);
    } else {
        LinearScaleAxis(scalePtr);
    }

    ComputeGeometry(scalePtr)
    angle = FMOD(scalePtr->tickAngle, 360.0f);
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    if (scalePtr->normalBg != NULL) {
        Blt_Bg_SetChangedProc(scalePtr->normalBg, Blt_UpdateGraph, 
                scalePtr);
    }
    scalePtr->tickAngle = angle;
    ResetTextStyles(scalePtr);

    scalePtr->titleWidth = scalePtr->titleHeight = 0;
    if (scalePtr->title != NULL) {
        unsigned int w, h;

        Blt_GetTextExtents(scalePtr->titleFont, 0, scalePtr->title, -1, &w, &h);
        scalePtr->titleWidth = w;
        scalePtr->titleHeight = h;
    }
    EventuallyRedraw(scalePtr);
    return TCL_OK;
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
    Blt_HashEntry *hPtr;
    int isNew;

    scalePtr = Blt_Calloc(1, sizeof(Scale));
    if (scalePtr == NULL) {
        Tcl_AppendResult(interp, "can't allocate memory for scale \"", 
                Tk_PathName(tkwin), "\"", (char *)NULL);
        return NULL;
    }
    Tk_SetClass(tkwin, "BltScale");
    scalePtr->display = Tk_Display(tkwin);
    scalePtr->interp = interp;
    scalePtr->tkwin = tkwin;
    scalePtr->relief = TK_RELIEF_FLAT;
    scalePtr->activeRelief = TK_RELIEF_RAISED;
    scalePtr->flags |= TIGHT;
    scalePtr->reqNumMinorTicks = 2;
    scalePtr->reqNumMajorTicks = 10;
    scalePtr->tickLength = 8;
    scalePtr->colorbar.thickness = 0;
    scalePtr->outerLeft = 0.1;
    scalePtr->outerRight = 1.0;
    scalePtr->flags = (TICKLABELS| AUTO_MAJOR| AUTO_MINOR | EXTERIOR);
    scalePtr->tickLabels = Blt_Chain_Create();
    scalePtr->tickLineWidth = 1;
    Blt_SetWindowInstanceData(tkwin, scalePtr);
    return scalePtr;
}

int
Blt_DefaultAxes(Graph *graphPtr)
{
    int i;
    unsigned int flags;

    for (i = 0; i < 4; i++) {
        Margin *marginPtr;

        marginPtr = graphPtr->margins + i;
        marginPtr->axes = Blt_Chain_Create();
        marginPtr->name = axisNames[i].name;
    }
    flags = Blt_GraphType(graphPtr);
    for (i = 0; i < 4; i++) {
        Axis *scalePtr;
        Margin *marginPtr;

        marginPtr = graphPtr->margins + i;
        /* Create a default axis for each chain. */
        scalePtr = NewAxis(graphPtr, marginPtr->name, i);
        if (scalePtr == NULL) {
            return TCL_ERROR;
        }
        scalePtr->refCount = 1;  /* Default axes are assumed in use. */
        scalePtr->marginPtr = marginPtr;
        Blt_GraphSetObjectClass(&scalePtr->obj, axisNames[i].classId);
        if (Blt_ConfigureComponentFromObj(graphPtr->interp, graphPtr->tkwin,
                scalePtr->obj.name, "Axis", configSpecs, 0, (Tcl_Obj **)NULL,
                (char *)scalePtr, flags) != TCL_OK) {
            return TCL_ERROR;
        }
        if (ConfigureAxis(scalePtr) != TCL_OK) {
            return TCL_ERROR;
        }
        scalePtr->link = Blt_Chain_Append(marginPtr->axes, scalePtr);
    }
    /* The extra axes are not attached to a specific margin. */
    for (i = 4; i < numAxisNames; i++) {
        Axis *scalePtr;

        scalePtr = NewAxis(graphPtr, axisNames[i].name, MARGIN_NONE);
        if (scalePtr == NULL) {
            return TCL_ERROR;
        }
        scalePtr->refCount = 1;          
        scalePtr->marginPtr = NULL;
        Blt_GraphSetObjectClass(&scalePtr->obj, axisNames[i].classId);
        if (Blt_ConfigureComponentFromObj(graphPtr->interp, graphPtr->tkwin,
                scalePtr->obj.name, "Axis", configSpecs, 0, (Tcl_Obj **)NULL,
                (char *)scalePtr, flags) != TCL_OK) {
            return TCL_ERROR;
        }
        if (ConfigureAxis(scalePtr) != TCL_OK) {
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * AxisCreateOp --
 *
 *      Creates a new axis.
 *
 * Results:
 *      Returns a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
AxisCreateOp(ClientData clientData, Tcl_Interp *interp, int objc, 
             Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    Axis *scalePtr;
    int flags;

    scalePtr = NewAxis(graphPtr, Tcl_GetString(objv[3]), MARGIN_NONE);
    if (scalePtr == NULL) {
        return TCL_ERROR;
    }
    flags = Blt_GraphType(graphPtr);
    if ((Blt_ConfigureComponentFromObj(interp, graphPtr->tkwin, 
        scalePtr->obj.name, "Axis", configSpecs, objc - 4, objv + 4, 
        (char *)scalePtr, flags) != TCL_OK) || 
        (ConfigureAxis(scalePtr) != TCL_OK)) {
        DestroyAxis(scalePtr);
        return TCL_ERROR;
    }
    Tcl_SetStringObj(Tcl_GetObjResult(interp), scalePtr->obj.name, -1);
    return TCL_OK;
}
/*
 *---------------------------------------------------------------------------
 *
 * AxisActivateOp --
 *
 *      Activates the axis, drawing the axis with its -activeforeground,
 *      -activebackgound, -activerelief attributes.
 *
 * Results:
 *      A standard TCL result.
 *
 * Side Effects:
 *      Graph will be redrawn to reflect the new axis attributes.
 *
 *---------------------------------------------------------------------------
 */
static int
AxisActivateOp(ClientData clientData, Tcl_Interp *interp, int objc, 
               Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    Axis *scalePtr;

    if (GetAxisFromObj(interp, graphPtr, objv[3], &scalePtr) != TCL_OK) {
        return TCL_ERROR;
    }
    return ActivateOp(scalePtr, interp, objc, objv);
}


/*-------------------------------------------------------------------------------
 *
 * AxisBindOp --
 *
 *    .g axis bind axisName sequence command
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
AxisBindOp(ClientData clientData, Tcl_Interp *interp, int objc, 
           Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    if (objc == 3) {
        Blt_HashEntry *hPtr;
        Blt_HashSearch cursor;
        Tcl_Obj *listObjPtr;

        listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
        for (hPtr = Blt_FirstHashEntry(&graphPtr->axes.bindTagTable, &cursor);
             hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
            const char *tagName;
            Tcl_Obj *objPtr;

            tagName = Blt_GetHashKey(&graphPtr->axes.bindTagTable, hPtr);
            objPtr = Tcl_NewStringObj(tagName, -1);
            Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
        }
        Tcl_SetObjResult(interp, listObjPtr);
        return TCL_OK;
    }
    return Blt_ConfigureBindingsFromObj(interp, graphPtr->bindTable, 
        Blt_MakeAxisTag(graphPtr, Tcl_GetString(objv[3])), objc - 4, objv + 4);
}


/*
 *---------------------------------------------------------------------------
 *
 * AxisCgetOp --
 *
 *      Queries axis attributes (font, line width, label, etc).
 *
 * Results:
 *      Return value is a standard TCL result.  If querying configuration
 *      values, interp->result will contain the results.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
AxisCgetOp(ClientData clientData, Tcl_Interp *interp, int objc,
           Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    Axis *scalePtr;

    if (GetAxisFromObj(interp, graphPtr, objv[3], &scalePtr) != TCL_OK) {
        return TCL_ERROR;
    }
    return CgetOp(scalePtr, interp, objc - 4, objv + 4);
}

/*
 *---------------------------------------------------------------------------
 *
 * AxisConfigureOp --
 *
 *      Queries or resets axis attributes (font, line width, label, etc).
 *
 * Results:
 *      Return value is a standard TCL result.  If querying configuration
 *      values, interp->result will contain the results.
 *
 * Side Effects:
 *      Axis resources are possibly allocated (GC, font). Axis layout is
 *      deferred until the height and width of the window are known.
 *
 *      pathName axis configure axisName ?option value ...?
 *---------------------------------------------------------------------------
 */
static int
AxisConfigureOp(ClientData clientData, Tcl_Interp *interp, int objc, 
                Tcl_Obj *const *objv)
{
    Axis *scalePtr;
    AxisIterator iter;
    Graph *graphPtr = clientData;

    /* Figure out where the option value pairs begin */
    if (objc == 4) {
        if (GetAxisFromObj(interp, graphPtr, objv[3], &scalePtr) != TCL_OK) {
            return TCL_ERROR;           /* Can't find named axis */
        }
        return Blt_ConfigureInfoFromObj(interp, graphPtr->tkwin, configSpecs,
                (char *)scalePtr, (Tcl_Obj *)NULL, BLT_CONFIG_OBJV_ONLY);
    } else if (objc == 5) {
        if (GetAxisFromObj(interp, graphPtr, objv[3], &scalePtr) != TCL_OK) {
            return TCL_ERROR;           /* Can't find named axis */
        }
        return Blt_ConfigureInfoFromObj(interp, graphPtr->tkwin, configSpecs,
                (char *)scalePtr, objv[4], BLT_CONFIG_OBJV_ONLY);
    } 
    if (GetAxisIterator(interp, graphPtr, objv[3], &iter) != TCL_OK) {
        return TCL_ERROR;
    }
    for (scalePtr = FirstTaggedAxis(&iter); scalePtr != NULL; 
         scalePtr = NextTaggedAxis(&iter)) {

        if (ConfigureOp(scalePtr, interp, objc - 4, objv + 4) != TCL_OK) {
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * AxisDeleteOp --
 *
 *      Deletes one or more axes.  The actual removal may be deferred until the
 *      axis is no longer used by any element. The axis can't be referenced by
 *      its name any longer and it may be recreated.
 *
 * Results:
 *      Returns a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
AxisDeleteOp(ClientData clientData, Tcl_Interp *interp, int objc, 
             Tcl_Obj *const *objv)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    Blt_HashTable selected;
    Graph *graphPtr = clientData;
    int i;

    Blt_InitHashTable(&selected, BLT_ONE_WORD_KEYS);
    for (i = 3; i < objc; i++) {
        Axis *scalePtr;
        AxisIterator iter;

        if (GetAxisIterator(interp, graphPtr, objv[i], &iter) != TCL_OK) {
            Blt_DeleteHashTable(&selected);
            return TCL_ERROR;
        }
        for (scalePtr = FirstTaggedAxis(&iter); scalePtr != NULL; 
             scalePtr = NextTaggedAxis(&iter)) {
            int isNew;
            Blt_HashEntry *hPtr;

            hPtr = Blt_CreateHashEntry(&selected, (char *)scalePtr, &isNew);
            Blt_SetHashValue(hPtr, scalePtr);
        }
    }
    for (hPtr = Blt_FirstHashEntry(&selected, &iter); hPtr != NULL;
         hPtr = Blt_NextHashEntry(&iter)) {
        Axis *scalePtr;

        scalePtr = Blt_GetHashValue(hPtr);
        scalePtr->flags |= DELETED;
        if (scalePtr->refCount == 0) {
            DestroyAxis(scalePtr);
        }
    }
    Blt_DeleteHashTable(&selected);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * AxisFocusOp --
 *
 *      Activates the axis, drawing the axis with its -activeforeground,
 *      -activebackgound, -activerelief attributes.
 *
 * Results:
 *      A standard TCL result.
 *
 * Side Effects:
 *      Graph will be redrawn to reflect the new axis attributes.
 *
 *---------------------------------------------------------------------------
 */
static int
AxisFocusOp(ClientData clientData, Tcl_Interp *interp, int objc,
            Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;

    if (objc > 3) {
        Axis *scalePtr;
        const char *string;

        scalePtr = NULL;
        string = Tcl_GetString(objv[3]);
        if ((string[0] != '\0') && 
            (GetAxisFromObj(interp, graphPtr, objv[3], &scalePtr) != TCL_OK)) {
            return TCL_ERROR;
        }
        graphPtr->focusPtr = scalePtr;
        Blt_SetFocusItem(graphPtr->bindTable, graphPtr->focusPtr, NULL);
    }
    /* Return the name of the axis that has focus. */
    if (graphPtr->focusPtr != NULL) {
        Tcl_SetStringObj(Tcl_GetObjResult(interp), 
                graphPtr->focusPtr->obj.name, -1);
    }
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
 *      pathName get min|max|current|rmin|rmax|
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
GetOp(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;

    /* Report only on axes. */
    if ((objPtr != NULL) && (!objPtr->deleted) &&
        ((objPtr->classId >= CID_NONE) && (objPtr->classId <= CID_AXIS_Z))) {
        char c;
        char  *string;

        string = Tcl_GetString(objv[3]);
        c = string[0];
        if ((c == 'c') && (strcmp(string, "current") == 0)) {
            Tcl_SetStringObj(Tcl_GetObjResult(interp), objPtr->name,-1);
        } else if ((c == 'd') && (strcmp(string, "detail") == 0)) {
            Axis *scalePtr;
            
            scalePtr = (Axis *)objPtr;
            Tcl_SetStringObj(Tcl_GetObjResult(interp), scalePtr->detail, -1);
        }
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * AxisInvTransformOp --
 *
 *      Maps the given window coordinate into an axis-value.
 *
 * Results:
 *      Returns a standard TCL result.  interp->result contains the axis
 *      value. If an error occurred, TCL_ERROR is returned and interp->result
 *      will contain an error message.
 *
 *---------------------------------------------------------------------------
 */
static int
AxisInvTransformOp(ClientData clientData, Tcl_Interp *interp, int objc, 
                   Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    Axis *scalePtr;

    if (GetAxisFromObj(interp, graphPtr, objv[3], &scalePtr) != TCL_OK) {
        return TCL_ERROR;
    }
    return InvTransformOp(scalePtr, interp, objc - 4, objv + 4);
}

/*
 *---------------------------------------------------------------------------
 *
 * AxisLimitsOp --
 *
 *      This procedure returns a string representing the axis limits of the
 *      graph.  The format of the string is { left top right bottom}.
 *
 * Results:
 *      Always returns TCL_OK.  The interp->result field is
 *      a list of the graph axis limits.
 *
 *---------------------------------------------------------------------------
 */
static int
AxisLimitsOp(ClientData clientData, Tcl_Interp *interp, int objc, 
             Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    Axis *scalePtr;

    if (GetAxisFromObj(interp, graphPtr, objv[3], &scalePtr) != TCL_OK) {
        return TCL_ERROR;
    }
    return LimitsOp(scalePtr, interp, objc - 4, objv + 4);
}

/*
 *---------------------------------------------------------------------------
 *
 * AxisMarginOp --
 *
 *      This procedure returns a string representing the axis limits of the
 *      graph.  The format of the string is "left top right bottom".
 *
 * Results:
 *      Always returns TCL_OK.  The interp->result field is a list of the
 *      graph axis limits.
 *
 *---------------------------------------------------------------------------
 */
static int
AxisMarginOp(ClientData clientData, Tcl_Interp *interp, int objc, 
             Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    Axis *scalePtr;

    if (GetAxisFromObj(interp, graphPtr, objv[3], &scalePtr) != TCL_OK) {
        return TCL_ERROR;
    }
    return MarginOp(scalePtr, interp, objc - 4, objv + 4);
}

typedef struct {
    int flags;
} NamesArgs;

#define ZOOM    (1<<0)
#define VISIBLE (1<<1)

static Blt_SwitchSpec namesSwitches[] = 
{
    {BLT_SWITCH_BITS_NOARG, "-zoom", "", (char *)NULL,
        Blt_Offset(NamesArgs, flags), 0, ZOOM},
    {BLT_SWITCH_BITS_NOARG, "-visible", "", (char *)NULL,
        Blt_Offset(NamesArgs, flags), 0, VISIBLE},
    {BLT_SWITCH_END}
};

/*
 *---------------------------------------------------------------------------
 *
 * AxisNamesOp --
 *
 *      Return a list of the names of all the axes.
 *
 * Results:
 *      Returns a standard TCL result.
 *
 *      pathName axis names -zoom -visible ?pattern...?
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
AxisNamesOp(ClientData clientData, Tcl_Interp *interp, int objc,
            Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    Tcl_Obj *listObjPtr;
    NamesArgs args;
    int i, count;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    
    count = 0;
    for (i = 3; i < objc; i++) {
        const char *string;

        string = Tcl_GetString(objv[i]);
        if (string[0] != '-') {
            break;
        }
        count++;
    }
    args.flags = 0;
    if (Blt_ParseSwitches(interp, namesSwitches, count, objv + 3, &args,
                          BLT_SWITCH_DEFAULTS) < 0) {
        return TCL_ERROR;
    }
    objc -= count;
    objv += count;
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    for (hPtr = Blt_FirstHashEntry(&graphPtr->axes.nameTable, &cursor);
         hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
        Axis *scalePtr;
        int match;
        
        scalePtr = Blt_GetHashValue(hPtr);
        if (scalePtr->flags & DELETED) {
            continue;
        }
        if (((scalePtr->flags & HIDDEN) || (scalePtr->marginPtr == NULL)) &&
            (args.flags & (VISIBLE|ZOOM))) {
            continue;               /* Don't zoom hidden axes. */
        }
        if ((scalePtr->obj.classId != CID_AXIS_X) &&
            (scalePtr->obj.classId != CID_AXIS_Y) &&
            (args.flags & ZOOM)) {
            continue;               /* Zoom only X or Y axes. */
        }
        if ((scalePtr->marginPtr != NULL) &&
            (scalePtr->marginPtr->numVisibleAxes > 1)) {
            continue;               /* Don't zoom stacked axes. */
        }
        match = FALSE;
        for (i = 3; i < objc; i++) {
            const char *pattern;

            pattern = Tcl_GetString(objv[i]);
            if (Tcl_StringMatch(scalePtr->obj.name, pattern)) {
                match = TRUE;
                break;
            }
        }
        if ((objc == 3) || (match)) {
            Tcl_ListObjAppendElement(interp, listObjPtr, 
                Tcl_NewStringObj(scalePtr->obj.name, -1));
        }
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * TagAddOp --
 *
 *      .g axis tag add tagName elem1 elem2 elem2 elem4
 *
 *---------------------------------------------------------------------------
 */
static int
TagAddOp(ClientData clientData, Tcl_Interp *interp, int objc,
         Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    const char *tag;

    tag = Tcl_GetString(objv[4]);
    if (strcmp(tag, "all") == 0) {
        Tcl_AppendResult(interp, "can't add reserved tag \"", tag, "\"", 
                         (char *)NULL);
        return TCL_ERROR;
    }
    if (objc == 5) {
        /* No axes specified.  Just add the tag. */
        Blt_Tags_AddTag(&graphPtr->axes.tags, tag);
    } else {
        int i;

        for (i = 5; i < objc; i++) {
            Axis *scalePtr;
            AxisIterator iter;
            
            if (GetAxisIterator(interp, graphPtr, objv[i], &iter) != TCL_OK){
                return TCL_ERROR;
            }
            for (scalePtr = FirstTaggedAxis(&iter); scalePtr != NULL; 
                 scalePtr = NextTaggedAxis(&iter)) {
                Blt_Tags_AddItemToTag(&graphPtr->axes.tags, tag, scalePtr);
            }
        }
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * TagDeleteOp --
 *
 *      .g axis tag delete tagName tab1 tab2 tab3
 *
 *---------------------------------------------------------------------------
 */
static int
TagDeleteOp(ClientData clientData, Tcl_Interp *interp, int objc,
            Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    const char *tag;
    int i;

    tag = Tcl_GetString(objv[4]);
    if (strcmp(tag, "all") == 0) {
        Tcl_AppendResult(interp, "can't delete reserved tag \"", tag, "\"", 
                         (char *)NULL);
        return TCL_ERROR;
    }
    for (i = 4; i < objc; i++) {
        Axis *scalePtr;
        AxisIterator iter;
        
        if (GetAxisIterator(interp, graphPtr, objv[i], &iter) != TCL_OK) {
            return TCL_ERROR;
        }
        for (scalePtr = FirstTaggedAxis(&iter); scalePtr != NULL; 
             scalePtr = NextTaggedAxis(&iter)) {
            Blt_Tags_RemoveItemFromTag(&graphPtr->axes.tags, tag, scalePtr);
        }
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * TagExistsOp --
 *
 *      Returns the existence of the one or more tags in the given node.  If
 *      the node has any the tags, true is return in the interpreter.
 *
 *      .g axis tag exists elem tag1 tag2 tag3...
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TagExistsOp(ClientData clientData, Tcl_Interp *interp, int objc,
            Tcl_Obj *const *objv)
{
    AxisIterator iter;
    Graph *graphPtr = clientData;
    int i;

    if (GetAxisIterator(interp, graphPtr, objv[4], &iter) != TCL_OK) {
        return TCL_ERROR;
    }
    for (i = 5; i < objc; i++) {
        const char *tag;
        Axis *scalePtr;

        tag = Tcl_GetString(objv[i]);
        for (scalePtr = FirstTaggedAxis(&iter); scalePtr != NULL; 
             scalePtr = NextTaggedAxis(&iter)) {
            if (Blt_Tags_ItemHasTag(&graphPtr->axes.tags, scalePtr, tag)) {
                Tcl_SetBooleanObj(Tcl_GetObjResult(interp), TRUE);
                return TCL_OK;
            }
        }
    }
    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), FALSE);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagForgetOp --
 *
 *      Removes the given tags from all tabs.
 *
 *      .g axis tag forget tag1 tag2 tag3...
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
TagForgetOp(ClientData clientData, Tcl_Interp *interp, int objc,
            Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    int i;

    for (i = 4; i < objc; i++) {
        const char *tag;

        tag = Tcl_GetString(objv[i]);
        Blt_Tags_ForgetTag(&graphPtr->axes.tags, tag);
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagGetOp --
 *
 *      Returns tag names for a given node.  If one of more pattern arguments
 *      are provided, then only those matching tags are returned.
 *
 *      .t axis tag get elem pat1 pat2...
 *
 *---------------------------------------------------------------------------
 */
static int
TagGetOp(ClientData clientData, Tcl_Interp *interp, int objc,
         Tcl_Obj *const *objv)
{
    Axis *scalePtr; 
    AxisIterator iter;
    Graph *graphPtr = clientData;
    Tcl_Obj *listObjPtr;

    if (GetAxisIterator(interp, graphPtr, objv[4], &iter) != TCL_OK) {
        return TCL_ERROR;
    }
    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    for (scalePtr = FirstTaggedAxis(&iter); scalePtr != NULL; 
         scalePtr = NextTaggedAxis(&iter)) {
        if (objc == 5) {
            Blt_Tags_AppendTagsToObj(&graphPtr->axes.tags, scalePtr, 
                listObjPtr);
            Tcl_ListObjAppendElement(interp, listObjPtr, 
                Tcl_NewStringObj("all", 3));
        } else {
            int i;
            
            /* Check if we need to add the special tags "all" */
            for (i = 5; i < objc; i++) {
                const char *pattern;

                pattern = Tcl_GetString(objv[i]);
                if (Tcl_StringMatch("all", pattern)) {
                    Tcl_Obj *objPtr;

                    objPtr = Tcl_NewStringObj("all", 3);
                    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
                    break;
                }
            }
            /* Now process any standard tags. */
            for (i = 5; i < objc; i++) {
                Blt_ChainLink link;
                const char *pattern;
                Blt_Chain chain;

                chain = Blt_Chain_Create();
                Blt_Tags_AppendTagsToChain(&graphPtr->axes.tags, scalePtr, 
                        chain);
                pattern = Tcl_GetString(objv[i]);
                for (link = Blt_Chain_FirstLink(chain); link != NULL; 
                     link = Blt_Chain_NextLink(link)) {
                    const char *tag;
                    Tcl_Obj *objPtr;

                    tag = (const char *)Blt_Chain_GetValue(link);
                    if (!Tcl_StringMatch(tag, pattern)) {
                        continue;
                    }
                    objPtr = Tcl_NewStringObj(tag, -1);
                    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
                }
                Blt_Chain_Destroy(chain);
            }
        }    
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagNamesOp --
 *
 *      Returns the names of all the tags in the graph.  If one of more
 *      axis arguments are provided, then only the tags found in those
 *      axes are returned.
 *
 *      .g axis tag names elem elem elem...
 *
 *---------------------------------------------------------------------------
 */
static int
TagNamesOp(ClientData clientData, Tcl_Interp *interp, int objc,
           Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    Tcl_Obj *listObjPtr, *objPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    objPtr = Tcl_NewStringObj("all", -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    if (objc == 4) {
        Blt_Tags_AppendAllTagsToObj(&graphPtr->axes.tags, listObjPtr);
    } else {
        Blt_HashTable selected;
        int i;

        Blt_InitHashTable(&selected, BLT_STRING_KEYS);
        for (i = 4; i < objc; i++) {
            AxisIterator iter;
            Axis *scalePtr;

            if (GetAxisIterator(interp, graphPtr, objv[i], &iter) != TCL_OK){
                goto error;
            }
            for (scalePtr = FirstTaggedAxis(&iter); scalePtr != NULL; 
                 scalePtr = NextTaggedAxis(&iter)) {
                Blt_ChainLink link;
                Blt_Chain chain;

                chain = Blt_Chain_Create();
                Blt_Tags_AppendTagsToChain(&graphPtr->axes.tags, scalePtr,
                        chain);
                for (link = Blt_Chain_FirstLink(chain); link != NULL; 
                     link = Blt_Chain_NextLink(link)) {
                    const char *tag;
                    int isNew;

                    tag = Blt_Chain_GetValue(link);
                    Blt_CreateHashEntry(&selected, tag, &isNew);
                }
                Blt_Chain_Destroy(chain);
            }
        }
        {
            Blt_HashEntry *hPtr;
            Blt_HashSearch hiter;

            for (hPtr = Blt_FirstHashEntry(&selected, &hiter); hPtr != NULL;
                 hPtr = Blt_NextHashEntry(&hiter)) {
                objPtr = Tcl_NewStringObj(Blt_GetHashKey(&selected, hPtr), -1);
                Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
            }
        }
        Blt_DeleteHashTable(&selected);
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
 error:
    Tcl_DecrRefCount(listObjPtr);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagSearchOp --
 *
 *      Returns the names of axis associated with the given tags.  The
 *      name returned will represent the union of tabs for all the given
 *      tags.
 *
 *      pathName axis tag search tag1 tag2 tag3...
 *
 *---------------------------------------------------------------------------
 */
static int
TagSearchOp(ClientData clientData, Tcl_Interp *interp, int objc,
            Tcl_Obj *const *objv)
{
    Blt_HashTable selected;
    Graph *graphPtr = clientData;
    int i;
        
    Blt_InitHashTable(&selected, BLT_ONE_WORD_KEYS);
    for (i = 4; i < objc; i++) {
        const char *tag;

        tag = Tcl_GetString(objv[i]);
        if (strcmp(tag, "all") == 0) {
            break;
        } else {
            Blt_Chain chain;

            chain = Blt_Tags_GetItemList(&graphPtr->axes.tags, tag);
            if (chain != NULL) {
                Blt_ChainLink link;

                for (link = Blt_Chain_FirstLink(chain); link != NULL; 
                     link = Blt_Chain_NextLink(link)) {
                    Axis *scalePtr;
                    int isNew;
                    
                    scalePtr = Blt_Chain_GetValue(link);
                    Blt_CreateHashEntry(&selected, (char *)scalePtr, &isNew);
                }
            }
            continue;
        }
        Tcl_AppendResult(interp, "can't find a tag \"", tag, "\"",
                         (char *)NULL);
        goto error;
    }
    {
        Blt_HashEntry *hPtr;
        Blt_HashSearch iter;
        Tcl_Obj *listObjPtr;

        listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
        for (hPtr = Blt_FirstHashEntry(&selected, &iter); hPtr != NULL; 
             hPtr = Blt_NextHashEntry(&iter)) {
            Axis *scalePtr;
            Tcl_Obj *objPtr;

            scalePtr = (Axis *)Blt_GetHashKey(&selected, hPtr);
            objPtr = Tcl_NewStringObj(scalePtr->obj.name, -1);
            Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
        }
        Tcl_SetObjResult(interp, listObjPtr);
    }
    Blt_DeleteHashTable(&selected);
    return TCL_OK;

 error:
    Blt_DeleteHashTable(&selected);
    return TCL_ERROR;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagSetOp --
 *
 *      Sets one or more tags for a given tab.  Tag names can't start with a
 *      digit (to distinquish them from node ids) and can't be a reserved tag
 *      ("all").
 *
 *      pathName axis tag set axisName tag1 tag2...
 *
 *---------------------------------------------------------------------------
 */
static int
TagSetOp(ClientData clientData, Tcl_Interp *interp, int objc,
         Tcl_Obj *const *objv)
{
    AxisIterator iter;
    Graph *graphPtr = clientData;
    int i;

    if (GetAxisIterator(interp, graphPtr, objv[4], &iter) != TCL_OK) {
        return TCL_ERROR;
    }
    for (i = 5; i < objc; i++) {
        const char *tag;
        Axis *scalePtr;

        tag = Tcl_GetString(objv[i]);
        if (strcmp(tag, "all") == 0) {
            Tcl_AppendResult(interp, "can't add reserved tag \"", tag, "\"",
                             (char *)NULL);     
            return TCL_ERROR;
        }
        for (scalePtr = FirstTaggedAxis(&iter); scalePtr != NULL; 
             scalePtr = NextTaggedAxis(&iter)) {
            Blt_Tags_AddItemToTag(&graphPtr->axes.tags, tag, scalePtr);
        }    
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagUnsetOp --
 *
 *      Removes one or more tags from a given axis. If a tag doesn't exist 
 *      or is a reserved tag ("all"), nothing will be done and no error
 *      message will be returned.
 *
 *      pathName axis tag unset axisName tagName...
 *
 *---------------------------------------------------------------------------
 */
static int
TagUnsetOp(ClientData clientData, Tcl_Interp *interp, int objc,
       Tcl_Obj *const *objv)
{
    Axis *scalePtr;
    AxisIterator iter;
    Graph *graphPtr = clientData;

    if (GetAxisIterator(interp, graphPtr, objv[4], &iter) != TCL_OK) {
        return TCL_ERROR;
    }
    for (scalePtr = FirstTaggedAxis(&iter); scalePtr != NULL; 
         scalePtr = NextTaggedAxis(&iter)) {
        int i;

        for (i = 5; i < objc; i++) {
            const char *tag;

            tag = Tcl_GetString(objv[i]);
            Blt_Tags_RemoveItemFromTag(&graphPtr->axes.tags, tag, scalePtr);
        }    
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TagOp --
 *
 *      This procedure is invoked to process tag operations.
 *
 * Results:
 *      A standard TCL result.
 *
 * Side Effects:
 *      See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
static Blt_OpSpec tagOps[] =
{
    {"add",      1, TagAddOp,      5, 0, "tagName ?axisName ...?",},
    {"delete",   1, TagDeleteOp,   5, 0, "axisName ?tagName ...?",},
    {"exists",   1, TagExistsOp,   5, 0, "axisName ?tagName ...?",},
    {"forget",   1, TagForgetOp,   4, 0, "?tagName ...?",},
    {"get",      1, TagGetOp,      5, 0, "axisName ?pattern ...?",},
    {"names",    1, TagNamesOp,    4, 0, "?axisName ...?",},
    {"search",   3, TagSearchOp,   4, 0, "?tagName ...?",},
    {"set",      3, TagSetOp,      5, 0, "axisName ?tagName ...?",},
    {"unset",    1, TagUnsetOp,    5, 0, "axisName ?tagName ...?",},
};

static int numTagOps = sizeof(tagOps) / sizeof(Blt_OpSpec);

static int
TagOp(ClientData clientData, Tcl_Interp *interp, int objc,
      Tcl_Obj *const *objv)
{
    Tcl_ObjCmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, numTagOps, tagOps, BLT_OP_ARG2,
        objc, objv, 0);
    if (proc == NULL) {
        return TCL_ERROR;
    }
    result = (*proc)(clientData, interp, objc, objv);
    return result;
}

/*
 *---------------------------------------------------------------------------
 *
 * AxisTransformOp --
 *
 *      Maps the given axis-value to a window coordinate.
 *
 * Results:
 *      Returns the window coordinate via interp->result.  If an error occurred,
 *      TCL_ERROR is returned and interp->result will contain an error message.
 *
 *---------------------------------------------------------------------------
 */
static int
AxisTransformOp(ClientData clientData, Tcl_Interp *interp, int objc, 
                Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    Axis *scalePtr;

    if (GetAxisFromObj(interp, graphPtr, objv[3], &scalePtr) != TCL_OK) {
        return TCL_ERROR;
    }
    return TransformOp(scalePtr, interp, objc - 4, objv + 4);
}

/*
 *---------------------------------------------------------------------------
 *
 * AxisTypeOp --
 *
 *      This procedure returns a string representing the type of the
 *      axis.  The format of the string is x, y or z.
 *
 * Results:
 *      Always returns TCL_OK.  The interp->result field is
 *      the type of the axis.
 *
 *---------------------------------------------------------------------------
 */
static int
AxisTypeOp(ClientData clientData, Tcl_Interp *interp, int objc, 
           Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    Axis *scalePtr;

    if (GetAxisFromObj(interp, graphPtr, objv[3], &scalePtr) != TCL_OK) {
        return TCL_ERROR;
    }
    return TypeOp(scalePtr, interp, objc - 4, objv + 4);
}


static int
AxisViewOp(ClientData clientData, Tcl_Interp *interp, int objc,
           Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    Axis *scalePtr;

    if (GetAxisFromObj(interp, graphPtr, objv[3], &scalePtr) != TCL_OK) {
        return TCL_ERROR;
    }
    return ViewOp(scalePtr, interp, objc - 4, objv + 4);
}

static Blt_OpSpec virtAxisOps[] = {
    {"activate",     1, AxisActivateOp,     4, 4, "axisName"},
    {"bind",         1, AxisBindOp,         3, 6, "bindTag sequence command"},
    {"cget",         2, AxisCgetOp,         5, 5, "axisName option"},
    {"configure",    2, AxisConfigureOp,    4, 0, "axisName ?option value?..."},
    {"create",       2, AxisCreateOp,       4, 0, "axisName ?option value?..."},
    {"deactivate",   3, AxisActivateOp,     4, 4, "axisName"},
    {"delete",       3, AxisDeleteOp,       3, 0, "?axisName?..."},
    {"focus",        1, AxisFocusOp,        3, 4, "?axisName?"},
    {"get",          1, AxisGetOp,          4, 4, "name"},
    {"invtransform", 1, AxisInvTransformOp, 5, 5, "axisName value"},
    {"limits",       1, AxisLimitsOp,       4, 4, "axisName"},
    {"margin",       1, AxisMarginOp,       4, 4, "axisName"},
    {"names",        1, AxisNamesOp,        3, 0, "?pattern?..."},
    {"tag",          2, TagOp,              2, 0, "args"},
    {"transform",    2, AxisTransformOp,    5, 5, "axisName value"},
    {"type",         2, AxisTypeOp,         4, 4, "axisName"},
    {"view",         1, AxisViewOp,         4, 7, "axisName ?moveto fract? "
        "?scroll number what?"},
};
static int numVirtAxisOps = sizeof(virtAxisOps) / sizeof(Blt_OpSpec);

int
Blt_VirtualAxisOp(ClientData clientData, Tcl_Interp *interp, int objc, 
                  Tcl_Obj *const *objv)
{
    Tcl_ObjCmdProc *proc;
    int result;

    proc = Blt_GetOpFromObj(interp, numVirtAxisOps, virtAxisOps, BLT_OP_ARG2, 
        objc, objv, 0);
    if (proc == NULL) {
        return TCL_ERROR;
    }
    result = (*proc) (clientData, interp, objc, objv);
    return result;
}

int
Blt_AxisOp(ClientData clientData, Tcl_Interp *interp, int margin, int objc,
           Tcl_Obj *const *objv)
{
    Graph *graphPtr = clientData;
    int result;
    Tcl_ObjCmdProc *proc;

    proc = Blt_GetOpFromObj(interp, numAxisOps, axisOps, BLT_OP_ARG2, 
        objc, objv, 0);
    if (proc == NULL) {
        return TCL_ERROR;
    }
    if (proc == UseOp) {
        lastMargin = margin;            /* Set global variable to the
                                         * margin in the argument
                                         * list. Needed only for UseOp. */
        result = (*proc)(clientData, interp, objc - 3, objv + 3);
    } else {
        Axis *scalePtr;

        scalePtr = FirstAxis(graphPtr->margins + margin);
        if (scalePtr == NULL) {
            return TCL_OK;
        }
        result = (*proc)(scalePtr, interp, objc - 3, objv + 3);
    }
    return result;
}

void
Blt_MapAxes(Graph *graphPtr)
{
    int i;
    
    for (i = 0; i < 4; i++) {
        Axis *scalePtr;
        Margin *marginPtr;
        float sum;
        
        marginPtr = graphPtr->margins + i;
        /* Reset the margin offsets (stacked and layered). */
        marginPtr->nextStackOffset = marginPtr->nextLayerOffset = 0;
        sum = 0.0;
        if (graphPtr->flags & STACK_AXES) {
            /* For stacked axes figure out the sum of the weights.*/
            for (scalePtr = FirstAxis(marginPtr); scalePtr != NULL; 
                 scalePtr = NextAxis(scalePtr)) {
                if (scalePtr->flags & (DELETED|HIDDEN)) {
                    continue;           /* Ignore axes that aren't in use
                                         * or have been deleted.  */
                }
                sum += scalePtr->weight;
            }
        }
        for (scalePtr = FirstAxis(marginPtr); scalePtr != NULL; 
             scalePtr = NextAxis(scalePtr)) {
            if (scalePtr->flags & DELETED) {
                continue;               /* Don't map axes that aren't being
                                         * used or have been deleted. */
            }
            if (HORIZONTAL(marginPtr)) {
                scalePtr->screenMin = graphPtr->hOffset;
                scalePtr->width = graphPtr->x2 - graphPtr->x1;
                scalePtr->screenRange = graphPtr->hRange;
            } else {
                scalePtr->screenMin = graphPtr->vOffset;
                scalePtr->height = graphPtr->y2 - graphPtr->y1;
                scalePtr->screenRange = graphPtr->vRange;
            }
            scalePtr->screenScale = 1.0 / scalePtr->screenRange;
            if (scalePtr->flags & HIDDEN) {
                continue;
            }
            if (graphPtr->flags & STACK_AXES) {
                if (scalePtr->reqNumMajorTicks <= 0) {
                    scalePtr->reqNumMajorTicks = 4;
                }
                MapStackedAxis(scalePtr, sum);
            } else {
                if (scalePtr->reqNumMajorTicks <= 0) {
                    scalePtr->reqNumMajorTicks = 4;
                }
                MapAxis(scalePtr);
                /* The next axis will start after the current axis. */
                marginPtr->nextLayerOffset += HORIZONTAL(scalePtr) ?
                    scalePtr->height : scalePtr->width;
            }
        }
    }
}

void
Blt_DrawAxes(Graph *graphPtr, Drawable drawable)
{
    int i;

    for (i = 0; i < 4; i++) {
        Axis *scalePtr;
        Margin *marginPtr;

        marginPtr = graphPtr->margins + i;
        for (scalePtr = FirstAxis(marginPtr); scalePtr != NULL; 
             scalePtr = NextAxis(scalePtr)) {
            if (scalePtr->flags & (DELETED|HIDDEN)) {
                continue;
            }
            DrawAxis(scalePtr, drawable);
        }
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_DrawAxisLimits --
 *
 *      Draws the min/max values of the axis in the plotting area.  The
 *      text strings are formatted according to the "sprintf" format
 *      descriptors in the limitsFmts array.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      Draws the numeric values of the axis limits into the outer regions
 *      of the plotting area.
 *
 *---------------------------------------------------------------------------
 */
void
Blt_DrawAxisLimits(Graph *graphPtr, Drawable drawable)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    char minString[200], maxString[200];
    int vMin, hMin, vMax, hMax;

#define SPACING 8
    vMin = vMax = graphPtr->x1 + graphPtr->padLeft + 2;
    hMin = hMax = graphPtr->y2 - graphPtr->padBottom - 2;   /* Offsets */

    for (hPtr = Blt_FirstHashEntry(&graphPtr->axes.nameTable, &cursor);
        hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
        Axis *scalePtr;
        Dim2d textDim;
        Tcl_Obj **objv;
        char *minPtr, *maxPtr;
        const char *minFmt, *maxFmt;
        int objc;
        
        scalePtr = Blt_GetHashValue(hPtr);
        if (scalePtr->flags & DELETED) {
            continue;                   /* Axis has been deleted. */
        } 
        if (scalePtr->limitsFmtsObjPtr == NULL) {
            continue;                   /* No limits format specified for
                                         * this axis. */
        }
        if (scalePtr->marginPtr == NULL) {
            continue;                   /* Axis is not associated with any
                                         * margin. */
        }
        Tcl_ListObjGetElements(NULL,scalePtr->limitsFmtsObjPtr, &objc, &objv);
        minPtr = maxPtr = NULL;
        minFmt = maxFmt = Tcl_GetString(objv[0]);
        if (objc > 1) {
            maxFmt = Tcl_GetString(objv[1]);
        }
        if (minFmt[0] != '\0') {
            minPtr = minString;
            Blt_FmtString(minString, 200, minFmt, scalePtr->tickMin);
        }
        if (maxFmt[0] != '\0') {
            maxPtr = maxString;
            Blt_FmtString(maxString, 200, maxFmt, scalePtr->tickMax);
        }
        if (scalePtr->flags & DECREASING) {
            char *tmp;

            tmp = minPtr, minPtr = maxPtr, maxPtr = tmp;
        }
        if (maxPtr != NULL) {
            if (HORIZONTAL(scalePtr->marginPtr)) {
                Blt_Ts_SetAngle(scalePtr->limitsTextStyle, 90.0);
                Blt_Ts_SetAnchor(scalePtr->limitsTextStyle, TK_ANCHOR_SE);
                Blt_DrawText2(graphPtr->tkwin, drawable, maxPtr,
                    &scalePtr->limitsTextStyle, graphPtr->x2, hMax, &textDim);
                hMax -= (textDim.height + SPACING);
            } else {
                Blt_Ts_SetAngle(scalePtr->limitsTextStyle, 0.0);
                Blt_Ts_SetAnchor(scalePtr->limitsTextStyle, TK_ANCHOR_NW);
                Blt_DrawText2(graphPtr->tkwin, drawable, maxPtr,
                    &scalePtr->limitsTextStyle, vMax, graphPtr->y1, &textDim);
                vMax += (textDim.width + SPACING);
            }
        }
        if (minPtr != NULL) {
            Blt_Ts_SetAnchor(scalePtr->limitsTextStyle, TK_ANCHOR_SW);
            if (HORIZONTAL(scalePtr->marginPtr)) {
                Blt_Ts_SetAngle(scalePtr->limitsTextStyle, 90.0);
                Blt_DrawText2(graphPtr->tkwin, drawable, minPtr,
                    &scalePtr->limitsTextStyle, graphPtr->x1, hMin, &textDim);
                hMin -= (textDim.height + SPACING);
            } else {
                Blt_Ts_SetAngle(scalePtr->limitsTextStyle, 0.0);
                Blt_DrawText2(graphPtr->tkwin, drawable, minPtr,
                    &scalePtr->limitsTextStyle, vMin, graphPtr->y2, &textDim);
                vMin += (textDim.width + SPACING);
            }
        }
    } /* Loop on axes */
}

void
Blt_AxisLimitsToPostScript(Graph *graphPtr, Blt_Ps ps)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    double vMin, hMin, vMax, hMax;
    char string[200];

#define SPACING 8
    vMin = vMax = graphPtr->x1 + graphPtr->padLeft + 2;
    hMin = hMax = graphPtr->y2 - graphPtr->padBottom - 2;   /* Offsets */
    for (hPtr = Blt_FirstHashEntry(&graphPtr->axes.nameTable, &cursor);
         hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
        Axis *scalePtr;
        Tcl_Obj **objv;
        const char *minFmt, *maxFmt;
        int objc;
        unsigned int textWidth, textHeight;

        scalePtr = Blt_GetHashValue(hPtr);
        if (scalePtr->flags & DELETED) {
            continue;                   /* Axis has been deleted. */
        } 
        if (scalePtr->limitsFmtsObjPtr == NULL) {
            continue;                   /* No limits format specified for
                                         * the axis. */
        }
        if (scalePtr->marginPtr == NULL) {
            continue;                   /* Axis is not associated with any
                                         * margin. */
        }
        Tcl_ListObjGetElements(NULL, scalePtr->limitsFmtsObjPtr, &objc, &objv);
        minFmt = maxFmt = Tcl_GetString(objv[0]);
        if (objc > 1) {
            maxFmt = Tcl_GetString(objv[1]);
        }
        if (*maxFmt != '\0') {
            Blt_FmtString(string, 200, maxFmt, scalePtr->tickMax);
            Blt_GetTextExtents(scalePtr->tickFont, 0, string, -1, &textWidth,
                &textHeight);
            if ((textWidth > 0) && (textHeight > 0)) {
                if (scalePtr->obj.classId == CID_AXIS_X) {
                    Blt_Ts_SetAngle(scalePtr->limitsTextStyle, 90.0);
                    Blt_Ts_SetAnchor(scalePtr->limitsTextStyle, TK_ANCHOR_SE);
                    Blt_Ps_DrawText(ps, string, &scalePtr->limitsTextStyle, 
                        (double)graphPtr->x2, hMax);
                    hMax -= (textWidth + SPACING);
                } else {
                    Blt_Ts_SetAngle(scalePtr->limitsTextStyle, 0.0);
                    Blt_Ts_SetAnchor(scalePtr->limitsTextStyle, TK_ANCHOR_NW);
                    Blt_Ps_DrawText(ps, string, &scalePtr->limitsTextStyle,
                        vMax, (double)graphPtr->y1);
                    vMax += (textWidth + SPACING);
                }
            }
        }
        if (*minFmt != '\0') {
            Blt_FmtString(string, 200, minFmt, scalePtr->tickMin);
            Blt_GetTextExtents(scalePtr->tickFont, 0, string, -1, &textWidth,
                &textHeight);
            if ((textWidth > 0) && (textHeight > 0)) {
                Blt_Ts_SetAnchor(scalePtr->limitsTextStyle, TK_ANCHOR_SW);
                if (scalePtr->obj.classId == CID_AXIS_X) {
                    Blt_Ts_SetAngle(scalePtr->limitsTextStyle, 90.0);
                    Blt_Ps_DrawText(ps, string, &scalePtr->limitsTextStyle, 
                        (double)graphPtr->x1, hMin);
                    hMin -= (textWidth + SPACING);
                } else {
                    Blt_Ts_SetAngle(scalePtr->limitsTextStyle, 0.0);
                    Blt_Ps_DrawText(ps, string, &scalePtr->limitsTextStyle, 
                        vMin, (double)graphPtr->y2);
                    vMin += (textWidth + SPACING);
                }
            }
        }
    }
}

Axis *
Blt_GetFirstAxis(Blt_Chain chain)
{
    Blt_ChainLink link;

    link = Blt_Chain_FirstLink(chain);
    if (link == NULL) {
        return NULL;
    }
    return Blt_Chain_GetValue(link);
}

Axis *
Blt_NearestAxis(Graph *graphPtr, int x, int y)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    
    for (hPtr = Blt_FirstHashEntry(&graphPtr->axes.nameTable, &cursor); 
         hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
        Axis *scalePtr;

        scalePtr = Blt_GetHashValue(hPtr);
        if ((scalePtr->marginPtr == NULL) ||
            (scalePtr->flags & (DELETED|HIDDEN))) {
            continue;
        }
        if (scalePtr->flags & TICKLABELS) {
            Blt_ChainLink link;

            for (link = Blt_Chain_FirstLink(scalePtr->tickLabels); link != NULL; 
                 link = Blt_Chain_NextLink(link)) {     
                TickLabel *labelPtr;
                Point2d t;
                double rw, rh;
                Point2d bbox[5];

                labelPtr = Blt_Chain_GetValue(link);
                Blt_GetBoundingBox((double)labelPtr->width, 
                        (double)labelPtr->height, 
                        scalePtr->tickAngle, &rw, &rh, bbox);
                t = Blt_AnchorPoint(labelPtr->anchorPos.x, 
                        labelPtr->anchorPos.y, rw, rh, scalePtr->tickAnchor);
                t.x = x - t.x - (rw * 0.5);
                t.y = y - t.y - (rh * 0.5);

                bbox[4] = bbox[0];
                if (Blt_PointInPolygon(&t, bbox, 5)) {
                    scalePtr->detail = "label";
                    return scalePtr;
                }
            }
        }
        if (scalePtr->title != NULL) {   /* and then the title string. */
            Point2d bbox[5];
            Point2d t;
            double rw, rh;
            unsigned int w, h;

            Blt_GetTextExtents(scalePtr->titleFont, 0, scalePtr->title,-1,&w,&h);
            Blt_GetBoundingBox((double)w, (double)h, scalePtr->titleAngle, 
                &rw, &rh, bbox);
            t = Blt_AnchorPoint(scalePtr->titlePos.x, scalePtr->titlePos.y, 
                rw, rh, scalePtr->titleAnchor);
            /* Translate the point so that the 0,0 is the upper left 
             * corner of the bounding box.  */
            t.x = x - t.x - (rw * 0.5);
            t.y = y - t.y - (rh * 0.5);
            
            bbox[4] = bbox[0];
            if (Blt_PointInPolygon(&t, bbox, 5)) {
                scalePtr->detail = "title";
                return scalePtr;
            }
        }
        if (scalePtr->tickLineWidth > 0) {   /* Check for the axis region */
            if ((x <= scalePtr->right) && (x >= scalePtr->left) && 
                (y <= scalePtr->bottom) && (y >= scalePtr->top)) {
                scalePtr->detail = "line";
                return scalePtr;
            }
        }
    }
    return NULL;
}
 
ClientData
Blt_MakeAxisTag(Graph *graphPtr, const char *tagName)
{
    Blt_HashEntry *hPtr;
    int isNew;

    hPtr = Blt_CreateHashEntry(&graphPtr->axes.bindTagTable, tagName, &isNew);
    return Blt_GetHashKey(&graphPtr->axes.bindTagTable, hPtr);
}

#include <time.h>
#include <sys/time.h>

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

static double
TimeFloor(Axis *scalePtr, double min, TimeUnits units, Blt_DateTime *datePtr)
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
TimeCeil(Axis *scalePtr, double max, TimeUnits units, Blt_DateTime *datePtr)
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

static void
YearTicks(Scale *scalePtr)
{
    Blt_DateTime date1, date2;
    double step;
    int numTicks;
    double tickMin, tickMax;            /* Years. */
    double axisMin, axisMax;            /* Seconds. */
    double numYears;

    scalePtr->major.ticks.axisScale = scalePtr->minor.ticks.axisScale = 
        AXIS_TIME;
    tickMin = TimeFloor(scalePtr, scalePtr->outerLeft, UNITS_YEARS, &date1);
    tickMax = TimeCeil(scalePtr, scalePtr->outerRight, UNITS_YEARS, &date2);
    step = 1.0;
    numYears = date2.year - date1.year;
    if (numYears > 10) {
        long minDays, maxDays;
        double range;

        scalePtr->major.ticks.timeFormat = FMT_YEARS10;
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
        scalePtr->major.ticks.year = tickMin;
        scalePtr->minor.ticks.timeUnits = UNITS_YEARS;
        if (step > 5) {
            scalePtr->minor.ticks.numSteps = 1;
            scalePtr->minor.ticks.step = step / 2;
        } else {
            scalePtr->minor.ticks.step = 1; /* Years */
            scalePtr->minor.ticks.numSteps = step - 1;
        }
    } else {
        numTicks = numYears + 1;
        step = 0;                       /* Number of days in the year */

        tickMin = NumberDaysFromEpoch(date1.year) * SECONDS_DAY;
        tickMax = NumberDaysFromEpoch(date2.year) * SECONDS_DAY;
        
        scalePtr->major.ticks.year = date1.year;
        if (numYears > 5) {
            scalePtr->major.ticks.timeFormat = FMT_YEARS5;

            scalePtr->minor.ticks.step = (SECONDS_YEAR+1) / 2; /* 1/2 year */
            scalePtr->minor.ticks.numSteps = 1;  /* 3 - 2 */
            scalePtr->minor.ticks.timeUnits = UNITS_YEARS;
        } else {
            scalePtr->major.ticks.timeFormat = FMT_YEARS1;
     
            scalePtr->minor.ticks.step = 0; /* Months */
            scalePtr->minor.ticks.numSteps = 11;  /* 12 - 1 */
            scalePtr->minor.ticks.timeUnits = UNITS_MONTHS;
            scalePtr->minor.ticks.month = date1.year;
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
    scalePtr->major.ticks.timeUnits = UNITS_YEARS;
    scalePtr->major.ticks.fmt = "%Y";
    scalePtr->major.ticks.axisScale = AXIS_TIME;
    scalePtr->major.ticks.range = tickMax - tickMin;
    scalePtr->major.ticks.initial = tickMin;
    scalePtr->major.ticks.numSteps = numTicks;
    scalePtr->major.ticks.step = step;
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

    scalePtr->major.ticks.initial = tickMin;
    scalePtr->major.ticks.numSteps = numTicks;
    scalePtr->major.ticks.step = step;
    scalePtr->major.ticks.range = tickMax - tickMin;
    scalePtr->major.ticks.isLeapYear = left.isLeapYear;
    scalePtr->major.ticks.fmt = "%h\n%Y";
    scalePtr->major.ticks.month = left.mon;
    scalePtr->major.ticks.year = left.year;
    scalePtr->major.ticks.timeUnits = UNITS_MONTHS;
    scalePtr->major.ticks.axisScale = AXIS_TIME;
    
    scalePtr->minor.ticks.numSteps = 5;
    scalePtr->minor.ticks.step = SECONDS_WEEK;
    scalePtr->minor.ticks.month = left.mon;
    scalePtr->minor.ticks.year = left.year;
    scalePtr->minor.ticks.timeUnits = UNITS_WEEKS;
    scalePtr->minor.ticks.axisScale = AXIS_TIME;

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
    
    scalePtr->major.ticks.step = step;
    scalePtr->major.ticks.initial = tickMin;
    scalePtr->major.ticks.numSteps = numTicks;
    scalePtr->major.ticks.timeUnits = UNITS_WEEKS;
    scalePtr->major.ticks.range = tickMax - tickMin;
    scalePtr->major.ticks.fmt = "%h %d";
    scalePtr->major.ticks.axisScale = AXIS_TIME;
    scalePtr->minor.ticks.step = SECONDS_DAY;
    scalePtr->minor.ticks.numSteps = 6;  
    scalePtr->minor.ticks.timeUnits = UNITS_DAYS;
    scalePtr->minor.ticks.axisScale = AXIS_TIME;
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

    scalePtr->major.ticks.step = step;
    scalePtr->major.ticks.initial = tickMin;
    scalePtr->major.ticks.numSteps = numTicks;
    scalePtr->major.ticks.timeUnits = UNITS_DAYS;
    scalePtr->major.ticks.range = tickMax - tickMin;
    scalePtr->major.ticks.axisScale = SCALE_TIME;
    scalePtr->major.ticks.fmt = "%h %d";
    scalePtr->minor.ticks.step = SECONDS_HOUR * 6;
    scalePtr->minor.ticks.initial = 0;
    scalePtr->minor.ticks.numSteps = 2;  /* 6 - 2 */
    scalePtr->minor.ticks.timeUnits = UNITS_HOURS;
    scalePtr->minor.ticks.axisScale = AXIS_TIME;
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

    scalePtr->major.ticks.step = step;
    scalePtr->major.ticks.initial = tickMin;
    scalePtr->major.ticks.numSteps = numTicks;
    scalePtr->major.ticks.range = tickMax - tickMin;
    scalePtr->major.ticks.fmt = "%H:%M\n%h %d";
    scalePtr->major.ticks.timeUnits = UNITS_HOURS;
    scalePtr->major.ticks.axisScale = AXIS_TIME;

    scalePtr->minor.ticks.step = step / 4;
    scalePtr->minor.ticks.numSteps = 4;  /* 6 - 2 */
    scalePtr->minor.ticks.timeUnits = UNITS_MINUTES;
    scalePtr->minor.ticks.axisScale = AXIS_TIME;
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

    scalePtr->major.ticks.step = step;
    scalePtr->major.ticks.initial = tickMin;
    scalePtr->major.ticks.numSteps = numTicks;
    scalePtr->major.ticks.timeUnits = UNITS_MINUTES;
    scalePtr->major.ticks.axisScale = AXIS_TIME;
    scalePtr->major.ticks.fmt = "%H:%M";
    scalePtr->major.ticks.range = tickMax - tickMin;

    scalePtr->minor.ticks.step = step / (scalePtr->reqNumMinorTicks - 1);
    scalePtr->minor.ticks.numSteps = scalePtr->reqNumMinorTicks;
    scalePtr->minor.ticks.timeUnits = UNITS_MINUTES;
    scalePtr->minor.ticks.axisScale = AXIS_TIME;
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
        scaleMin = scalePtr->outerLeft;
        scaleMax = scalePtr->outerRight;
    }
    SetAxisRange(&scalePtr->tickRange, axisMin, axisMax);
    scalePtr->tickMin = axisMin;
    scalePtr->tickMax = axisMax;

    scalePtr->tickMin = axisMin;
    scalePtr->tickMax = axisMax;
    scalePtr->major.ticks.initial = tickMin;
    scalePtr->major.ticks.numSteps = numTicks;
    scalePtr->major.ticks.step = step;
    scalePtr->major.ticks.fmt = "%H:%M:%s";
    scalePtr->major.ticks.timeUnits = UNITS_SECONDS;
    scalePtr->major.ticks.axisScale = AXIS_TIME;
    scalePtr->major.ticks.range = tickMax - tickMin;

    scalePtr->minor.ticks.step = 1.0 / (double)scalePtr->reqNumMinorTicks;
    scalePtr->minor.ticks.numSteps = scalePtr->reqNumMinorTicks - 1;
    scalePtr->minor.ticks.axisScale = AXIS_LINEAR;
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
        scalePtr->major.ticks.axisScale = AXIS_TIME;
        scalePtr->major.ticks.timeUnits = units;
        break;
    default:
        Blt_Panic("unknown time units");
    }
}

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


static Tick
FirstMajorTick(Scale *scalePtr)
{
    Ticks *ticksPtr;
    Tick tick;

    ticksPtr = &scalePtr->major.ticks;
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
                scalePtr->minor.ticks.numSteps = 
                    numDaysMonth[ticksPtr->isLeapYear][ticksPtr->month];  
                scalePtr->minor.ticks.step = SECONDS_DAY;
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

    ticksPtr = &scalePtr->major.ticks;
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

    ticksPtr = &scalePtr->minor.ticks;
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

    ticksPtr = &scalePtr->minor.ticks;
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
 * ConfigureTabset --
 *
 *      This procedure is called to process an objv/objc list, plus the Tk
 *      option database, in order to configure (or reconfigure) the widget.
 *
 * Results:
 *      The return value is a standard TCL result.  If TCL_ERROR is returned,
 *      then interp->result contains an error message.
 *
 * Side Effects:
 *      Configuration information, such as text string, colors, font, etc. get
 *      set for setPtr; old resources get freed, if there were any.  The widget
 *      is redisplayed.
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
    XGCValues gcValues;
    unsigned long gcMask;
    GC newGC;
    int slantLeft, slantRight;
    float angle;
    
    if (Blt_ConfigureWidgetFromObj(interp, scalePtr->tkwin, configSpecs, 
           objc, objv, (char *)scalePtr, flags) != TCL_OK) {
        return TCL_ERROR;
    }
    if (Blt_ConfigModified(configSpecs, "-width", "-height", "-gap",
        "-slant", "-rotate", "-tiers", "-tabwidth", 
        "-scrolltabs", "-showtabs", "-xbutton", "-justify",
        (char *)NULL)) {
        setPtr->flags |= (LAYOUT_PENDING | SCROLL_PENDING);
    }

    if (scalePtr->outerLeft >= scalePtr->outerRight) {
        /* Just reverse the values. */
        tmp = scalePtr->outerLeft;
        scalePtr->outerLeft = scalePtr->outerRight;
        scalePtr->outerRight = tmp;
    }
    if (IsLogScale(scalePtr)) {
        if (scalePtr->flags & CHECK_LIMITS) {
            /* Check that the logscale limits are positive.  */
            if (scalePtr->outerLeft <= 0.0) {
                Tcl_AppendResult(graphPtr->interp,"bad logscale -min limit \"", 
                        Blt_Dtoa(graphPtr->interp, scalePtr->outerLeft), 
                        "\" for scale \"", Tk_PathName(scalePtr->tkwin), "\"", 
                        (char *)NULL);
                return TCL_ERROR;
            }
        }
    }
    angle = FMOD(scalePtr->tickAngle, 360.0f);
    if (angle < 0.0f) {
        angle += 360.0f;
    }
    if (scalePtr->normalBg != NULL) {
        Blt_Bg_SetChangedProc(scalePtr->normalBg, Blt_UpdateGraph, 
                scalePtr);
    }
    scalePtr->tickAngle = angle;
    ResetTextStyles(scalePtr);

    scalePtr->titleWidth = scalePtr->titleHeight = 0;
    if (scalePtr->title != NULL) {
        unsigned int w, h;

        Blt_GetTextExtents(scalePtr->titleFont, 0, scalePtr->title, -1, &w, &h);
        scalePtr->titleWidth = w;
        scalePtr->titleHeight = h;
    }
    EventuallyRedraw(scalePtr);
    return TCL_OK;
    if ((setPtr->reqHeight > 0) && (setPtr->reqWidth > 0)) {
        Tk_GeometryRequest(setPtr->tkwin, setPtr->reqWidth, 
                setPtr->reqHeight);
    }
    
    if (setPtr->reqQuad != QUAD_AUTO) {
        setPtr->quad = setPtr->reqQuad;
    } else {
        if (setPtr->side == SIDE_RIGHT) {
            setPtr->quad = ROTATE_270;
        } else if (setPtr->side == SIDE_LEFT) {
            setPtr->quad = ROTATE_90;
        } else if (setPtr->side == SIDE_BOTTOM) {
            setPtr->quad = ROTATE_0;
        } else if (setPtr->side == SIDE_TOP) {
            setPtr->quad = ROTATE_0;
        }
    } 

    /*
     * GC for focus highlight.
     */
    gcMask = GCForeground;
    gcValues.foreground = setPtr->highlightColor->pixel;
    newGC = Tk_GetGC(setPtr->tkwin, gcMask, &gcValues);
    if (setPtr->highlightGC != NULL) {
        Tk_FreeGC(setPtr->display, setPtr->highlightGC);
    }
    setPtr->highlightGC = newGC;

    if (setPtr->troughBg != NULL) {
        Blt_Bg_SetChangedProc(setPtr->troughBg, BackgroundChangedProc, setPtr);
    }
    ConfigureStyle(setPtr,  &setPtr->defStyle);
    if (Blt_ConfigModified(configSpecs, "-font", "-*foreground", "-rotate",
                "-*background", (char *)NULL)) {
        Tab *tabPtr;

        for (tabPtr = FirstTab(setPtr, 0); tabPtr != NULL; 
             tabPtr = NextTab(tabPtr, 0)) {
            ConfigureTab(setPtr, tabPtr);
        }
        setPtr->flags |= (LAYOUT_PENDING | SCROLL_PENDING | REDRAW_ALL);
    }
    /* Swap slant flags if side is left. */
    slantLeft = slantRight = FALSE;
    if (setPtr->reqSlant & SLANT_LEFT) {
        slantLeft = TRUE;
    }
    if (setPtr->reqSlant & SLANT_RIGHT) {
        slantRight = TRUE;
    }
    if (setPtr->side & SIDE_LEFT) {
        SWAP(slantLeft, slantRight);
    }
    setPtr->flags &= ~SLANT_BOTH;
    if (slantLeft) {
        setPtr->flags |= SLANT_LEFT;
    }
    if (slantRight) {
        setPtr->flags |= SLANT_RIGHT;
    }
    EventuallyRedraw(setPtr);
    return TCL_OK;
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
    Tk_Window tkwin;
    int w, h;
    
    scalePtr->flags &= ~REDRAW_PENDING;
    if (scalePtr->tkwin == NULL) {
        return;                         /* Window has been destroyed (we
                                         * should not get here) */
    }
    tkwin = scalePtr->tkwin;
#ifdef notdef
    fprintf(stderr, "Calling DisplayProc(%s)\n", Tk_PathName(tkwin));
#endif
    if ((Tk_Width(tkwin) <= 1) || (Tk_Height(tkwin) <= 1)) {
        /* Don't bother computing the layout until the size of the window
         * is something reasonable. */
        return;
    }
    scalePtr->width = Tk_Width(tkwin);
    scalePtr->height = Tk_Height(tkwin);
    Blt_MapGraph(graphPtr);
    if (!Tk_IsMapped(tkwin)) {
        /* The graph's window isn't displayed, so don't bother drawing
         * anything.  By getting this far, we've at least computed the
         * coordinates of the graph's new layout.  */
        return;
    }
    /* Create a pixmap the size of the window for double buffering. */
    pixmap = Blt_GetPixmap(graphPtr->display, Tk_WindowId(tkwin), 
          scalePtr->width, scalePtr->height, Tk_Depth(tkwin));
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
    XCopyArea(scalePtr->display, pixmap, Tk_WindowId(tkwin),
        scalePtr->drawGC, 0, 0, scalePtr->width, scalePtr->height, 0, 0);
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
 *	pathName deactivate min/max/grip 
 *
 *---------------------------------------------------------------------------
 */
static int
ActivateOp(ClientData clientData, Tcl_Interp *interp, int objc,
           Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;
    const char *string;

    string = Tcl_GetString(objv[2]);
    if (string[0] == 'a') {
        scalePtr->flags |= ACTIVE;
    } else {
        scalePtr->flags &= ~ACTIVE;
    }
    EventuallyRedraw(scalePtr);
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
    Graph *graphPtr = scalePtr->obj.graphPtr;

    return Blt_ConfigureBindingsFromObj(interp, graphPtr->bindTable,
          Blt_MakeScaleTag(graphPtr, scalePtr->obj.name), objc, objv);
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
 *      of the graph.  The format of the string is { left top right bottom}.
 *
 * Results:
 *      Always returns TCL_OK.  The interp->result field is
 *      a list of the graph scale limits.
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
 *      pathName invtransform value 
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InvTransformOp(ClientData clientData, Tcl_Interp *interp, int objc, 
               Tcl_Obj *const *objv)
{
    Axis *scalePtr = clientData;
    double y;                           /* Resulting axis coordinate */
    int sy;                             /* Integer window coordinate*/

    if (Tcl_GetIntFromObj(interp, objv[0], &sy) != TCL_OK) {
        return TCL_ERROR;
    }
    if (HORIZONTAL(scalePtr)) {
        y = InvHMap(scalePtr, (double)sy);
    } else {
        y = InvVMap(scalePtr, (double)sy);
    }
    Tcl_SetDoubleObj(Tcl_GetObjResult(interp), y);
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
    Axis *scalePtr = clientData;
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

static int
ViewOp(ClientData clientData, Tcl_Interp *interp, int objc,
       Tcl_Obj *const *objv)
{
    Scale *scalePtr = clientData;
    double axisOffset, axisScale;
    double fract;
    double viewMin, viewMax, worldMin, worldMax;
    double viewWidth, worldWidth;

    worldMin = scalePtr->outerLeft;
    worldMax = scalePtr->outerRight;
    /* Override data dimensions with user-selected limits. */
    if (DEFINED(scalePtr->scrollMin)) {
        worldMin = scalePtr->scrollMin;
    }
    if (DEFINED(scalePtr->scrollMax)) {
        worldMax = scalePtr->scrollMax;
    }
    viewMin = scalePtr->min;
    viewMax = scalePtr->max;
    /* Bound the view within scroll region. */ 
    if (viewMin < worldMin) {
        viewMin = worldMin;
    } 
    if (viewMax > worldMax) {
        viewMax = worldMax;
    }
    if (IsLogScale(scalePtr)) {
        worldMin = log10(worldMin);
        worldMax = log10(worldMax);
        viewMin  = log10(viewMin);
        viewMax  = log10(viewMax);
    }
    worldWidth = worldMax - worldMin;
    viewWidth  = viewMax - viewMin;

    /* Unlike horizontal axes, vertical axis values run opposite of the
     * scrollbar first/last values.  So instead of pushing the axis minimum
     * around, we move the maximum instead. */
    if (AxisIsHorzontal(scalePtr) != ((scalePtr->flags & DECREASING) != 0)) {
        axisOffset  = viewMin - worldMin;
        axisScale = graphPtr->hScale;
    } else {
        axisOffset  = worldMax - viewMax;
        axisScale = graphPtr->vScale;
    }
    if (objc == 4) {
        Tcl_Obj *listObjPtr;
        double first, last;

        first = Clamp(axisOffset / worldWidth);
        last = Clamp((axisOffset + viewWidth) / worldWidth);
        listObjPtr = Tcl_NewListObj(0, NULL);
        Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewDoubleObj(first));
        Tcl_ListObjAppendElement(interp, listObjPtr, Tcl_NewDoubleObj(last));
        Tcl_SetObjResult(interp, listObjPtr);
        return TCL_OK;
    }
    fract = axisOffset / worldWidth;
    if (GetAxisScrollInfo(interp, objc, objv, &fract, 
        viewWidth / worldWidth, scalePtr->scrollUnits, axisScale) != TCL_OK) {
        return TCL_ERROR;
    }
    if (AxisIsHorzontal(scalePtr) != ((scalePtr->flags & DECREASING) != 0)) {
        scalePtr->reqMin = (fract * worldWidth) + worldMin;
        scalePtr->reqMax = scalePtr->reqMin + viewWidth;
    } else {
        scalePtr->reqMax = worldMax - (fract * worldWidth);
        scalePtr->reqMin = scalePtr->reqMax - viewWidth;
    }
    if (IsLogScale(scalePtr)) {
        if (scalePtr->min > 0.0) {
            scalePtr->reqMin = EXP10(scalePtr->reqMin);
            scalePtr->reqMax = EXP10(scalePtr->reqMax);
        } else {
            scalePtr->reqMin = EXP10(scalePtr->reqMin) + scalePtr->min - 1.0;
            scalePtr->reqMax = EXP10(scalePtr->reqMax) + scalePtr->min - 1.0;
        }
    }
    scalePtr->flags |= (GET_AXIS_GEOMETRY | LAYOUT_NEEDED);
    EventuallyRedraw(graphPtr);
    return TCL_OK;
}

static Blt_OpSpec scaleOps[] = {
    {"activate",     1, ActivateOp,     2, 2, ""},
    {"bind",         1, BindOp,         2, 5, "sequence command"},
    {"cget",         2, CgetOp,         3, 3, "option"},
    {"configure",    2, ConfigureOp,    2, 0, "?option value?..."},
    {"deactivate",   1, ActivateOp,     2, 2, ""},
    {"identify",     1, IdentifyOp,     4, 4, "x y"},
    {"invtransform", 1, InvTransformOp, 3, 3, "value"},
    {"min",          1, MinOp,          2, 3, "?value?"},
    {"max",          1, MaxOp,          2, 3, "?value?"},
    {"nom",          1, NomOp,          2, 3, "?value?"},
    {"limits",       1, LimitsOp,       2, 2, ""},
    {"transform",    1, TransformOp,    4, 4, "x y"},
    {"view",         1, ViewOp,         2, 5, "?moveto fract? "},
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
    static Blt_CmdSpec cmdSpec[] = {
        "scale", ScaleCmd 
    };
    return Blt_InitCmd(interp, "::blt", cmdSpec);
}



/*
 *---------------------------------------------------------------------------
 *
 * MapHorizontalLayout --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
MapHorizontalLayout(Scale *scalePtr) 
{
    int cavityWidth, cavityHeigth;
    int h;
    TickLabel *minLabelPtr, *maxLabelPtr;
    int leftPad, rightPad;

    cavityWidth = Tk_Width(scalePtr->tkwin);
    cavityHeight = Tk_Height(scalePtr->tkwin);
    if (scalePtr->reqWidth > 0) {
        cavityWidth = scalePtr->reqWidth;
    }
    cavityWidth -= 2 * scalePtr->inset;
    cavityHeight -= 2 * scalePtr->inset;
    
    if (scalePtr->flags & DECREASING) {
        minLabelPtr = Blt_Chain_LastLink(scalePtr->chain);
        maxLabelPtr = Blt_Chain_FirstLink(scalePtr->chain);
    } else {
        maxLabelPtr = Blt_Chain_LastLink(scalePtr->chain);
        minLabelPtr = Blt_Chain_FirstLink(scalePtr->chain);
    }
    leftPad = MAX(minLabelPtr->width / 2, scalePtr->arrowWidth / 2);
    rightPad = MAX(maxLabelPtr->width / 2, scalePtr->arrowWidth / 2);

    x1 = scalePtr->inset + leftPad;    
    x2 = Tk_Width(scalePtr->tkwin) - (scalePtr->inset + rightPad);
    AddAxisLine(scalePtr, x1, y, x2, y);

    scalePtr->screenOffset = scalePtr->x1;
    scalePtr->screenRange = scalePtr->x2 - scalePtr->x1;

    h = 0;
    h += PADY;
    if (scalePtr->showFlags & SHOW_TITLE) {
        h += scalePtr->titleHeight;
    }
    if (scalePtr->showFlags & SHOW_COLORBAR) {
        h += scalePtr->colorbarHeight;
    }
    h += scalePtr->tickLineWidth + 2;
    if (scalePtr->showFlags & SHOW_TICKS) {
        h += scalePtr->tickLength;
    }
    if (scalePtr->showFlags & SHOW_TICKLABELS) {
        h += scalePtr->tickHeight;
    }
    h += PADY;
    scalePtr->height = h;
    scalePtr->height = cavityWidth;
    if (scalePtr->reqHeight > 0) {
        scalePtr->height = scalePtr->reqHeight;
    }
    if ((scalePtr->width != Tk_ReqWidth(scalePtr->tkwin)) ||
        (scalePtr->height != Tk_ReqHeight(scalePtr->tkwin))) {
        Tk_GeometryRequest(scalePtr->tkwin, scalePtr->width, scalePtr->height);
    }
}    

/*
 *---------------------------------------------------------------------------
 *
 * MapVerticalLayout --
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
MapVerticalLayout(Scale *scalePtr) 
{
    int cavityWidth, cavityHeigth;
    int w;
    TickLabel *minLabelPtr, *maxLabelPtr;
    int topPad, botPad;

    cavityWidth = Tk_Width(scalePtr->tkwin);
    cavityHeight = Tk_Height(scalePtr->tkwin);
    if (scalePtr->reqWidth > 0) {
        cavityWidth = scalePtr->reqWidth;
    }
    cavityWidth -= 2 * scalePtr->inset;
    cavityHeight -= 2 * scalePtr->inset;
    
    if (scalePtr->showFlags & SHOW_TITLE) {
        cavityHeight -= scalePtr->titleHeight;
    }
    if (scalePtr->flags & DECREASING) {
        minLabelPtr = Blt_Chain_LastLink(scalePtr->chain);
        maxLabelPtr = Blt_Chain_FirstLink(scalePtr->chain);
    } else {
        maxLabelPtr = Blt_Chain_LastLink(scalePtr->chain);
        minLabelPtr = Blt_Chain_FirstLink(scalePtr->chain);
    }
    topPad = MAX(minLabelPtr->height / 2, scalePtr->arrowHeight / 2);
    botPad = MAX(maxLabelPtr->height / 2, scalePtr->arrowHeight / 2);

    scalePtr->y1 = scalePtr->inset + topPad;    
    scalePtr->y2 = Tk_Height(scalePtr->tkwin) - (scalePtr->inset + botPad);

    info.axisLength = y2 - y1;

    scalePtr->screenOffset = scalePtr->y1;
    scalePtr->screenRange = scalePtr->y2 - scalePtr->y1;

    w = 0;
    w += PADY;
    if (scalePtr->showFlags & SHOW_COLORBAR) {
        w += scalePtr->colorbarHeight;
        info.colorbar = scalePtr->colorbarHeight;
    }
    w += scalePtr->tickLineWidth + 2;
    if (scalePtr->showFlags & SHOW_TICKS) {
        w += scalePtr->tickLength;
        info.t1 = scalePtr->tickLength;
        if (scalePtr->tickLineWidth > 0) {
            info.t1 = scalePtr->tickLength;
            info.t2 = (info.t1 * 10) / 15;
        }
        info.labelOffset = info.t1 + AXIS_PAD_TITLE;
        if (scalePtr->flags & EXTERIOR) {
            info.labelOffset += scalePtr->tickLineWidth;
        }
    }
    if (scalePtr->showFlags & SHOW_TICKLABELS) {
        w += scalePtr->tickHeight;
    }
    w += PADY;
    scalePtr->width = w;
    scalePtr->height = cavityWidth;
    if (scalePtr->reqHeight > 0) {
        scalePtr->height = scalePtr->reqHeight;
    }
    if ((scalePtr->width != Tk_ReqWidth(scalePtr->tkwin)) ||
        (scalePtr->height != Tk_ReqHeight(scalePtr->tkwin))) {
        Tk_GeometryRequest(scalePtr->tkwin, scalePtr->width, scalePtr->height);
    }
}    

static void
MakeSegments(Scale *scalePtr, AxisInfo *infoPtr)
{
    int arraySize;
    int numMajorTicks, numMinorTicks;
    Segment2d *segments;
    Segment2d *s;

    if (scalePtr->segments != NULL) {
        Blt_Free(scalePtr->segments);
    }
    numMajorTicks = scalePtr->major.ticks.numSteps;
    numMinorTicks = scalePtr->minor.ticks.numSteps;
    arraySize = 1 + numMajorTicks + (numMajorTicks * (numMinorTicks + 1));
    segments = Blt_AssertMalloc(arraySize * sizeof(Segment2d));
    s = segments;
    if (scalePtr->tickLineWidth > 0) {
        /* Axis baseline */
        MakeAxisLine(scalePtr, infoPtr->scaleLength, s);
        s++;
    }
    if (scalePtr->flags & TICKLABELS) {
        Blt_ChainLink link;
        double labelPos;
        Tick left, right;

        link = Blt_Chain_FirstLink(scalePtr->tickLabels);
        labelPos = (double)infoPtr->label;
        for (left = FirstMajorTick(scalePtr); left.isValid; left = right) {
            right = NextMajorTick(scalePtr);
            if (right.isValid) {
                Tick minor;

                /* If this isn't the last major tick, add minor ticks. */
                scalePtr->minor.ticks.range = right.value - left.value;
                scalePtr->minor.ticks.initial = left.value;
                for (minor = FirstMinorTick(scalePtr); minor.isValid; 
                     minor = NextMinorTick(scalePtr)) {
                    if (InRange(minor.value, &scalePtr->tickRange)) {
                        /* Add minor tick. */
                        MakeTick(scalePtr, minor.value, infoPtr->t2, 
                                infoPtr->scaleLength, s);
                        s++;
                    }
                }        
            }
            if (InRange(left.value, &scalePtr->tickRange)) {
                double mid;

                /* Add major tick. This could be the last major tick. */
                MakeTick(scalePtr, left.value, infoPtr->t1,
                        infoPtr->scaleLength, s);
                mid = left.value;
                if ((scalePtr->flags & LABELOFFSET) && (right.isValid)) {
                    mid = (right.value - left.value) * 0.5;
                }
                if (InRange(mid, &scalePtr->tickRange)) {
                    TickLabel *labelPtr;

                    labelPtr = Blt_Chain_GetValue(link);
                    link = Blt_Chain_NextLink(link);
                    
                    /* Set the position of the tick label. */
                    if (HORIZONTAL(scalePtr->marginPtr)) {
                        labelPtr->anchorPos.x = s->p.x;
                        labelPtr->anchorPos.y = labelPos;
                    } else {
                        labelPtr->anchorPos.x = labelPos;
                        labelPtr->anchorPos.y = s->p.y;
                    }
                }
                s++;
            }
        }
    }
    scalePtr->segments = segments;
    scalePtr->numSegments = s - segments;
#ifdef notdef
    fprintf(stderr,
            "scale=%s numSegments=%d, arraySize=%d numMajor=%d numMinor=%d\n",
            scalePtr->obj.name, scalePtr->numSegments, arraySize,
            numMajorTicks, numMinorTicks);
#endif
    assert(scalePtr->numSegments <= arraySize);
}



/*
 *---------------------------------------------------------------------------
 *
 * AxisOffsets --
 *
 *      Determines the sides of the axis, major and minor ticks, and title
 *      of the axis.
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
static void
AxisOffsets(Axis *scalePtr, AxisInfo *infoPtr)
{
    int pad;                            /* Offset of axis from interior
                                         * region. This includes a possible
                                         * border and the axis line
                                         * width. */
    int axisLine;
    int t1, t2, labelOffset;
    int tickLabel, axisPad;
    int inset, mark;
    int x, y;
    float fangle;

    tickLabel = axisLine = t1 = t2 = 0;
    labelOffset = AXIS_PAD_TITLE;
    if (scalePtr->tickLineWidth > 0) {
        if (scalePtr->flags & SHOW_TICKS) {
            t1 = scalePtr->tickLength;
            t2 = (t1 * 10) / 15;
        }
        labelOffset = t1 + AXIS_PAD_TITLE;
        if (scalePtr->flags & EXTERIOR) {
            labelOffset += scalePtr->tickLineWidth;
        }
    }
    axisPad = 0;
    /* Adjust offset for the interior border width and the line width */
    pad = 0;                            /* FIXME: test */
    /*
     * Pre-calculate the x-coordinate positions of the axis, tick labels,
     * and the individual major and minor ticks.
     */
    inset = pad + scalePtr->tickLineWidth / 2;
    if (HORIZONTAL(scalePtr)) {
        /*
         *  ----------- bottom + plot borderwidth
         *      mark --------------------------------------------
         *          ===================== axisLine (linewidth)
         *                   tick
         *                  title
         *
         *          ===================== axisLine (linewidth)
         *  ----------- bottom + plot borderwidth
         *      mark --------------------------------------------
         *                   tick
         *                  title
         */
        axisLine = scalePtr->y2;
        if (scalePtr->colorbar.thickness > 0) {
            axisLine += scalePtr->colorbar.thickness + COLORBAR_PAD;
        }
        if (scalePtr->flags & EXTERIOR) {
            axisLine += axisPad + scalePtr->tickLineWidth / 2;
            axisLine += 2;
            tickLabel = axisLine + 2;
            if (scalePtr->tickLineWidth > 0) {
                tickLabel += scalePtr->tickLength;
            }
        } else {
            axisLine -= axisPad + scalePtr->tickLineWidth / 2;
            tickLabel = scalePtr->y2 + 2;
            axisLine--;
        }
        fangle = FMOD(scalePtr->tickAngle, 90.0f);
        if (fangle == 0.0) {
            scalePtr->tickAnchor = TK_ANCHOR_N;
        } else {
            int quadrant;

            quadrant = (int)(scalePtr->tickAngle / 90.0);
            if ((quadrant == 0) || (quadrant == 2)) {
                scalePtr->tickAnchor = TK_ANCHOR_NE;
            } else {
                scalePtr->tickAnchor = TK_ANCHOR_NW;
            }
        }
        scalePtr->left = scalePtr->screenMin - inset - 2;
        scalePtr->right = scalePtr->screenMin + scalePtr->screenRange + inset-1;
        scalePtr->top = scalePtr->y2 + labelOffset - t1;
        mark = scalePtr->y2;
        scalePtr->bottom = mark + scalePtr->height - 1;
        x = (scalePtr->right + scalePtr->left) / 2;
        y = mark + scalePtr->height - AXIS_PAD_TITLE;
        scalePtr->titleAnchor = TK_ANCHOR_S; 
        scalePtr->titlePos.x = x;
        scalePtr->titlePos.y = y;
        infoPtr->colorbar = axisLine - scalePtr->colorbar.thickness;
    } else {
        axisLine = scalePtr->x2;
        if (scalePtr->colorbar.thickness > 0) {
            axisLine += scalePtr->colorbar.thickness + COLORBAR_PAD;
        }
        if (scalePtr->flags & EXTERIOR) {
            axisLine += axisPad + scalePtr->tickLineWidth / 2;
            tickLabel = axisLine + 2;
            if (scalePtr->tickLineWidth > 0) {
                tickLabel += scalePtr->tickLength;
            }
            axisLine++;
        } else {
            axisLine -= axisPad + scalePtr->tickLineWidth / 2;
            tickLabel = axisLine + 2;
        }
        mark = scalePtr->x2 + pad;
        scalePtr->tickAnchor = TK_ANCHOR_W;
        scalePtr->left = mark;
        scalePtr->right = mark + scalePtr->width - 1;
        scalePtr->top = scalePtr->screenMin - inset - 2;
        scalePtr->bottom = scalePtr->screenMin + scalePtr->screenRange + inset-1;
        x = mark + scalePtr->width - AXIS_PAD_TITLE;
        y = (scalePtr->bottom + scalePtr->top) / 2;
        scalePtr->titleAnchor = TK_ANCHOR_E;
        scalePtr->titlePos.x = x;
        scalePtr->titlePos.y = y;
        infoPtr->colorbar = axisLine - scalePtr->colorbar.thickness;
    }
    infoPtr->axisLength = axisLine;
    infoPtr->t1 = axisLine + t1;
    infoPtr->t2 = axisLine + t2;
    if (tickLabel > 0) {
        infoPtr->label = tickLabel;
    } else {
        infoPtr->label = axisLine + labelOffset;
    }
    if ((scalePtr->flags & EXTERIOR) == 0) {
        /*infoPtr->label = axisLine + labelOffset - t1; */
        infoPtr->t1 = axisLine - t1;
        infoPtr->t2 = axisLine - t2;
    } 
}
