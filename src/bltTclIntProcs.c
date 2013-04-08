#define BUILD_BLT_TCL_PROCS 1
#include <bltInt.h>

/* !BEGIN!: Do not edit below this line. */

BltTclIntProcs bltTclIntProcs = {
    TCL_STUB_MAGIC,
    NULL,
    NULL, /* 0 */
    Blt_DBuffer_VarAppend, /* 1 */
    Blt_DBuffer_Format, /* 2 */
    Blt_DBuffer_Init, /* 3 */
    Blt_DBuffer_Free, /* 4 */
    Blt_DBuffer_Extend, /* 5 */
    Blt_DBuffer_AppendData, /* 6 */
    Blt_DBuffer_Concat, /* 7 */
    Blt_DBuffer_Resize, /* 8 */
    Blt_DBuffer_SetLength, /* 9 */
    Blt_DBuffer_Create, /* 10 */
    Blt_DBuffer_Destroy, /* 11 */
    Blt_DBuffer_LoadFile, /* 12 */
    Blt_DBuffer_SaveFile, /* 13 */
    Blt_DBuffer_AppendByte, /* 14 */
    Blt_DBuffer_AppendShort, /* 15 */
    Blt_DBuffer_AppendInt, /* 16 */
    Blt_DBuffer_ByteArrayObj, /* 17 */
    Blt_DBuffer_StringObj, /* 18 */
    Blt_DBuffer_String, /* 19 */
    Blt_DBuffer_Base64Decode, /* 20 */
    Blt_DBuffer_Base64Encode, /* 21 */
    Blt_DBuffer_Base64EncodeToObj, /* 22 */
    Blt_Malloc, /* 23 */
    Blt_Realloc, /* 24 */
    Blt_Free, /* 25 */
    Blt_Calloc, /* 26 */
    Blt_Strdup, /* 27 */
    Blt_MallocAbortOnError, /* 28 */
    Blt_CallocAbortOnError, /* 29 */
    Blt_StrdupAbortOnError, /* 30 */
    Blt_DictionaryCompare, /* 31 */
    Blt_GetUid, /* 32 */
    Blt_FreeUid, /* 33 */
    Blt_FindUid, /* 34 */
    Blt_CreatePipeline, /* 35 */
    Blt_InitHexTable, /* 36 */
    Blt_DStringAppendElements, /* 37 */
    Blt_LoadLibrary, /* 38 */
    Blt_Panic, /* 39 */
    Blt_Warn, /* 40 */
    Blt_GetSideFromObj, /* 41 */
    Blt_NameOfSide, /* 42 */
    Blt_OpenFile, /* 43 */
    Blt_ExprDoubleFromObj, /* 44 */
    Blt_ExprIntFromObj, /* 45 */
    Blt_Itoa, /* 46 */
    Blt_Ltoa, /* 47 */
    Blt_Utoa, /* 48 */
    Blt_Dtoa, /* 49 */
    Blt_Base64_Decode, /* 50 */
    Blt_Base64_DecodeToBuffer, /* 51 */
    Blt_Base64_DecodeToObj, /* 52 */
    Blt_Base64_Encode, /* 53 */
    Blt_Base64_EncodeToObj, /* 54 */
    Blt_Base85_Encode, /* 55 */
    Blt_Base16_Encode, /* 56 */
    Blt_IsBase64, /* 57 */
    Blt_GetDoubleFromString, /* 58 */
    Blt_GetDoubleFromObj, /* 59 */
    Blt_GetTimeFromObj, /* 60 */
    Blt_GetTime, /* 61 */
    Blt_GetDateFromObj, /* 62 */
    Blt_GetDate, /* 63 */
    Blt_GetPositionFromObj, /* 64 */
    Blt_GetCountFromObj, /* 65 */
    Blt_SimplifyLine, /* 66 */
    Blt_GetLong, /* 67 */
    Blt_GetLongFromObj, /* 68 */
    Blt_FormatString, /* 69 */
    Blt_LowerCase, /* 70 */
    Blt_GetPlatformId, /* 71 */
    Blt_LastError, /* 72 */
    Blt_NaN, /* 73 */
    Blt_AlmostEquals, /* 74 */
    Blt_GetArrayFromObj, /* 75 */
    Blt_NewArrayObj, /* 76 */
    Blt_RegisterArrayObj, /* 77 */
    Blt_IsArrayObj, /* 78 */
    Blt_Assert, /* 79 */
    Blt_InitCmd, /* 80 */
    Blt_InitCmds, /* 81 */
    Blt_GetOpFromObj, /* 82 */
    Blt_GetVariableNamespace, /* 83 */
    Blt_GetCommandNamespace, /* 84 */
    Blt_EnterNamespace, /* 85 */
    Blt_LeaveNamespace, /* 86 */
    Blt_ParseObjectName, /* 87 */
    Blt_MakeQualifiedName, /* 88 */
    Blt_CommandExists, /* 89 */
    Blt_CreateSpline, /* 90 */
    Blt_EvaluateSpline, /* 91 */
    Blt_FreeSpline, /* 92 */
    Blt_CreateParametricCubicSpline, /* 93 */
    Blt_EvaluateParametricCubicSpline, /* 94 */
    Blt_FreeParametricCubicSpline, /* 95 */
    Blt_CreateCatromSpline, /* 96 */
    Blt_EvaluateCatromSpline, /* 97 */
    Blt_FreeCatromSpline, /* 98 */
    Blt_ComputeNaturalSpline, /* 99 */
    Blt_ComputeQuadraticSpline, /* 100 */
    Blt_ComputeNaturalParametricSpline, /* 101 */
    Blt_ComputeCatromParametricSpline, /* 102 */
    Blt_ParseSwitches, /* 103 */
    Blt_FreeSwitches, /* 104 */
    Blt_SwitchChanged, /* 105 */
    Blt_SwitchInfo, /* 106 */
    Blt_SwitchValue, /* 107 */
    Blt_GetCachedVar, /* 108 */
    Blt_FreeCachedVars, /* 109 */
};

/* !END!: Do not edit above this line. */
