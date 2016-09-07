/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * bltBgexec.c --
 *
 * This module implements a background "exec" command for the BLT toolkit.
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
 */

#define BUILD_BLT_TCL_PROCS 1
#include "bltInt.h"

#ifndef NO_BGEXEC

#include <fcntl.h>
#include <signal.h>

#ifdef HAVE_STDLIB_H
  #include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#ifdef HAVE_STRING_H
  #include <string.h>
#endif /* HAVE_STRING_H */

#ifdef HAVE_CTYPE_H
  #include <ctype.h>
#endif /* HAVE_CTYPE_H */

#ifdef HAVE_SYS_PARAM_H
  #include <sys/param.h>
#endif  /* HAVE_SYS_PARAM_H */

#ifdef HAVE_SYS_TYPES_H
  #include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */

#ifdef HAVE_UNISTD_H
  #include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef MACOSX
  #include <crt_externs.h>
#endif

#include "bltAlloc.h"
#include "bltChain.h"
#include "bltSwitch.h"
#include "bltWait.h"
#include "bltInitCmd.h"

static Tcl_ObjCmdProc BgexecCmdProc;

#define WINDEBUG        0

#if (_TCL_VERSION <  _VERSION(8,1,0)) 
typedef void *Tcl_Encoding;             /* Make up dummy type for
                                         * encoding.  */
#endif

#define ENCODING_ASCII          ((Tcl_Encoding)NULL)
#define ENCODING_BINARY         ((Tcl_Encoding)1)

#ifdef WIN32 
  #ifndef __GNUC__
     #ifdef O_NONBLOCK
        #define O_NONBLOCK        1
     #endif
  #endif /* __GNUC__ */
#endif /* WIN32 */

/*
 *  This module creates a stand in for the old Tcl_CreatePipeline call in
 *  the TCL C library.  The prescribed routine is Tcl_OpenCommandChannel.
 *  But it hides the pids of the pipeline away (unless of course you pry
 *  open the undocumented structure PipeStatus as clientData).  The bigger
 *  problem is that I couldn't figure any way to make one side of the pipe
 *  to be non-blocking.
 */

#ifdef WIN32
  #define read(fd, buf, size)     Blt_AsyncRead((fd),(buf),(size))
  #define close(fd)               CloseHandle((HANDLE)fd)
  #define Tcl_CreateFileHandler   Blt_CreateFileHandler
  #define Tcl_DeleteFileHandler   Blt_DeleteFileHandler
  #define kill                    KillProcess
  #define waitpid                 WaitProcess
#endif  /* WIN32 */

#define BGEXEC_THREAD_KEY       "BLT Bgexec Data"

#define READ_AGAIN      (0)
#define READ_EOF        (-1)
#define READ_ERROR      (-2)

/* The wait-related definitions are taken from tclUnix.h */

#define TRACE_FLAGS (TCL_TRACE_WRITES | TCL_TRACE_UNSETS | TCL_GLOBAL_ONLY)

#define BLOCK_SIZE      1024            /* Size of allocation blocks for
                                         * buffer */
#define DEF_BUFFER_SIZE (BLOCK_SIZE * 8)
#define MAX_READS       100             /* Maximum # of successful reads
                                         * before stopping to let TCL catch
                                         * up on events */
#define SINKOPEN(sinkPtr)  ((sinkPtr)->fd != -1)

#ifndef NSIG
#define NSIG            32              /* # of signals available */
#endif /*NSIG*/

#ifndef SIGINT
#define SIGINT          2
#endif /* SIGINT */

#ifndef SIGQUIT
#define SIGQUIT         3
#endif /* SIGQUIT */

#ifndef SIGKILL
#define SIGKILL         9
#endif /* SIGKILL */

#ifndef SIGTERM
#define SIGTERM         14
#endif /* SIGTERM */

typedef struct {
    int number;
    const char *name;
} SignalToken;

static SignalToken signalTokens[] =
{
#ifdef SIGABRT
    {SIGABRT, "SIGABRT"},
#endif
#ifdef SIGALRM
    {SIGALRM, "SIGALRM"},
#endif
#ifdef SIGBUS
    {SIGBUS, "SIGBUS"},
#endif
#ifdef SIGCHLD
    {SIGCHLD, "SIGCHLD"},
#endif
#if defined(SIGCLD) && (!defined(SIGCHLD) || (SIGCLD != SIGCHLD))
    {SIGCLD, "SIGCLD"},
#endif
#ifdef SIGCONT
    {SIGCONT, "SIGCONT"},
#endif
#if defined(SIGEMT) && (!defined(SIGXCPU) || (SIGEMT != SIGXCPU))
    {SIGEMT, "SIGEMT"},
#endif
#ifdef SIGFPE
    {SIGFPE, "SIGFPE"},
#endif
#ifdef SIGHUP
    {SIGHUP, "SIGHUP"},
#endif
#ifdef SIGILL
    {SIGILL, "SIGILL"},
#endif
#ifdef SIGINT
    {SIGINT, "SIGINT"},
#endif
#ifdef SIGIO
    {SIGIO, "SIGIO"},
#endif
#if defined(SIGIOT) && (!defined(SIGABRT) || (SIGIOT != SIGABRT))
    {SIGIOT, "SIGIOT"},
#endif
#ifdef SIGKILL
    {SIGKILL, "SIGKILL"},
#endif
#if defined(SIGLOST) && (!defined(SIGIOT) || (SIGLOST != SIGIOT)) && (!defined(SIGURG) || (SIGLOST != SIGURG))
    {SIGLOST, "SIGLOST"},
#endif
#ifdef SIGPIPE
    {SIGPIPE, "SIGPIPE"},
#endif
#if defined(SIGPOLL) && (!defined(SIGIO) || (SIGPOLL != SIGIO))
    {SIGPOLL, "SIGPOLL"},
#endif
#ifdef SIGPROF
    {SIGPROF, "SIGPROF"},
#endif
#if defined(SIGPWR) && (!defined(SIGXFSZ) || (SIGPWR != SIGXFSZ))
    {SIGPWR, "SIGPWR"},
#endif
#ifdef SIGQUIT
    {SIGQUIT, "SIGQUIT"},
#endif
#ifdef SIGSEGV
    {SIGSEGV, "SIGSEGV"},
#endif
#ifdef SIGSTOP
    {SIGSTOP, "SIGSTOP"},
#endif
#ifdef SIGSYS
    {SIGSYS, "SIGSYS"},
#endif
#ifdef SIGTERM
    {SIGTERM, "SIGTERM"},
#endif
#ifdef SIGTRAP
    {SIGTRAP, "SIGTRAP"},
#endif
#ifdef SIGTSTP
    {SIGTSTP, "SIGTSTP"},
#endif
#ifdef SIGTTIN
    {SIGTTIN, "SIGTTIN"},
#endif
#ifdef SIGTTOU
    {SIGTTOU, "SIGTTOU"},
#endif
#if defined(SIGURG) && (!defined(SIGIO) || (SIGURG != SIGIO))
    {SIGURG, "SIGURG"},
#endif
#if defined(SIGUSR1) && (!defined(SIGIO) || (SIGUSR1 != SIGIO))
    {SIGUSR1, "SIGUSR1"},
#endif
#if defined(SIGUSR2) && (!defined(SIGURG) || (SIGUSR2 != SIGURG))
    {SIGUSR2, "SIGUSR2"},
#endif
#ifdef SIGVTALRM
    {SIGVTALRM, "SIGVTALRM"},
#endif
#ifdef SIGWINCH
    {SIGWINCH, "SIGWINCH"},
#endif
#ifdef SIGXCPU
    {SIGXCPU, "SIGXCPU"},
#endif
#ifdef SIGXFSZ
    {SIGXFSZ, "SIGXFSZ"},
#endif
    {-1, "unknown signal"},
};

#ifdef TCL_THREADS
static Tcl_Mutex *mutexPtr = NULL;
#endif /* TCL_THREADS */

typedef struct _Bgexec Bgexec;

static Blt_Chain activePipelines;       /* List of active pipelines and
                                         * their bgexec structures. */

/*
 * Sink buffer:
 *   ____________________
 *  |                    |  "size"      current allocated length of buffer.
 *  |                    |
 *  |--------------------|  "fill"      fill point (# characters in buffer).
 *  |  Raw               |
 *  |--------------------|  "mark"      Marks end of cooked characters.
 *  |                    |
 *  |  Cooked            |
 *  |                    |
 *  |                    |
 *  |--------------------|  "lastMark"  Mark end of processed characters.
 *  |                    |
 *  |                    |
 *  |  Processed         |
 *  |                    |
 *  |____________________| 0
 */
typedef struct {
    Bgexec *bgPtr;
    const char *name;                   /* Name of the sink */
    const char *doneVar;                /* Name of a TCL variable
                                         * (malloc'ed) set to the collected
                                         * data of the last UNIX
                                         * subprocess. */
    const char *updateVar;              /* Name of a TCL variable (malloc'ed)
                                         * updated as data is read from the
                                         * pipe. */
    Tcl_Obj *cmdObjPtr;                 /* Start of a TCL command executed
                                         * whenever data is read from the
                                         * pipe. */
    int flags;                  
    Tcl_Encoding encoding;              /* Decoding scheme to use when
                                         * translating data. */
    int fd;                             /* File descriptor of the pipe. */
    int status;
    int echo;                           /* Indicates if the pipeline's stderr
                                         * stream should be echoed */

    unsigned char *bytes;               /* Stores pipeline output (malloc-ed):
                                         * Initially points to static
                                         * storage */
    size_t size;                        /* Size of dynamically allocated
                                         * buffer. */

    size_t fill;                        /* # of bytes read into the
                                         * buffer. Marks the current fill
                                         * point of the buffer. */

    size_t mark;                        /* # of bytes translated
                                         * (cooked). */
    size_t lastMark;                    /* # of bytes as of the last
                                         * read. This indicates the start
                                         * of the new data in the buffer
                                         * since the last time the "update"
                                         * variable was set. */
    size_t maxSize;
    unsigned char staticSpace[DEF_BUFFER_SIZE]; /* Static space */
} Sink;

#define SINK_NOTIFY             (1<<2)

struct _Bgexec {
    const char *statVar;                /* Name of a TCL variable set to
                                         * the exit status of the last
                                         * process. Setting this variable
                                         * triggers the termination of all
                                         * subprocesses (regardless whether
                                         * they have already completed) */
    int signalNum;                      /* If non-zero, indicates the
                                         * signal to send subprocesses when
                                         * cleaning up.*/
    unsigned int flags;                 /* Various bit flags: see below. */
    int interval;                       /* Interval to poll for the exiting
                                         * processes in milliseconds. */
    /* Private */
    Tcl_Interp *interp;                 /* Interpreter containing variables */
    int numProcs;                       /* # of processes in pipeline */
    Blt_Pid *procTable;                 /* Array of process tokens from
                                         * pipeline.  Under Unix, tokens
                                         * are pid_t, while for Win32
                                         * they're handles. */
    Tcl_TimerToken timerToken;          /* Token for timer handler which
                                         * polls for the exit status of
                                         * each sub-process. If zero,
                                         * there's no timer handler
                                         * queued. */
    int *exitCodePtr;                   /* Pointer to a memory location to
                                         * contain the last process' exit
                                         * code. */
    int *donePtr;
    Sink err, out;                      /* Data sinks for pipeline's output
                                         * and error channels. */
    Blt_ChainLink link;
    char *const *env;                   /* Table of overriding environment
                                         * variables. */
};

#define KEEPNEWLINE     (1<<0)          /* Indicates to set TCL output
                                         * variables with trailing newlines
                                         * intact */
#define LINEBUFFERED    (1<<1)          /* Indicates to provide data to
                                         * update variable and update proc
                                         * on a line-by-line * basis. */
#define IGNOREEXITCODE  (1<<2)          /* Don't check for 0 exit status of
                                         * the pipeline.  */
#define TRACED          (1<<3)          /* Indicates that the status
                                         * variable is currently being
                                         * traced. */
#define DETACHED        (1<<4)          /* Indicates that the pipeline is
                                         * detached from standard I/O,
                                         * running in the background. */
#define DONTKILL        (1<<6)          /* Indicates that the detached
                                         * pipeline should not be signaled
                                         * on exit. */

static Blt_SwitchParseProc ObjToEnvironProc;
static Blt_SwitchFreeProc FreeEnvironProc;
static Blt_SwitchCustom environSwitch =
{
    ObjToEnvironProc, NULL, FreeEnvironProc, (ClientData)0,
};

static Blt_SwitchParseProc ObjToSignalProc;
static Blt_SwitchCustom killSignalSwitch =
{
    ObjToSignalProc, NULL, NULL, (ClientData)0,
};

static Blt_SwitchParseProc ObjToEncodingProc;
static Blt_SwitchFreeProc FreeEncodingProc;
static Blt_SwitchCustom encodingSwitch =
{
    ObjToEncodingProc, NULL, FreeEncodingProc, (ClientData)0
};

static Blt_SwitchSpec switchSpecs[] = 
{
    {BLT_SWITCH_CUSTOM,  "-decodeerror",        "encodingName", (char *)NULL,
         Blt_Offset(Bgexec, err.encoding),  0, 0, &encodingSwitch},
    {BLT_SWITCH_CUSTOM,  "-decodeoutput",       "encodingName", (char *)NULL,
        Blt_Offset(Bgexec, out.encoding),   0, 0, &encodingSwitch}, 
    {BLT_SWITCH_BOOLEAN, "-detach",             "bool", (char *)NULL,
        Blt_Offset(Bgexec, flags),          0, DONTKILL},
    {BLT_SWITCH_BOOLEAN, "-echo",               "bool",  (char *)NULL,
         Blt_Offset(Bgexec, err.echo),      0},
    {BLT_SWITCH_CUSTOM, "-environ",             "list",  (char *)NULL,
         Blt_Offset(Bgexec, env),           0, 0, &environSwitch},
    {BLT_SWITCH_STRING,  "-error",              "varName", (char *)NULL,
        Blt_Offset(Bgexec, err.doneVar),    0},
    {BLT_SWITCH_BOOLEAN, "-ignoreexitcode",     "bool", (char *)NULL,
        Blt_Offset(Bgexec, flags),          0, IGNOREEXITCODE},
    {BLT_SWITCH_BOOLEAN, "-keepnewline",        "bool", (char *)NULL,
        Blt_Offset(Bgexec, flags),          0, KEEPNEWLINE}, 
    {BLT_SWITCH_CUSTOM,  "-killsignal",         "signalName", (char *)NULL,
        Blt_Offset(Bgexec, signalNum),      0, 0, &killSignalSwitch},
    {BLT_SWITCH_STRING,  "-lasterror",          "varName", (char *)NULL,
        Blt_Offset(Bgexec, err.updateVar),  0},
    {BLT_SWITCH_STRING,  "-lastoutput",         "varName", (char *)NULL,
        Blt_Offset(Bgexec, out.updateVar),  0},
    {BLT_SWITCH_BOOLEAN, "-linebuffered",       "bool", (char *)NULL,
        Blt_Offset(Bgexec, flags),          0, LINEBUFFERED},
    {BLT_SWITCH_OBJ,     "-onerror",            "cmdString", (char *)NULL,
        Blt_Offset(Bgexec, err.cmdObjPtr),  0},
    {BLT_SWITCH_OBJ,     "-onoutput",           "cmdString", (char *)NULL,
        Blt_Offset(Bgexec, out.cmdObjPtr),  0},
    {BLT_SWITCH_STRING,  "-output",             "varName", (char *)NULL,
        Blt_Offset(Bgexec, out.doneVar),    0},
    {BLT_SWITCH_INT,     "-poll",               "milliseconds", (char *)NULL,
        Blt_Offset(Bgexec, interval),       0},
    {BLT_SWITCH_STRING,  "-update",             "varName", (char *)NULL,
         Blt_Offset(Bgexec, out.updateVar), 0},
    {BLT_SWITCH_END}
};

static Tcl_VarTraceProc VariableProc;
static Tcl_TimerProc TimerProc;
static Tcl_FileProc StdoutProc, StderrProc;
static Tcl_ExitProc BgexecExitProc;

/*
 *---------------------------------------------------------------------------
 *
 * CreateEnviron --
 *
 *      Creates an environ array from the current enviroment variable and
 *      the override variables stored in the given hash table.
 *
 * Results:
 *      Returns an new environ array.  
 *
 * Side effects:
 *      The array is malloc-ed. It is the responsibility of the caller to
 *      free the array when it is done with it.
 *
 *---------------------------------------------------------------------------
 */
static int
CreateEnviron(Tcl_Interp *interp, int objc, Tcl_Obj **objv,
              char *const **envPtrPtr)
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch iter;
    Blt_HashTable varTable;
    int count, i, n, numBytes;
    char **array;
    char *const *envPtr;
    char *p;
#ifdef MACOSX
    char **environ;
    environ = _NSGetEnviron();
#else
    extern char **environ;
#endif
    
    Blt_InitHashTable(&varTable, BLT_STRING_KEYS);
    count = 0;        /* Counter for length of new buffer */
    /* Step 1: Add the new enviroment variables to the table. */
    for (i = 0; i < objc; i += 2) {
        Blt_HashEntry *hPtr;
        const char *name, *value;
        int isNew, length;
        
        name = Tcl_GetStringFromObj(objv[i], &length);
        count += length;
        hPtr = Blt_CreateHashEntry(&varTable, name, &isNew);
        value = Tcl_GetStringFromObj(objv[i+1], &length);
        Blt_SetHashValue(hPtr, value);
        count += length;
        count += 2;                     /* Include '\0' plus '=' */
    }
    /* Step 2:  Add the current environment variables, but don't overwrite
     *          the existing table entries. */
    for (envPtr = environ; *envPtr != NULL; envPtr++) {
        char *eqsign;
        
        eqsign = NULL;
        /* Search for the end of the string, save the equalize. */
        for (p = (char *)*envPtr; *p != '\0'; p++) {
            if ((*p == '=') && (eqsign == NULL)) {
                eqsign = p;
            }
        }
        if (*envPtr == p) {
            break;
        }
        if (eqsign != NULL) {
            Blt_HashEntry *hPtr;
            int isNew;
            
            /* name=value\0 
             *  envPtr  eq     p
             */
            /* Overwrite and restore the equalsign. */
            *eqsign = '\0';
            hPtr = Blt_CreateHashEntry(&varTable, *envPtr, &isNew);
            if (isNew) {
                Blt_SetHashValue(hPtr, eqsign + 1);
                /* Not already in table, add variable as is including the
                 * NUL byte. */
                count += p - *envPtr + 1;    /* Include NUL byte */
            }
            *eqsign = '=';
        }
    }
    count++;                           /* Final NUL byte. */
    assert(count < 100000);
    /* Step 3: Allocate an environment array */
    numBytes = (varTable.numEntries + 1) * sizeof(char **);
    array = Blt_AssertMalloc(numBytes + count * sizeof(char));
    p = (char *)array + numBytes;
    /* Step 4: Add the name/value pairs from the hashtable to the array. */
    n = 0;
    for (hPtr = Blt_FirstHashEntry(&varTable, &iter); hPtr != NULL;
         hPtr = Blt_NextHashEntry(&iter)) {
        const char *name, *value;
        
        name = Blt_GetHashKey(&varTable, hPtr);
        value = Blt_GetHashValue(hPtr);
        numBytes = sprintf(p, "%s=%s", name, value);
        array[n] = p;
        p += numBytes;
        *p++ = '\0';
        n++;
    }
    array[n] = '\0';                       /* Add final NUL byte. */
    Blt_DeleteHashTable(&varTable);
    *envPtrPtr = (char **)array;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToSignalProc --
 *
 *      Convert a Tcl_Obj representing a signal number into its integer
 *      value.
 *
 * Results:
 *      The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToSignalProc(
    ClientData clientData,              /* Not used. */
    Tcl_Interp *interp,                 /* Intrepreter to return results */
    const char *switchName,             /* Not used. */
    Tcl_Obj *objPtr,                    /* Value representation */
    char *record,                       /* Structure record */
    int offset,                         /* Offset to field in structure */
    int flags)                          /* Not used. */
{
    char *string;
    int *signalPtr = (int *)(record + offset);
    int signalNum;

    string = Tcl_GetString(objPtr);
    if (string[0] == '\0') {
        *signalPtr = 0;
        return TCL_OK;
    }
    if (isdigit(UCHAR(string[0]))) {
        if (Tcl_GetIntFromObj(interp, objPtr, &signalNum) != TCL_OK) {
            return TCL_ERROR;
        }
    } else {
        char *name;
        SignalToken *sp;

        name = string;

        /*  Clip off any "SIG" prefix from the signal name */
        if ((name[0] == 'S') && (name[1] == 'I') && (name[2] == 'G')) {
            name += 3;
        }
        signalNum = -1;
        for (sp = signalTokens; sp->number > 0; sp++) {
            if (strcmp(sp->name + 3, name) == 0) {
                signalNum = sp->number;
                break;
            }
        }
        if (signalNum < 0) {
            Tcl_AppendResult(interp, "unknown signal \"", string, "\"",
                (char *)NULL);
            return TCL_ERROR;
        }
    }
    if ((signalNum < 0) || (signalNum > NSIG)) {
        /* Outside range of signals */
        Tcl_AppendResult(interp, "signal number \"", string,
            "\" is out of range", (char *)NULL);
        return TCL_ERROR;
    }
    *signalPtr = signalNum;
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToEncodingProc --
 *
 *      Convert a Tcl_Obj representing a encoding into a Tcl_Encoding.
 *
 * Results:
 *      The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToEncodingProc(
    ClientData clientData,              /* Not used. */
    Tcl_Interp *interp,                 /* Intrepreter to return results */
    const char *switchName,             /* Not used. */
    Tcl_Obj *objPtr,                    /* Value representation */
    char *record,                       /* Structure record */
    int offset,                         /* Offset to field in structure */
    int flags)                          /* Not used. */
{
    Tcl_Encoding *encodingPtr = (Tcl_Encoding *)(record + offset);
    Tcl_Encoding encoding;
    const char *name;

    name = Tcl_GetString(objPtr);
    encoding = ENCODING_ASCII;
    if (name != NULL) {
        if (strcmp(name, "binary") == 0) {
            encoding = ENCODING_BINARY;
        } else {
#if (_TCL_VERSION >= _VERSION(8,1,0)) 
            encoding = Tcl_GetEncoding(interp, name);
            if (encoding == NULL) {
                return TCL_ERROR;
            }
#endif
        }
    }
    if ((*encodingPtr != ENCODING_BINARY) && (*encodingPtr != ENCODING_ASCII)) {
        Tcl_FreeEncoding(*encodingPtr);
    }
    *encodingPtr = encoding;
    return TCL_OK;
}

/*ARGSUSED*/
static void
FreeEncodingProc(ClientData clientData, char *record, int offset, int flags)
{
    Tcl_Encoding *encodingPtr = (Tcl_Encoding *)(record + offset);

    if ((*encodingPtr != ENCODING_BINARY) && (*encodingPtr != ENCODING_ASCII)) {
        Tcl_FreeEncoding(*encodingPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * ObjToEnvironProc --
 *
 *      Convert a Tcl_Obj representing a list of variable names and values
 *      into an environment string.
 *
 * Results:
 *      The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ObjToEnvironProc(ClientData clientData, Tcl_Interp *interp,
                 const char *switchName, Tcl_Obj *objPtr, char *record,
                 int offset, int flags)
{
    char *const **envPtrPtr = (char *const **)(record + offset);
    char *const *env;
    Tcl_Obj **objv;
    int objc;
    
    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
        return TCL_ERROR;
    }
    if (*envPtrPtr != NULL) {
        Blt_Free(*envPtrPtr);
        *envPtrPtr = NULL;
    }
    if (objc & 0x1) {
        Tcl_AppendResult(interp,
                         "odd number of arguments: should be \"name value\"",
                         (char *)NULL);
        return TCL_ERROR;
    }
    if (objc == 0) {
        return TCL_OK;
    }
    if (CreateEnviron(interp, objc, objv, &env) != TCL_OK) {
        return TCL_ERROR;
    }
    *envPtrPtr = env;
    return TCL_OK;
}

/*ARGSUSED*/
static void
FreeEnvironProc(ClientData clientData, char *record, int offset, int flags)
{
    char *const **envPtrPtr = (char *const **)(record + offset);

    if (*envPtrPtr != NULL) {
        Blt_Free(*envPtrPtr);
        *envPtrPtr = NULL;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * GetSinkData --
 *
 *      Returns the data currently saved in the buffer
 *
 *---------------------------------------------------------------------------
 */
static void
GetSinkData(Sink *sinkPtr, unsigned char **dataPtr, size_t *lengthPtr)
{
    size_t length;

    sinkPtr->bytes[sinkPtr->mark] = '\0';
    length = sinkPtr->mark;
    if ((sinkPtr->mark > 0) && (sinkPtr->encoding != ENCODING_BINARY)) {
        unsigned char *last;
        Bgexec *bgPtr;
        
        last = sinkPtr->bytes + (sinkPtr->mark - 1);
        bgPtr = sinkPtr->bgPtr;
        if (((bgPtr->flags & KEEPNEWLINE) == 0) && (*last == '\n')) {
            length--;
        }
    }
    *dataPtr = sinkPtr->bytes;
    *lengthPtr = length;
}

/*
 *---------------------------------------------------------------------------
 *
 * NextBlock --
 *
 *      Returns the next block of data since the last time this routine was
 *      called.
 *
 *---------------------------------------------------------------------------
 */
static unsigned char *
NextBlock(Sink *sinkPtr, int *lengthPtr)
{
    unsigned char *string;
    int length;

    string = sinkPtr->bytes + sinkPtr->lastMark;
    length = sinkPtr->mark - sinkPtr->lastMark;
    sinkPtr->lastMark = sinkPtr->mark;
    if (length > 0) {
        Bgexec *bgPtr;
        
        bgPtr = sinkPtr->bgPtr;
        if ((!(bgPtr->flags & KEEPNEWLINE)) && (string[length-1] == '\n')) {
            length--;
        }
        *lengthPtr = length;
        return string;
    }
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * NextLine --
 *
 *      Returns the next line of data.
 *
 *---------------------------------------------------------------------------
 */
static unsigned char *
NextLine(Sink *sinkPtr, int *lengthPtr)
{
    if (sinkPtr->mark > sinkPtr->lastMark) {
        unsigned char *string;
        int newBytes;
        int i;

        string = sinkPtr->bytes + sinkPtr->lastMark;
        newBytes = sinkPtr->mark - sinkPtr->lastMark;
        for (i = 0; i < newBytes; i++) {
            if (string[i] == '\n') {
                int length;
                Bgexec *bgPtr;
                
                length = i + 1;
                sinkPtr->lastMark += length;
                bgPtr = sinkPtr->bgPtr;
                if ((bgPtr->flags & KEEPNEWLINE) == 0) {
                    length--;           /* Backup over the newline. */
                }
                *lengthPtr = length;
                return string;
            }
        }
        /* Newline not found.  On errors or EOF, also return a partial line. */
        if (sinkPtr->status < 0) {
            *lengthPtr = newBytes;
            sinkPtr->lastMark = sinkPtr->mark;
            return string;
        }
    }
    return NULL;
}
/*
 *---------------------------------------------------------------------------
 *
 * ResetSink --
 *
 *      Removes the bytes already processed from the buffer, possibly
 *      resetting it to empty.  This used when we don't care about keeping
 *      all the data collected from the channel (no -output flag and the
 *      process is detached).
 *
 *---------------------------------------------------------------------------
 */
static void
ResetSink(Sink *sinkPtr)
{ 
    Bgexec *bgPtr;

    bgPtr = sinkPtr->bgPtr;
    if ((bgPtr->flags & LINEBUFFERED) && (sinkPtr->fill > sinkPtr->lastMark)) {
        size_t i, j;

        /* There may be bytes remaining in the buffer, awaiting another
         * read before we see the next newline.  So move the bytes to the
         * front of the array. */

        for (i = 0, j = sinkPtr->lastMark; j < sinkPtr->fill; i++, j++) {
            sinkPtr->bytes[i] = sinkPtr->bytes[j];
        }
        /* Move back the fill point and processed point. */
        sinkPtr->fill -= sinkPtr->lastMark;
        sinkPtr->mark -= sinkPtr->lastMark;
    } else {
        sinkPtr->mark = sinkPtr->fill = 0;
    }
    sinkPtr->lastMark = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * InitSink --
 *
 *      Initializes the buffer's storage.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Storage is cleared.
 *
 *---------------------------------------------------------------------------
 */
static void
InitSink(Bgexec *bgPtr, Sink *sinkPtr, const char *name)
{
    sinkPtr->bgPtr = bgPtr;
    sinkPtr->name = name;
    sinkPtr->echo = FALSE;
    sinkPtr->fd = -1;
    sinkPtr->bytes = sinkPtr->staticSpace;
    sinkPtr->size = DEF_BUFFER_SIZE;
    sinkPtr->encoding = ENCODING_ASCII;
    ResetSink(sinkPtr);
}

static void
ConfigureSink(Bgexec *bgPtr, Sink *sinkPtr)
{
    if ((sinkPtr->cmdObjPtr != NULL) || 
        (sinkPtr->updateVar != NULL) ||
        (sinkPtr->echo)) {
        sinkPtr->flags |= SINK_NOTIFY;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * FreeSinkBuffer --
 *
 *      Frees the buffer's storage, freeing any malloc'ed space.
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
static void
FreeSinkBuffer(Sink *sinkPtr)
{
    if (sinkPtr->bytes != sinkPtr->staticSpace) {
        Blt_Free(sinkPtr->bytes);
        sinkPtr->bytes = sinkPtr->staticSpace;
    }
    sinkPtr->fd = -1;
}


/*
 *---------------------------------------------------------------------------
 *
 * ExtendSinkBuffer --
 *
 *      Doubles the size of the current buffer.
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
static ssize_t
ExtendSinkBuffer(Sink *sinkPtr)
{
    unsigned char *bytes;
    size_t newSize;
    /*
     * Allocate a new array, double the old size
     */
    newSize = sinkPtr->size + sinkPtr->size;
    sinkPtr->bytes[sinkPtr->mark] = '\0';
    if (sinkPtr->bytes == sinkPtr->staticSpace) {
        bytes = Blt_Malloc(sizeof(unsigned char) * newSize);
    } else {
        bytes = Blt_Realloc(sinkPtr->bytes, sizeof(unsigned char) * newSize);
    }
    if (bytes != NULL) {
        if (sinkPtr->bytes == sinkPtr->staticSpace) {
            unsigned char *sp, *dp, *send;

            dp = bytes;
            for (sp = sinkPtr->bytes, send = sp + sinkPtr->fill; sp < send; 
                 /*empty*/) {
                *dp++ = *sp++;
            }
        }
        sinkPtr->bytes = bytes;
        sinkPtr->size = newSize;
        return (sinkPtr->size - sinkPtr->fill); /* Return bytes left. */
    }
    sinkPtr->bytes[sinkPtr->mark] = '\0';
    return -1;
}

/*
 *---------------------------------------------------------------------------
 *
 * ReadBytes --
 *
 *      Reads and appends any available data from a given file descriptor
 *      to the buffer.
 *
 * Results:
 *      Returns TCL_OK when EOF is found, TCL_RETURN if reading data would
 *      block, and TCL_ERROR if an error occurred.
 *
 *---------------------------------------------------------------------------
 */
static int
ReadBytes(Sink *sinkPtr)
{
    int i;
    ssize_t numBytes;

    /*
     *  Worry about indefinite postponement.
     *
     *  Typically we want to stay in the read loop as long as it takes to
     *  collect all the data that's currently available.  But if it's
     *  coming in at a constant high rate, we need to arbitrarily break out
     *  at some point. This allows for both setting the update variable and
     *  the Tk program to handle idle events.
     */
    numBytes = 0;
    for (i = 0; i < MAX_READS; i++) {
        ssize_t bytesLeft;
        unsigned char *array;

        /* Allocate a larger buffer when the number of remaining bytes is
         * below the threshold BLOCK_SIZE.  */

        bytesLeft = sinkPtr->size - sinkPtr->fill;

        if (bytesLeft < BLOCK_SIZE) {
            bytesLeft = ExtendSinkBuffer(sinkPtr);
            if (bytesLeft < 0) {
                errno = ENOMEM;
                sinkPtr->status = READ_ERROR;
                return TCL_ERROR;
            }
        }
        array = sinkPtr->bytes + sinkPtr->fill;

        /* Read into a buffer but make sure we leave room for a trailing
         * NUL byte. */
        numBytes = read(sinkPtr->fd, array, bytesLeft - 1);
        if (numBytes == 0) {            /* EOF: break out of loop. */
            sinkPtr->status = READ_EOF;
            return TCL_BREAK;
        }
        if (numBytes < 0) {
#ifdef O_NONBLOCK
  #define BLOCKED         EAGAIN
#else
  #define BLOCKED         EWOULDBLOCK
#endif /*O_NONBLOCK*/
            /* Either an error has occurred or no more data is currently
             * available to read.  */
            if (errno == BLOCKED) {
                sinkPtr->status = READ_AGAIN;
                return TCL_CONTINUE;
            }
            sinkPtr->bytes[0] = '\0';
            sinkPtr->status = READ_ERROR;
            return TCL_ERROR;
        }
        sinkPtr->fill += numBytes;
        sinkPtr->bytes[sinkPtr->fill] = '\0';
    }
    sinkPtr->status = numBytes;
    return TCL_OK;
}


static void
CloseSink(Sink *sinkPtr)
{
    if (SINKOPEN(sinkPtr)) {
        close(sinkPtr->fd);
        Tcl_DeleteFileHandler(sinkPtr->fd);
        sinkPtr->fd = -1;
        if (sinkPtr->doneVar != NULL) {
            Tcl_Interp *interp;
            unsigned char *data;
            size_t length;

            interp = sinkPtr->bgPtr->interp;
            /* 
             * If data is to be collected, set the "done" variable with the
             * contents of the buffer.
             */
            GetSinkData(sinkPtr, &data, &length);
#if (_TCL_VERSION <  _VERSION(8,1,0)) 
            data[length] = '\0';
            if (Tcl_SetVar(interp, sinkPtr->doneVar, data, 
                           TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG) == NULL) {
                Tcl_BackgroundError(interp);
            }
#else
            if (Tcl_SetVar2Ex(interp, sinkPtr->doneVar, NULL, 
                              Tcl_NewByteArrayObj(data, (int)length),
                              TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG) == NULL) {
                Tcl_BackgroundError(interp);
            }
#endif
        }
#if WINDEBUG
        PurifyPrintf("CloseSink %s: done\n", sinkPtr->name);
#endif
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * CookSink --
 *
 *      For Windows, translate CR/NL combinations to NL alone.
 *
 * Results:
 *      None.
 *
 * Side Effects:
 *      The size of the byte array may shrink and array contents shifted as
 *      carriage returns are found and removed.
 *
 *---------------------------------------------------------------------------
 */
static int
CookSink(Tcl_Interp *interp, Sink *sinkPtr)
{
    unsigned char *srcPtr, *endPtr;
#ifdef WIN32
    int oldMark;

    oldMark = sinkPtr->mark;
#endif
    if (sinkPtr->encoding == ENCODING_BINARY) { /* binary */
        /* No translation needed. */
        sinkPtr->mark = sinkPtr->fill; 
    } else if (sinkPtr->encoding == ENCODING_ASCII) { /* ascii */
#if (_TCL_VERSION <  _VERSION(8,1,0)) 
        /* Convert NUL bytes to question marks. */
        srcPtr = sinkPtr->bytes + sinkPtr->mark;
        endPtr = sinkPtr->bytes + sinkPtr->fill;
        while (srcPtr < endPtr) {
            if (*srcPtr == '\0') {
                *srcPtr = '?';
            }
            srcPtr++;
        }
#endif /* < 8.1.0 */
        /* One-to-one translation. mark == fill. */
        sinkPtr->mark = sinkPtr->fill;
#if (_TCL_VERSION >= _VERSION(8,1,0)) 
    } else { /* unicode. */
        int numSrcCooked, numCooked;
        int result;
        ssize_t spaceLeft, cookedSize, needed;
        size_t numRaw, numLeftOver;
        unsigned char *destPtr;
        unsigned char *raw, *cooked;
        unsigned char leftover[100];
        
        raw = sinkPtr->bytes + sinkPtr->mark;
        numRaw = sinkPtr->fill - sinkPtr->mark;
        /* Ideally, the cooked buffer size should be smaller */
        cookedSize = numRaw * TCL_UTF_MAX + 1;
        cooked = Blt_AssertMalloc(cookedSize);
        result = Tcl_ExternalToUtf(interp, sinkPtr->encoding, 
                        (char *)raw, numRaw, 0, NULL, (char *)cooked, 
                        cookedSize, &numSrcCooked, &numCooked, NULL);
        numLeftOver = 0;
        if (result == TCL_CONVERT_MULTIBYTE) {
            /* 
             * Last multibyte sequence wasn't completed.  Save the extra
             * characters in a temporary buffer.
             */
            numLeftOver = (numRaw - numSrcCooked);
            srcPtr = sinkPtr->bytes + (sinkPtr->mark + numSrcCooked); 
            endPtr = srcPtr + numLeftOver;
            destPtr = leftover;
            while (srcPtr < endPtr) {
                *destPtr++ = *srcPtr++;
            }
        } 
        /*
         * Create a bigger sink.
         */
        needed = numLeftOver + numCooked;
        spaceLeft = sinkPtr->size - sinkPtr->mark;
        if (spaceLeft >= needed) {
            spaceLeft = ExtendSinkBuffer(sinkPtr);
            if (spaceLeft < 0) {
                errno = ENOMEM;
                sinkPtr->status = READ_ERROR;
                return TCL_ERROR;
            }
        }
        assert(spaceLeft > needed);
        /* 
         * Replace the characters from the mark with the translated
         * characters.
         */
        srcPtr = cooked;
        endPtr = cooked + numCooked;
        destPtr = sinkPtr->bytes + sinkPtr->mark;
        while (srcPtr < endPtr) {
            *destPtr++ = *srcPtr++;
        }
        /* Add the number of newly translated characters to the mark */
        sinkPtr->mark += numCooked;
        
        srcPtr = leftover;
        endPtr = leftover + numLeftOver;
        while (srcPtr < endPtr) {
            *destPtr++ = *srcPtr++;
        }
        sinkPtr->fill = sinkPtr->mark + numLeftOver;
#endif /* >= 8.1.0  */
    }
#ifdef WIN32
    /* 
     * Translate CRLF character sequences to LF characters.  We have to do
     * this after converting the string to UTF from UNICODE.
     */
    if (sinkPtr->encoding != ENCODING_BINARY) {
        size_t count;
        unsigned char *destPtr;

        destPtr = srcPtr = sinkPtr->bytes + oldMark;
        endPtr = sinkPtr->bytes + sinkPtr->fill;
        *endPtr = '\0';
        count = 0;
        for (endPtr--; srcPtr < endPtr; srcPtr++) {
            if ((*srcPtr == '\r') && (*(srcPtr + 1) == '\n')) {
                count++;
                continue;               /* Skip the CR in CR/LF sequences. */
            }
            if (srcPtr != destPtr) {
                *destPtr = *srcPtr;     /* Collapse the string, overwriting
                                         * the \r's encountered. */
            }
            destPtr++;
        }
        sinkPtr->mark -= count;
        sinkPtr->fill -= count;
        *destPtr = *srcPtr;             /* Copy the last byte */
        if (*destPtr == '\r') {
            sinkPtr->mark--;
        }
    }
#endif /* WIN32 */
    return TCL_OK;
}

#ifdef WIN32
/*
 *---------------------------------------------------------------------------
 *
 * WaitProcess --
 *
 *      Emulates the waitpid system call under the Win32 API.
 *
 * Results:
 *      Returns 0 if the process is still alive, -1 on an error, or the pid
 *      on a clean close.
 *
 * Side effects:
 *      Unless WNOHANG is set and the wait times out, the process
 *      information record will be deleted and the process handle will be
 *      closed.
 *
 *---------------------------------------------------------------------------
 */
static int
WaitProcess(
    Blt_Pid child,
    int *statusPtr,
    int flags)
{
    DWORD status, exitCode;
    int result;
    int timeout;

#if WINDEBUG
    PurifyPrintf("WAITPID(%x)\n", child.hProcess);
#endif
    *statusPtr = 0;
    if (child.hProcess == INVALID_HANDLE_VALUE) {
        errno = EINVAL;
        return -1;
    }
#if WINDEBUG
    PurifyPrintf("WAITPID: waiting for 0x%x\n", child.hProcess);
#endif
    timeout = (flags & WNOHANG) ? 0 : INFINITE;
    status = WaitForSingleObject(child.hProcess, timeout);
                                 
#if WINDEBUG
    PurifyPrintf("WAITPID: wait status is %d\n", status);
#endif
    switch (status) {
    case WAIT_FAILED:
        errno = ECHILD;
        *statusPtr = ECHILD;
        result = -1;
        break;

    case WAIT_TIMEOUT:
        if (timeout == 0) {
            return 0;                   /* Try again */
        }
        result = 0;
        break;

    default:
    case WAIT_ABANDONED:
    case WAIT_OBJECT_0:
        GetExitCodeProcess(child.hProcess, &exitCode);
        *statusPtr = ((exitCode << 8) & 0xff00);
#if WINDEBUG
        PurifyPrintf("WAITPID: exit code of %d is %d (%x)\n", child.hProcess,
            *statusPtr, exitCode);
#endif
        result = child.pid;
        assert(result != -1);
        break;
    }
    CloseHandle(child.hProcess);
    return result;
}

static BOOL CALLBACK
CloseMessageProc(HWND hWnd, LPARAM lParam)
{
    DWORD pid = 0;
    Blt_Pid *procPtr = (Blt_Pid *)lParam;

    GetWindowThreadProcessId(hWnd, &pid);
    if (pid == procPtr->pid) {
        PostMessage(hWnd, WM_CLOSE, 0, 0);
    }
    return TRUE;
}

/*
 *---------------------------------------------------------------------------
 *
 * KillProcess --
 *
 *      Emulates the UNIX kill system call under Win32 API.
 *
 * Results:
 *      Returns 0 if the process is killed, -1 on an error.
 *
 * Side effects:
 *      Process is terminated.
 *
 *---------------------------------------------------------------------------
 */
static int
KillProcess(Blt_Pid proc, int signal)
{
    DWORD status;

    if ((proc.hProcess == NULL) || (proc.hProcess == INVALID_HANDLE_VALUE)) {
        errno = EINVAL;
        return -1;
    }
#ifdef notdef
    if (GenerateConsoleCrtlEvent(CTRL_C_EVENT, proc.hProcess)) {
        Blt_Warn("can't send control event to process (handle=%d): %s\n",
                proc.hProcess, Blt_LastError());
        EnumWindows(CloseMessageProc, (LPARAM)&proc);
    }
#else
    EnumWindows(CloseMessageProc, (LPARAM)&proc);
#endif
    /* 
     * Wait on the handle. If it signals, great. If it times out, then call
     * TerminateProcess on it.
     *
     * On Windows 95/98 this also has the added benefit of stopping
     * KERNEL32.dll from dumping.  The two second delay is a guess (one
     * second seems to fail intermittently).
     */
    status = WaitForSingleObject(proc.hProcess, 2000);
    if (status == WAIT_OBJECT_0) {
        return 0;
    }
    if (!TerminateProcess(proc.hProcess, 1)) {
#if WINDEBUG
        PurifyPrintf("can't terminate process (handle=%d): %s\n",
                     proc.hProcess, Blt_LastError());
#endif /* WINDEBUG */
        return -1;
    }
    return 0;
}

#endif /* WIN32 */

#if (_TCL_VERSION < _VERSION(8,1,0)) 

static void
NotifyOnUpdate(Tcl_Interp *interp, Sink *sinkPtr, unsigned char *data, 
               int numBytes)
{
    char save;
#if WINDEBUG
    PurifyPrintf("read %s\n", data);
#endif
    if (data[0] == '\0') {
        return;
    }
    if (
    save = data[numBytes];
    data[numBytes] = '\0';
    if (sinkPtr->echo) {
        Tcl_Channel channel;
        
        channel = Tcl_GetStdChannel(TCL_STDERR);
        if (channel == NULL) {
            Tcl_AppendResult(interp, "can't get stderr channel", (char *)NULL);
            Tcl_BackgroundError(interp);
            sinkPtr->echo = FALSE;
        } else {
            Tcl_Write(channel, data, numBytes);
            if (save == '\n') {
                Tcl_Write(channel, "\n", 1);
            }
            Tcl_Flush(channel);
        }
    }
    objPtr = Tcl_NewByteArrayObj(data, (int)numBytes);
    Tcl_IncrRefCount(objPtr);
    if (sinkPtr->cmdObjPtr != NULL) {
        Tcl_Obj *cmdObjPtr, *objPtr;
        int result;

        cmdObjPtr = Tcl_DuplicateObj(sinkPtr->cmdObjPtr);
        Tcl_ListObjAppendElement(interp, cmdObjPtr, objPtr);
        Tcl_IncrRefCount(cmdObjPtr);
        result = Tcl_EvalObjEx(interp, cmdObjPtr, TCL_EVAL_GLOBAL);
        Tcl_DecrRefCount(cmdObjPtr);
        if (result != TCL_OK) {
            Tcl_BackgroundError(interp);
        }
    }
    if (sinkPtr->updateVar != NULL) {
        if (Tcl_SetVar2Ex(interp, sinkPtr->updateVar, NULL, objPtr, 
                TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG) == NULL) {
            Tcl_BackgroundError(interp);
        }
    }
    Tcl_DecrRefCount(objPtr);
    data[numBytes] = save;
}

#else 

static void
NotifyOnUpdate(Tcl_Interp *interp, Sink *sinkPtr, unsigned char *data, 
               int numBytes)
{
    Tcl_Obj *objPtr;

#if WINDEBUG
    PurifyPrintf("read %s\n", data);
#endif
    if ((numBytes == 0) || (data[0] == '\0')) {
        return;
    }
    if (sinkPtr->echo) {
        Tcl_Channel channel;
        
        channel = Tcl_GetStdChannel(TCL_STDERR);
        if (channel == NULL) {
            Tcl_AppendResult(interp, "can't get stderr channel", (char *)NULL);
            Tcl_BackgroundError(interp);
            sinkPtr->echo = FALSE;
        } else {
            if (data[numBytes] == '\n') {
                objPtr = Tcl_NewByteArrayObj(data, numBytes + 1);
            } else {
                objPtr = Tcl_NewByteArrayObj(data, numBytes);
            }
            Tcl_WriteObj(channel, objPtr);
            Tcl_Flush(channel);
        }
    }

    objPtr = Tcl_NewByteArrayObj(data, numBytes);
    Tcl_IncrRefCount(objPtr);
    if (sinkPtr->cmdObjPtr != NULL) {
        Tcl_Obj *cmdObjPtr;
        int result;
        
        cmdObjPtr = Tcl_DuplicateObj(sinkPtr->cmdObjPtr);
        Tcl_ListObjAppendElement(interp, cmdObjPtr, objPtr);
        Tcl_IncrRefCount(cmdObjPtr);
        result = Tcl_EvalObjEx(interp, cmdObjPtr, TCL_EVAL_GLOBAL);
        Tcl_DecrRefCount(cmdObjPtr);
        if (result != TCL_OK) {
            Tcl_BackgroundError(interp);
        }
    }
    if (sinkPtr->updateVar != NULL) {
        if (Tcl_SetVar2Ex(interp, sinkPtr->updateVar, NULL, objPtr, 
                TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG) == NULL) {
            Tcl_BackgroundError(interp);
        }
    }
    Tcl_DecrRefCount(objPtr);
}

#endif /* < 8.1.0 */

static int
CollectData(Sink *sinkPtr)
{
    Bgexec *bgPtr;
    int result;
    
    bgPtr = sinkPtr->bgPtr;
    if ((bgPtr->flags & DETACHED) && (sinkPtr->doneVar == NULL)) {
        ResetSink(sinkPtr);
    }
    result = ReadBytes(sinkPtr);
    if (result == TCL_ERROR) {
        Tcl_BackgroundError(bgPtr->interp);
        return TCL_ERROR;
    }
    result = CookSink(bgPtr->interp, sinkPtr);
    if (result == TCL_ERROR) {
        Tcl_BackgroundError(bgPtr->interp);
        return TCL_ERROR;
    }
    if ((sinkPtr->mark > sinkPtr->lastMark) && (sinkPtr->flags & SINK_NOTIFY)) {
        if (bgPtr->flags & LINEBUFFERED) {
            int length;
            unsigned char *data;

            /* For line-by-line updates, call NotifyOnUpdate for each new
             * complete line.  */
            while ((data = NextLine(sinkPtr, &length)) != NULL) {
                NotifyOnUpdate(bgPtr->interp, sinkPtr, data, length);
            }
        } else {
            int length;
            unsigned char *data;

            length = 0;                 /* Suppress compiler warning. */
            data = NextBlock(sinkPtr, &length);
            NotifyOnUpdate(bgPtr->interp, sinkPtr, data, length);
        }
    }
    if (sinkPtr->status >= 0) {
        return TCL_OK;
    }
    if (sinkPtr->status == READ_ERROR) {
        Tcl_AppendResult(bgPtr->interp, "can't read data from ", sinkPtr->name,
            ": ", Tcl_PosixError(bgPtr->interp), (char *)NULL);
        Tcl_BackgroundError(bgPtr->interp);
        return TCL_ERROR;
    }
#if WINDEBUG
    PurifyPrintf("CollectData %s: done\n", sinkPtr->name);
#endif
    return TCL_RETURN;
}

/*
 *---------------------------------------------------------------------------
 *
 * CreateSinkHandler --
 *
 *      Creates a file handler for the given sink.  The file descriptor is
 *      also set for non-blocking I/O.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The memory allocated to the Bgexec structure released.
 *
 *---------------------------------------------------------------------------
 */
static int
CreateSinkHandler(Sink *sinkPtr, Tcl_FileProc *proc)
{
#ifndef WIN32
    int flags;

    flags = fcntl(sinkPtr->fd, F_GETFL);
#ifdef O_NONBLOCK
    flags |= O_NONBLOCK;
#else
    flags |= O_NDELAY;
#endif
    if (fcntl(sinkPtr->fd, F_SETFL, flags) < 0) {
        Tcl_AppendResult(sinkPtr->bgPtr->interp, "can't set file descriptor ",
                         Blt_Itoa(sinkPtr->fd), " to non-blocking:",
            Tcl_PosixError(sinkPtr->bgPtr->interp), (char *)NULL);
        return TCL_ERROR;
    }
#endif /* WIN32 */
    Tcl_CreateFileHandler(sinkPtr->fd, TCL_READABLE, proc, sinkPtr);
    return TCL_OK;
}

static void
DisableTriggers(Bgexec *bgPtr)          /* Background info record. */
{
    if (bgPtr->flags & TRACED) {
        Tcl_UntraceVar(bgPtr->interp, bgPtr->statVar, TRACE_FLAGS, 
                VariableProc, bgPtr);
        bgPtr->flags &= ~TRACED;
    }
    if (SINKOPEN(&bgPtr->out)) {
        CloseSink(&bgPtr->out);
    }
    if (SINKOPEN(&bgPtr->err)) {
        CloseSink(&bgPtr->err);
    }
    if (bgPtr->timerToken != (Tcl_TimerToken) 0) {
        Tcl_DeleteTimerHandler(bgPtr->timerToken);
        bgPtr->timerToken = 0;
    }
    if (bgPtr->donePtr != NULL) {
        *bgPtr->donePtr = TRUE;
    }
}


/*
 *---------------------------------------------------------------------------
 *
 * FreeBgexec --
 *
 *      Releases the memory allocated for the backgrounded process.
 *
 *---------------------------------------------------------------------------
 */
static void
FreeBgexec(Bgexec *bgPtr)
{
    Blt_FreeSwitches(switchSpecs, (char *)bgPtr, 0);
    if (bgPtr->statVar != NULL) {
        Blt_Free(bgPtr->statVar);
    }
    if (bgPtr->procTable != NULL) {
        Blt_Free(bgPtr->procTable);
    }
    if (bgPtr->env != NULL) {
        Blt_Free(bgPtr->env);
    }
    if (bgPtr->link != NULL) {
        Tcl_MutexLock(mutexPtr);
        Blt_Chain_DeleteLink(activePipelines, bgPtr->link);
        Tcl_MutexUnlock(mutexPtr);
    }
    Blt_Free(bgPtr);
}


/*
 *---------------------------------------------------------------------------
 *
 * KillProcesses --
 *
 *      Cleans up background execution processes and memory.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The memory allocated to the Bgexec structure released.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
KillProcesses(Bgexec *bgPtr)            /* Background info record. */
{
    if (bgPtr->procTable != NULL) {
        int i;

        for (i = 0; i < bgPtr->numProcs; i++) {
            Tcl_Pid tclPid;

            if (bgPtr->signalNum > 0) {
#ifdef WIN32
                kill(bgPtr->procTable[i], bgPtr->signalNum);
#else
                kill(bgPtr->procTable[i].pid, bgPtr->signalNum);
#endif
            }
#ifdef WIN32
            tclPid = (Tcl_Pid)bgPtr->procTable[i].pid;
#else
            {
                unsigned long pid;

                pid = (long)bgPtr->procTable[i].pid;
                tclPid = (Tcl_Pid)pid;
            }
#endif /* WIN32 */
            Tcl_DetachPids(1, &tclPid);
        }
    }
    Tcl_ReapDetachedProcs();
}

/*
 *---------------------------------------------------------------------------
 *
 * DestroyBgexec --
 *
 *      Cleans up background execution processes and memory.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The memory allocated to the Bgexec structure released.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
DestroyBgexec(Bgexec *bgPtr)            /* Background info record. */
{
    DisableTriggers(bgPtr);
    FreeSinkBuffer(&bgPtr->err);
    FreeSinkBuffer(&bgPtr->out);
    KillProcesses(bgPtr);
    FreeBgexec(bgPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * VariableProc --
 *
 *      Kills all currently running subprocesses (given the specified
 *      signal). This procedure is called when the user sets the status
 *      variable associated with this group of child subprocesses.
 *
 * Results:
 *      Always returns NULL.  Only called from a variable trace.
 *
 * Side effects:
 *      The subprocesses are signaled for termination using the specified
 *      kill signal.  Additionally, any resources allocated to track the
 *      subprocesses is released.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static char *
VariableProc(
    ClientData clientData,      /* File output information. */
    Tcl_Interp *interp,         /* Not used. */
    const char *part1,          /* Not used. */
    const char *part2,          /* Not Used. */
    int flags)
{
    if (flags & TRACE_FLAGS) {
        Bgexec *bgPtr = clientData;

        /* Kill all child processes that remain alive. */
        DisableTriggers(bgPtr);
        KillProcesses(bgPtr);
        if (bgPtr->flags & DETACHED) {
            DestroyBgexec(bgPtr);
        }
    }
    return NULL;
}

/*
 *---------------------------------------------------------------------------
 *
 * TimerProc --
 *
 *      This is a timer handler procedure which gets called periodically to
 *      reap any of the sub-processes if they have terminated.  After the
 *      last process has terminated, the contents of standard output are
 *      stored in the output variable, which triggers the cleanup proc
 *      (using a variable trace). The status the last process to exit is
 *      written to the status variable.
 *
 * Results:
 *      None.  Called from the TCL event loop.
 *
 * Side effects:
 *      Many. The contents of procTable is shifted, leaving only those
 *      sub-processes which have not yet terminated.  If there are still
 *      subprocesses left, this procedure is placed in the timer queue
 *      again. Otherwise the output and possibly the status variables are
 *      updated.  The former triggers the cleanup routine which will
 *      destroy the information and resources associated with these
 *      background processes.
 *
 *---------------------------------------------------------------------------
 */
static void
TimerProc(ClientData clientData)
{
    enum PROCESS_STATUS { 
        PROCESS_EXITED, PROCESS_STOPPED, PROCESS_KILLED, PROCESS_UNKNOWN
    } pcode;
    static const char *tokens[] = { 
        "EXITED", "KILLED", "STOPPED", "UNKNOWN"
    };
    Bgexec *bgPtr = clientData;
    Tcl_Obj *listObjPtr, *objPtr;
    Tcl_Interp *interp;
    WAIT_STATUS_TYPE waitStatus, lastStatus;
    char string[200];
    const char *mesg;
    int code;
    int i;
    int numLeft;                        /* # of processes still not
                                         * reaped. */
    unsigned int lastPid;

    interp = bgPtr->interp;
    mesg = NULL;                        /* Suppress compiler warning.  */
    lastPid = (unsigned int)-1;
    *((int *)&waitStatus) = 0;
    *((int *)&lastStatus) = 0;

    numLeft = 0;
    for (i = 0; i < bgPtr->numProcs; i++) {
        int pid;

#ifdef WIN32
        pid = WaitProcess(bgPtr->procTable[i], (int *)&waitStatus, WNOHANG);
#else
        pid = waitpid(bgPtr->procTable[i].pid, (int *)&waitStatus, WNOHANG);
#endif
        if (pid == 0) {                 /* Process has not terminated yet. */
            if (numLeft < i) {
                bgPtr->procTable[numLeft] = bgPtr->procTable[i];
            }
            numLeft++;                  /* Count the # of processes left. */
        } else if (pid != -1) {
            /*
             * Save the status information associated with the subprocess.
             * We'll use it only if this is the last subprocess to be
             * reaped.
             */
            lastStatus = waitStatus;
            lastPid = (unsigned int)pid;
        }
    }
    bgPtr->numProcs = numLeft;

    if ((numLeft > 0) || (SINKOPEN(&bgPtr->out)) || 
        (SINKOPEN(&bgPtr->err))) {
        /* Keep polling for the status of the children that are left */
        bgPtr->timerToken = Tcl_CreateTimerHandler(bgPtr->interval, TimerProc,
           bgPtr);
#if WINDEBUG
        PurifyPrintf("schedule TimerProc(numProcs=%d)\n", numLeft);
#endif
        return;
    }

    /*
     * All child processes have completed.  Set the status variable with
     * the status of the last process reaped.  The status is a list of an
     * error token, the exit status, and a message.
     */

    listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
    code = WEXITSTATUS(lastStatus);
    if (WIFEXITED(lastStatus)) {
        pcode = PROCESS_EXITED;
    } else if (WIFSIGNALED(lastStatus)) {
        pcode = PROCESS_KILLED;
        code = -1;
    } else if (WIFSTOPPED(lastStatus)) {
        pcode = PROCESS_STOPPED;
        code = -1;
    } else {
        pcode = PROCESS_UNKNOWN;
    }
    /* Token */
    objPtr = Tcl_NewStringObj(tokens[pcode], -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    /* Pid */
    objPtr = Tcl_NewLongObj(lastPid);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    /* Exit code. */
    objPtr = Tcl_NewIntObj(code);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    switch(pcode) {
    case PROCESS_EXITED:
        mesg = "child completed normally";
        break;
    case PROCESS_KILLED:
        mesg = Tcl_SignalMsg((int)(WTERMSIG(lastStatus)));
        break;
    case PROCESS_STOPPED:
        mesg = Tcl_SignalMsg((int)(WSTOPSIG(lastStatus)));
        break;
    case PROCESS_UNKNOWN:
        Blt_FormatString(string, 200,
                         "child completed with unknown status 0x%x",
                         *((int *)&lastStatus));
        mesg = string;
        break;
    }
    /* Message */
    objPtr = Tcl_NewStringObj(mesg, -1);
    Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
    Tcl_SetObjErrorCode(interp, listObjPtr);
    if (bgPtr->exitCodePtr != NULL) {
        *bgPtr->exitCodePtr = code;
    }
    DisableTriggers(bgPtr);
    /* It's OK to set the status variable, the variable trace was disabled
     * above. */
    if (Tcl_SetVar2Ex(interp, bgPtr->statVar, NULL, listObjPtr, 
        TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG) == NULL) {
        Tcl_BackgroundError(interp);
    }
    if (bgPtr->flags & DETACHED) {
        DestroyBgexec(bgPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * Stdoutproc --
 *
 *      This procedure is called when output from the detached pipeline is
 *      available.  The output is read and saved in a buffer in the Bgexec
 *      structure.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Data is stored in the buffer.  This character array may be
 *      increased as more space is required to contain the output of the
 *      pipeline.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
StdoutProc(ClientData clientData, int mask)
{
    Sink *sinkPtr = clientData;
    Bgexec *bgPtr;
    int result;
    
    result = CollectData(sinkPtr);
    if (result == TCL_OK) {
        return;
    }
    
    /*
     * Either EOF or an error has occurred.  In either case, close the
     * sink. Note that closing the sink will also remove the file handler,
     * so this routine will not be called again.
     */
    CloseSink(sinkPtr);

    /*
     * If both sinks (stdout and stderr) are closed, this doesn't
     * necessarily mean that the process has terminated.  Set up a timer
     * handler to periodically poll for the exit status of each process.
     * Initially check at the next idle interval.
     */
    bgPtr = sinkPtr->bgPtr;
    if (!SINKOPEN(&bgPtr->err)) {
        bgPtr->timerToken = Tcl_CreateTimerHandler(0, TimerProc, bgPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * StderrProc --
 *
 *      This procedure is called when error from the detached pipeline is
 *      available.  The error is read and saved in a buffer in the Bgexec
 *      structure.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Data is stored in the buffer.  This character array may be
 *      increased as more space is required to contain the stderr of the
 *      pipeline.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
StderrProc(ClientData clientData, int mask)
{
    Sink *sinkPtr = clientData;
    Bgexec *bgPtr;
    int result;
    
    result = CollectData(sinkPtr);
    if (result == TCL_OK) {
        return;
    }
    /*
     * Either EOF or an error has occurred.  In either case, close the
     * sink. Note that closing the sink will also remove the file handler,
     * so this routine will not be called again.
     */
    CloseSink(sinkPtr);

    /*
     * If both sinks (stdout and stderr) are closed, this doesn't
     * necessarily mean that the process has terminated.  Set up a timer
     * handler to periodically poll for the exit status of each process.
     * Initially check at the next idle interval.
     */
    bgPtr = sinkPtr->bgPtr;
    if (!SINKOPEN(&bgPtr->out)) {
        bgPtr->timerToken = Tcl_CreateTimerHandler(0, TimerProc, bgPtr);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * BgexecCmdProc --
 *
 *      This procedure is invoked to process the "bgexec" TCL command.  See
 *      the user documentation for details on what it does.
 *
 * Results:
 *      A standard TCL result.
 *
 * Side effects:
 *      See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
/* ARGSUSED */
static int
BgexecCmdProc(
    ClientData clientData,              /* Not used. */
    Tcl_Interp *interp,                 /* Current interpreter. */
    int objc,                           /* # of arguments. */
    Tcl_Obj *const *objv)               /* Argument strings. */
{
    Bgexec *bgPtr;
    Blt_Pid *pidsPtr;
    char *lastArg;
    int *outFdPtr, *errFdPtr;
    int isDetached;
    int i;
    int numProcs;

    if (objc < 3) {
        Tcl_AppendResult(interp, "wrong # args: should be \"", 
                Tcl_GetString(objv[0]), " varName ?options? command ?arg...?\"",
                (char *)NULL);
        return TCL_ERROR;
    }

    /* Check if the command line is to be run detached (the last argument is
     * "&") */
    lastArg = Tcl_GetString(objv[objc - 1]);
    isDetached = ((lastArg[0] == '&') && (lastArg[1] == '\0'));
    if (isDetached) {
        objc--;                         /* Remove the '&' argument */
    }
    bgPtr = Blt_AssertCalloc(1, sizeof(Bgexec));
    /* Initialize the background information record */
    bgPtr->interp = interp;
    bgPtr->signalNum = SIGTERM;
    bgPtr->numProcs = -1;
    bgPtr->interval = 1000;             /* One second. */
    if (isDetached) {
        bgPtr->flags |= DETACHED;
    }
    bgPtr->statVar = Blt_AssertStrdup(Tcl_GetString(objv[1]));
    Tcl_MutexLock(mutexPtr);
    bgPtr->link = Blt_Chain_Append(activePipelines, bgPtr);
    Tcl_MutexUnlock(mutexPtr);
    InitSink(bgPtr, &bgPtr->out, "stdout");
    InitSink(bgPtr, &bgPtr->err, "stderr");

    /* Try to clean up any detached processes */
    Tcl_ReapDetachedProcs();
    i = Blt_ParseSwitches(interp, switchSpecs, objc - 2, objv + 2, bgPtr, 
                BLT_SWITCH_OBJV_PARTIAL);
    if (i < 0) {
        FreeBgexec(bgPtr);
        return TCL_ERROR;
    }
    i += 2;
    /* Must be at least one argument left as the command to execute. */
    if (objc <= i) {
        Tcl_AppendResult(interp, "missing command to execute: should be \"",
            Tcl_GetString(objv[0]), " varName ?options? command ?arg...?\"", 
                (char *)NULL);
        FreeBgexec(bgPtr);
        return TCL_ERROR;
    }

    ConfigureSink(bgPtr, &bgPtr->out);
    ConfigureSink(bgPtr, &bgPtr->err);

    /* Put a trace on the exit status variable.  The will also allow the
     * user to terminate the pipeline by simply setting the variable.  */
    Tcl_TraceVar(interp, bgPtr->statVar, TRACE_FLAGS, VariableProc, bgPtr);
    bgPtr->flags |= TRACED;

    outFdPtr = errFdPtr = (int *)NULL;
#ifdef WIN32
    if ((!isDetached) || (bgPtr->out.doneVar != NULL) || 
        (bgPtr->out.updateVar != NULL) || (bgPtr->out.cmdObjPtr != NULL)) {
        outFdPtr = &bgPtr->out.fd;
    }
#else
    outFdPtr = &bgPtr->out.fd;
#endif
    if ((bgPtr->err.doneVar != NULL) || (bgPtr->err.updateVar != NULL) ||
        (bgPtr->err.cmdObjPtr != NULL) || (bgPtr->err.echo)) {
        errFdPtr = &bgPtr->err.fd;
    }
    numProcs = Blt_CreatePipeline(interp, objc - i, objv + i, &pidsPtr, 
        (int *)NULL, outFdPtr, errFdPtr, bgPtr->env);
    if (numProcs < 0) {
        goto error;
    }
    bgPtr->procTable = pidsPtr;
    bgPtr->numProcs = numProcs;
    if (bgPtr->out.fd == -1) {
        /* 
         * If output has been redirected, start polling immediately for the
         * exit status of each process.  Normally, this is done only after
         * stdout has been closed by the last process, but here stdout has
         * been redirected. The default polling interval is every 1 second.
         */
        bgPtr->timerToken = Tcl_CreateTimerHandler(bgPtr->interval, TimerProc,
           bgPtr);

    } else if (CreateSinkHandler(&bgPtr->out, StdoutProc) != TCL_OK) {
        goto error;
    }
    if ((bgPtr->err.fd != -1) &&
        (CreateSinkHandler(&bgPtr->err, StderrProc) != TCL_OK)) {
        goto error;
    }
    if (isDetached) {   
        Tcl_Obj *listObjPtr;

        /* If detached, return a list of the child process ids instead of
         * the output of the pipeline. */
        listObjPtr = Tcl_NewListObj(0, (Tcl_Obj **) NULL);
        for (i = 0; i < numProcs; i++) {
            Tcl_Obj *objPtr;
#ifdef WIN32
            objPtr = Tcl_NewLongObj((unsigned long)bgPtr->procTable[i].pid);
#else 
            objPtr = Tcl_NewLongObj(bgPtr->procTable[i].pid);
#endif
            Tcl_ListObjAppendElement(interp, listObjPtr, objPtr);
        }
        Tcl_SetObjResult(interp, listObjPtr);
    } else {
        int exitCode;
        int done;

        bgPtr->exitCodePtr = &exitCode;
        bgPtr->donePtr = &done;

        exitCode = done = 0;
        while (!done) {
            Tcl_DoOneEvent(0);
        }
        DisableTriggers(bgPtr);
        if ((bgPtr->flags & IGNOREEXITCODE) || (exitCode == 0)) {
            if (bgPtr->out.doneVar == NULL) {
                unsigned char *data;
                size_t length;
                
                /* Return the output of the pipeline. */
                GetSinkData(&bgPtr->out, &data, &length);
                assert(length <= UINT_MAX);
#if (_TCL_VERSION <  _VERSION(8,1,0)) 
                data[length] = '\0';
                Tcl_SetObjResult(interp, Tcl_NewStringObj(data, length));
#else
                Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(data, length));
#endif
            }
        } else {
            DestroyBgexec(bgPtr);
            Tcl_AppendResult(interp, "child process exited abnormally",
                (char *)NULL);
            return TCL_ERROR;
        }
        DestroyBgexec(bgPtr);
    }
    return TCL_OK;
  error:
    DestroyBgexec(bgPtr);
    return TCL_ERROR;
}



static void
BgexecExitProc(ClientData clientData)
{
    Blt_ChainLink link, next;

    Tcl_MutexLock(mutexPtr);
    for (link = Blt_Chain_FirstLink(activePipelines); link != NULL; 
         link = next) {
        Bgexec *bgPtr;
        next = Blt_Chain_NextLink(link);

        bgPtr = Blt_Chain_GetValue(link);
        bgPtr->link = NULL;
        if ((bgPtr->flags & DONTKILL) == 0) {
            KillProcesses(bgPtr);
        }
    }
    Blt_Chain_Destroy(activePipelines);
    Tcl_MutexUnlock(mutexPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * Blt_BgexecCmdInitProc --
 *
 *      This procedure is invoked to initialize the "bgexec" TCL command.  See
 *      the user documentation for details on what it does.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See the user documentation.
 *
 *---------------------------------------------------------------------------
 */
int
Blt_BgexecCmdInitProc(Tcl_Interp *interp)
{
    static Blt_CmdSpec cmdSpec = { "bgexec", BgexecCmdProc, };

    if (activePipelines == NULL) {
#ifdef TCL_THREADS
        mutexPtr = Tcl_GetAllocMutex();
#endif
        activePipelines = Blt_Chain_Create();
        Tcl_CreateExitHandler(BgexecExitProc, activePipelines);
    }
    return Blt_InitCmd(interp, "::blt", &cmdSpec);
}

#endif /* NO_BGEXEC */

