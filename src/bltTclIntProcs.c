/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#define BUILD_BLT_TCL_PROCS 1
#include <bltInt.h>

/* !BEGIN!: Do not edit below this line. */

BltTclIntProcs bltTclIntProcs = {
    TCL_STUB_MAGIC,
    NULL,
    NULL, /* 0 */
    Blt_GetArrayFromObj, /* 1 */
    Blt_NewArrayObj, /* 2 */
    Blt_RegisterArrayObj, /* 3 */
    Blt_IsArrayObj, /* 4 */
    Blt_Assert, /* 5 */
    Blt_DBuffer_VarAppend, /* 6 */
    Blt_DBuffer_Format, /* 7 */
    Blt_DBuffer_Init, /* 8 */
    Blt_DBuffer_Free, /* 9 */
    Blt_DBuffer_Extend, /* 10 */
    Blt_DBuffer_AppendData, /* 11 */
    Blt_DBuffer_AppendString, /* 12 */
    Blt_DBuffer_DeleteData, /* 13 */
    Blt_DBuffer_InsertData, /* 14 */
    Blt_DBuffer_SetFromObj, /* 15 */
    Blt_DBuffer_Concat, /* 16 */
    Blt_DBuffer_Resize, /* 17 */
    Blt_DBuffer_SetLength, /* 18 */
    Blt_DBuffer_Create, /* 19 */
    Blt_DBuffer_Destroy, /* 20 */
    Blt_DBuffer_LoadFile, /* 21 */
    Blt_DBuffer_SaveFile, /* 22 */
    Blt_DBuffer_AppendByte, /* 23 */
    Blt_DBuffer_AppendShort, /* 24 */
    Blt_DBuffer_AppendInt, /* 25 */
    Blt_DBuffer_ByteArrayObj, /* 26 */
    Blt_DBuffer_StringObj, /* 27 */
    Blt_DBuffer_String, /* 28 */
    Blt_DBuffer_Base64Decode, /* 29 */
    Blt_DBuffer_Base64EncodeToObj, /* 30 */
    Blt_DBuffer_AppendBase85, /* 31 */
    Blt_DBuffer_AppendBase64, /* 32 */
    Blt_InitCmd, /* 33 */
    Blt_InitCmds, /* 34 */
    Blt_FreeMesh, /* 35 */
    Blt_GetMeshFromObj, /* 36 */
    Blt_GetMesh, /* 37 */
    Blt_Triangulate, /* 38 */
    Blt_Mesh_CreateNotifier, /* 39 */
    Blt_Mesh_DeleteNotifier, /* 40 */
    Blt_Mesh_Name, /* 41 */
    Blt_Mesh_Type, /* 42 */
    Blt_Mesh_GetVertices, /* 43 */
    Blt_Mesh_GetHull, /* 44 */
    Blt_Mesh_GetExtents, /* 45 */
    Blt_Mesh_GetTriangles, /* 46 */
    Blt_GetVariableNamespace, /* 47 */
    Blt_GetCommandNamespace, /* 48 */
    Blt_EnterNamespace, /* 49 */
    Blt_LeaveNamespace, /* 50 */
    Blt_ParseObjectName, /* 51 */
    Blt_MakeQualifiedName, /* 52 */
    Blt_CommandExists, /* 53 */
    Blt_GetOpFromObj, /* 54 */
    Blt_CreateSpline, /* 55 */
    Blt_EvaluateSpline, /* 56 */
    Blt_FreeSpline, /* 57 */
    Blt_CreateParametricCubicSpline, /* 58 */
    Blt_EvaluateParametricCubicSpline, /* 59 */
    Blt_FreeParametricCubicSpline, /* 60 */
    Blt_CreateCatromSpline, /* 61 */
    Blt_EvaluateCatromSpline, /* 62 */
    Blt_FreeCatromSpline, /* 63 */
    Blt_ComputeNaturalSpline, /* 64 */
    Blt_ComputeQuadraticSpline, /* 65 */
    Blt_ComputeNaturalParametricSpline, /* 66 */
    Blt_ComputeCatromParametricSpline, /* 67 */
    Blt_ParseSwitches, /* 68 */
    Blt_FreeSwitches, /* 69 */
    Blt_SwitchChanged, /* 70 */
    Blt_SwitchInfo, /* 71 */
    Blt_SwitchValue, /* 72 */
    Blt_Malloc, /* 73 */
    Blt_Realloc, /* 74 */
    Blt_Free, /* 75 */
    Blt_Calloc, /* 76 */
    Blt_Strdup, /* 77 */
    Blt_Strndup, /* 78 */
    Blt_MallocAbortOnError, /* 79 */
    Blt_CallocAbortOnError, /* 80 */
    Blt_ReallocAbortOnError, /* 81 */
    Blt_StrdupAbortOnError, /* 82 */
    Blt_StrndupAbortOnError, /* 83 */
    Blt_DictionaryCompare, /* 84 */
    Blt_GetUid, /* 85 */
    Blt_FreeUid, /* 86 */
    Blt_FindUid, /* 87 */
    Blt_CreatePipeline, /* 88 */
    Blt_InitHexTable, /* 89 */
    Blt_DStringAppendElements, /* 90 */
    Blt_LoadLibrary, /* 91 */
    Blt_Panic, /* 92 */
    Blt_Warn, /* 93 */
    Blt_GetSideFromObj, /* 94 */
    Blt_NameOfSide, /* 95 */
    Blt_OpenFile, /* 96 */
    Blt_ExprDoubleFromObj, /* 97 */
    Blt_ExprIntFromObj, /* 98 */
    Blt_Itoa, /* 99 */
    Blt_Ltoa, /* 100 */
    Blt_Utoa, /* 101 */
    Blt_Dtoa, /* 102 */
    Blt_Base64_Decode, /* 103 */
    Blt_Base64_DecodeToBuffer, /* 104 */
    Blt_Base64_DecodeToObj, /* 105 */
    Blt_Base64_EncodeToObj, /* 106 */
    Blt_Base64_MaxBufferLength, /* 107 */
    Blt_Base64_Encode, /* 108 */
    Blt_Base85_MaxBufferLength, /* 109 */
    Blt_Base85_Encode, /* 110 */
    Blt_Base16_Encode, /* 111 */
    Blt_IsBase64, /* 112 */
    Blt_GetDoubleFromString, /* 113 */
    Blt_GetDoubleFromObj, /* 114 */
    Blt_GetTimeFromObj, /* 115 */
    Blt_GetTime, /* 116 */
    Blt_SecondsToDate, /* 117 */
    Blt_DateToSeconds, /* 118 */
    Blt_FormatDate, /* 119 */
    Blt_GetPositionFromObj, /* 120 */
    Blt_GetCountFromObj, /* 121 */
    Blt_SimplifyLine, /* 122 */
    Blt_GetLong, /* 123 */
    Blt_GetLongFromObj, /* 124 */
    Blt_FormatString, /* 125 */
    Blt_LowerCase, /* 126 */
    Blt_UpperCase, /* 127 */
    Blt_GetPlatformId, /* 128 */
    Blt_LastError, /* 129 */
    Blt_NaN, /* 130 */
    Blt_AlmostEquals, /* 131 */
    Blt_ConvertListToList, /* 132 */
    Blt_GetCachedVar, /* 133 */
    Blt_FreeCachedVars, /* 134 */
};

/* !END!: Do not edit above this line. */
