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
    Blt_LineRectClip, /* 7 */
    Blt_GetPrivateGC, /* 8 */
    Blt_GetPrivateGCFromDrawable, /* 9 */
    Blt_FreePrivateGC, /* 10 */
    Blt_GetWindowFromObj, /* 11 */
    Blt_GetWindowName, /* 12 */
    Blt_GetChildrenFromWindow, /* 13 */
    Blt_GetParentWindow, /* 14 */
    Blt_FindChild, /* 15 */
    Blt_FirstChild, /* 16 */
    Blt_NextChild, /* 17 */
    Blt_RelinkWindow, /* 18 */
    Blt_Toplevel, /* 19 */
    Blt_GetPixels, /* 20 */
    Blt_NameOfFill, /* 21 */
    Blt_NameOfResize, /* 22 */
    Blt_GetXY, /* 23 */
    Blt_GetProjection, /* 24 */
    Blt_GetProjection2, /* 25 */
    Blt_DrawArrowOld, /* 26 */
    Blt_DrawArrow, /* 27 */
    Blt_MakeTransparentWindowExist, /* 28 */
    Blt_TranslateAnchor, /* 29 */
    Blt_AnchorPoint, /* 30 */
    Blt_MaxRequestSize, /* 31 */
    Blt_GetWindowId, /* 32 */
    Blt_InitXRandrConfig, /* 33 */
    Blt_SizeOfScreen, /* 34 */
    Blt_RootX, /* 35 */
    Blt_RootY, /* 36 */
    Blt_RootCoordinates, /* 37 */
    Blt_MapToplevelWindow, /* 38 */
    Blt_UnmapToplevelWindow, /* 39 */
    Blt_RaiseToplevelWindow, /* 40 */
    Blt_LowerToplevelWindow, /* 41 */
    Blt_ResizeToplevelWindow, /* 42 */
    Blt_MoveToplevelWindow, /* 43 */
    Blt_MoveResizeToplevelWindow, /* 44 */
    Blt_GetWindowRegion, /* 45 */
    Blt_GetWindowInstanceData, /* 46 */
    Blt_SetWindowInstanceData, /* 47 */
    Blt_DeleteWindowInstanceData, /* 48 */
    Blt_ReparentWindow, /* 49 */
    Blt_GetDrawableAttributes, /* 50 */
    Blt_SetDrawableAttributes, /* 51 */
    Blt_SetDrawableAttributesFromWindow, /* 52 */
    Blt_FreeDrawableAttributes, /* 53 */
    Blt_GetBitmapGC, /* 54 */
    Blt_GetPixmapAbortOnError, /* 55 */
    Blt_ScreenDPI, /* 56 */
    Blt_OldConfigModified, /* 57 */
    Blt_GetLineExtents, /* 58 */
    Blt_GetBoundingBox, /* 59 */
    Blt_ParseTifTags, /* 60 */
    Blt_GetFont, /* 61 */
    Blt_AllocFontFromObj, /* 62 */
    Blt_DrawWithEllipsis, /* 63 */
    Blt_GetFontFromObj, /* 64 */
    Blt_Font_GetMetrics, /* 65 */
    Blt_TextWidth, /* 66 */
    Blt_Font_GetInterp, /* 67 */
    Blt_Font_GetFile, /* 68 */
    Blt_NewTileBrush, /* 69 */
    Blt_NewLinearGradientBrush, /* 70 */
    Blt_NewStripesBrush, /* 71 */
    Blt_NewCheckersBrush, /* 72 */
    Blt_NewRadialGradientBrush, /* 73 */
    Blt_NewConicalGradientBrush, /* 74 */
    Blt_NewColorBrush, /* 75 */
    Blt_GetBrushType, /* 76 */
    Blt_GetBrushName, /* 77 */
    Blt_GetBrushColor, /* 78 */
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
    Blt_SetBrushRegion, /* 91 */
    Blt_GetBrushAlpha, /* 92 */
    Blt_GetBrushOrigin, /* 93 */
    Blt_GetAssociatedColorFromBrush, /* 94 */
    Blt_PaintRectangle, /* 95 */
    Blt_PaintPolygon, /* 96 */
    Blt_CreateBrushNotifier, /* 97 */
    Blt_DeleteBrushNotifier, /* 98 */
    Blt_GetBg, /* 99 */
    Blt_GetBgFromObj, /* 100 */
    Blt_Bg_BorderColor, /* 101 */
    Blt_Bg_Border, /* 102 */
    Blt_Bg_PaintBrush, /* 103 */
    Blt_Bg_Name, /* 104 */
    Blt_Bg_Free, /* 105 */
    Blt_Bg_DrawRectangle, /* 106 */
    Blt_Bg_FillRectangle, /* 107 */
    Blt_Bg_DrawPolygon, /* 108 */
    Blt_Bg_FillPolygon, /* 109 */
    Blt_Bg_GetOrigin, /* 110 */
    Blt_Bg_SetChangedProc, /* 111 */
    Blt_Bg_SetOrigin, /* 112 */
    Blt_Bg_DrawFocus, /* 113 */
    Blt_Bg_BorderGC, /* 114 */
    Blt_Bg_SetFromBackground, /* 115 */
    Blt_Bg_UnsetClipRegion, /* 116 */
    Blt_Bg_SetClipRegion, /* 117 */
    Blt_Bg_GetColor, /* 118 */
    Blt_3DBorder_SetClipRegion, /* 119 */
    Blt_3DBorder_UnsetClipRegion, /* 120 */
    Blt_DestroyBindingTable, /* 121 */
    Blt_CreateBindingTable, /* 122 */
    Blt_ConfigureBindings, /* 123 */
    Blt_ConfigureBindingsFromObj, /* 124 */
    Blt_PickCurrentItem, /* 125 */
    Blt_DeleteBindings, /* 126 */
    Blt_MoveBindingTable, /* 127 */
    Blt_Afm_SetPrinting, /* 128 */
    Blt_Afm_IsPrinting, /* 129 */
    Blt_Afm_TextWidth, /* 130 */
    Blt_Afm_GetMetrics, /* 131 */
    Blt_Afm_GetPostscriptFamily, /* 132 */
    Blt_Afm_GetPostscriptName, /* 133 */
    Blt_SetDashes, /* 134 */
    Blt_ResetLimits, /* 135 */
    Blt_GetLimitsFromObj, /* 136 */
    Blt_ConfigureInfoFromObj, /* 137 */
    Blt_ConfigureValueFromObj, /* 138 */
    Blt_ConfigureWidgetFromObj, /* 139 */
    Blt_ConfigureComponentFromObj, /* 140 */
    Blt_ConfigModified, /* 141 */
    Blt_NameOfState, /* 142 */
    Blt_FreeOptions, /* 143 */
    Blt_ObjIsOption, /* 144 */
    Blt_GetPixelsFromObj, /* 145 */
    Blt_GetPadFromObj, /* 146 */
    Blt_GetStateFromObj, /* 147 */
    Blt_GetFillFromObj, /* 148 */
    Blt_GetResizeFromObj, /* 149 */
    Blt_GetDashesFromObj, /* 150 */
    Blt_Image_IsDeleted, /* 151 */
    Blt_Image_GetMaster, /* 152 */
    Blt_Image_GetType, /* 153 */
    Blt_Image_GetInstanceData, /* 154 */
    Blt_Image_Name, /* 155 */
    Blt_Image_NameOfType, /* 156 */
    Blt_FreePainter, /* 157 */
    Blt_GetPainter, /* 158 */
    Blt_GetPainterFromDrawable, /* 159 */
    Blt_PainterGC, /* 160 */
    Blt_PainterDepth, /* 161 */
    Blt_SetPainterClipRegion, /* 162 */
    Blt_UnsetPainterClipRegion, /* 163 */
    Blt_PaintPicture, /* 164 */
    Blt_PaintPictureWithBlend, /* 165 */
    Blt_PaintCheckbox, /* 166 */
    Blt_PaintRadioButton, /* 167 */
    Blt_PaintRadioButtonOld, /* 168 */
    Blt_PaintDelete, /* 169 */
    Blt_Ps_Create, /* 170 */
    Blt_Ps_Free, /* 171 */
    Blt_Ps_GetValue, /* 172 */
    Blt_Ps_GetDBuffer, /* 173 */
    Blt_Ps_GetInterp, /* 174 */
    Blt_Ps_GetScratchBuffer, /* 175 */
    Blt_Ps_SetInterp, /* 176 */
    Blt_Ps_Append, /* 177 */
    Blt_Ps_AppendBytes, /* 178 */
    Blt_Ps_VarAppend, /* 179 */
    Blt_Ps_Format, /* 180 */
    Blt_Ps_SetClearBackground, /* 181 */
    Blt_Ps_IncludeFile, /* 182 */
    Blt_Ps_GetPicaFromObj, /* 183 */
    Blt_Ps_GetPadFromObj, /* 184 */
    Blt_Ps_ComputeBoundingBox, /* 185 */
    Blt_Ps_DrawPicture, /* 186 */
    Blt_Ps_Rectangle, /* 187 */
    Blt_Ps_Rectangle2, /* 188 */
    Blt_Ps_SaveFile, /* 189 */
    Blt_Ps_XSetLineWidth, /* 190 */
    Blt_Ps_XSetBackground, /* 191 */
    Blt_Ps_XSetBitmapData, /* 192 */
    Blt_Ps_XSetForeground, /* 193 */
    Blt_Ps_XSetFont, /* 194 */
    Blt_Ps_XSetDashes, /* 195 */
    Blt_Ps_XSetLineAttributes, /* 196 */
    Blt_Ps_XSetStipple, /* 197 */
    Blt_Ps_Polyline, /* 198 */
    Blt_Ps_XDrawLines, /* 199 */
    Blt_Ps_XDrawSegments, /* 200 */
    Blt_Ps_DrawPolyline, /* 201 */
    Blt_Ps_DrawSegments2d, /* 202 */
    Blt_Ps_Draw3DRectangle, /* 203 */
    Blt_Ps_Fill3DRectangle, /* 204 */
    Blt_Ps_XFillRectangle, /* 205 */
    Blt_Ps_XFillRectangles, /* 206 */
    Blt_Ps_XFillPolygon, /* 207 */
    Blt_Ps_DrawPhoto, /* 208 */
    Blt_Ps_XDrawWindow, /* 209 */
    Blt_Ps_DrawText, /* 210 */
    Blt_Ps_DrawBitmap, /* 211 */
    Blt_Ps_XSetCapStyle, /* 212 */
    Blt_Ps_XSetJoinStyle, /* 213 */
    Blt_Ps_PolylineFromXPoints, /* 214 */
    Blt_Ps_Polygon, /* 215 */
    Blt_Ps_SetPrinting, /* 216 */
    Blt_Ts_CreateLayout, /* 217 */
    Blt_Ts_TitleLayout, /* 218 */
    Blt_Ts_DrawLayout, /* 219 */
    Blt_Ts_GetExtents, /* 220 */
    Blt_Ts_ResetStyle, /* 221 */
    Blt_Ts_FreeStyle, /* 222 */
    Blt_Ts_SetDrawStyle, /* 223 */
    Blt_DrawText, /* 224 */
    Blt_DrawText2, /* 225 */
    Blt_Ts_Bitmap, /* 226 */
    Blt_DrawTextWithRotatedFont, /* 227 */
    Blt_DrawLayout, /* 228 */
    Blt_GetTextExtents, /* 229 */
    Blt_RotateStartingTextPositions, /* 230 */
    Blt_ComputeTextLayout, /* 231 */
    Blt_DrawTextLayout, /* 232 */
    Blt_CharBbox, /* 233 */
    Blt_UnderlineTextLayout, /* 234 */
    Blt_Ts_UnderlineLayout, /* 235 */
    Blt_Ts_DrawText, /* 236 */
    Blt_MeasureText, /* 237 */
    Blt_FreeTextLayout, /* 238 */
};

/* !END!: Do not edit above this line. */
