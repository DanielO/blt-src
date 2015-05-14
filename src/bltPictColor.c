/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * bltPictColor.c --
 *
 * This module implements a color name lookup for picture images.
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
 * The color search is taken from X11/xcolors.c in the Tk library.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 *
 *   This software is copyrighted by the Regents of the University of
 *   California, Sun Microsystems, Inc., and other parties.  The following
 *   terms apply to all files associated with the software unless
 *   explicitly disclaimed in individual files.
 * 
 *   The authors hereby grant permission to use, copy, modify, distribute,
 *   and license this software and its documentation for any purpose,
 *   provided that existing copyright notices are retained in all copies
 *   and that this notice is included verbatim in any distributions. No
 *   written agreement, license, or royalty fee is required for any of the
 *   authorized uses.  Modifications to this software may be copyrighted by
 *   their authors and need not follow the licensing terms described here,
 *   provided that the new terms are clearly indicated on the first page of
 *   each file where they apply.
 * 
 *   IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
 *   FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 *   ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
 *   DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 * 
 *   THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 *   NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, AND
 *   THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 *   MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 *   GOVERNMENT USE: If you are acquiring this software on behalf of the
 *   U.S. government, the Government shall have only "Restricted Rights" in
 *   the software and related documentation as defined in the Federal
 *   Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 *   are acquiring the software on behalf of the Department of Defense, the
 *   software shall be classified as "Commercial Computer Software" and the
 *   Government shall have only "Restricted Rights" as defined in Clause
 *   252.227-7013 (b) (3) of DFARs.  Notwithstanding the foregoing, the
 *   authors grant the U.S. Government and others acting in its behalf
 *   permission to use and distribute the software in accordance with the
 *   terms specified in this license.

 */

#define BUILD_BLT_TK_PROCS 1
#include "bltInt.h"
#ifdef HAVE_CTYPE_H
#  include <ctype.h>
#endif /* HAVE_CTYPE_H */
#include <stdlib.h>
#include "bltString.h"
#include "bltPicture.h"
#include "bltSwitch.h"

/*
 * Define an array that defines the mapping from color names to RGB values.
 * Note that this array must be kept sorted alphabetically we are using a 
 * binary search.
 */
typedef struct {
    const char *name;
    unsigned char red, green, blue;
} ColorEntry;

static ColorEntry colorTable[] = {
     { "alice blue",            240, 248, 255 },
     { "aliceblue",             240, 248, 255 },
     { "antique white",         250, 235, 215 },
     { "antiquewhite",          250, 235, 215 },
     { "antiquewhite1",         255, 239, 219 },
     { "antiquewhite2",         238, 223, 204 },
     { "antiquewhite3",         205, 192, 176 },
     { "antiquewhite4",         139, 131, 120 },
     { "aquamarine",            127, 255, 212 },
     { "aquamarine1",           127, 255, 212 },
     { "aquamarine2",           118, 238, 198 },
     { "aquamarine3",           102, 205, 170 },
     { "aquamarine4",            69, 139, 116 },
     { "azure",                 240, 255, 255 },
     { "azure1",                240, 255, 255 },
     { "azure2",                224, 238, 238 },
     { "azure3",                193, 205, 205 },
     { "azure4",                131, 139, 139 },
     { "beige",                 245, 245, 220 },
     { "bisque",                255, 228, 196 },
     { "bisque1",               255, 228, 196 },
     { "bisque2",               238, 213, 183 },
     { "bisque3",               205, 183, 158 },
     { "bisque4",               139, 125, 107 },
     { "black",                   0,   0,   0 },
     { "blanched almond",       255, 235, 205 },
     { "blanchedalmond",        255, 235, 205 },
     { "blue violet",           138,  43, 226 },
     { "blue",                    0,   0, 255 },
     { "blue1",                   0,   0, 255 },
     { "blue2",                   0,   0, 238 },
     { "blue3",                   0,   0, 205 },
     { "blue4",                   0,   0, 139 },
     { "blueviolet",            138,  43, 226 },
     { "brown",                 165,  42,  42 },
     { "brown1",                255,  64,  64 },
     { "brown2",                238,  59,  59 },
     { "brown3",                205,  51,  51 },
     { "brown4",                139,  35,  35 },
     { "burlywood",             222, 184, 135 },
     { "burlywood1",            255, 211, 155 },
     { "burlywood2",            238, 197, 145 },
     { "burlywood3",            205, 170, 125 },
     { "burlywood4",            139, 115,  85 },
     { "cadet blue",             95, 158, 160 },
     { "cadetblue",              95, 158, 160 },
     { "cadetblue1",            152, 245, 255 },
     { "cadetblue2",            142, 229, 238 },
     { "cadetblue3",            122, 197, 205 },
     { "cadetblue4",             83, 134, 139 },
     { "chartreuse",            127, 255,   0 },
     { "chartreuse1",           127, 255,   0 },
     { "chartreuse2",           118, 238,   0 },
     { "chartreuse3",           102, 205,   0 },
     { "chartreuse4",            69, 139,   0 },
     { "chocolate",             210, 105,  30 },
     { "chocolate1",            255, 127,  36 },
     { "chocolate2",            238, 118,  33 },
     { "chocolate3",            205, 102,  29 },
     { "chocolate4",            139, 69,   19 },
     { "coral",                 255, 127,  80 },
     { "coral1",                255, 114,  86 },
     { "coral2",                238, 106,  80 },
     { "coral3",                205,  91,  69 },
     { "coral4",                139,  62,  47 },
     { "cornflower blue",       100, 149, 237 },
     { "cornflowerblue",        100, 149, 237 },
     { "cornsilk",              255, 248, 220 },
     { "cornsilk1",             255, 248, 220 },
     { "cornsilk2",             238, 232, 205 },
     { "cornsilk3",             205, 200, 177 },
     { "cornsilk4",             139, 136, 120 },
     { "cyan",                    0, 255, 255 },
     { "cyan1",                   0, 255, 255 },
     { "cyan2",                   0, 238, 238 },
     { "cyan3",                   0, 205, 205 },
     { "cyan4",                   0, 139, 139 },
     { "dark blue",               0,   0, 139 },
     { "dark cyan",               0, 139, 139 },
     { "dark goldenrod",        184, 134,  11 },
     { "dark gray",             169, 169, 169 },
     { "dark green",              0, 100,   0 },
     { "dark grey",             169, 169, 169 },
     { "dark khaki",            189, 183, 107 },
     { "dark magenta",          139,   0, 139 },
     { "dark olive green",       85, 107,  47 },
     { "dark orange",           255, 140,   0 },
     { "dark orchid",           153,  50, 204 },
     { "dark red",              139,   0,   0 },
     { "dark salmon",           233, 150, 122 },
     { "dark sea green",        143, 188, 143 },
     { "dark slate blue",        72,  61, 139 },
     { "dark slate gray",        47,  79,  79 },
     { "dark slate grey",        47,  79,  79 },
     { "dark turquoise",          0, 206, 209 },
     { "dark violet",           148,   0, 211 },
     { "darkblue",                0,   0, 139 },
     { "darkcyan",                0, 139, 139 },
     { "darkgoldenrod",         184, 134,  11 },
     { "darkgoldenrod1",        255, 185,  15 },
     { "darkgoldenrod2",        238, 173,  14 },
     { "darkgoldenrod3",        205, 149,  12 },
     { "darkgoldenrod4",        139, 101,   8 },
     { "darkgray",              169, 169, 169 },
     { "darkgreen",               0, 100,   0 },
     { "darkgrey",              169, 169, 169 },
     { "darkkhaki",             189, 183, 107 },
     { "darkmagenta",           139,   0, 139 },
     { "darkolivegreen",         85, 107,  47 },
     { "darkolivegreen1",       202, 255, 112 },
     { "darkolivegreen2",       188, 238, 104 },
     { "darkolivegreen3",       162, 205,  90 },
     { "darkolivegreen4",       110, 139,  61 },
     { "darkorange",            255, 140,   0 },
     { "darkorange1",           255, 127,   0 },
     { "darkorange2",           238, 118,   0 },
     { "darkorange3",           205, 102,   0 },
     { "darkorange4",           139,  69,   0 },
     { "darkorchid",            153,  50, 204 },
     { "darkorchid1",           191,  62, 255 },
     { "darkorchid2",           178,  58, 238 },
     { "darkorchid3",           154,  50, 205 },
     { "darkorchid4",           104,  34, 139 },
     { "darkred",               139,   0,   0 },
     { "darksalmon",            233, 150, 122 },
     { "darkseagreen",          143, 188, 143 },
     { "darkseagreen1",         193, 255, 193 },
     { "darkseagreen2",         180, 238, 180 },
     { "darkseagreen3",         155, 205, 155 },
     { "darkseagreen4",         105, 139, 105 },
     { "darkslateblue",          72,  61, 139 },
     { "darkslategray",          47,  79,  79 },
     { "darkslategray1",        151, 255, 255 },
     { "darkslategray2",        141, 238, 238 },
     { "darkslategray3",        121, 205, 205 },
     { "darkslategray4",         82, 139, 139 },
     { "darkslategrey",          47,  79,  79 },
     { "darkturquoise",           0, 206, 209 },
     { "darkviolet",            148,   0, 211 },
     { "deep pink",             255,  20, 147 },
     { "deep sky blue",           0, 191, 255 },
     { "deeppink",              255,  20, 147 },
     { "deeppink1",             255,  20, 147 },
     { "deeppink2",             238,  18, 137 },
     { "deeppink3",             205,  16, 118 },
     { "deeppink4",             139,  10,  80 },
     { "deepskyblue",             0, 191, 255 },
     { "deepskyblue1",            0, 191, 255 },
     { "deepskyblue2",            0, 178, 238 },
     { "deepskyblue3",            0, 154, 205 },
     { "deepskyblue4",            0, 104, 139 },
     { "dim gray",              105, 105, 105 },
     { "dim grey",              105, 105, 105 },
     { "dimgray",               105, 105, 105 },
     { "dimgrey",               105, 105, 105 },
     { "dodger blue",            30, 144, 255 },
     { "dodgerblue",             30, 144, 255 },
     { "dodgerblue1",            30, 144, 255 },
     { "dodgerblue2",            28, 134, 238 },
     { "dodgerblue3",            24, 116, 205 },
     { "dodgerblue4",            16,  78, 139 },
     { "firebrick",             178,  34,  34 },
     { "firebrick1",            255,  48,  48 },
     { "firebrick2",            238,  44,  44 },
     { "firebrick3",            205,  38,  38 },
     { "firebrick4",            139,  26,  26 },
     { "floral white",          255, 250, 240 },
     { "floralwhite",           255, 250, 240 },
     { "forest green",           34, 139,  34 },
     { "forestgreen",            34, 139,  34 },
     { "gainsboro",             220, 220, 220 },
     { "ghost white",           248, 248, 255 },
     { "ghostwhite",            248, 248, 255 },
     { "gold",                  255, 215,   0 },
     { "gold1",                 255, 215,   0 },
     { "gold2",                 238, 201,   0 },
     { "gold3",                 205, 173,   0 },
     { "gold4",                 139, 117,   0 },
     { "goldenrod",             218, 165,  32 },
     { "goldenrod1",            255, 193,  37 },
     { "goldenrod2",            238, 180,  34 },
     { "goldenrod3",            205, 155,  29 },
     { "goldenrod4",            139, 105,  20 },
     { "gray",                  190, 190, 190 },
     { "gray0",                   0,   0,   0 },
     { "gray1",                   3,   3,   3 },
     { "gray10",                 26,  26,  26 },
     { "gray100",               255, 255, 255 },
     { "gray11",                 28,  28,  28 },
     { "gray12",                 31,  31,  31 },
     { "gray13",                 33,  33,  33 },
     { "gray14",                 36,  36,  36 },
     { "gray15",                 38,  38,  38 },
     { "gray16",                 41,  41,  41 },
     { "gray17",                 43,  43,  43 },
     { "gray18",                 46,  46,  46 },
     { "gray19",                 48,  48,  48 },
     { "gray2",                   5,   5,   5 },
     { "gray20",                 51,  51,  51 },
     { "gray21",                 54,  54,  54 },
     { "gray22",                 56,  56,  56 },
     { "gray23",                 59,  59,  59 },
     { "gray24",                 61,  61,  61 },
     { "gray25",                 64,  64,  64 },
     { "gray26",                 66,  66,  66 },
     { "gray27",                 69,  69,  69 },
     { "gray28",                 71,  71,  71 },
     { "gray29",                 74,  74,  74 },
     { "gray3",                   8,   8,   8 },
     { "gray30",                 77,  77,  77 },
     { "gray31",                 79,  79,  79 },
     { "gray32",                 82,  82,  82 },
     { "gray33",                 84,  84,  84 },
     { "gray34",                 87,  87,  87 },
     { "gray35",                 89,  89,  89 },
     { "gray36",                 92,  92,  92 },
     { "gray37",                 94,  94,  94 },
     { "gray38",                 97,  97,  97 },
     { "gray39",                 99,  99,  99 },
     { "gray4",                  10,  10,  10 },
     { "gray40",                102, 102, 102 },
     { "gray41",                105, 105, 105 },
     { "gray42",                107, 107, 107 },
     { "gray43",                110, 110, 110 },
     { "gray44",                112, 112, 112 },
     { "gray45",                115, 115, 115 },
     { "gray46",                117, 117, 117 },
     { "gray47",                120, 120, 120 },
     { "gray48",                122, 122, 122 },
     { "gray49",                125, 125, 125 },
     { "gray5",                  13,  13,  13 },
     { "gray50",                127, 127, 127 },
     { "gray51",                130, 130, 130 },
     { "gray52",                133, 133, 133 },
     { "gray53",                135, 135, 135 },
     { "gray54",                138, 138, 138 },
     { "gray55",                140, 140, 140 },
     { "gray56",                143, 143, 143 },
     { "gray57",                145, 145, 145 },
     { "gray58",                148, 148, 148 },
     { "gray59",                150, 150, 150 },
     { "gray6",                  15,  15,  15 },
     { "gray60",                153, 153, 153 },
     { "gray61",                156, 156, 156 },
     { "gray62",                158, 158, 158 },
     { "gray63",                161, 161, 161 },
     { "gray64",                163, 163, 163 },
     { "gray65",                166, 166, 166 },
     { "gray66",                168, 168, 168 },
     { "gray67",                171, 171, 171 },
     { "gray68",                173, 173, 173 },
     { "gray69",                176, 176, 176 },
     { "gray7",                  18,  18,  18 },
     { "gray70",                179, 179, 179 },
     { "gray71",                181, 181, 181 },
     { "gray72",                184, 184, 184 },
     { "gray73",                186, 186, 186 },
     { "gray74",                189, 189, 189 },
     { "gray75",                191, 191, 191 },
     { "gray76",                194, 194, 194 },
     { "gray77",                196, 196, 196 },
     { "gray78",                199, 199, 199 },
     { "gray79",                201, 201, 201 },
     { "gray8",                  20,  20,  20 },
     { "gray80",                204, 204, 204 },
     { "gray81",                207, 207, 207 },
     { "gray82",                209, 209, 209 },
     { "gray83",                212, 212, 212 },
     { "gray84",                214, 214, 214 },
     { "gray85",                217, 217, 217 },
     { "gray86",                219, 219, 219 },
     { "gray87",                222, 222, 222 },
     { "gray88",                224, 224, 224 },
     { "gray89",                227, 227, 227 },
     { "gray9",                  23,  23,  23 },
     { "gray90",                229, 229, 229 },
     { "gray91",                232, 232, 232 },
     { "gray92",                235, 235, 235 },
     { "gray93",                237, 237, 237 },
     { "gray94",                240, 240, 240 },
     { "gray95",                242, 242, 242 },
     { "gray96",                245, 245, 245 },
     { "gray97",                247, 247, 247 },
     { "gray98",                250, 250, 250 },
     { "gray99",                252, 252, 252 },
     { "green yellow",          173, 255,  47 },
     { "green",                   0, 255,   0 },
     { "green1",                  0, 255,   0 },
     { "green2",                  0, 238,   0 },
     { "green3",                  0, 205,   0 },
     { "green4",                  0, 139,   0 },
     { "greenyellow",           173, 255,  47 },
     { "grey",                  190, 190, 190 },
     { "grey0",                   0,   0,   0 },
     { "grey1",                   3,   3,   3 },
     { "grey10",                 26,  26,  26 },
     { "grey100",               255, 255, 255 },
     { "grey11",                 28,  28,  28 },
     { "grey12",                 31,  31,  31 },
     { "grey13",                 33,  33,  33 },
     { "grey14",                 36,  36,  36 },
     { "grey15",                 38,  38,  38 },
     { "grey16",                 41,  41,  41 },
     { "grey17",                 43,  43,  43 },
     { "grey18",                 46,  46,  46 },
     { "grey19",                 48,  48,  48 },
     { "grey2",                   5,   5,   5 },
     { "grey20",                 51,  51,  51 },
     { "grey21",                 54,  54,  54 },
     { "grey22",                 56,  56,  56 },
     { "grey23",                 59,  59,  59 },
     { "grey24",                 61,  61,  61 },
     { "grey25",                 64,  64,  64 },
     { "grey26",                 66,  66,  66 },
     { "grey27",                 69,  69,  69 },
     { "grey28",                 71,  71,  71 },
     { "grey29",                 74,  74,  74 },
     { "grey3",                   8,   8,   8 },
     { "grey30",                 77,  77,  77 },
     { "grey31",                 79,  79,  79 },
     { "grey32",                 82,  82,  82 },
     { "grey33",                 84,  84,  84 },
     { "grey34",                 87,  87,  87 },
     { "grey35",                 89,  89,  89 },
     { "grey36",                 92,  92,  92 },
     { "grey37",                 94,  94,  94 },
     { "grey38",                 97,  97,  97 },
     { "grey39",                 99,  99,  99 },
     { "grey4",                  10,  10,  10 },
     { "grey40",                102, 102, 102 },
     { "grey41",                105, 105, 105 },
     { "grey42",                107, 107, 107 },
     { "grey43",                110, 110, 110 },
     { "grey44",                112, 112, 112 },
     { "grey45",                115, 115, 115 },
     { "grey46",                117, 117, 117 },
     { "grey47",                120, 120, 120 },
     { "grey48",                122, 122, 122 },
     { "grey49",                125, 125, 125 },
     { "grey5",                  13,  13,  13 },
     { "grey50",                127, 127, 127 },
     { "grey51",                130, 130, 130 },
     { "grey52",                133, 133, 133 },
     { "grey53",                135, 135, 135 },
     { "grey54",                138, 138, 138 },
     { "grey55",                140, 140, 140 },
     { "grey56",                143, 143, 143 },
     { "grey57",                145, 145, 145 },
     { "grey58",                148, 148, 148 },
     { "grey59",                150, 150, 150 },
     { "grey6",                  15,  15,  15 },
     { "grey60",                153, 153, 153 },
     { "grey61",                156, 156, 156 },
     { "grey62",                158, 158, 158 },
     { "grey63",                161, 161, 161 },
     { "grey64",                163, 163, 163 },
     { "grey65",                166, 166, 166 },
     { "grey66",                168, 168, 168 },
     { "grey67",                171, 171, 171 },
     { "grey68",                173, 173, 173 },
     { "grey69",                176, 176, 176 },
     { "grey7",                  18,  18,  18 },
     { "grey70",                179, 179, 179 },
     { "grey71",                181, 181, 181 },
     { "grey72",                184, 184, 184 },
     { "grey73",                186, 186, 186 },
     { "grey74",                189, 189, 189 },
     { "grey75",                191, 191, 191 },
     { "grey76",                194, 194, 194 },
     { "grey77",                196, 196, 196 },
     { "grey78",                199, 199, 199 },
     { "grey79",                201, 201, 201 },
     { "grey8",                  20,  20,  20 },
     { "grey80",                204, 204, 204 },
     { "grey81",                207, 207, 207 },
     { "grey82",                209, 209, 209 },
     { "grey83",                212, 212, 212 },
     { "grey84",                214, 214, 214 },
     { "grey85",                217, 217, 217 },
     { "grey86",                219, 219, 219 },
     { "grey87",                222, 222, 222 },
     { "grey88",                224, 224, 224 },
     { "grey89",                227, 227, 227 },
     { "grey9",                  23,  23,  23 },
     { "grey90",                229, 229, 229 },
     { "grey91",                232, 232, 232 },
     { "grey92",                235, 235, 235 },
     { "grey93",                237, 237, 237 },
     { "grey94",                240, 240, 240 },
     { "grey95",                242, 242, 242 },
     { "grey96",                245, 245, 245 },
     { "grey97",                247, 247, 247 },
     { "grey98",                250, 250, 250 },
     { "grey99",                252, 252, 252 },
     { "honeydew",              240, 255, 240 },
     { "honeydew1",             240, 255, 240 },
     { "honeydew2",             224, 238, 224 },
     { "honeydew3",             193, 205, 193 },
     { "honeydew4",             131, 139, 131 },
     { "hot pink",              255, 105, 180 },
     { "hotpink",               255, 105, 180 },
     { "hotpink1",              255, 110, 180 },
     { "hotpink2",              238, 106, 167 },
     { "hotpink3",              205,  96, 144 },
     { "hotpink4",              139,  58,  98 },
     { "indian red",            205,  92,  92 },
     { "indianred",             205,  92,  92 },
     { "indianred1",            255, 106, 106 },
     { "indianred2",            238,  99,  99 },
     { "indianred3",            205,  85,  85 },
     { "indianred4",            139,  58,  58 },
     { "ivory",                 255, 255, 240 },
     { "ivory1",                255, 255, 240 },
     { "ivory2",                238, 238, 224 },
     { "ivory3",                205, 205, 193 },
     { "ivory4",                139, 139, 131 },
     { "khaki",                 240, 230, 140 },
     { "khaki1",                255, 246, 143 },
     { "khaki2",                238, 230, 133 },
     { "khaki3",                205, 198, 115 },
     { "khaki4",                139, 134,  78 },
     { "lavender blush",        255, 240, 245 },
     { "lavender",              230, 230, 250 },
     { "lavenderblush",         255, 240, 245 },
     { "lavenderblush1",        255, 240, 245 },
     { "lavenderblush2",        238, 224, 229 },
     { "lavenderblush3",        205, 193, 197 },
     { "lavenderblush4",        139, 131, 134 },
     { "lawn green",            124, 252,   0 },
     { "lawngreen",             124, 252,   0 },
     { "lemon chiffon",         255, 250, 205 },
     { "lemonchiffon",          255, 250, 205 },
     { "lemonchiffon1",         255, 250, 205 },
     { "lemonchiffon2",         238, 233, 191 },
     { "lemonchiffon3",         205, 201, 165 },
     { "lemonchiffon4",         139, 137, 112 },
     { "light blue",            173, 216, 230 },
     { "light coral",           240, 128, 128 },
     { "light cyan",            224, 255, 255 },
     { "light goldenrod yellow", 250, 250, 210 },
     { "light goldenrod",       238, 221, 130 },
     { "light gray",            211, 211, 211 },
     { "light green",           144, 238, 144 },
     { "light grey",            211, 211, 211 },
     { "light pink",            255, 182, 193 },
     { "light salmon",          255, 160, 122 },
     { "light sea green",        32, 178, 170 },
     { "light sky blue",        135, 206, 250 },
     { "light slate blue",      132, 112, 255 },
     { "light slate gray",      119, 136, 153 },
     { "light slate grey",      119, 136, 153 },
     { "light steel blue",      176, 196, 222 },
     { "light yellow",          255, 255, 224 },
     { "lightblue",             173, 216, 230 },
     { "lightblue1",            191, 239, 255 },
     { "lightblue2",            178, 223, 238 },
     { "lightblue3",            154, 192, 205 },
     { "lightblue4",            104, 131, 139 },
     { "lightcoral",            240, 128, 128 },
     { "lightcyan",             224, 255, 255 },
     { "lightcyan1",            224, 255, 255 },
     { "lightcyan2",            209, 238, 238 },
     { "lightcyan3",            180, 205, 205 },
     { "lightcyan4",            122, 139, 139 },
     { "lightgoldenrod",        238, 221, 130 },
     { "lightgoldenrod1",       255, 236, 139 },
     { "lightgoldenrod2",       238, 220, 130 },
     { "lightgoldenrod3",       205, 190, 112 },
     { "lightgoldenrod4",       139, 129,  76 },
     { "lightgoldenrodyellow",  250, 250, 210 },
     { "lightgray",             211, 211, 211 },
     { "lightgreen",            144, 238, 144 },
     { "lightgrey",             211, 211, 211 },
     { "lightpink",             255, 182, 193 },
     { "lightpink1",            255, 174, 185 },
     { "lightpink2",            238, 162, 173 },
     { "lightpink3",            205, 140, 149 },
     { "lightpink4",            139,  95, 101 },
     { "lightsalmon",           255, 160, 122 },
     { "lightsalmon1",          255, 160, 122 },
     { "lightsalmon2",          238, 149, 114 },
     { "lightsalmon3",          205, 129,  98 },
     { "lightsalmon4",          139,  87,  66 },
     { "lightseagreen",          32, 178, 170 },
     { "lightskyblue",          135, 206, 250 },
     { "lightskyblue1",         176, 226, 255 },
     { "lightskyblue2",         164, 211, 238 },
     { "lightskyblue3",         141, 182, 205 },
     { "lightskyblue4",          96, 123, 139 },
     { "lightslateblue",        132, 112, 255 },
     { "lightslategray",        119, 136, 153 },
     { "lightslategrey",        119, 136, 153 },
     { "lightsteelblue",        176, 196, 222 },
     { "lightsteelblue1",       202, 225, 255 },
     { "lightsteelblue2",       188, 210, 238 },
     { "lightsteelblue3",       162, 181, 205 },
     { "lightsteelblue4",       110, 123, 139 },
     { "lightyellow",           255, 255, 224 },
     { "lightyellow1",          255, 255, 224 },
     { "lightyellow2",          238, 238, 209 },
     { "lightyellow3",          205, 205, 180 },
     { "lightyellow4",          139, 139, 122 },
     { "lime green",             50, 205,  50 },
     { "limegreen",              50, 205,  50 },
     { "linen",                 250, 240, 230 },
     { "magenta",               255,   0, 255 },
     { "magenta1",              255,   0, 255 },
     { "magenta2",              238,   0, 238 },
     { "magenta3",              205,   0, 205 },
     { "magenta4",              139,   0, 139 },
     { "maroon",                176,  48,  96 },
     { "maroon1",               255,  52, 179 },
     { "maroon2",               238,  48, 167 },
     { "maroon3",               205,  41, 144 },
     { "maroon4",               139,  28,  98 },
     { "medium aquamarine",     102, 205, 170 },
     { "medium blue",             0,   0, 205 },
     { "medium orchid",         186,  85, 211 },
     { "medium purple",         147, 112, 219 },
     { "medium sea green",       60, 179, 113 },
     { "medium slate blue",     123, 104, 238 },
     { "medium spring green",     0, 250, 154 },
     { "medium turquoise",       72, 209, 204 },
     { "medium violet red",     199,  21, 133 },
     { "mediumaquamarine",      102, 205, 170 },
     { "mediumblue",              0,   0, 205 },
     { "mediumorchid",          186,  85, 211 },
     { "mediumorchid1",         224, 102, 255 },
     { "mediumorchid2",         209,  95, 238 },
     { "mediumorchid3",         180,  82, 205 },
     { "mediumorchid4",         122,  55, 139 },
     { "mediumpurple",          147, 112, 219 },
     { "mediumpurple1",         171, 130, 255 },
     { "mediumpurple2",         159, 121, 238 },
     { "mediumpurple3",         137, 104, 205 },
     { "mediumpurple4",          93,  71, 139 },
     { "mediumseagreen",         60, 179, 113 },
     { "mediumslateblue",       123, 104, 238 },
     { "mediumspringgreen",       0, 250, 154 },
     { "mediumturquoise",        72, 209, 204 },
     { "mediumvioletred",       199,  21, 133 },
     { "midnight blue",          25,  25, 112 },
     { "midnightblue",           25,  25, 112 },
     { "mint cream",            245, 255, 250 },
     { "mintcream",             245, 255, 250 },
     { "misty rose",            255, 228, 225 },
     { "mistyrose",             255, 228, 225 },
     { "mistyrose1",            255, 228, 225 },
     { "mistyrose2",            238, 213, 210 },
     { "mistyrose3",            205, 183, 181 },
     { "mistyrose4",            139, 125, 123 },
     { "moccasin",              255, 228, 181 },
     { "navajo white",          255, 222, 173 },
     { "navajowhite",           255, 222, 173 },
     { "navajowhite1",          255, 222, 173 },
     { "navajowhite2",          238, 207, 161 },
     { "navajowhite3",          205, 179, 139 },
     { "navajowhite4",          139, 121,  94 },
     { "navy blue",               0,   0, 128 },
     { "navy",                    0,   0, 128 },
     { "navyblue",                0,   0, 128 },
     { "old lace",              253, 245, 230 },
     { "oldlace",               253, 245, 230 },
     { "olive drab",            107, 142,  35 },
     { "olivedrab",             107, 142,  35 },
     { "olivedrab1",            192, 255,  62 },
     { "olivedrab2",            179, 238,  58 },
     { "olivedrab3",            154, 205,  50 },
     { "olivedrab4",            105, 139,  34 },
     { "orange",                255, 165,   0 },
     { "orange red",            255,  69,   0 },
     { "orange1",               255, 165,   0 },
     { "orange2",               238, 154,   0 },
     { "orange3",               205, 133,   0 },
     { "orange4",               139,  90,   0 },
     { "orangered",             255,  69,   0 },
     { "orangered1",            255,  69,   0 },
     { "orangered2",            238,  64,   0 },
     { "orangered3",            205,  55,   0 },
     { "orangered4",            139,  37,   0 },
     { "orchid",                218, 112, 214 },
     { "orchid1",               255, 131, 250 },
     { "orchid2",               238, 122, 233 },
     { "orchid3",               205, 105, 201 },
     { "orchid4",               139,  71, 137 },
     { "pale goldenrod",        238, 232, 170 },
     { "pale green",            152, 251, 152 },
     { "pale turquoise",        175, 238, 238 },
     { "pale violet red",       219, 112, 147 },
     { "palegoldenrod",         238, 232, 170 },
     { "palegreen",             152, 251, 152 },
     { "palegreen1",            154, 255, 154 },
     { "palegreen2",            144, 238, 144 },
     { "palegreen3",            124, 205, 124 },
     { "palegreen4",             84, 139,  84 },
     { "paleturquoise",         175, 238, 238 },
     { "paleturquoise1",        187, 255, 255 },
     { "paleturquoise2",        174, 238, 238 },
     { "paleturquoise3",        150, 205, 205 },
     { "paleturquoise4",        102, 139, 139 },
     { "palevioletred",         219, 112, 147 },
     { "palevioletred1",        255, 130, 171 },
     { "palevioletred2",        238, 121, 159 },
     { "palevioletred3",        205, 104, 137 },
     { "palevioletred4",        139,  71,  93 },
     { "papaya whip",           255, 239, 213 },
     { "papayawhip",            255, 239, 213 },
     { "peach puff",            255, 218, 185 },
     { "peachpuff",             255, 218, 185 },
     { "peachpuff1",            255, 218, 185 },
     { "peachpuff2",            238, 203, 173 },
     { "peachpuff3",            205, 175, 149 },
     { "peachpuff4",            139, 119, 101 },
     { "peru",                  205, 133,  63 },
     { "pink",                  255, 192, 203 },
     { "pink1",                 255, 181, 197 },
     { "pink2",                 238, 169, 184 },
     { "pink3",                 205, 145, 158 },
     { "pink4",                 139,  99, 108 },
     { "plum",                  221, 160, 221 },
     { "plum1",                 255, 187, 255 },
     { "plum2",                 238, 174, 238 },
     { "plum3",                 205, 150, 205 },
     { "plum4",                 139, 102, 139 },
     { "powder blue",           176, 224, 230 },
     { "powderblue",            176, 224, 230 },
     { "purple",                160,  32, 240 },
     { "purple1",               155,  48, 255 },
     { "purple2",               145,  44, 238 },
     { "purple3",               125,  38, 205 },
     { "purple4",                85,  26, 139 },
     { "red",                   255,   0,   0 },
     { "red1",                  255,   0,   0 },
     { "red2",                  238,   0,   0 },
     { "red3",                  205,   0,   0 },
     { "red4",                  139,   0,   0 },
     { "rosy brown",            188, 143, 143 },
     { "rosybrown",             188, 143, 143 },
     { "rosybrown1",            255, 193, 193 },
     { "rosybrown2",            238, 180, 180 },
     { "rosybrown3",            205, 155, 155 },
     { "rosybrown4",            139, 105, 105 },
     { "royal blue",             65, 105, 225 },
     { "royalblue",              65, 105, 225 },
     { "royalblue1",             72, 118, 255 },
     { "royalblue2",             67, 110, 238 },
     { "royalblue3",             58,  95, 205 },
     { "royalblue4",             39,  64, 139 },
     { "saddle brown",          139,  69,  19 },
     { "saddlebrown",           139,  69,  19 },
     { "salmon",                250, 128, 114 },
     { "salmon1",               255, 140, 105 },
     { "salmon2",               238, 130,  98 },
     { "salmon3",               205, 112,  84 },
     { "salmon4",               139,  76,  57 },
     { "sandy brown",           244, 164,  96 },
     { "sandybrown",            244, 164,  96 },
     { "sea green",              46, 139,  87 },
     { "seagreen",               46, 139,  87 },
     { "seagreen1",              84, 255, 159 },
     { "seagreen2",              78, 238, 148 },
     { "seagreen3",              67, 205, 128 },
     { "seagreen4",              46, 139,  87 },
     { "seashell",              255, 245, 238 },
     { "seashell1",             255, 245, 238 },
     { "seashell2",             238, 229, 222 },
     { "seashell3",             205, 197, 191 },
     { "seashell4",             139, 134, 130 },
     { "sienna",                160,  82,  45 },
     { "sienna1",               255, 130,  71 },
     { "sienna2",               238, 121,  66 },
     { "sienna3",               205, 104,  57 },
     { "sienna4",               139,  71,  38 },
     { "sky blue",              135, 206, 235 },
     { "skyblue",               135, 206, 235 },
     { "skyblue1",              135, 206, 255 },
     { "skyblue2",              126, 192, 238 },
     { "skyblue3",              108, 166, 205 },
     { "skyblue4",               74, 112, 139 },
     { "slate blue",            106,  90, 205 },
     { "slate gray",            112, 128, 144 },
     { "slate grey",            112, 128, 144 },
     { "slateblue",             106,  90, 205 },
     { "slateblue1",            131, 111, 255 },
     { "slateblue2",            122, 103, 238 },
     { "slateblue3",            105,  89, 205 },
     { "slateblue4",             71,  60, 139 },
     { "slategray",             112, 128, 144 },
     { "slategray1",            198, 226, 255 },
     { "slategray2",            185, 211, 238 },
     { "slategray3",            159, 182, 205 },
     { "slategray4",            108, 123, 139 },
     { "slategrey",             112, 128, 144 },
     { "snow",                  255, 250, 250 },
     { "snow1",                 255, 250, 250 },
     { "snow2",                 238, 233, 233 },
     { "snow3",                 205, 201, 201 },
     { "snow4",                 139, 137, 137 },
     { "spring green",            0, 255, 127 },
     { "springgreen",             0, 255, 127 },
     { "springgreen1",            0, 255, 127 },
     { "springgreen2",            0, 238, 118 },
     { "springgreen3",            0, 205, 102 },
     { "springgreen4",            0, 139,  69 },
     { "steel blue",             70, 130, 180 },
     { "steelblue",              70, 130, 180 },
     { "steelblue1",             99, 184, 255 },
     { "steelblue2",             92, 172, 238 },
     { "steelblue3",             79, 148, 205 },
     { "steelblue4",             54, 100, 139 },
     { "tan",                   210, 180, 140 },
     { "tan1",                  255, 165,  79 },
     { "tan2",                  238, 154,  73 },
     { "tan3",                  205, 133,  63 },
     { "tan4",                  139,  90,  43 },
     { "thistle",               216, 191, 216 },
     { "thistle1",              255, 225, 255 },
     { "thistle2",              238, 210, 238 },
     { "thistle3",              205, 181, 205 },
     { "thistle4",              139, 123, 139 },
     { "tomato",                255,  99,  71 },
     { "tomato1",               255,  99,  71 },
     { "tomato2",               238,  92,  66 },
     { "tomato3",               205,  79,  57 },
     { "tomato4",               139,  54,  38 },
     { "turquoise",              64, 224, 208 },
     { "turquoise1",              0, 245, 255 },
     { "turquoise2",              0, 229, 238 },
     { "turquoise3",              0, 197, 205 },
     { "turquoise4",              0, 134, 139 },
     { "violet red",            208,  32, 144 },
     { "violet",                238, 130, 238 },
     { "violetred",             208,  32, 144 },
     { "violetred1",            255,  62, 150 },
     { "violetred2",            238,  58, 140 },
     { "violetred3",            205,  50, 120 },
     { "violetred4",            139,  34,  82 },
     { "wheat",                 245, 222, 179 },
     { "wheat1",                255, 231, 186 },
     { "wheat2",                238, 216, 174 },
     { "wheat3",                205, 186, 150 },
     { "wheat4",                139, 126, 102 },
     { "white smoke",           245, 245, 245 },
     { "white",                 255, 255, 255 },
     { "whitesmoke",            245, 245, 245 },
     { "yellow green",          154, 205,  50 },
     { "yellow",                255, 255,   0 },
     { "yellow1",               255, 255,   0 },
     { "yellow2",               238, 238,   0 },
     { "yellow3",               205, 205,   0 },
     { "yellow4",               139, 139,   0 },
     { "yellowgreen",           154, 205,  50 },
};

static int numColorEntries = sizeof(colorTable) / sizeof(ColorEntry);

int
Blt_GetPixel(Tcl_Interp *interp, const char *string, Blt_Pixel *pixelPtr)
{
    if ((string[0] == '0') && (string[1] == 'x')) {
        unsigned int value;
        char *term;

        /* Hexidecimal number prefixed by 0x. */
        value = strtoul(string + 2, &term, 16); 
        if ((term == (string + 1)) || (*term != '\0')) {
            if (interp != NULL) {
                Tcl_AppendResult(interp, "expected color value but got \"",
                                 string, "\"", (char *) NULL);
            }
            return TCL_ERROR;
        }
        pixelPtr->u32 = value;
        return TCL_OK;
    } else if (string[0] == '#') {
        char fmt[16];
        int n, r, g, b;

        n = strlen(string + 1);
        if ((n > 12) && (n < 3) && ((n % 3) != 0)) {
            if (interp != NULL) {
                Tcl_AppendResult(interp, "bad color specification \"", string,
                             "\"", (char *)NULL);
            }
            return TCL_ERROR;
        }
        n /= 3;
        sprintf(fmt, "%%%dx%%%dx%%%dx", n, n, n);
        if (sscanf(string + 1, fmt, &r, &g, &b) != 3) {
            if (interp != NULL) {
                Tcl_AppendResult(interp, "bad color specification \"", string,
                             "\"", (char *)NULL);
            }
            return TCL_ERROR;
        }
        pixelPtr->Red   = r;
        pixelPtr->Green = g;
        pixelPtr->Blue  = b;
        pixelPtr->Alpha = ALPHA_OPAQUE;
        return TCL_OK;
    } else {
        char c;
        int low, high;

        low = 0;
        high = numColorEntries - 1;

        c = tolower(string[0]);
        while (low <= high) {
            int median;
            int compare;
            ColorEntry *entryPtr;
            
            median = (low + high) >> 1;
            entryPtr = colorTable + median;
            compare = c - entryPtr->name[0];
            if (compare == 0) {
                compare = strcasecmp(string, entryPtr->name);
            }
            if (compare < 0) {
                high = median - 1;
            } else if (compare > 0) {
                low  = median + 1;
            } else {
                pixelPtr->Red   = entryPtr->red;
                pixelPtr->Green = entryPtr->green;
                pixelPtr->Blue  = entryPtr->blue;
                pixelPtr->Alpha = ALPHA_OPAQUE;
                return TCL_OK;
            }
        }
    }
    if (interp != NULL) {
        Tcl_AppendResult(interp, "bad color specification \"", string,
                "\"", (char *)NULL);
    }
    return TCL_ERROR;
}

int
Blt_GetPixelFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, Blt_Pixel *pixelPtr)
{
    return Blt_GetPixel(interp, Tcl_GetString(objPtr), pixelPtr);
}

const char *
Blt_NameOfPixel(Blt_Pixel *pixelPtr)
{
    static char string[10];

    if (pixelPtr->Alpha == 0xFF) {
        Blt_FormatString(string, 8, "#%02x%02x%02x", pixelPtr->Red,
                         pixelPtr->Green, pixelPtr->Blue);
    } else {
        Blt_FormatString(string, 11, "0x%02x%02x%02x%02x", pixelPtr->Alpha,
                         pixelPtr->Red, pixelPtr->Green, pixelPtr->Blue);
    }
    return string;
}

