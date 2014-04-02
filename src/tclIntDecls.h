/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/*
 * tclIntDecls.h --
 *
 * This file contains the declarations for all unsupported functions
 * that are exported by the TCL library.  These interfaces are not
 * guaranteed to remain the same between versions.  Use at your own
 * risk.
 *
 *	Copyright 2003-2004 George A Howlett.
 *
 *	Permission is hereby granted, free of charge, to any person
 *	obtaining a copy of this software and associated documentation
 *	files (the "Software"), to deal in the Software without
 *	restriction, including without limitation the rights to use,
 *	copy, modify, merge, publish, distribute, sublicense, and/or
 *	sell copies of the Software, and to permit persons to whom the
 *	Software is furnished to do so, subject to the following
 *	conditions:
 *
 *	The above copyright notice and this permission notice shall be
 *	included in all copies or substantial portions of the
 *	Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 *	KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 *	WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 *	PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 *	OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 *	OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 *	OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file was adapted from TclIntDecls.h of the TCL library distribution.
 *
 *	Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 *	See the file "license.terms" for information on usage and
 *	redistribution of this file, and for a DISCLAIMER OF ALL
 *	WARRANTIES.
 *
 */

#ifndef _TCL_INT_DECLS_H
#define _TCL_INT_DECLS_H

/*
 * WARNING: This file is automatically generated by the tools/genStubs.tcl
 * script.  Any modifications to the function declarations below should be made
 * in the generic/tclInt.decls script.
 */

typedef struct _Tcl_ResolvedVarInfo Tcl_ResolvedVarInfo;

typedef int (Tcl_ResolveCompiledVarProc)(Tcl_Interp *interp, const char *name,
	int length, Tcl_Namespace *context, Tcl_ResolvedVarInfo **rPtr);

typedef int (Tcl_ResolveVarProc)(Tcl_Interp *interp, const char *name, 
	Tcl_Namespace *context, int flags, Tcl_Var *rPtr);

typedef int (Tcl_ResolveCmdProc)(Tcl_Interp *interp, const char *name, 
	Tcl_Namespace *context, int flags, Tcl_Command *rPtr);

/* !BEGIN!: Do not edit below this line. */

/*
 * Exported function declarations:
 */
/* 36 */
extern int		TclGetLong(Tcl_Interp * interp, char *str, 
				   long *longPtr);
/* 69 */
extern char *		TclpAlloc(unsigned int size);
/* 74 */
extern void		TclpFree(char * ptr);
/* 81 */
extern char *		TclpRealloc(char * ptr, unsigned int size);
/* 11 */
extern void             Tcl_AddInterpResolvers(Tcl_Interp *interp, 
				const char *name, Tcl_ResolveCmdProc *cmdProc,
                                Tcl_ResolveVarProc *varProc,
                                Tcl_ResolveCompiledVarProc *compiledVarProc);
#if (_TCL_VERSION < _VERSION(8,5,0)) 
/* 113 */
extern Tcl_Namespace *	Tcl_CreateNamespace(Tcl_Interp * interp, 
				const char *name, ClientData clientData, 
				Tcl_NamespaceDeleteProc * deleteProc);
/* 114 */
extern void		Tcl_DeleteNamespace(Tcl_Namespace * nsPtr);
/* 115 */
extern int		Tcl_Export(Tcl_Interp * interp, 
				Tcl_Namespace * nsPtr, const char * pattern, 
				int resetListFirst);
/* 116 */
extern Tcl_Command	Tcl_FindCommand(Tcl_Interp * interp, const char * name,
				Tcl_Namespace * contextNsPtr, int flags);
/* 117 */
extern Tcl_Namespace *	Tcl_FindNamespace(Tcl_Interp * interp, 
				const char *name, Tcl_Namespace *contextNsPtr,
				int flags);
#endif	/* < 8.5.0 */

/* 120 */
extern Tcl_Var		Tcl_FindNamespaceVar(Tcl_Interp * interp, 
				const char *name, Tcl_Namespace *contextNsPtr,
				int flags);
#if (_TCL_VERSION < _VERSION(8,5,0)) 
/* 124 */
extern Tcl_Namespace *	Tcl_GetCurrentNamespace(Tcl_Interp * interp);
/* 125 */
extern Tcl_Namespace *	Tcl_GetGlobalNamespace(Tcl_Interp * interp);
#endif	/* < 8.5.0 */

/* 128 */
extern void		Tcl_PopCallFrame(Tcl_Interp* interp);
/* 129 */
extern int		Tcl_PushCallFrame(Tcl_Interp* interp, 
				Tcl_CallFrame * framePtr, 
				Tcl_Namespace * nsPtr, int isProcCallFrame);
/* 130 */
extern int              Tcl_RemoveInterpResolvers(Tcl_Interp *interp, 
				const char * name);

/* 131 */
extern void		Tcl_SetNamespaceResolvers(
				Tcl_Namespace *nsPtr, 
				Tcl_ResolveCmdProc *cmdProc, 
				Tcl_ResolveVarProc *varProc, 
				Tcl_ResolveCompiledVarProc *compiledVarProc);
/* 132 */
extern int		TclpHasSockets(Tcl_Interp *interp);

typedef struct TclIntStubs {
    int magic;
    struct TclIntStubHooks *hooks;

    void *tclAccess;		/* 0 */
    void *tclAccessDeleteProc;	/* 1 */
    void *tclAccessInsertProc;	/* 2 */
    void *tclAllocateFreeObjects; /* 3 */
    void *reserved4;
    void *tclCleanupChildren;	/* 5 */
    void *tclCleanupCommand;	/* 6 */
    void *tclCopyAndCollapse;	/* 7 */
    void *tclCopyChannel;	/* 8 */
    void *tclCreatePipeline;	/* 9 */
    void *tclCreateProc;	/* 10 */
    void *tclDeleteCompiledLocalVars; /* 11 */
    void *tclDeleteVars;	/* 12 */
    void *tclDoGlob;		/* 13 */
    void *tclDumpMemoryInfo;	/* 14 */
    void *reserved15;
    void *tclExprFloatError;	/* 16 */
    void *tclFileAttrsCmd;	/* 17 */
    void *tclFileCopyCmd;	/* 18 */
    void *tclFileDeleteCmd;	/* 19 */
    void *tclFileMakeDirsCmd;	/* 20 */
    void *tclFileRenameCmd;	/* 21 */
    void *tclFindElement;	/* 22 */
    void *tclFindProc;		/* 23 */
    void *tclFormatInt;		/* 24 */
    void *tclFreePackageInfo;	/* 25 */
    void *reserved26;
    void *tclGetDate;		/* 27 */
    void *tclpGetDefaultStdChannel; /* 28 */
    void *tclGetElementOfIndexedArray; /* 29 */
    void *reserved30;
    void *tclGetExtension;	/* 31 */
    void *tclGetFrame;		/* 32 */
    void *tclGetInterpProc;	/* 33 */
    void *tclGetIntForIndex;	/* 34 */
    void *tclGetIndexedScalar;  /* 35 */

    int (*tclGetLong)(Tcl_Interp *interp, char *string, long *longPtr);	/* 36 */

    void *tclGetLoadedPackages; /* 37 */
    void *tclGetNamespaceForQualName;  /* 38 */
    void *tclGetObjInterpProc;	/* 39 */
    void *tclGetOpenMode;	/* 40 */
    void *tclGetOriginalCommand; /* 41 */
    void *tclpGetUserHome;	/* 42 */
    void *tclGlobalInvoke;	/* 43 */
    void *tclGuessPackageName;  /* 44 */
    void *tclHideUnsafeCommands; /* 45 */
    void *tclInExit;		/* 46 */
    void *tclIncrElementOfIndexedArray; /* 47 */
    void *tclIncrIndexedScalar; /* 48 */
    void *tclIncrVar2;		/* 49 */
    void *tclInitCompiledLocals;  /* 50 */
    void *tclInterpInit;	/* 51 */
    void *tclInvoke;		/* 52 */
    void *tclInvokeObjectCommand; /* 53 */
    void *tclInvokeStringCommand; /* 54 */
    void *tclIsProc;		/* 55 */
    void *reserved56;
    void *reserved57;
    void *tclLookupVar;		/* 58 */
    void *tclpMatchFiles;	/* 59 */
    void *tclNeedSpace;		/* 60 */
    void *tclNewProcBodyObj;	/* 61 */
    void *tclObjCommandComplete; /* 62 */
    void *tclObjInterpProc;	/* 63 */
    void *tclObjInvoke;		/* 64 */
    void *tclObjInvokeGlobal;	/* 65 */
    void *tclOpenFileChannelDeleteProc;  /* 66 */
    void *tclOpenFileChannelInsertProc;  /* 67 */
    void *tclpAccess;		/* 68 */
    void *tclpAlloc;		/* 69 */
    void *tclpCopyFile;		/* 70 */
    void *tclpCopyDirectory;	/* 71 */
    void *tclpCreateDirectory;	/* 72 */
    void *tclpDeleteFile;	/* 73 */

    void (*tclpFree)(char * ptr); /* 74 */

    void *tclpGetClicks;	/* 75 */
    void *tclpGetSeconds;	/* 76 */
    void *tclpGetTime;		/* 77 */
    void *tclpGetTimeZone;	/* 78 */
    void *tclpListVolumes;	/* 79 */
    void *tclpOpenFileChannel;  /* 80 */

    char *(*tclpRealloc)(char * ptr, unsigned int size); /* 81 */

    void *tclpRemoveDirectory;	/* 82 */
    void *tclpRenameFile;	/* 83 */
    void *reserved84;
    void *reserved85;
    void *reserved86;
    void *reserved87;
    void *tclPrecTraceProc;	/* 88 */
    void *tclPreventAliasLoop;  /* 89 */
    void *reserved90;
    void *tclProcCleanupProc;	/* 91 */
    void *tclProcCompileProc;	/* 92 */
    void *tclProcDeleteProc;	/* 93 */
    void *tclProcInterpProc;	/* 94 */
    void *tclpStat;		/* 95 */
    void *tclRenameCommand;	/* 96 */
    void *tclResetShadowedCmdRefs; /* 97 */
    void *tclServiceIdle;	/* 98 */
    void *tclSetElementOfIndexedArray; /* 99 */
    void *tclSetIndexedScalar;	/* 100 */
    void *tclSetPreInitScript;	/* 101 */
    void *tclSetupEnv;		/* 102 */
    void *tclSockGetPort;	/* 103 */
    void *tclSockMinimumBuffers; /* 104 */
    void *tclStat;		/* 105 */
    void *tclStatDeleteProc;	/* 106 */
    void *tclStatInsertProc;	/* 107 */
    void *tclTeardownNamespace; /* 108 */
    void *tclUpdateReturnInfo;	/* 109 */
    void *reserved110;
    void (*tcl_AddInterpResolvers)(Tcl_Interp *interp, const char *name, 
	Tcl_ResolveCmdProc *cmdProc, Tcl_ResolveVarProc *varProc, 
	Tcl_ResolveCompiledVarProc *compiledVarProc); /* 111 */
    void *tcl_AppendExportList; /* 112 */

    Tcl_Namespace * (*tcl_CreateNamespace)(Tcl_Interp *interp, char *name, 
	ClientData clientData, Tcl_NamespaceDeleteProc *deleteProc); /* 113 */

    void (*tcl_DeleteNamespace) (Tcl_Namespace * nsPtr); /* 114 */

    int (*tcl_Export) (Tcl_Interp *interp, Tcl_Namespace *nsPtr, char *pattern,
	int resetListFirst);	/* 115 */

    Tcl_Command (*tcl_FindCommand) (Tcl_Interp *interp, char *name, 
	Tcl_Namespace *contextNsPtr, int flags); /* 116 */

    Tcl_Namespace *(*tcl_FindNamespace)(Tcl_Interp *interp, char *name, 
	Tcl_Namespace *contextNsPtr, int flags); /* 117 */

    void *tcl_GetInterpResolvers; /* 118 */
    void *tcl_GetNamespaceResolvers; /* 119 */

    Tcl_Var (*tcl_FindNamespaceVar)(Tcl_Interp *interp, char *name, 
	Tcl_Namespace *contextNsPtr, int flags); /* 120 */

    void *tcl_ForgetImport;	/* 121 */
    void *tcl_GetCommandFromObj; /* 122 */
    void *tcl_GetCommandFullName; /* 123 */

    Tcl_Namespace *(*tcl_GetCurrentNamespace)(Tcl_Interp *interp); /* 124 */

    Tcl_Namespace *(*tcl_GetGlobalNamespace)(Tcl_Interp *interp); /* 125 */

    void *tcl_GetVariableFullName; /* 126 */
    void *tcl_Import;		/* 127 */

    void (*tcl_PopCallFrame)(Tcl_Interp *interp); /* 128 */

    int (*tcl_PushCallFrame)(Tcl_Interp *interp, Tcl_CallFrame *framePtr, 
	Tcl_Namespace *nsPtr, int isProcCallFrame); /* 129 */

    int (*tcl_RemoveInterpResolvers)(Tcl_Interp *interp, 
	const char *name); /* 130 */

    void (*tcl_SetNamespaceResolvers) (Tcl_Namespace *nsPtr, 
	Tcl_ResolveCmdProc *cmdProc, Tcl_ResolveVarProc *varProc, 
	Tcl_ResolveCompiledVarProc *compiledVarProc);  /* 131 */

    int (*tclpHasSockets) (Tcl_Interp *interp); /* 132 */
    void *tclpGetDate;		/* 133 */
    void *tclpStrftime;		/* 134 */
    void *tclpCheckStackSpace;  /* 135 */
    void *reserved136;
    void *tclpChdir;		/* 137 */
    void *tclGetEnv;		/* 138 */
    void *tclpLoadFile;		/* 139 */
    void *tclLooksLikeInt;	/* 140 */
    void *tclpGetCwd;		/* 141 */
    void *tclSetByteCodeFromAny; /* 142 */
    void *tclAddLiteralObj;	/* 143 */
    void *tclHideLiteral;	/* 144 */
    void *tclGetAuxDataType;	/* 145 */
    void *tclHandleCreate;	/* 146 */
    void *tclHandleFree;	/* 147 */
    void *tclHandlePreserve;	/* 148 */
    void *tclHandleRelease;	/* 149 */
    void *tclRegAbout;		/* 150 */
    void *tclRegExpRangeUniChar; /* 151 */
    void *tclSetLibraryPath;	/* 152 */
    void *tclGetLibraryPath;	/* 153 */
    void *reserved154;
    void *reserved155;
    void *tclRegError;		/* 156 */
    void *tclVarTraceExists;	/* 157 */
    void *tclSetStartupScriptFileName; /* 158 */
    void *tclGetStartupScriptFileName; /* 159 */
    void *tclpMatchFilesTypes;  /* 160 */
    void *tclChannelTransform;	/* 161 */
    void *tclChannelEventScriptInvoker; /* 162 */
    void *tclGetInstructionTable; /* 163 */
    void *tclExpandCodeArray;	/* 164 */
} TclIntStubs;

extern TclIntStubs *tclIntStubsPtr;

#if defined(USE_TCL_STUBS) && !defined(USE_TCL_STUB_PROCS)

/*
 * Inline function declarations:
 */
#ifndef TclGetLong
#define TclGetLong \
	(tclIntStubsPtr->tclGetLong) /* 36 */
#endif

#ifndef TclpAlloc
#define TclpAlloc \
	(tclIntStubsPtr->tclpAlloc) /* 69 */
#endif

#ifndef TclpFree
#define TclpFree \
	(tclIntStubsPtr->tclpFree) /* 74 */
#endif

#ifndef TclpRealloc
#define TclpRealloc \
	(tclIntStubsPtr->tclpRealloc) /* 81 */
#endif

#ifndef Tcl_AddInterpResolvers
#define Tcl_AddInterpResolvers \
	(tclIntStubsPtr->tcl_AddInterpResolvers) /* 111 */
#endif

#ifndef Tcl_CreateNamespace
#define Tcl_CreateNamespace \
	(tclIntStubsPtr->tcl_CreateNamespace) /* 113 */
#endif

#ifndef Tcl_DeleteNamespace
#define Tcl_DeleteNamespace \
	(tclIntStubsPtr->tcl_DeleteNamespace) /* 114 */
#endif

#ifndef Tcl_Export
#define Tcl_Export \
	(tclIntStubsPtr->tcl_Export) /* 115 */
#endif

#ifndef Tcl_FindCommand
#define Tcl_FindCommand \
	(tclIntStubsPtr->tcl_FindCommand) /* 116 */
#endif

#ifndef Tcl_FindNamespace
#define Tcl_FindNamespace \
	(tclIntStubsPtr->tcl_FindNamespace) /* 117 */
#endif

#ifndef Tcl_FindNamespaceVar
#define Tcl_FindNamespaceVar \
	(tclIntStubsPtr->tcl_FindNamespaceVar) /* 120 */
#endif

#ifndef Tcl_GetCurrentNamespace
#define Tcl_GetCurrentNamespace \
	(tclIntStubsPtr->tcl_GetCurrentNamespace) /* 124 */
#endif

#ifndef Tcl_GetGlobalNamespace
#define Tcl_GetGlobalNamespace \
	(tclIntStubsPtr->tcl_GetGlobalNamespace) /* 125 */
#endif

#ifndef Tcl_PopCallFrame
#define Tcl_PopCallFrame \
	(tclIntStubsPtr->tcl_PopCallFrame) /* 128 */
#endif

#ifndef Tcl_PushCallFrame
#define Tcl_PushCallFrame \
	(tclIntStubsPtr->tcl_PushCallFrame) /* 129 */
#endif

#ifndef Tcl_RemoveInterpResolvers
#define Tcl_RemoveInterpResolvers \
	(tclIntStubsPtr->tcl_RemoveInterpResolvers) /* 130 */
#endif

#ifndef Tcl_SetNamespaceResolvers
#define Tcl_SetNamespaceResolvers \
	(tclIntStubsPtr->tcl_SetNamespaceResolvers) /* 131 */
#endif
#ifndef TclpHasSockets
#define TclpHasSockets \
	(tclIntStubsPtr->tclpHasSockets) /* 132 */
#endif

#endif 
#endif /* _TCL_INT_DECLS_H */
