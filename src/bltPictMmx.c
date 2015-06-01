/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * bltPictMMX.c --
 *
 * This module implements image processing procedures for the BLT toolkit.
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

#ifdef HAVE_STRING_H
  #include <string.h>
#endif /* HAVE_STRING_H */

#include "bltAlloc.h"
#include "bltChain.h"
#include "bltPicture.h"
#include "bltPictInt.h"

#define imul8x8(a,b,t)  ((t) = (a)*(b)+128,(((t)+((t)>>8))>>8))

#ifdef HAVE_X86_ASM

#define FEATURE_MMX                (1L << 23)
#define FEATURE_MMXEXT             (1L << 24)
#define FEATURE_3DNOW              (1L << 31)
#define FEATURE_SSE                (1L << 25)
#define FEATURE_SSE2               (1L << 26)
#define FEATURE_SSSE3              (1L << 41)
#define FEATURE_SSE41              (1L << 51)
#define FEATURE_SSE42              (1L << 52)

struct cpuid {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
};

static Blt_ApplyPictureToPictureProc ApplyPictureToPicture;
static Blt_ApplyScalarToPictureProc  ApplyScalarToPicture;
#ifdef notdef
static Blt_ApplyPictureToPictureWithMaskProc ApplyPictureToPictureWithMask;
static Blt_ApplyScalarToPictureWithMaskProc  ApplyScalarToPictureWithMask;
static Blt_BlendPicturesProc BlendPictures;
static Blt_AssociateColorsProc AssociateColors;
static Blt_UnassociateColorsProc UnassociateColors;
static Blt_CopyPicturesProc CopyPictures;
#endif
static Blt_TentHorizontallyProc TentHorizontally;
static Blt_TentVerticallyProc TentVertically;
static Blt_ZoomHorizontallyProc ZoomHorizontally;
static Blt_ZoomVerticallyProc ZoomVertically;
static Blt_SelectPixelsProc SelectPixels;

#ifdef notdef
static Blt_PictureProcs mmxPictureProcs = {
    ApplyPictureToPicture,
    ApplyScalarToPicture,
    NULL,                              /* ApplyPictureToPictureWithMask, */
    NULL,                              /* ApplyScalarToPictureWithMask, */
    TentHorizontally,
    TentVertically,
    ZoomHorizontally,
    ZoomVertically,
    BlendPictures,
    SelectPixels,
    AssociateColors,
    UnassociateColors,
    CopyPictures
};
#endif

/* 
 * sR sG sB sA 
 * dA dA dA dA 
 * dR sG dB dA
 * sA sA sA sA 
 *
 */
static void
SelectPixels(
    Pict *destPtr,
    Pict *srcPtr, 
    Blt_Pixel *lowerPtr,
    Blt_Pixel *upperPtr)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;
    int y;

    if (srcPtr != destPtr) {
        Blt_ResizePicture(destPtr, srcPtr->width, srcPtr->height);
    }

    asm volatile (
        /* Put lower and upper pixels in registers. */
        "movd %0, %%mm4         # mm4 = L\n\t"
        "movd %1, %%mm5         # mm5 = H\n\t"
        "pxor %%mm6, %%mm6      # mm6 = 0\n\t"
        "punpckldq %%mm4, %%mm4 # mm4 = L,L\n\t" 
        "punpckldq %%mm5, %%mm5 # mm5 = H,H\n\t" 
        : /* output registers */ 
        : /* input registers */
          "r" (lowerPtr->u32), 
          "r" (upperPtr->u32));

    destRowPtr = destPtr->bits, srcRowPtr = srcPtr->bits;
    for (y = 0; y < srcPtr->height; y++) {
        Blt_Pixel *dp, *sp, *send;

        dp = destRowPtr;
        for(sp = srcRowPtr, send = sp + srcPtr->width; sp < send; sp += 2) {
            asm volatile (
                /* Compare two pixels at a time */
                "movq (%1), %%mm3       # mm3 = S1,S2\n\t"
                "movq %%mm4, %%mm0      # mm0 = L,L\n\t"

                /* We want to test (S >= L) && (S <= H). Since the operands
                 * are all unsigned, pcmp* ops are out.  Instead use
                 * saturated, unsigned subtraction.  ((L psub S) == 0) is
                 * the same as (S >= L) */

                "psubusb %%mm3, %%mm0   # mm0 = L - S\n\t"
                "movq %%mm3, %%mm1      # mm1 = S\n\t"
                "psubusb %%mm5, %%mm1   # mm1 = S - H\n\t"

                /* "or" the two results and compare 32-bit values to 0
                 * (inverting the logic). */

                "por %%mm1, %%mm0       # mm0 = (S >= L)|(H >= S)\n\t"
                "pcmpeqd %%mm6, %%mm0   # invert logic\n\t"
                "movq %%mm0, (%0)       # dp = new value\n" 
                : /* output registers */
                  "+r" (dp) 
                : /* input registers */
                  "r" (sp));
            dp += 2;
        }
        srcRowPtr  += srcPtr->pixelsPerRow;
        destRowPtr += destPtr->pixelsPerRow;
    }
    asm volatile ("emms");
    destPtr->flags &= ~BLT_PIC_BLEND;
    destPtr->flags |= BLT_PIC_MASK;
}

static void
AddPictureToPicture(Pict *destPtr, Pict *srcPtr, int x, int y, int w, int h, 
                    int dx, int dy)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm7, %mm7     # mm5 = -1 \n");

    destRowPtr = destPtr->bits + ((dy * destPtr->pixelsPerRow) + dx);
    srcRowPtr  = srcPtr->bits + ((y * srcPtr->pixelsPerRow) + x);
    for (y = 0; y < h; y++) {
        Blt_Pixel *sp, *dp, *dend;

        sp = srcRowPtr;
        for (dp = destRowPtr, dend = dp + w; dp < dend; dp += 2, sp += 2) {
            asm volatile (
                "movq (%0), %%mm0\n\t" 
                "paddusb (%1), %%mm0\n\t" 
                "movq %%mm0, (%0)" 
                : /* output registers */
                  "+r" (dp) 
                : /* input registers */
                  "r" (sp));
        }
        destRowPtr += destPtr->pixelsPerRow;
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

static void
SubPictureToPicture(Pict *destPtr, Pict *srcPtr, int x, int y, int w, int h, 
                    int dx, int dy)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm7, %mm7     # mm5 = -1 \n");

    destRowPtr = destPtr->bits + ((dy * destPtr->pixelsPerRow) + dx);
    srcRowPtr  = srcPtr->bits + ((y * srcPtr->pixelsPerRow) + x);
    for (y = 0; y < h; y++) {
        Blt_Pixel *sp, *dp, *dend;

        sp = srcRowPtr;
        for (dp = destRowPtr, dend = dp + w; dp < dend; dp += 2, sp += 2) {
            asm volatile (
                "movq (%0), %%mm0\n\t" 
                "psubusb (%1), %%mm0\n\t" 
                "movq %%mm0, (%0)" 
                : /* output registers */
                  "+r" (dp) 
                : /* input registers */
                  "r" (sp));
        }
        destRowPtr += destPtr->pixelsPerRow;
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

static void
RSubPictureToPicture(Pict *destPtr, Pict *srcPtr, int x, int y, int w, int h, 
                     int dx, int dy)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm7, %mm7     # mm5 = -1 \n");

    destRowPtr = destPtr->bits + ((dy * destPtr->pixelsPerRow) + dx);
    srcRowPtr  = srcPtr->bits + ((y * srcPtr->pixelsPerRow) + x);
    for (y = 0; y < h; y++) {
        Blt_Pixel *sp, *dp, *dend;

        sp = srcRowPtr;
        for (dp = destRowPtr, dend = dp + w; dp < dend; dp += 2, sp += 2) {
            asm volatile (
                "movq (%1), %%mm1\n\t" 
                "psubusb (%0), %%mm1\n\t" 
                "movq %%mm1, (%0)" 
                : /* output registers */
                  "+r" (dp) 
                : /* input registers */
                  "r" (sp));
        }
        destRowPtr += destPtr->pixelsPerRow;
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

static void
AndPictureToPicture(Pict *destPtr, Pict *srcPtr, int x, int y,  int w, int h, 
                    int dx, int dy)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm7, %mm7     # mm5 = -1 \n");

    destRowPtr = destPtr->bits + ((dy * destPtr->pixelsPerRow) + dx);
    srcRowPtr  = srcPtr->bits + ((y * srcPtr->pixelsPerRow) + x);
    for (y = 0; y < h; y++) {
        Blt_Pixel *sp, *dp, *dend;

        sp = srcRowPtr;
        for (dp = destRowPtr, dend = dp + w; dp < dend; dp += 2, sp += 2) {
            asm volatile (
                "movq (%0), %%mm0\n\t" 
                "pand (%1), %%mm0\n\t" 
                "movq %%mm0, (%0)" 
                : /* output registers */
                  "+r" (dp) 
                : /* input registers */
                  "r" (sp));
        }
        destRowPtr += destPtr->pixelsPerRow;
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

static void
OrPictureToPicture(Pict *destPtr, Pict *srcPtr, int x, int y, int w, int h, 
                   int dx, int dy)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm7, %mm7     # mm5 = -1 \n");

    destRowPtr = destPtr->bits + ((dy * destPtr->pixelsPerRow) + dx);
    srcRowPtr  = srcPtr->bits + ((y * srcPtr->pixelsPerRow) + x);
    for (y = 0; y < h; y++) {
        Blt_Pixel *sp, *dp, *dend;

        sp = srcRowPtr;
        for (dp = destRowPtr, dend = dp + w; dp < dend; dp += 2, sp += 2) {
            asm volatile (
                "movq (%0), %%mm0\n\t" 
                "por (%1), %%mm0\n\t" 
                "movq %%mm0, (%0)" 
                : /* output registers */
                  "+r" (dp) 
                : /* input registers */
                  "r" (sp));
        }
        destRowPtr += destPtr->pixelsPerRow;
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

static void
NandPictureToPicture(Pict *destPtr, Pict *srcPtr, int x, int y, int w, int h, 
                     int dx, int dy)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm7, %mm7     # mm5 = -1 \n");

    destRowPtr = destPtr->bits + ((dy * destPtr->pixelsPerRow) + dx);
    srcRowPtr  = srcPtr->bits + ((y * srcPtr->pixelsPerRow) + x);
    for (y = 0; y < h; y++) {
        Blt_Pixel *sp, *dp, *dend;

        sp = srcRowPtr;
        for (dp = destRowPtr, dend = dp + w; dp < dend; dp += 2, sp += 2) {
            asm volatile (
                "movq (%0), %%mm0\n\t" 
                "pand (%1), %%mm0\n\t" 
                "pxor %%mm7, %%mm0\n\t" 
                "movq %%mm0, (%0)" 
                : /* output registers */
                  "+r" (dp) 
                : /* input registers */
                  "r" (sp));
        }
        destRowPtr += destPtr->pixelsPerRow;
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

static void
NorPictureToPicture(Pict *destPtr, Pict *srcPtr, int x, int y, int w, int h, 
                    int dx, int dy)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm7, %mm7     # mm5 = -1 \n");

    destRowPtr = destPtr->bits + ((dy * destPtr->pixelsPerRow) + dx);
    srcRowPtr  = srcPtr->bits + ((y * srcPtr->pixelsPerRow) + x);
    for (y = 0; y < h; y++) {
        Blt_Pixel *sp, *dp, *dend;

        sp = srcRowPtr;
        for (dp = destRowPtr, dend = dp + w; dp < dend; dp += 2, sp += 2) {
            asm volatile (
                "movq (%0), %%mm0\n\t" 
                "por (%1), %%mm0\n\t" 
                "pxor %%mm7, %%mm0\n\t" 
                "movq %%mm0, (%0)" 
                : /* output registers */
                  "+r" (dp) 
                : /* input registers */
                  "r" (sp));
        }
        destRowPtr += destPtr->pixelsPerRow;
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

static void
XorPictureToPicture(Pict *destPtr, Pict *srcPtr, int x, int y, int w, int h, 
                    int dx, int dy)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm7, %mm7     # mm5 = -1 \n");

    destRowPtr = destPtr->bits + ((dy * destPtr->pixelsPerRow) + dx);
    srcRowPtr  = srcPtr->bits + ((y * srcPtr->pixelsPerRow) + x);
    for (y = 0; y < h; y++) {
        Blt_Pixel *sp, *dp, *dend;

        sp = srcRowPtr;
        for (dp = destRowPtr, dend = dp + w; dp < dend; dp += 2, sp += 2) {
            asm volatile (
                "movq (%0), %%mm0\n\t" 
                "pxor (%1), %%mm0\n\t" 
                "movq %%mm0, (%0)" 
                : /* output registers */
                  "+r" (dp) 
                : /* input registers */
                  "r" (sp));
        }
        destRowPtr += destPtr->pixelsPerRow;
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

static void
MinPictureToPicture(Pict *destPtr, Pict *srcPtr, int x, int y, int w, int h, 
                    int dx, int dy)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm7, %mm7     # mm5 = -1 \n");

    destRowPtr = destPtr->bits + ((dy * destPtr->pixelsPerRow) + dx);
    srcRowPtr  = srcPtr->bits + ((y * srcPtr->pixelsPerRow) + x);
    for (y = 0; y < h; y++) {
        Blt_Pixel *sp, *dp, *dend;

        sp = srcRowPtr;
        for (dp = destRowPtr, dend = dp + w; dp < dend; dp += 2, sp += 2) {
            asm volatile (
                "movq (%0), %%mm0               # mm0 = A\n\t" 
                "movq (%1), %%mm1               # mm1 = B\n\t" 
                "movq %%mm0, %%mm2              # mm2 = A\n\t" 
                "psubusb %%mm1, %%mm2           # mm2 = A - B\n\t"
                "pcmpeqb %%mm6, %%mm2           # mm2 = 0s A>B 1s A<=B\n\t"
                "pand %%mm2, %%mm0              # mm2 = mask & A\n\t" 
                "pxor %%mm7, %%mm2              # mm2 = ~mask\n\t" 
                "pand %%mm2, %%mm1              # mm0 = ~mask & B\n\t" 
                "por %%mm1, %%mm0               # mm0 = R1 | R2\n\t" 
                "movq %%mm0, (%0)" 
                : /* output registers */
                  "+r" (dp) 
                : /* input registers */
                  "r" (sp));
        }
        destRowPtr += destPtr->pixelsPerRow;
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

static void
MaxPictureToPicture(Pict *destPtr, Pict *srcPtr, int x, int y, int w, int h, 
                    int dx, int dy)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm7, %mm7     # mm5 = -1 \n");

    destRowPtr = destPtr->bits + ((dy * destPtr->pixelsPerRow) + dx);
    srcRowPtr  = srcPtr->bits + ((y * srcPtr->pixelsPerRow) + x);
    for (y = 0; y < h; y++) {
        Blt_Pixel *sp, *dp, *dend;

        sp = srcRowPtr;
        for (dp = destRowPtr, dend = dp + w; dp < dend; dp += 2, sp += 2) {
            asm volatile (
                "movq (%0), %%mm0               # mm0 = A\n\t" 
                "movq (%1), %%mm1               # mm1 = B\n\t" 
                "movq %%mm0, %%mm2              # mm2 = A\n\t" 
                "psubusb %%mm1, %%mm2           # mm2 = A - B\n\t"
                "pcmpeqb %%mm6, %%mm2           # mm2 = 0s A>B 1s A<=B\n\t"
                "pand %%mm2, %%mm1              # mm1 = mask & B\n\t" 
                "pxor %%mm7, %%mm2              # mm2 = ~mask\n\t" 
                "pand %%mm2, %%mm0              # mm0 = ~mask & A\n\t" 
                "por %%mm1, %%mm0               # mm3 = R1 | R2\n\t" 
                "movq %%mm0, (%0)" 
                : /* output registers */
                  "+r" (dp) 
                : /* input registers */
                  "r" (sp));
        }
        destRowPtr += destPtr->pixelsPerRow;
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

static void
ApplyPictureToPicture(
    Pict *destPtr, 
    Pict *srcPtr, 
    int x, int y, int w, int h, int dx, int dy, 
    Blt_PictureArithOps op)
{
    if ((x + w) > srcPtr->width) {
        w -= srcPtr->width - x; 
    }
    if ((y + h) > srcPtr->height) {
        h -= srcPtr->height - y;
    }
    if ((dx + w) > destPtr->width) {
        w -= destPtr->width - dx;
    }
    if ((dy + h) > destPtr->height) {
        h -= destPtr->height - dy;
    }
    switch(op) {
    case PIC_ARITH_ADD:
        AddPictureToPicture(destPtr, srcPtr, x, y, w, h, dx, dy);
        break;
    case PIC_ARITH_SUB:
        SubPictureToPicture(destPtr, srcPtr, x, y, w, h, dx, dy);
        break;
    case PIC_ARITH_RSUB:
        RSubPictureToPicture(destPtr, srcPtr, x, y, w, h, dx, dy);
        break;
    case PIC_ARITH_AND:
        AndPictureToPicture(destPtr, srcPtr, x, y, w, h, dx, dy);
        break;
    case PIC_ARITH_OR:
        OrPictureToPicture(destPtr, srcPtr, x, y, w, h, dx, dy);
        break;
    case PIC_ARITH_NAND:
        NandPictureToPicture(destPtr, srcPtr, x, y, w, h, dx, dy);
        break;
    case PIC_ARITH_NOR:
        NorPictureToPicture(destPtr, srcPtr, x, y, w, h, dx, dy);
        break;
    case PIC_ARITH_XOR:
        XorPictureToPicture(destPtr, srcPtr, x, y, w, h, dx, dy);
        break;
    case PIC_ARITH_MIN:
        MinPictureToPicture(destPtr, srcPtr, x, y, w, h, dx, dy);
        break;
    case PIC_ARITH_MAX:
        MaxPictureToPicture(destPtr, srcPtr, x, y, w, h, dx, dy);
        break;
    }
}


static void
ApplyScalarToPicture(
    Pict *srcPtr, 
    Blt_Pixel *colorPtr,
    Blt_PictureArithOps op)
{
    Blt_Pixel *srcRowPtr;
    int y;
    
    /*
     * mm7 = -1
     * mm6 = 0x0
     * mm4 = scalar,scalar
     */
    asm volatile (
        /* Generate constants needed below. */
        "pxor %%mm6, %%mm6        # mm6 = 0\n\t"
        "pcmpeqw %%mm7, %%mm7     # mm5 = -1 \n\t"
        /* Put the scalar into hi/lo 32-bit words.*/
        "movd %0, %%mm4           # mm4 = scalar\n\t"
        "punpckldq %%mm4, %%mm4   # mm2 = S,S\n" 
        : /* output registers */
        : /* input registers */
          "r" (colorPtr->u32));

    srcRowPtr = srcPtr->bits;
    for (y = 0; y < srcPtr->height; y++) {
        Blt_Pixel *sp, *send;

        sp = srcRowPtr;
        send = sp + srcPtr->width;
        switch(op) {
        case PIC_ARITH_ADD:
            while (sp < send) {
                asm volatile (
                    "movq (%0), %%mm0\n\t" 
                    "paddusb %%mm4, %%mm0\n\t" 
                    "movq %%mm0, (%0)" 
                    : /* output registers */
                      "+r" (sp));
                sp += 2;
            }
            break;

        case PIC_ARITH_SUB:
            while (sp < send) {
                asm volatile (
                     "movq (%0), %%mm0\n\t" 
                     "psubusb %%mm4, %%mm0\n\t" 
                     "movq %%mm0, (%0)" 
                     : /* output registers */
                       "+r" (sp));
                sp += 2;
            }
            break;

        case PIC_ARITH_RSUB:
            while (sp < send) {
                asm volatile (
                     "movq (%0), %%mm0\n\t" 
                     "movq %%mm4, %%mm1\n\t"
                     "psubusb %%mm0, %%mm1\n\t" 
                     "movq %%mm1, (%0)" 
                     : /* output registers */
                       "+r" (sp));
                sp += 2;
            }
            break;

        case PIC_ARITH_AND:
            while (sp < send) {
                asm volatile (
                    "movq (%0), %%mm0\n\t" 
                    "pand %%mm4, %%mm0\n\t" 
                    "movq %%mm0, (%0)" 
                    : /* output registers */
                      "+r" (sp));
                sp += 2;
            }
            break;

        case PIC_ARITH_OR:
            while (sp < send) {
                asm volatile (
                    "movq (%0), %%mm0\n\t" 
                    "por %%mm4, %%mm0\n\t" 
                    "movq %%mm0, (%0)" 
                    : /* output registers */
                      "+r" (sp));
                sp += 2;
            }
            break;

        case PIC_ARITH_NAND:
            while (sp < send) {
                asm volatile (
                    "movq (%0), %%mm0\n\t" 
                    "pand %%mm4, %%mm0\n\t" 
                    "pxor %%mm7, %%mm0\n\t" 
                    "movq %%mm0, (%0)" 
                    : /* output registers */
                      "+r" (sp));
                sp += 2;
            }
            break;

        case PIC_ARITH_NOR:
            while (sp < send) {
                asm volatile (
                    "movq (%0), %%mm0\n\t" 
                    "por %%mm4, %%mm0\n\t" 
                    "pxor %%mm7, %%mm0\n\t" 
                    "movq %%mm0, (%0)" 
                    : /* output registers */
                      "+r" (sp));
                sp += 2;
            }
            break;

        case PIC_ARITH_XOR:
            while (sp < send) {
                asm volatile (
                    "movq (%0), %%mm0\n\t" 
                    "pxor %%mm4, %%mm0\n\t" 
                    "movq %%mm0, (%0)" 
                    : /* output registers */
                      "+r" (sp));
                sp += 2;
            }
            break;

        case PIC_ARITH_MIN:
            while (sp < send) {
                asm volatile (
                    "movq (%0), %%mm0     # mm0 = Color\n\t" 
                    "movq %%mm0, %%mm1    # mm1 = Color\n\t" 
                    "psubusb %%mm4, %%mm1 # mm1 = C - S\n\t"
                    "pcmpeqb %%mm6, %%mm1 # mm2 = mask: 0s C>S 1s C<=S\n\t"
                    "pand %%mm1, %%mm0    # mm0 = mask & C\n\t" 
                    "pxor %%mm7, %%mm1    # mm1 = ~mask\n\t" 
                    "pand %%mm4, %%mm1    # mm1 = S & ~mask\n\t" 
                    "por %%mm1, %%mm0     # mm0 = (S&~mask)|(mask&C)\n\t" 
                    "movq %%mm0, (%0)" 
                    : /* output registers */
                      "+r" (sp));
                sp += 2;
            }
            break;

        case PIC_ARITH_MAX:
            while (sp < send) {
                asm volatile (
                    "movq (%0), %%mm0     # mm0 = Color\n\t" 
                    "movq %%mm4, %%mm1    # mm1 = Scalar\n\t" 
                    "psubusb %%mm0, %%mm1 # mm1 = S - C\n\t"
                    "pcmpeqb %%mm6, %%mm1 # mm1 = mask: 0s S>C 1s S<=C\n\t"
                    "pand %%mm1, %%mm0    # mm0 = mask & C\n\t" 
                    "pxor %%mm7, %%mm1    # mm1 = ~mask\n\t" 
                    "pand %%mm4, %%mm1    # mm1 = S & ~mask\n\t" 
                    "por %%mm1, %%mm0     # mm0 = (S&~mask)|(mask&C)\n\t" 
                    "movq %%mm0, (%0)" 
                    : /* output registers */
                      "+r" (sp));
                sp += 2;
            }
            break;
        }
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

static void
ZoomVertically(Pict *destPtr, Pict *srcPtr, ResampleFilter *filterPtr)
{
    Sample *samples, *send;
    int x;
    int bytesPerSample;                 /* Size of sample. */

    /* Pre-calculate filter contributions for each row. */
    bytesPerSample = Blt_ComputeWeights(srcPtr->height, destPtr->height, 
        filterPtr, &samples);
    send = (Sample *)((char *)samples + (destPtr->height * bytesPerSample));

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm2, %mm2     # mm2 = -1 \n\t"
        "psubw %mm6, %mm2       # mm2 = 1,1,1,1\n\t"
        "psllw $4, %mm2         # mm2 = BIAS\n");
    /* Apply filter to each row. */
    for (x = 0; x < srcPtr->width; x++) {
        Blt_Pixel *dp, *srcColPtr;
        Sample *splPtr;

        srcColPtr = srcPtr->bits + x;
        dp = destPtr->bits + x;
        for (splPtr = samples; splPtr < send; 
             splPtr = (Sample *)((char *)splPtr + bytesPerSample)) {
            Blt_Pixel *sp;
            PixelWeight *wp;

            asm volatile (
                /* Clear the accumulator mm5. */
                "pxor %mm5, %mm5        #  mm5 = 0\n\t");
            sp = srcColPtr + (splPtr->start * srcPtr->pixelsPerRow);
            for (wp = splPtr->weights; wp < splPtr->wend; wp++) {
                asm volatile (
                    /* Load the weighting factor into mm1. */
                    "movd (%0), %%mm1          #  mm1 = 0,0,0,W\n\t"
                    /* Load the source pixel into mm0. */
                    "movd (%1), %%mm0          #  mm0 = S\n\t" 
                    /* Unpack the weighting factor into mm1. */
                    "punpcklwd %%mm1, %%mm1    #  mm1 = 0,0,W,W\n\t"
                    /* Unpack the pixel components into 16-bit words.*/
                    "punpcklbw %%mm6, %%mm0    #  mm0 = Sa,Sb,Sg,Sr\n\t" 
                    /*  */
                    "punpcklwd %%mm1, %%mm1    #  mm1 = W,W,W,W\n\t"
                    /* Scale the 8-bit components to 14 bits. (S * 257) >> 2 */
                    "movq %%mm0, %%mm3         #  mm3 = S8\n\t" 
                    "psllw $8, %%mm3           #  mm3 = S8 * 256\n\t" 
                    "paddw %%mm3, %%mm0        #  mm0 = S16\n\t" 
                    "psrlw $1, %%mm0           #  mm0 = S15\n\t" 
                    /* Multiple each pixel component by the weight.  Note that
                     * the lower 16-bits of the product are truncated (bad)
                     * creating round-off error in the sum. */
                    "pmulhw %%mm1, %%mm0       #  mm0 = S15 * W14\n\t" 
                    /* Accumulate upper 16-bit results of product in mm5. */
                    "paddsw %%mm0, %%mm5       #  mm5 = prod + mm5\n\t" 
                    : /* output registers */ 
                    : /* input registers */
                      "r" (wp), 
                      "r" (sp));
                sp += srcPtr->pixelsPerRow;
            }
            asm volatile (
                /* Add a rounding bias to the pixel sum */
                "paddw %%mm2, %%mm5        # mm5 = A13 + BIAS\n\t" 
                /* Shift off fractional part */
                "psraw $5, %%mm5           # mm5 = A8\n\t" 
                /* Pack 16-bit components into lower 4 bytes. */
                "packuswb  %%mm5, %%mm5    # Pack 4 low-order bytes.\n\t" 
                /* Save the word (pixel) in the destination. */
                "movd %%mm5,(%0)           # dp = word\n" 
                : /* output registers */ 
                  "+r" (dp));
            dp += destPtr->pixelsPerRow;

        }
    }
    asm volatile ("emms");
    /* Free the memory allocated for filter weights. */
    Blt_Free(samples);
}

static void
ZoomHorizontally(
    Pict *destPtr,
    Pict *srcPtr, 
    ResampleFilter *filterPtr)
{
    Sample *samples, *send;
    int y;
    Blt_Pixel *srcRowPtr, *destRowPtr;
    int bytesPerSample;                 /* Size of sample. */

    /* Pre-calculate filter contributions for each column. */
    bytesPerSample = Blt_ComputeWeights(srcPtr->width, destPtr->width, 
        filterPtr, &samples);
    send = (Sample *)((char *)samples + (destPtr->width * bytesPerSample));

    /* Apply filter to each column. */
    srcRowPtr = srcPtr->bits;
    destRowPtr = destPtr->bits;

    asm volatile (
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pxor %mm3, %mm3        # mm3 = 0\n\t"
        "pcmpeqw %mm2, %mm2     # mm2 = -1\n\t"
        "psubw %mm3, %mm2       # mm2 = 1,1,1,1\n\t"
        "psllw $4, %mm2         # mm2 = BIAS\n");

    for (y = 0; y < srcPtr->height; y++) {
        Blt_Pixel *dp;
        Sample *splPtr;

        dp = destRowPtr;
        for (splPtr = samples; splPtr < send; 
             splPtr = (Sample *)((char *)splPtr + bytesPerSample)) {
            Blt_Pixel *sp;
            PixelWeight *wp;

            sp = srcRowPtr + splPtr->start;
            asm volatile (
                /* Clear the accumulator mm5. */
                "pxor %mm5, %mm5        #  mm5 = 0\n\n");
            for (wp = splPtr->weights; wp < splPtr->wend; wp++) {
                asm volatile (
                   /* Load the weighting factor into mm1. */
                   "movd (%0), %%mm1         #  mm1 = W\n\t" 
                   /* Get the source RGBA pixel. */
                   "movd (%1), %%mm0         #  mm0 = sp\n\t" 
                   /* Unpack the weighting factor into mm1. */
                   "punpcklwd %%mm1, %%mm1   #  mm1 = 0,0,W,W\n\t"
                   /* Unpack the pixel into mm0. */
                   "punpcklbw %%mm6, %%mm0   #  mm0 = Sa,Sr,Sg,Sb\n\t" 
                   /*  */
                   "punpcklwd %%mm1, %%mm1   #  mm1 = W,W,W,W\n\t"
                   /* Scale the 8-bit components to 14 bits: (S * 257) >> 2 */
                   "movq %%mm0, %%mm3        #  mm3 = S8\n\t" 
                   "psllw $8, %%mm3          #  mm3 = S8 * 256\n\t" 
                   "paddw %%mm3, %%mm0       #  mm0 = S16\n\t" 
                   "psrlw $1, %%mm0          #  mm0 = S15\n\t" 
                   /* Multiple each pixel component by the weight.  Note
                    * that the lower 16-bits of the product are
                    * truncated (bad) creating round-off error in the
                    * sum. */
                   "pmulhw %%mm1, %%mm0      #  mm0 = S15 * W14\n\t"  
                   /* Add the 16-bit components to mm5. */
                   "paddsw %%mm0, %%mm5      #  mm5 = A13 + mm5\n\t" 
                   : /* output registers */ 
                   : /* input registers */
                     "r" (wp), 
                     "r" (sp));
                sp++;
            }                   
            asm volatile (
                /* Add a rounding bias to the pixel sum. */
                "paddw %%mm2, %%mm5       # mm5 = A13 + BIAS\n\t" 
                /* Shift off fractional portion. */
                "psraw $5, %%mm5          # mm5 = A8\n\t" 
                /* Pack 16-bit components into lower 4 bytes. */
                "packuswb %%mm5, %%mm5    # Pack A8 into low 4 bytes.\n\t" 
                /* Store the word (pixel) in the destination. */
                "movd %%mm5,(%0)           # dp = word\n" 
                : /* output registers */ 
                  "+r" (dp));
            dp++;
        }
        srcRowPtr += srcPtr->pixelsPerRow;
        destRowPtr += destPtr->pixelsPerRow;
    }
    asm volatile ("emms");
    /* Free the memory allocated for horizontal filter weights. */
    Blt_Free(samples);
}


static void
TentVertically(Pict *destPtr, Pict *srcPtr) 
{
    Blt_Pixel *srcColPtr, *destColumnPtr;
    int x;
    size_t numPixels;
    
    asm volatile (
        /* Establish constants used below. */
        "pxor %mm6, %mm6        # mm6 = 0\n");

    numPixels = srcPtr->height * srcPtr->pixelsPerRow; 
    srcColPtr = srcPtr->bits;
    destColumnPtr = destPtr->bits;
    for (x = 0; x < srcPtr->width; x++) {
        Blt_Pixel *dp, *rp, *rend;

        /* 
         * mm0 = 
         * mm1 = unpacked center pixel 
         * mm2 = unpacked left pixel  
         * mm3 = unpacked right pixel 
         * mm4 = 
         * mm5 = 
         * mm6 = 0
         * mm7 = 
         */
        dp = destColumnPtr;
        rp = srcColPtr + srcPtr->pixelsPerRow;
        asm volatile (
            "movd (%2), %%mm1         # mm1 = cp\n\t"
            "movd (%1), %%mm3         # mm3 = rp\n\t"
            "punpcklbw %%mm6, %%mm1   # mm1 = S8\n\t"
            "movq %%mm1, %%mm2        # mm2 = lp = S8\n\t"
            "punpcklbw %%mm6, %%mm3   # mm3 = S8\n\t"
            "movq  %%mm1, %%mm0       # mm0 = cp\n\t"
            "psllw $1, %%mm0          # mm0 = cp << 1\n\t"
            "paddw %%mm2, %%mm0       # mm0 = lp + (cp << 1)\n\t"
            "paddw %%mm3, %%mm0       # mm0 = lp + (cp << 1) + rp\n\t"
            "psraw $2, %%mm0          # mm0 = (lp + (cp << 1) + rp) >> 2\n\t"
            "packuswb %%mm0, %%mm0    # Pack into low 4 bytes.\n\t"  
            "movd %%mm0,(%0)          # dp = word\n\t"
            "movq %%mm3, %%mm1        # cp = rp\n" 
            : /* output registers */ 
              "=r" (dp), 
              "+r" (rp) 
            : /* input registers */
              "r" (srcColPtr));
        dp += destPtr->pixelsPerRow;
        rp += srcPtr->pixelsPerRow;

        for (rend = srcColPtr + numPixels; rp < rend; /*empty*/) {
            asm volatile (
                "movd (%1), %%mm3         #  mm3 = rp\n\t" 
                "punpcklbw %%mm6, %%mm3   #  mm3 = S8\n\t"
                "movq  %%mm1, %%mm0       #  mm0 = cp\n\t"
                "psllw $1, %%mm0          #  mm0 = cp << 1\n\t"
                "paddw %%mm2, %%mm0       #  mm0 = lp + (cp << 1)\n\t"
                "paddw %%mm3, %%mm0       #  mm0 = lp + (cp << 1) + rp\n\t"
                "psraw $2, %%mm0          #  mm0 = (lp + (cp<<1) + rp) >> 2\n\t"
                "packuswb %%mm0, %%mm0    #  Pack into low 4 bytes.\n\t"  
                "movd %%mm0,(%0)          #  dp = word\n\t" 
                "movq %%mm1, %%mm2        #  lp = cp\n\t"
                "movq %%mm3, %%mm1        #  cp = rp\n"
                : /* output registers */ 
                  "=r" (dp), 
                  "+r" (rp));
            dp += destPtr->pixelsPerRow;
            rp += srcPtr->pixelsPerRow;
        }       
        asm volatile (
            "movq %%mm1, %%mm3        #  rp = cp\n\t"
            "movq %%mm1, %%mm0        #  mm0 = cp\n\t"
            "psllw $1, %%mm0          #  mm0 = cp << 1\n\t"
            "paddw %%mm2, %%mm0       #  mm0 = lp + (cp << 1)\n\t"
            "paddw %%mm3, %%mm0       #  mm0 = lp + (cp << 1) + rp\n\t"
            "psraw $2, %%mm0          #  mm0 = (lp + (cp << 1) + rp) >> 2\n\t"
            "packuswb %%mm0, %%mm0    #  Pack into low 4 bytes.\n\t"  
            "movd %%mm0,(%0)          #  dp = word\n" 
            : /* output registers */ 
              "=r" (dp));

        srcColPtr++, destColumnPtr++;
    }
    asm volatile ("emms");
}

static void
TentHorizontally(Pict *destPtr, Pict *srcPtr) 
{
    Blt_Pixel *srcRowPtr, *destRowPtr;
    int y;

    asm volatile (
        /* Establish constants used below. */
        "pxor %mm6, %mm6        # mm6 = 0\n");

    srcRowPtr = srcPtr->bits;
    destRowPtr = destPtr->bits;
    for (y = 0; y < srcPtr->height; y++) {
        Blt_Pixel *dp;
        Blt_Pixel *rp, *rend;
        
        /* 
         * mm0 = 
         * mm1 = unpacked center pixel 
         * mm2 = unpacked left pixel  
         * mm3 = unpacked right pixel 
         * mm4 = 
         * mm5 = 
         * mm6 = 0
         * mm7 = 
         */
        dp = destRowPtr;
        rp = srcRowPtr + 1;
        asm volatile (
            "movd (%2), %%mm1         #  mm1 = cp\n\t"
            "movq %%mm1, %%mm2        #  mm2 = lp\n\t"
            "movd (%1), %%mm3         #  mm3 = rp\n\t"
            "punpcklbw %%mm6, %%mm1   #  mm1 = S8\n\t"
            "punpcklbw %%mm6, %%mm2   #  mm2 = S8\n\t"
            "punpcklbw %%mm6, %%mm3   #  mm3 = S8\n\t"
            "movq  %%mm1, %%mm0       #  mm0 = cp\n\t"
            "psllw $1, %%mm0          #  mm0 = cp << 1\n\t"
            "paddw %%mm2, %%mm0       #  mm0 = lp + (cp << 1)\n\t"
            "paddw %%mm3, %%mm0       #  mm0 = lp + (cp << 1) + rp\n\t"
            "psraw $2, %%mm0          #  mm0 = (lp + (cp << 1) + rp) >> 2\n\t"
            "packuswb %%mm0, %%mm0    #  Pack into low 4 bytes.\n\t"  
            "movd %%mm0,(%0)          #  dp = word\n\t"
            "movq %%mm3, %%mm1        #  cp = rp\n" 
            : /* output registers */ 
              "=r" (dp), 
              "+r" (rp) 
            : /* input registers */
              "r" (srcRowPtr));
        dp++, rp++;

        for (rend = srcRowPtr + srcPtr->width; rp < rend; /*empty*/) {
            asm volatile (
                "movd (%1), %%mm3         # mm3 = rp\n\t" 
                "punpcklbw %%mm6, %%mm3   # mm3 = S8\n\t"
                "movq  %%mm1, %%mm0       #  mm0 = cp\n\t"
                "psllw $1, %%mm0          #  mm0 = cp << 1\n\t"
                "paddw %%mm2, %%mm0       #  mm0 = lp + (cp << 1)\n\t"
                "paddw %%mm3, %%mm0       #  mm0 = lp + (cp << 1) + rp\n\t"
                "psraw $2, %%mm0          #  mm0 = (lp + (cp<<1) + rp) >> 2\n\t"
                "packuswb %%mm0, %%mm0    #  Pack into low 4 bytes.\n\t"  
                "movd %%mm0,(%0)          #  dp = word\n\t" 
                "movq %%mm1, %%mm2        #  lp = cp\n\t"
                "movq %%mm3, %%mm1        #  cp = rp\n" 
                : /* output registers */ 
                  "=r" (dp), 
                  "+r" (rp));
            dp++, rp++;
        }

        asm volatile (
            "movq %%mm1, %%mm3        #  rp = cp\n\t"
            "movq %%mm1, %%mm0        #  mm0 = cp\n\t"
            "psllw $1, %%mm0          #  mm0 = cp << 1\n\t"
            "paddw %%mm2, %%mm0       #  mm0 = lp + (cp << 1)\n\t"
            "paddw %%mm3, %%mm0       #  mm0 = lp + (cp << 1) + rp\n\t"
            "psraw $2, %%mm0          #  mm0 = (lp + (cp << 1) + rp) >> 2\n\t"
            "packuswb %%mm0, %%mm0    #  Pack into low 4 bytes.\n\t"  
            "movd %%mm0,(%0)          #  dp = word\n" 
            : /* output registers */ 
              "=r" (dp));

        srcRowPtr += srcPtr->pixelsPerRow;
        destRowPtr += destPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

#ifdef notdef
static void
BlendRegion(
    Pict *destPtr,                      /* (in/out) Background picture.
                                         * Composite overwrites region in
                                         * background. */
    Pict *srcPtr,                       /* Foreground picture. */
    int sx, int sy,                     /* Origin of foreground region in
                                         * source. */
    int w, int h,                       /* Dimension of area to be
                                         * blended. */
    int dx, int dy)                     /* Origin of background region in
                                         * destination. */
{
    Blt_Pixel *srcRowPtr, *destRowPtr;
    int y;

    if ((srcPtr->flags & BLT_PIC_ASSOCIATED_COLORS) == 0) {
        Blt_AssociateColors(srcPtr);
    }
    if ((destPtr->flags & BLT_PIC_ASSOCIATED_COLORS) == 0) {
        Blt_AssociateColors(destPtr);
    }
    destRowPtr = destPtr->bits + ((dy * destPtr->pixelsPerRow) + dx);
    srcRowPtr = srcPtr->bits + ((sy * srcPtr->pixelsPerRow) + sx);

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm5, %mm5     # mm5 = -1 \n\t"
        "psubw %mm6, %mm5       # mm5 = 1,1,1,1\n\t"
        "psllw $7, %mm5         # mm5 = ROUND = 128\n");

    for (y = 0; y < h; y++) {
        Blt_Pixel *sp, *send, *dp;

        dp = destRowPtr;
        for (sp = srcRowPtr, send = sp + w; sp < send; sp++, dp++) {
            /* Blend the foreground and background together. */
            if (sp->Alpha == 0xFF) {
                *dp = *sp;
            } else if (sp->Alpha != 0x00) {
                unsigned long beta;
                
                beta = sp->Alpha ^ 0xFF; /* beta = 1 - alpha */

                /*
                 * Small wins:  
                 *
                 * We can compute 
                 *      dest = fg + (beta * bg);
                 * for all RGBA components at once. 
                 *
                 * Packing unsigned with saturation performs the necessary
                 * clamping without the branch misprediction penalty.
                 *
                 * FIXME: 
                 *     Check if it's faster to do the blend calcution all
                 *     the time (even when alpha is 0 or 255). There's a
                 *     good probability that the majority of pixels are
                 *     opaque (interior) or completely transparent
                 *     (exterior).  Only the edge pixels would require
                 *     blending.
                 */
                asm volatile (
                    /* 
                     * mm0 = dp
                     * mm1 = sp
                     * mm2 = beta = 1 - alpha
                     * mm3 = temp
                     * mm4 = 
                     * mm5 = ROUND = 128,128,128,128
                     * mm6 = 0
                     * mm7 = 
                     */
                    "movd (%0), %%mm0         #  mm0 = dp\n\t" 
                    "movd (%1), %%mm1         #  mm1 = sp\n\t" 
                    "movd %2, %%mm2           #  mm2 = beta\n\t" 
                    "punpcklbw %%mm6, %%mm0   #  mm0 = Da,Dr,Dg,Db\n\t" 
                    "punpcklwd %%mm2, %%mm2   #  mm2 = 0,0,B,B\n\t"
                    "punpcklbw %%mm6, %%mm1   #  mm1 = Sa,Sr,Sg,Sb\n\t" 
                    "punpcklwd %%mm2, %%mm2   #  mm2 = B,B,B,B\n\t"
                    "pmullw %%mm0, %%mm2      #  mm2 = D*B\n\t" 
                    "paddw %%mm5, %%mm2       #  mm2 = (D*B)+ROUND\n\t"
                    "movq %%mm2, %%mm3        #  mm3 = P16\n\t" 
                    "psrlw $8, %%mm3          #  mm3 = P16 / 256\n\t"
                    "paddw %%mm2, %%mm3       #  mm3 = (P16 / 256) + P16\n\t" 
                    "psrlw $8, %%mm3          #  mm3 = P8 ~= P16 / 257\n\t"
                    "paddw %%mm1, %%mm3       #  mm3 = S + P\n\t"
                    "packuswb %%mm3, %%mm3    #  Pack 4 low bytes.\n\t" 
                    "movd %%mm3, (%0)         #  *dp = word\n" 
                    : "+r" (dp) 
                    : "r" (sp), 
                      "r" (beta));
            }
        }
        srcRowPtr += srcPtr->pixelsPerRow;
        destRowPtr += destPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}
#endif

#ifdef notdef
static void
ConvolvePictureVertically(Pict *destPtr, Pict *srcPtr, 
                          ResampleFilter *filterPtr)
{
    Sample *samples, *send;
    int x;
    int bytesPerSample;         /* Size of sample. */
    long bytesPerRow;

    /* Pre-calculate filter contributions for each row. */
    bytesPerSample = Blt_ComputeWeights(srcPtr->height, destPtr->height, 
        filterPtr, &samples);
    bytesPerRow = sizeof(Blt_Pixel) * srcPtr->pixelsPerRow;
    send = (Sample *)((char *)samples + (destPtr->height * bytesPerSample));

    asm volatile (
        /* Generate constants needed below. */
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pcmpeqw %mm2, %mm2     # mm2 = -1 \n\t"
        "psubw %mm6, %mm2       # mm2 = 1,1,1,1\n\t"
        "psllw $4, %mm2         # mm2 = BIAS\n");

    /* Apply filter to each row. */
    for (x = 0; x < srcPtr->width; x++) {
        Blt_Pixel *dp, *srcColPtr;
        Sample *splPtr;

        srcColPtr = srcPtr->bits + x;
        dp = destPtr->bits + x;
        for (splPtr = samples; splPtr < send; 
             splPtr = (Sample *)((char *)splPtr + bytesPerSample)) {
            Blt_Pixel *sp;

            sp = srcColPtr + (splPtr->start * srcPtr->pixelsPerRow);
            asm volatile (
                /* Clear the accumulator mm5. */
                 "pxor %%mm5, %%mm5         # mm5 = 0\n\n" 
                 ".Lasm%=:\n\t" 
                 /* Load the weighting factor into mm1. */
                 "movd (%1), %%mm1          # mm1 = 0,0,0,W\n\t"
                 /* Load the source pixel into mm0. */
                 "movd (%3), %%mm0          # mm0 = S\n\t" 
                 /* Unpack the weighting factor into mm1. */
                 "punpcklwd %%mm1, %%mm1    # mm1 = 0,0,W,W\n\t"
                 "punpcklwd %%mm1, %%mm1    # mm1 = W,W,W,W\n\t"
                 /* Unpack the pixel components into 16-bit words.*/
                 "punpcklbw %%mm6, %%mm0    # mm0 = Sa,Sb,Sg,Sr\n\t" 
                 /* Scale the 8-bit components to 14 bits. (S * 257) >> 2 */
                 "movq %%mm0, %%mm3         # mm3 = S8\n\t" 
                 "psllw $8, %%mm3           # mm3 = S8 * 256\n\t" 
                 "paddw %%mm3, %%mm0        # mm0 = S16\n\t" 
                 "psrlw $1, %%mm0           # mm0 = S15\n\t" 
                 /* Multiple each pixel component by the weight.  Note that
                  * the lower 16-bits of the product are truncated (bad)
                  * creating round-off error in the sum. */
                 "pmulhw %%mm1, %%mm0       # mm0 = S15 * W14\n\t" 
                 /* Move the pointers to the next weight and pixel */
                 "add $4, %1                # wp++\n\t" 
                 "add %4, %3                # sp++\n\t"
                 /* Accumulate upper 16-bit results of product in mm5. */
                 "paddsw %%mm0, %%mm5        # mm5 = prod + mm5\n\t" 
                 "cmp %2, %1                # wend == wp\n\t" 
                 "jnz .Lasm%=\n\t" 
                 /* end loop */
                 /* Add a rounding bias to the pixel sum */
                 "paddw %%mm2, %%mm5        # mm5 = A13 + BIAS\n\t" 
                 /* Shift off fractional part */
                 "psraw $5, %%mm5           # mm5 = A8\n\t" 
                 /* Pack 16-bit components into lower 4 bytes. */
                 "packuswb  %%mm5, %%mm5    # Pack 4 low-order bytes.\n\t" 
                 /* Save the word (pixel) in the destination. */
                 "movd %%mm5,(%0)           # dp = word\n" 
                 /* output registers */ 
                 : "=r" (dp) 
                 /* input registers */
                 : "r" (splPtr->weights), 
                   "r" (splPtr->wend), 
                   "r" (sp),
                   "r" (bytesPerRow));
#ifdef notdef
            if (dp->Alpha != 0xFF) {g
                fprintf(stdout, "mmx v-alpha=0x%x\n", dp->Alpha);
            }
#endif
            dp += destPtr->pixelsPerRow;

        }
    }
    asm volatile ("emms");
    /* Free the memory allocated for filter weights. */
    Blt_Free(samples);
}

static void
ConvolvePictureHorizontally(Pict *destPtr, Pict *srcPtr, 
                            ResampleFilter *filterPtr)
{
    Sample *samples, *send;
    int y;
    Blt_Pixel *srcRowPtr, *destRowPtr;
    int bytesPerSample;         /* Size of sample. */

    /* Pre-calculate filter contributions for each column. */
    bytesPerSample = Blt_ComputeWeights(srcPtr->width, destPtr->width, 
        filterPtr, &samples);
    send = (Sample *)((char *)samples + (destPtr->width * bytesPerSample));

    /* Apply filter to each column. */
    srcRowPtr = srcPtr->bits;
    destRowPtr = destPtr->bits;

    asm volatile (
        "pxor %mm6, %mm6        # mm6 = 0\n\t"
        "pxor %mm3, %mm3        # mm3 = 0\n\t"
        "pcmpeqw %mm2, %mm2     # mm2 = -1\n\t"
        "psubw %mm3, %mm2       # mm2 = 1,1,1,1\n\t"
        "psllw $4, %mm2         # mm2 = BIAS\n");

    for (y = 0; y < srcPtr->height; y++) {
        Blt_Pixel *dp;
        Sample *splPtr;

        dp = destRowPtr;
        for (splPtr = samples; splPtr < send; 
             splPtr = (Sample *)((char *)splPtr + bytesPerSample)) {

            Blt_Pixel *sp;
            sp = srcRowPtr + splPtr->start;
            asm volatile (
                /* Clear the accumulator mm5. */
                 "pxor %%mm5, %%mm5        #  mm5 = 0\n\n" 
                 ".Lasm%=:\n\t" 
                 /* Load the weighting factor into mm1. */
                 "movd (%1), %%mm1         #  mm1 = W\n\t" 
                 /* Get the source RGBA pixel. */
                 "movd (%3), %%mm0         # mm0 = sp\n\t" 
                 /* Unpack the weighting factor into mm1. */
                 "punpcklwd %%mm1, %%mm1   # mm1 = 0,0,W,W\n\t"
                 "punpcklwd %%mm1, %%mm1   # mm1 = W,W,W,W\n\t"
                 /* Unpack the pixel into mm0. */
                 "punpcklbw %%mm6, %%mm0   # mm0 = Sa,Sr,Sg,Sb\n\t" 
                 /* Scale the 8-bit components to 14 bits: (S * 257) >> 2 */
                 "movq %%mm0, %%mm3        # mm3 = S8\n\t" 
                 "psllw $8, %%mm3          # mm3 = S8 * 256\n\t" 
                 "paddw %%mm3, %%mm0       # mm0 = S16\n\t" 
                 "psrlw $1, %%mm0          # mm0 = S15\n\t" 
                 /* Multiple each pixel component by the weight.  Note that
                  * the lower 16-bits of the product are truncated (bad)
                  * creating round-off error in the sum. */
                 "movq %%mm0, %%mm7        # mm5 = S15\n\t" 
                 "pmulhw %%mm1, %%mm0      # mm0 = S15 * W14\n\t" 
                 "pmullw %%mm1, %%mm7      # mm0 = S15 * W14\n\t" 
                 "psrlw $15, %%mm7         # mm0 = S1\n\t" 
                 "paddsw %%mm7, %%mm0      # mm5 = A13 + mm5\n\t" 
                 
                 /* Add the 16-bit components to mm5. */
                 "paddsw %%mm0, %%mm5      # mm5 = A13 + mm5\n\t" 
                 /* Move the pointers to the next weight and pixel */
                 "add $4, %1               # wp++\n\t" 
                 "add $4, %3               # sp++\n\t" 
                 "cmp %2, %1               # wend == wp\n\t" 
                 "jnz .Lasm%=\n\t" 
                 /* end loop */
                 /* Add a rounding bias to the pixel sum. */
                 "paddw %%mm2, %%mm5       # mm5 = A13 + BIAS\n\t" 
                 /* Shift off fractional portion. */
                 "psraw $5, %%mm5          # mm5 = A8\n\t" 
                 /* Pack 16-bit components into lower 4 bytes. */
                 "packuswb %%mm5, %%mm5    # Pack A8 into low 4 bytes.\n\t" 
                 /* Store the word (pixel) in the destination. */
                 "movd %%mm5,(%0)          # dp = word\n" 
                 /* output registers */ 
                 : "=r" (dp) 
                 /* input registers */
                 : "r" (splPtr->weights), 
                   "r" (splPtr->wend), 
                   "r" (sp));
#ifdef notdef
            if (dp->Alpha != 0xFF) {
                fprintf(stdout, "mmx h-alpha=0x%x\n", dp->Alpha);
            }
#endif
            dp++;
        }
        srcRowPtr += srcPtr->pixelsPerRow;
        destRowPtr += destPtr->pixelsPerRow;
    }
    asm volatile ("emms");
    /* Free the memory allocated for horizontal filter weights. */
    Blt_Free(samples);
}
#endif


#ifdef notdef
static void
AssociateColors(Pict *srcPtr)           /* (in/out) picture */
{
    Blt_Pixel *srcRowPtr;
    int y;
    Blt_Pixel mask;

    /* Create mask for alpha component.  We'll use this mask to make sure we
     * don't change the alpha component of a pixel.  */
    mask.u32 = 0;
    mask.Alpha = 0xFF;
    asm volatile (
        /* Generate constants needed below. */
        "pxor %%mm6, %%mm6      # mm6 = 0\n\t"
        "pcmpeqw %%mm5, %%mm5   # mm5 = -1 \n\t"
        "psubw %%mm6, %%mm5     # mm5 = 1,1,1,1\n\t"
        "movd %0, %%mm4         # mm4 = mask\n\t" 
        "psllw $7, %%mm5        # mm5 = ROUND = 128\n" 
        "punpcklbw %%mm6, %%mm4 # mm4 = 0,0,0,FF\n\t" 
        : /* outputs */
        : /* inputs */
          "r" (mask.u32));

    srcRowPtr = srcPtr->bits;
    for (y = 0; y < srcPtr->height; y++) {
        Blt_Pixel *sp, *send;

#ifdef notdef
        for (sp = srcRowPtr, send = sp + srcPtr->width; sp < send; sp += 2) {
            Blt_Pixel *p, *q;
                
            /*
             * Small wins:  
             *
             * We can compute 
             *      dest = (fg * alpha) + (beta * bg);
             * for all RGBA components at once. 
             *
             * Packing unsigned with saturation performs the necessary
             * clamping without the branch misprediction penalty.
             */
            p = sp, q = sp+1;
            asm volatile (
                /* 
                 * mm0 = P
                 * mm1 = Q
                 * mm2 = Pa
                 * mm3 = Qa
                 * mm4 = temp
                 * mm5 = ROUND = 128,128,128,128
                 * mm6 = 0
                 * mm7 = 0,0,0,FF
                 */
                "movd (%0), %%mm0         #  mm0 = P\n\t" 
                "movd (%1), %%mm1         #  mm1 = Q\n\t" 
                "movd %2, %%mm2           #  mm2 = 0,0,0,Pa\n\t" 
                "movd %3, %%mm3           #  mm2 = 0,0,0,Qa\n\t" 
                "punpcklwd %%mm2, %%mm2   #  mm2 = 0,0,Pa,Pa\n\t"
                "punpcklwd %%mm3, %%mm3   #  mm3 = 0,0,Qa,Qa\n\t"
                "punpcklbw %%mm6, %%mm0   #  mm0 = Pa,Pr,Pg,Pb\n\t" 
                "punpcklwd %%mm2, %%mm2   #  mm2 = Pa,Pa,Pa,Pa\n\t"
                "punpcklbw %%mm6, %%mm1   #  mm1 = Qa,Qr,Qg,Qb\n\t" 
                "por %%mm7, %%mm2         #  mm2 = 0xff,Pa,Pa,Pa\n\t"
                "punpcklwd %%mm3, %%mm3   #  mm3 = Qa,Qa,Qa,Qa\n\t"
                "por %%mm7, %%mm3         #  mm2 = 0xff,Qa,Qa,Qa\n\t"
                "pmullw %%mm0, %%mm2      #  mm2 = P*Pa\n\t" 
                "pmullw %%mm1, %%mm3      #  mm3 = Q*Qa\n\t" 
                "paddw %%mm5, %%mm2       #  mm2 = (P*Pa)+ROUND\n\t"
                "paddw %%mm5, %%mm3       #  mm3 = (Q*Qa)+ROUND\n\t"
                "movq %%mm2, %%mm0        #  mm0 = P16\n\t" 
                "movq %%mm3, %%mm1        #  mm1 = Q16\n\t" 
                "psrlw $8, %%mm0          #  mm0 = P16 / 256\n\t"
                "psrlw $8, %%mm1          #  mm1 = Q16 / 256\n\t"
                "paddw %%mm2, %%mm0       #  mm0 = (P16 / 256) + P16\n\t" 
                "paddw %%mm3, %%mm1       #  mm1 = (Q16 / 256) + Q16\n\t" 
                "psrlw $8, %%mm0          #  mm0 = P8 ~= P16 / 257\n\t"
                "psrlw $8, %%mm1          #  mm1 = Q8 ~= Q16 / 257\n\t"
                "packuswb %%mm0, %%mm0    #  Pack 4 low bytes.\n\t" 
                "packuswb %%mm1, %%mm1    #  Pack 4 low bytes.\n\t" 
                "movd %%mm0, (%0)         #  *P = word\n" 
                "movd %%mm1, (%1)         #  *Q = word\n" 
                : /* outputs */
                  "+r" (p), 
                  "+r" (q)
                : /* inputs */
                  "r" ((unsigned int)(p->Alpha)), 
                  "r" ((unsigned int)(q->Alpha)));
        }
        srcRowPtr += srcPtr->pixelsPerRow;
    }
#else 
        for (sp = srcRowPtr, send = sp + srcPtr->width; sp < send; sp++) {
            unsigned int alpha;
                
            /*
             * Small wins:  
             *
             * We can compute 
             *      dest = (src * alpha);
             * for all RGBA components at once. 
             *
             * Packing unsigned with saturation performs the necessary
             * clamping without the branch misprediction penalty.
             */
            alpha = sp->Alpha;
            asm volatile (
                /* 
                 * mm0 = 
                 * mm1 = sp
                 * mm2 = alpha
                 * mm3 = temp
                 * mm4 = 0,0,0,FF
                 * mm5 = ROUND = 128,128,128,128
                 * mm6 = 0
                 * mm7 = 
                 */
                "movd %1, %%mm2           #  mm2 = 0,0,0,A\n\t" 
                "movd (%0), %%mm1         #  mm1 = sp\n\t" 
                "punpcklwd %%mm2, %%mm2   #  mm2 = 0,0,A,A\n\t"
                "punpcklbw %%mm6, %%mm1   #  mm1 = Sa,Sr,Sg,Sb\n\t" 
                "punpcklwd %%mm2, %%mm2   #  mm2 = A,A,A,A\n\t"
                "por %%mm4, %%mm2         #  mm2 = 0xff,A,A,A\n\t"
                "pmullw %%mm1, %%mm2      #  mm2 = S*A\n\t" 
                "paddw %%mm5, %%mm2       #  mm2 = (S*A)+ROUND\n\t"
                "movq %%mm2, %%mm3        #  mm3 = P16\n\t" 
                "psrlw $8, %%mm3          #  mm3 = P16 / 256\n\t"
                "paddw %%mm2, %%mm3       #  mm3 = (P16 / 256) + P16\n\t" 
                "psrlw $8, %%mm3          #  mm3 = P8 ~= P16 / 257\n\t"
                "packuswb %%mm3, %%mm3    #  Pack 4 low bytes.\n\t" 
                "movd %%mm3, (%0)         #  *sp = word\n" :
                "+r" (sp) :
                "r" (alpha));
        }
        srcRowPtr += srcPtr->pixelsPerRow;
    }
#endif
    asm volatile ("emms");
    srcPtr->flags |= BLT_PIC_ASSOCIATED_COLORS;
}

/* 
 * 
 */
static void
UnassociateColors(Pict *srcPtr)         /* (in/out) picture */
{
    Blt_Pixel *srcRowPtr;
    int y;
    Blt_Pixel mask;

    /* Create mask for alpha component.  We'll use this mask to make sure we
     * don't change the alpha component of a pixel.  */
    mask.u32 = 0;
    mask.Alpha = 0xFF;
    asm volatile (
        /* Generate constants needed below. */
        "pxor %%mm6, %%mm6      # mm6 = 0\n\t"
        "pcmpeqw %%mm5, %%mm5   # mm5 = -1 \n\t"
        "psubw %%mm6, %%mm5     # mm5 = 1,1,1,1\n\t"
        "movd %0, %%mm4         # mm4 = mask\n\t" 
        "psllw $7, %%mm5        # mm5 = ROUND = 128\n" 
        "punpcklbw %%mm6, %%mm4 # mm4 = 0,0,0,FF\n\t" 
        : /* inputs */
        : "r" (mask.u32));

    srcRowPtr = srcPtr->bits;
    for (y = 0; y < srcPtr->height; y++) {
        Blt_Pixel *sp, *send;

        for (sp = srcRowPtr, send = sp + srcPtr->width; sp < send; sp++) {
            unsigned int alpha;
                
            /*
             * Small wins:  
             *
             * We can compute 
             *      dest = (fg * alpha) + (beta * bg);
             * for all RGBA components at once. 
             *
             * C = (Ca * ia) + bias >> 16 
             * Packing unsigned with saturation performs the necessary
             * clamping without the branch misprediction penalty.
             */
            alpha = sp->Alpha;
            asm volatile (
                /* 
                 * mm0 = 
                 * mm1 = sp
                 * mm2 = alpha
                 * mm3 = temp
                 * mm4 = 0,0,0,FF
                 * mm5 = ROUND = 128,128,128,128
                 * mm6 = 0
                 * mm7 = 
                 */
                "movd %1, %%mm2           #  mm2 = 0,0,0,IA\n\t" 
                "movd (%0), %%mm1         #  mm1 = sp\n\t" 
                "punpcklwd %%mm2, %%mm2   #  mm2 = 0,0,IA,IA\n\t"
                "punpcklbw %%mm6, %%mm1   #  mm1 = Sa,Sr,Sg,Sb\n\t" 
                "punpcklwd %%mm2, %%mm2   #  mm2 = IA,IA,IA,IA\n\t"
                "por %%mm4, %%mm2         #  mm2 = 0xff,IA,IA,IA\n\t"
                "pmullw %%mm1, %%mm2      #  mm2 = S*IA\n\t" 
                "paddw %%mm5, %%mm2       #  mm2 = (S*A)+ROUND\n\t"
                "movq %%mm2, %%mm3        #  mm3 = P16\n\t" 
                "psrlw $8, %%mm3          #  mm3 = P16 / 256\n\t"
                "paddw %%mm2, %%mm3       #  mm3 = (P16 / 256) + P16\n\t" 
                "psrlw $8, %%mm3          #  mm3 = P8 ~= P16 / 257\n\t"
                "packuswb %%mm3, %%mm3    #  Pack 4 low bytes.\n\t" 
                "movd %%mm3, (%0)         #  *sp = word\n" 
                : "+r" (sp) 
                : "r" (alpha));
        }
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
    srcPtr->flags &= ~BLT_PIC_ASSOCIATED_COLORS;
}
#endif

#ifdef notdef
/* 
 *---------------------------------------------------------------------------
 *
 * CopyPictures --
 *
 *      Creates a copy of the given picture using SSE xmm registers.  
 * 
 * Results: 
 *      None.
 *
 * Side effects: 
 *      The area specified in the source picture is copied to the
 *      destination picture.
 *
 * -------------------------------------------------------------------------- 
 */
static void
CopyPictures(Pict *destPtr, Pict *srcPtr)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;
    int y;

    srcRowPtr  = srcPtr->bits;
    destRowPtr = destPtr->bits;
    for (y = 0; y < srcPtr->height; y++) {
        Blt_Pixel *sp, *send, *dp;

        dp = destRowPtr;
        for (sp = srcRowPtr, send = sp + srcPtr->width; sp < send; sp += 4) {
            asm volatile (
                "movdqa (%1), %%xmm1\n\t"
                "movdqa %%xmm1, (%0)\n\t" 
                : /* outputs */
                  "=r" (dp) 
                : /* inputs */
                  "r" (sp));
            dp += 4;
        }
        srcRowPtr += srcPtr->pixelsPerRow;
        destRowPtr += destPtr->pixelsPerRow;
    }
    destPtr->flags = (srcPtr->flags | BLT_PIC_DIRTY);
    asm volatile ("emms");
}
#endif

#ifdef notdef
static void
BoxCarVertically(Pict *destPtr, Pict *srcPtr, size_t r)
{
    unsigned int x;
    size_t *map;
    int fscale;
    size_t fwidth;                      /* Filter width */
    float s;
    Blt_Pixel mask;

    map = CreateNeighborhoodMap(srcPtr->height, r);
    fwidth = r + r + 1;
    s = 1.0f / fwidth;
    fscale = float2si(s);

    fwidth--;
    /* 
     * mm3 = r16 g16 b16 a16  accumulators
     */
    /* Apply filter to each row. */

    /* Create mask for alpha component.  We'll use this mask to make sure
     * we don't change the alpha component of a pixel.  */
    mask.u32 = 0;
    mask.Alpha = 0xFF;
    asm volatile (
        /* Generate constants needed below. */
        "pxor %%mm6, %%mm6      # mm6 = 0\n\t"
        "pcmpeqw %%mm5, %%mm5   # mm5 = -1 \n\t"
        "psubw %%mm6, %%mm5     # mm5 = 1,1,1,1\n\t"
        "movd %0, %%mm4         # mm4 = mask\n\t" 
        "psllw $7, %%mm5        # mm5 = ROUND = 128\n" 
        "punpcklbw %%mm6, %%mm4 # mm4 = 0,0,0,FF\n\t" 
        : /* inputs */
        : "r" (mask.u32));

    for (x = 0; x < srcPtr->width; x++) {
        Blt_Pixel *dp, *srcColPtr;
        int r, g, b, a;
        unsigned int y;

        srcColPtr = srcPtr->bits + x;
        asm volatile (
            /* Initialize 16 bit accumulators. */
            "pxor %%mm4, %%mm4  # mm4 = 0\n\t");
        /* Prime the pump. */
        for (y = 0; y < fwidth; y++) {
            Blt_Pixel *sp;

            sp = srcColPtr + (srcPtr->pixelsPerRow * map[y]);
            asm volatile (
                /* 
                 * mm0 = 
                 * mm1 = sp
                 * mm4 = accumulator
                 * mm6 = 0
                 * mm7 = 
                 */
                "movd (%0), %%mm1         #  mm1 = sp\n\t" 
                "punpcklbw %%mm6, %%mm1   #  mm1 = Sa,Sr,Sg,Sb\n\t" 
                "paddw %%mm1, %%mm4       #  mm4 = accumulator\n\t"
                : "+r" (sp));
        }
        dp = destPtr->bits + x;
        for (y = 0; y < srcPtr->height; y++) {
            Blt_Pixel *s1, s2;
            int fr, fg, fb, fa;

            s1 = srcColPtr + (srcPtr->pixelsPerRow * map[y + fwidth]);
            s2 = srcColPtr + (srcPtr->pixelsPerRow * map[y]);
            asm volatile (
                /* 
                 * mm0 = 
                 * mm1 = sp
                 * mm2 = alpha
                 * mm3 = temp
                 * mm4 = 0,0,0,FF
                 * mm5 = ROUND = 128,128,128,128
                 * mm6 = 0
                 * mm7 = 
                 */
                "movd (%0), %%mm1         #  mm1 = next\n\t" 
                "movd (%1), %%mm2         #  mm2 = prev\n\t" 
                "punpcklbw %%mm6, %%mm1   #  mm1 = Sa,Sr,Sg,Sb\n\t" 
                "punpcklbw %%mm6, %%mm2   #  mm2 = Sa,Sr,Sg,Sb\n\t" 
                "paddw %%mm1, %%mm4       #  mm4 = accumulator\n\t"
                "movq %%mm4, %%mm5        #  mm3 = P16\n\t" 
                "pmulhw %%mm3, %%mm5      #  mm0 = S15 * W14\n\t"  
                "psrlw $8, %%mm3          #  mm3 = P16 / 256\n\t"
                "paddw %%mm2, %%mm3       #  mm3 = (P16 / 256) + P16\n\t" 
                "psrlw $8, %%mm3          #  mm3 = P8 ~= P16 / 257\n\t"
                "packuswb %%mm3, %%mm3    #  Pack 4 low bytes.\n\t" 
                "movd %%mm3, (%0)         #  *sp = word\n" 
                : "+r" (s1) "+r" (s2) 
                : "r" (alpha));
        }
            r += sp->Red;
            g += sp->Green;
            b += sp->Blue;
            a += sp->Alpha;
            fr = r * fscale;
            fg = g * fscale;
            fb = b * fscale;
            fa = a * fscale;
            dp->Red = (unsigned char)SICLAMP(fr);
            dp->Green = (unsigned char)SICLAMP(fg);
            dp->Blue = (unsigned char)SICLAMP(fb);
            dp->Alpha = (unsigned char)SICLAMP(fa);
            sp = srcColPtr + (srcPtr->pixelsPerRow * map[y]);
            r -= sp->Red;
            g -= sp->Green;
            b -= sp->Blue;
            a -= sp->Alpha;
            dp += destPtr->pixelsPerRow;
        }
    }
    /* Free the memory allocated for filter weights. */
    Blt_Free(map);
}

static void
BoxCarHorizontally(Pict *destPtr, Pict *srcPtr, size_t r)
{
    Blt_Pixel *srcRowPtr, *destRowPtr;
    float s;
    size_t *map;
    int fscale;
    unsigned int y;
    size_t fwidth;              /* Filter width */

    fwidth = r + r + 1;
    map = CreateNeighborhoodMap(srcPtr->width, r);
    s = 1.0f / fwidth;
    fscale = float2si(s);
    destRowPtr = destPtr->bits;
    srcRowPtr = srcPtr->bits;
    fwidth--;
    for (y = 0; y < srcPtr->height; y++) {
        Blt_Pixel *dp;
        unsigned int x;
        int r, g, b, a;
        
        /* Prime the pump. Get sums for each component for the first (fwidth)
         * pixels in the column. */
        r = g = b = a = 0;
        for (x = 0; x < fwidth; x++) {
            Blt_Pixel *sp;

            sp = srcRowPtr + map[x];
            r += sp->Red;
            g += sp->Green;
            b += sp->Blue;
            a += sp->Alpha;
        }
        dp = destRowPtr;
        for (x = 0; x < srcPtr->width; x++) {
            Blt_Pixel *sp;
            int fr, fg, fb, fa;

            sp = srcRowPtr + map[x + fwidth];
            r += sp->Red;
            g += sp->Green;
            b += sp->Blue;
            a += sp->Alpha;
            fr = r * fscale;
            fg = g * fscale;
            fb = b * fscale;
            fa = a * fscale;
            dp->Red = (unsigned char)SICLAMP(fr);
            dp->Green = (unsigned char)SICLAMP(fg);
            dp->Blue = (unsigned char)SICLAMP(fb);
            dp->Alpha = (unsigned char)SICLAMP(fa);
            sp = srcRowPtr + map[x];
            r -= sp->Red;
            g -= sp->Green;
            b -= sp->Blue;
            a -= sp->Alpha;
            dp++;
        }
        destRowPtr += destPtr->pixelsPerRow;
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    /* Free the memory allocated for map. */
    Blt_Free(map);
}
#endif


/* 
 *---------------------------------------------------------------------------
 *
 * BlendPictures --
 *
 *      Composites two pictures (over operation) using SSSE3 operations.
 *      It is assumed that the background and foreground pictures already
 *      have premultiplied alphas.  Blending is therefore implemented as
 *
 *              pixel = fg + (bg * (1 - fgAlpha))
 *              
 *      Pictures are also guaranteed to be quadword aligned (16 bytes).
 *      The stride (pixelsPerRow) is always a multiple of 4 pixels.  This
 *      lets us blend 4 pixels at a time.  We also use Jim Blinn's method
 *      of multiplying two 8-bit numbers togther.
 *
 *      Reference: Jim Blinn's Corner: Dirty Pixels.
 *
 * Results: 
 *      None.
 *
 * -------------------------------------------------------------------------- 
 */

static uint8_t populate_alpha[16] __attribute__((aligned(16))) = {
    /* B G R A B G R A */
    /* The first 4 words, get the first alpha. */
    0xff, 0x03, 0xff, 0x03, 0xff, 0x03, 0xff, 0x03, 
    /* and the second 4 words, get the second alpha. */
    0xff, 0x07, 0xff, 0x07, 0xff, 0x07, 0xff, 0x07,
};

void
Blt_BlendPictures(Pict *destPtr, Pict *srcPtr)
{
    int y;
    Blt_Pixel *destRowPtr, *srcRowPtr;

    if ((srcPtr->flags & BLT_PIC_ASSOCIATED_COLORS) == 0) {
        Blt_AssociateColors(srcPtr);
    }
    if ((destPtr->flags & BLT_PIC_ASSOCIATED_COLORS) == 0) {
        Blt_AssociateColors(destPtr);
    }
    asm volatile (
        "pxor      %%xmm6, %%xmm6       # xmm6 = 0\n\t"
        "movdqa    (%0), %%xmm7         # xmm7 = shuffle map\n\t"
        : /* outputs */
        : /* imputs */
          "r" (populate_alpha));

    destRowPtr = destPtr->bits, srcRowPtr = srcPtr->bits;
    for (y = 0; y < srcPtr->height; y++) {
        Blt_Pixel *dp, *sp, *send;

        dp = destRowPtr;
        for (sp = srcRowPtr, send = sp + srcPtr->width; sp < send; sp += 4) {
            asm volatile (
		"movdqa    (%1), %%xmm0         # xmm0 = [F0][F1][F2][F3]\n\t" 
		"movdqa    (%0), %%xmm1         # xmm1 = [B0][B1][B2][B3]\n\t"
		"movdqa    %%xmm0, %%xmm2       # xmm2 = m0 (fg)\n\t"
                /* Move the upper quadword to lower so that we can use the
                 * same shuffle mask for both pairs of pixels. */
		"movhlps   %%xmm0, %%xmm3       # xmm3 = (F1)\n\t"
                /* Unpack background */
		"movdqa    %%xmm1, %%xmm4       # xmm4 = m1 (bg)\n\t"
		"movhlps   %%xmm1, %%xmm5       # xmm5 = m1 (bg)\n\t"
		"punpcklbw %%xmm6, %%xmm4	# xmm4 = (B1) BGRA BGRA\n\t"
		"punpcklbw %%xmm6, %%xmm5	# xmm5 = (B2) BGRA BGRA\n\t"
                /* Shift color components to upper byte */
                "psllw     $8, %%xmm4           # xmm4 = B G R A\n\t"
                "psllw     $8, %%xmm5           # xmm5 = B G R A\n\t"
                /* Create XOR mask to for beta values. */
                "pcmpeqw   %%xmm1, %%xmm1       # xmm1 = 0xFFFF\n\t"
                "psllw     $8, %%xmm1           # xmm1 = 0xFF00\n\t"
                /* Shuffle bytes in foregound pixels to get alphas. */
		"pshufb	   %%xmm7, %%xmm2       # xmm2 = AAAA AAAA\n\t"
		"pshufb	   %%xmm7, %%xmm3       # xmm3 = AAAA AAAA\n\t"
                /* beta = alpha ^ 0xFF. */
                "pxor      %%xmm1, %%xmm2       # xmm2 = bbbb bbbb\n\t"
                "pxor      %%xmm1, %%xmm3       # xmm3 = bbbb bbbb\n\t"
                /* Multiple each background component by beta. */
		"pmulhuw   %%xmm2, %%xmm4	# xmm4 = beta * B0 B1n\t"
                /* Create rounding bias 128. */
                "pcmpeqw   %%xmm1, %%xmm1       # xmm1 = 0xFFFF\n\t"
		"punpckhbw %%xmm6, %%xmm1	# xmm1 = 00,FF,00,FF\n\t"
                "psrlw     $7, %%xmm1           # xmm1 = 00,01,00,01\n\t"
                "psllw     $7, %%xmm1           # xmm1 = 00,10,00,10 = 128\n\t"
                /* Multiple each background component by beta. */
                /* Something weird here.  If multiple instructions are
                 * consecutive, the result is corrupted. */
		"pmulhuw   %%xmm3, %%xmm5	# xmm5 = beta * B2 B3\n\t"
                 /* Add the bias. */
                "paddsw    %%xmm1, %%xmm4       # xmm4 += bias\n\t"
                "paddsw    %%xmm1, %%xmm5       # xmm5 += bias\n\t"
                /* Save copy of original (bg * beta) + bias. */
                "movdqa    %%xmm4, %%xmm2       # xmm2 = m4\n"
                "movdqa    %%xmm5, %%xmm3       # xmm3 = m5\n"
                /* Approximate dividing by 257. (((x >> 8) + x) >> 8) */
                "psrlw     $8, %%xmm2           # xmm2 = m4 / 256\n\t"
                "psrlw     $8, %%xmm3           # xmm3 = m5 / 256\n\t"
                /* Add original */
                "paddw     %%xmm2, %%xmm4       # (P / 256) + P\n\t"
                "paddw     %%xmm3, %%xmm5       # (P / 256) + P\n\t"
                /* Divide again by 256 */
                "psrlw     $8, %%xmm4           # P / 257\n\t"
                "psrlw     $8, %%xmm5           # P / 257\n\t"
		/* Convert words to bytes  */
		"packuswb  %%xmm5, %%xmm4       # xmm5 = B1 B2 B3 B4\n\t"
		/* Add to background to foreground */
		"paddusb   %%xmm4, %%xmm0	# xmm0 = Bg + Fg\n\t"
		"movdqa    %%xmm0, (%0)     	# Save pixel\n\t"
		: /* outputs */
                  "+r" (dp)
		: /* imputs */
                  "r" (sp));
            dp += 4;
        }
        destRowPtr += destPtr->pixelsPerRow;
        srcRowPtr += srcPtr->pixelsPerRow;
    }
    asm volatile ("emms");
}

#ifdef notdef
/* 
 *---------------------------------------------------------------------------
 *
 * BlankPicture --
 *
 *      Creates a copy of the given picture using SSE xmm registers.  
 *      
 * Results: 
 *      None.
 *
 * Side effects: 
 *      The area specified in the source picture is copied to the
 *      destination picture.
 *
 * -------------------------------------------------------------------------- 
 */
static void
BlankPicture(Pict *destPtr, unsigned int colorValue)
{
    Blt_Pixel *destRowPtr;
    int y;

    asm volatile (
        /* How do you load a xmm register? */
        /* Generate constants needed below. */
        "pxor %%mm6, %%mm6      # mm6 = 0\n\t"
        "pcmpeqw %%mm5, %%mm5   # mm5 = -1 \n\t"
        "psubw %%mm6, %%mm5     # mm5 = 1,1,1,1\n\t"
        "movd %0, %%mm4         # mm4 = C\n\t" 
        "psllw $7, %%mm5        # mm5 = ROUND = 128\n" 
        "punpcklbw %%mm6, %%mm4 # mm4 = 0,0,0,FF\n\t" 
        : /* inputs */
        : "r" (colorValue));
    destRowPtr = destPtr->bits;
    for (y = 0; y < destPtr->height; y++) {
        Blt_Pixel *dp, *dend;

        for (dp = destRowPtr, dend = dp + (destPtr->width & ~3); dp < dend; 
             dp += 4)
            asm volatile (
                "movdqa %%xmm1, (%0)\n\t" 
                : "=r" (dp));
        }
        switch (width & 3) {
        case 3:         dp->u32 = colorValue; dp++;
        case 2:         dp->u32 = colorValue; dp++;
        case 1:         dp->u32 = colorValue; dp++;
        case 0:         break;
        }
        destRowPtr += destPtr->pixelsPerRow;
    }
    destPtr->flags = (srcPtr->flags | BLT_PIC_DIRTY);
    asm volatile ("emms");
}
#endif

static int
HaveCpuId(void)
{
    unsigned int ecx;

    asm volatile (
        /* See if ID instruction is supported. Save a copy of EFLAGS in eax
         * and ecx */
#ifdef __amd64__
        "pushq %%rbx\n\t"
#else 
        "push %%ebx\n\t"
#endif
        "pushf\n\t"
#ifdef __amd64__
        "popq %%rax\n\t" 
#else 
        "pop %%eax\n\t" 
#endif
        "mov %%eax, %%ecx\n\t"
        /* Toggle the CPUID bit in one copy and store to the EFLAGS
         * reg */
        "xorl $0x200000, %%eax\n\t"
#ifdef __amd64__
        "pushq %%rax\n\t"
#else 
        "push %%eax\n\t"
#endif
        "popf\n\t"
        /* Get the (hopefully modified) EFLAGS */
        "pushf\n\t"
#ifdef __amd64__
        "popq %%rax\n\t"
#else 
        "pop %%eax\n\t"
#endif
        /* Compare the result with the previous. */
        "xor %%eax, %%ecx\n\t"
        "movl %%ecx, %0\n\t" 
#ifdef __amd64__
        "popq %%rbx\n\t"
#else 
        "pop %%ebx\n\t"
#endif
        : "=c" (ecx));
    if (ecx & 0x200000) {
        return 1;
    } 
    return 0;
}

static int
cpuid(int eax, struct cpuid *resultPtr)
{
    if (!HaveCpuId()) {
        return 0;
    }
    asm volatile (
#if defined(__i386__) 
        "push %%ebx\n\t"        
#endif
        "movl %0, %%eax\n\t"
        "cpuid\n\t"
#if defined(__i386__) 
        "pop %%ebx\n\t" 
#endif
        : "=a" (resultPtr->eax),
#if defined(__i386__) && defined(__PIC__)
          "=r" (resultPtr->ebx),
#else
          "=b" (resultPtr->ebx),
#endif
          "=c" (resultPtr->ecx),
          "=d" (resultPtr->edx)
        : "a" (eax)
        : "memory");
    return 1;
}

static int
CpuVersion(char *version)
{
    struct cpuid result;

    if (!cpuid(0, &result)) {
        return 0;
    }
    memcpy (version,   &result.ebx, 4);
    memcpy (version+4, &result.edx, 4);
    memcpy (version+8, &result.ecx, 4);
    version[12] = '\0';
#ifdef notdef
    fprintf(stderr, "version=%s\n", version);
#endif
    return 1;
}

static unsigned long
CpuFlags(void)
{
    struct cpuid result;
    unsigned long flags;

    if (!cpuid(1, &result)) {
        return 0L;
    }
#if (SIZEOF_LONG == 8) 
    flags = (((unsigned long)result.ecx) << 32) | result.edx;    
#else 
    flags = result.edx;
#endif
    return flags;
}

static void
PrintFeatures(Tcl_Interp *interp, unsigned long flags)
{
    char version[13];
    Tcl_Obj *objPtr, *listObjPtr;

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **)NULL);
    CpuVersion(version);
    objPtr = Tcl_NewStringObj(version, 12);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    Tcl_AppendElement(interp, version);
    if (flags & FEATURE_MMX) {
        objPtr = Tcl_NewStringObj("mmx", 3);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (flags & FEATURE_MMXEXT) {
        objPtr = Tcl_NewStringObj("mmxext", 6);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (flags & FEATURE_3DNOW) {
        objPtr = Tcl_NewStringObj("3dnow", 5);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (flags & FEATURE_SSE) {
        objPtr = Tcl_NewStringObj("sse", 3);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (flags & FEATURE_SSE2) {
        objPtr = Tcl_NewStringObj("sse2", 4);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
#if (SIZEOF_LONG == 8) 
    if (flags & FEATURE_SSSE3) {
        objPtr = Tcl_NewStringObj("ssse3", 5);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (flags & FEATURE_SSE41) {
        objPtr = Tcl_NewStringObj("sse4.1", 6);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
    if (flags & FEATURE_SSE42) {
        objPtr = Tcl_NewStringObj("sse4.2", 6);
        Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    }
#endif
    Tcl_SetVar2Ex(interp, "::blt::cpu_info", NULL, listObjPtr, 
                  TCL_GLOBAL_ONLY);
}

int
Blt_CpuFeatures(Tcl_Interp *interp, unsigned long *flagsPtr)
{
    unsigned long flags;

    flags = CpuFlags();
    if (flags & FEATURE_MMX) {
#ifdef notdef
        bltPictProcsPtr = &mmxPictureProcs;
#else 
        bltPictProcsPtr->applyPictureToPictureProc = ApplyPictureToPicture;
        bltPictProcsPtr->applyScalarToPictureProc = ApplyScalarToPicture;
        bltPictProcsPtr->tentHorizontallyProc = TentHorizontally;
        bltPictProcsPtr->tentVerticallyProc = TentVertically;
        bltPictProcsPtr->zoomHorizontallyProc = ZoomHorizontally;
        bltPictProcsPtr->zoomVerticallyProc = ZoomVertically;
#ifdef notdef
        bltPictProcsPtr->associateColorsProc = AssociateColors;
#endif
        bltPictProcsPtr->selectPixelsProc = SelectPixels;
#ifdef notdef
        if (flags & FEATURE_SSSE3) {
            bltPictProcsPtr->blendPicturesProc = BlendPictures;
            bltPictProcsPtr->copyPicturesProc = CopyPictures;
        }
#endif
#endif
    }
    if (flagsPtr != NULL) {
        *flagsPtr = flags;
    }
    if (interp != NULL) {
        PrintFeatures(interp, flags);
    }
    return TCL_OK;
}

#else 

int
Blt_CpuFeatures(Tcl_Interp *interp, unsigned long *flagsPtr)
{
    if (flagsPtr != NULL) {
        *flagsPtr = 0L;
    }
    return TCL_OK;
}

#endif /* HAVE_X86_ASM */
