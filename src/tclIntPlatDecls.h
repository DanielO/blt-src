/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * tclIntPlatDecls.h --
 *
 * This file contains the declarations for all platform dependent
 * unsupported functions that are exported by the TCL library.  These
 * * interfaces are not guaranteed to remain the same between
 * versions.  Use at your own risk.
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
 * This file was adapted from tclIntPlatDecls.h of the TCL library distribution.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.  All rights reserved.
 *
 */

#ifndef _TCLINTPLATDECLS
#define _TCLINTPLATDECLS

/*
 * WARNING: This file is automatically generated by the tools/genStubs.tcl
 * script.  Any modifications to the function declarations below should be made
 * in the generic/tclInt.decls script.
 */

/* !BEGIN!: Do not edit below this line. */

/*
 * Exported function declarations:
 */

#ifdef __WIN32__
/* 0 */
extern void TclWinConvertError(DWORD errCode);

/* 4 */
extern HINSTANCE TclWinGetTclInstance(void);
#endif /* __WIN32__ */

typedef struct TclIntPlatStubs {
    int magic;
    struct TclIntPlatStubHooks *hooks;

#ifdef WIN32
    void (*tclWinConvertError)(DWORD errCode); /* 0 */
#else
    void *hook0;
#endif
    void *hook1;
    void *hook2;
    void *hook3;
#ifdef WIN32
    HINSTANCE (*tclWinGetTclInstance)(void); /* 4 */
#else
    void *hook4;
#endif
    void *hook5;
    void *hook6;
    void *hook7;
    void *hook8;
    void *hook9;
    void *hook10;
    void *hook11;
    void *hook12;
    void *hook13;
    void *hook14;
    void *hook15;
    void *hook16;
    void *hook17;
    void *hook18;
    void *hook19;
    void *hook20;
    void *hook21;
    void *hook22;
    void *hook23;
    void *hook24;
    void *hook25;

} TclIntPlatStubs;

#ifdef __cplusplus
extern "C" {
#endif
extern TclIntPlatStubs *tclIntPlatStubsPtr;
#ifdef __cplusplus
}
#endif

#if defined(USE_TCL_STUBS) && !defined(USE_TCL_STUB_PROCS)

/*
 * Inline function declarations:
 */

#ifdef __WIN32__
#ifndef TclWinConvertError
#define TclWinConvertError \
        (tclIntPlatStubsPtr->tclWinConvertError) /* 0 */
#endif
#ifndef TclWinGetTclInstance
#define TclWinGetTclInstance \
        (tclIntPlatStubsPtr->tclWinGetTclInstance) /* 4 */
#endif
#endif /* __WIN32__ */

#endif /* defined(USE_TCL_STUBS) && !defined(USE_TCL_STUB_PROCS) */

/* !END!: Do not edit above this line. */

#endif /* _TCLINTPLATDECLS */
