/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#define BUILD_BLT_TK_PROCS 1
#include <bltInt.h>

/* !BEGIN!: Do not edit below this line. */

BltTkIntProcs bltTkIntProcs = {
    TCL_STUB_MAGIC,
    NULL,
    NULL, /* 0 */
    Blt_Draw3DRectangle, /* 1 */
    Blt_Fill3DRectangle, /* 2 */
    Blt_AdjustViewport, /* 3 */
    Blt_GetScrollInfoFromObj, /* 4 */
    Blt_UpdateScrollbar, /* 5 */
    Blt_FreeColorPair, /* 6 */
    Blt_GetPrivateGC, /* 7 */
    Blt_GetPrivateGCFromDrawable, /* 8 */
    Blt_FreePrivateGC, /* 9 */
    Blt_GetWindowFromObj, /* 10 */
    Blt_GetWindowName, /* 11 */
    Blt_GetChildrenFromWindow, /* 12 */
    Blt_GetParentWindow, /* 13 */
    Blt_GetNameUid, /* 14 */
    Blt_SetNameUid, /* 15 */
    Blt_FindChild, /* 16 */
    Blt_FirstChild, /* 17 */
    Blt_NextChild, /* 18 */
    Blt_RelinkWindow, /* 19 */
    Blt_Toplevel, /* 20 */
    Blt_GetPixels, /* 21 */
    Blt_GetXY, /* 22 */
    Blt_DrawArrowOld, /* 23 */
    Blt_DrawArrow, /* 24 */
    Blt_MakeTransparentWindowExist, /* 25 */
    Blt_TranslateAnchor, /* 26 */
    Blt_AnchorPoint, /* 27 */
    Blt_MaxRequestSize, /* 28 */
    Blt_GetWindowId, /* 29 */
    Blt_InitXRandrConfig, /* 30 */
    Blt_SizeOfScreen, /* 31 */
    Blt_RootX, /* 32 */
    Blt_RootY, /* 33 */
    Blt_RootCoordinates, /* 34 */
    Blt_MapToplevelWindow, /* 35 */
    Blt_UnmapToplevelWindow, /* 36 */
    Blt_RaiseToplevelWindow, /* 37 */
    Blt_LowerToplevelWindow, /* 38 */
    Blt_ResizeToplevelWindow, /* 39 */
    Blt_MoveToplevelWindow, /* 40 */
    Blt_MoveResizeToplevelWindow, /* 41 */
    Blt_GetWindowExtents, /* 42 */
    Blt_GetWindowInstanceData, /* 43 */
    Blt_SetWindowInstanceData, /* 44 */
    Blt_DeleteWindowInstanceData, /* 45 */
    Blt_ReparentWindow, /* 46 */
    Blt_GetDrawableAttributes, /* 47 */
    Blt_SetDrawableAttributes, /* 48 */
    Blt_SetDrawableAttributesFromWindow, /* 49 */
    Blt_FreeDrawableAttributes, /* 50 */
    Blt_GetBitmapGC, /* 51 */
    Blt_GetPixmapAbortOnError, /* 52 */
    Blt_ScreenDPI, /* 53 */
    Blt_OldConfigModified, /* 54 */
    Blt_GetLineExtents, /* 55 */
    Blt_GetBoundingBox, /* 56 */
    Blt_ParseTifTags, /* 57 */
    Blt_GetFont, /* 58 */
    Blt_AllocFontFromObj, /* 59 */
    Blt_DrawWithEllipsis, /* 60 */
    Blt_GetFontFromObj, /* 61 */
    Blt_Font_GetMetrics, /* 62 */
    Blt_TextWidth, /* 63 */
    Blt_Font_GetInterp, /* 64 */
    Blt_Font_GetFile, /* 65 */
    Blt_NewTileBrush, /* 66 */
    Blt_NewLinearGradientBrush, /* 67 */
    Blt_NewStripesBrush, /* 68 */
    Blt_NewCheckersBrush, /* 69 */
    Blt_NewRadialGradientBrush, /* 70 */
    Blt_NewConicalGradientBrush, /* 71 */
    Blt_NewColorBrush, /* 72 */
    Blt_GetBrushTypeName, /* 73 */
    Blt_GetBrushName, /* 74 */
    Blt_GetBrushColorName, /* 75 */
    Blt_GetBrushPixel, /* 76 */
    Blt_GetBrushType, /* 77 */
    Blt_GetXColorFromBrush, /* 78 */
    Blt_ConfigurePaintBrush, /* 79 */
    Blt_GetBrushTypeFromObj, /* 80 */
    Blt_FreeBrush, /* 81 */
    Blt_GetPaintBrushFromObj, /* 82 */
    Blt_GetPaintBrush, /* 83 */
    Blt_SetLinearGradientBrushPalette, /* 84 */
    Blt_SetLinearGradientBrushCalcProc, /* 85 */
    Blt_SetLinearGradientBrushColors, /* 86 */
    Blt_SetTileBrushPicture, /* 87 */
    Blt_SetColorBrushColor, /* 88 */
    Blt_SetBrushOrigin, /* 89 */
    Blt_SetBrushOpacity, /* 90 */
    Blt_SetBrushArea, /* 91 */
    Blt_GetBrushAlpha, /* 92 */
    Blt_GetBrushOrigin, /* 93 */
    Blt_GetAssociatedColorFromBrush, /* 94 */
    Blt_IsVerticalLinearBrush, /* 95 */
    Blt_IsHorizontalLinearBrush, /* 96 */
    Blt_PaintRectangle, /* 97 */
    Blt_PaintPolygon, /* 98 */
    Blt_CreateBrushNotifier, /* 99 */
    Blt_DeleteBrushNotifier, /* 100 */
    Blt_GetBg, /* 101 */
    Blt_GetBgFromObj, /* 102 */
    Blt_Bg_BorderColor, /* 103 */
    Blt_Bg_Border, /* 104 */
    Blt_Bg_PaintBrush, /* 105 */
    Blt_Bg_Name, /* 106 */
    Blt_Bg_Free, /* 107 */
    Blt_Bg_DrawRectangle, /* 108 */
    Blt_Bg_FillRectangle, /* 109 */
    Blt_Bg_DrawPolygon, /* 110 */
    Blt_Bg_FillPolygon, /* 111 */
    Blt_Bg_GetOrigin, /* 112 */
    Blt_Bg_SetChangedProc, /* 113 */
    Blt_Bg_SetOrigin, /* 114 */
    Blt_Bg_DrawFocus, /* 115 */
    Blt_Bg_BorderGC, /* 116 */
    Blt_Bg_SetFromBackground, /* 117 */
    Blt_Bg_UnsetClipRegion, /* 118 */
    Blt_Bg_SetClipRegion, /* 119 */
    Blt_Bg_GetColor, /* 120 */
    Blt_3DBorder_SetClipRegion, /* 121 */
    Blt_3DBorder_UnsetClipRegion, /* 122 */
    Blt_DestroyBindingTable, /* 123 */
    Blt_CreateBindingTable, /* 124 */
    Blt_ConfigureBindings, /* 125 */
    Blt_ConfigureBindingsFromObj, /* 126 */
    Blt_PickCurrentItem, /* 127 */
    Blt_DeleteBindings, /* 128 */
    Blt_MoveBindingTable, /* 129 */
    Blt_Afm_SetPrinting, /* 130 */
    Blt_Afm_IsPrinting, /* 131 */
    Blt_Afm_TextWidth, /* 132 */
    Blt_Afm_GetMetrics, /* 133 */
    Blt_Afm_GetPostscriptFamily, /* 134 */
    Blt_Afm_GetPostscriptName, /* 135 */
    Blt_SetDashes, /* 136 */
    Blt_ResetLimits, /* 137 */
    Blt_GetLimitsFromObj, /* 138 */
    Blt_ConfigureInfoFromObj, /* 139 */
    Blt_ConfigureValueFromObj, /* 140 */
    Blt_ConfigureWidgetFromObj, /* 141 */
    Blt_ConfigureComponentFromObj, /* 142 */
    Blt_ConfigModified, /* 143 */
    Blt_FreeOptions, /* 144 */
    Blt_ObjIsOption, /* 145 */
    Blt_GetPixelsFromObj, /* 146 */
    Blt_GetPadFromObj, /* 147 */
    Blt_GetDashesFromObj, /* 148 */
    Blt_Image_IsDeleted, /* 149 */
    Blt_Image_GetMaster, /* 150 */
    Blt_Image_GetType, /* 151 */
    Blt_Image_GetInstanceData, /* 152 */
    Blt_Image_GetMasterData, /* 153 */
    Blt_Image_Name, /* 154 */
    Blt_Image_NameOfType, /* 155 */
    Blt_FreePainter, /* 156 */
    Blt_GetPainter, /* 157 */
    Blt_GetPainterFromDrawable, /* 158 */
    Blt_PainterGC, /* 159 */
    Blt_PainterDepth, /* 160 */
    Blt_SetPainterClipRegion, /* 161 */
    Blt_UnsetPainterClipRegion, /* 162 */
    Blt_PaintPicture, /* 163 */
    Blt_PaintPictureWithBlend, /* 164 */
    Blt_PaintCheckbox, /* 165 */
    Blt_PaintRadioButton, /* 166 */
    Blt_PaintRadioButtonOld, /* 167 */
    Blt_PaintDelete, /* 168 */
    Blt_PaintArrowHead, /* 169 */
    Blt_PaintArrowHead2, /* 170 */
    Blt_PaintChevron, /* 171 */
    Blt_PaintArrow, /* 172 */
    Blt_Ps_Create, /* 173 */
    Blt_Ps_Free, /* 174 */
    Blt_Ps_GetValue, /* 175 */
    Blt_Ps_GetDBuffer, /* 176 */
    Blt_Ps_GetInterp, /* 177 */
    Blt_Ps_GetScratchBuffer, /* 178 */
    Blt_Ps_SetInterp, /* 179 */
    Blt_Ps_Append, /* 180 */
    Blt_Ps_AppendBytes, /* 181 */
    Blt_Ps_VarAppend, /* 182 */
    Blt_Ps_Format, /* 183 */
    Blt_Ps_SetClearBackground, /* 184 */
    Blt_Ps_IncludeFile, /* 185 */
    Blt_Ps_GetPicaFromObj, /* 186 */
    Blt_Ps_GetPadFromObj, /* 187 */
    Blt_Ps_ComputeBoundingBox, /* 188 */
    Blt_Ps_DrawPicture, /* 189 */
    Blt_Ps_Rectangle, /* 190 */
    Blt_Ps_Rectangle2, /* 191 */
    Blt_Ps_SaveFile, /* 192 */
    Blt_Ps_XSetLineWidth, /* 193 */
    Blt_Ps_XSetBackground, /* 194 */
    Blt_Ps_XSetBitmapData, /* 195 */
    Blt_Ps_XSetForeground, /* 196 */
    Blt_Ps_XSetFont, /* 197 */
    Blt_Ps_XSetDashes, /* 198 */
    Blt_Ps_XSetLineAttributes, /* 199 */
    Blt_Ps_XSetStipple, /* 200 */
    Blt_Ps_Polyline, /* 201 */
    Blt_Ps_XDrawLines, /* 202 */
    Blt_Ps_XDrawSegments, /* 203 */
    Blt_Ps_DrawPolyline, /* 204 */
    Blt_Ps_DrawSegments2d, /* 205 */
    Blt_Ps_Draw3DRectangle, /* 206 */
    Blt_Ps_Fill3DRectangle, /* 207 */
    Blt_Ps_XFillRectangle, /* 208 */
    Blt_Ps_XFillRectangles, /* 209 */
    Blt_Ps_XFillPolygon, /* 210 */
    Blt_Ps_DrawPhoto, /* 211 */
    Blt_Ps_XDrawWindow, /* 212 */
    Blt_Ps_DrawText, /* 213 */
    Blt_Ps_DrawBitmap, /* 214 */
    Blt_Ps_XSetCapStyle, /* 215 */
    Blt_Ps_XSetJoinStyle, /* 216 */
    Blt_Ps_PolylineFromXPoints, /* 217 */
    Blt_Ps_Polygon, /* 218 */
    Blt_Ps_SetPrinting, /* 219 */
    Blt_Ps_TextLayout, /* 220 */
    Blt_Ps_TextString, /* 221 */
    Blt_Ps_GetString, /* 222 */
    Blt_DrawText, /* 223 */
    Blt_DrawText2, /* 224 */
    Blt_DrawTextWithRotatedFont, /* 225 */
    Blt_DrawLayout, /* 226 */
    Blt_GetTextExtents, /* 227 */
    Blt_MeasureText, /* 228 */
    Blt_RotateStartingTextPositions, /* 229 */
    Blt_TkTextLayout_CharBbox, /* 230 */
    Blt_TkTextLayout_Compute, /* 231 */
    Blt_TkTextLayout_Draw, /* 232 */
    Blt_TkTextLayout_Free, /* 233 */
    Blt_TkTextLayout_UnderlineSingleChar, /* 234 */
    Blt_Ts_Bitmap, /* 235 */
    Blt_Ts_CreateLayout, /* 236 */
    Blt_Ts_DrawLayout, /* 237 */
    Blt_Ts_DrawText, /* 238 */
    Blt_Ts_FreeStyle, /* 239 */
    Blt_Ts_GetExtents, /* 240 */
    Blt_Ts_ResetStyle, /* 241 */
    Blt_Ts_SetDrawStyle, /* 242 */
    Blt_Ts_TitleLayout, /* 243 */
    Blt_Ts_UnderlineChars, /* 244 */
};

/* !END!: Do not edit above this line. */
