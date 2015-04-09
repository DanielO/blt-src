/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#define BUILD_BLT_TCL_PROCS 1
#include <bltInt.h>

extern BltTclIntProcs bltTclIntProcs;

/* !BEGIN!: Do not edit below this line. */

static BltTclStubHooks bltTclStubHooks = {
    &bltTclIntProcs
};

BltTclProcs bltTclProcs = {
    TCL_STUB_MAGIC,
    &bltTclStubHooks,
    NULL, /* 0 */
    Blt_AllocInit, /* 1 */
    Blt_Chain_Init, /* 2 */
    Blt_Chain_Create, /* 3 */
    Blt_Chain_Destroy, /* 4 */
    Blt_Chain_NewLink, /* 5 */
    Blt_Chain_AllocLink, /* 6 */
    Blt_Chain_Append, /* 7 */
    Blt_Chain_Prepend, /* 8 */
    Blt_Chain_Reset, /* 9 */
    Blt_Chain_InitLink, /* 10 */
    Blt_Chain_LinkAfter, /* 11 */
    Blt_Chain_LinkBefore, /* 12 */
    Blt_Chain_UnlinkLink, /* 13 */
    Blt_Chain_DeleteLink, /* 14 */
    Blt_Chain_GetNthLink, /* 15 */
    Blt_Chain_Sort, /* 16 */
    Blt_Chain_Reverse, /* 17 */
    Blt_Chain_IsBefore, /* 18 */
    blt_table_release_tags, /* 19 */
    blt_table_new_tags, /* 20 */
    blt_table_get_column_tag_table, /* 21 */
    blt_table_get_row_tag_table, /* 22 */
    blt_table_exists, /* 23 */
    blt_table_create, /* 24 */
    blt_table_open, /* 25 */
    blt_table_close, /* 26 */
    blt_table_same_object, /* 27 */
    blt_table_row_get_label_table, /* 28 */
    blt_table_column_get_label_table, /* 29 */
    blt_table_get_row, /* 30 */
    blt_table_get_column, /* 31 */
    blt_table_get_row_by_label, /* 32 */
    blt_table_get_column_by_label, /* 33 */
    blt_table_get_row_by_index, /* 34 */
    blt_table_get_column_by_index, /* 35 */
    blt_table_set_row_label, /* 36 */
    blt_table_set_column_label, /* 37 */
    blt_table_name_to_column_type, /* 38 */
    blt_table_set_column_type, /* 39 */
    blt_table_column_type_to_name, /* 40 */
    blt_table_set_column_tag, /* 41 */
    blt_table_set_row_tag, /* 42 */
    blt_table_create_row, /* 43 */
    blt_table_create_column, /* 44 */
    blt_table_extend_rows, /* 45 */
    blt_table_extend_columns, /* 46 */
    blt_table_delete_row, /* 47 */
    blt_table_delete_column, /* 48 */
    blt_table_move_row, /* 49 */
    blt_table_move_column, /* 50 */
    blt_table_get_obj, /* 51 */
    blt_table_set_obj, /* 52 */
    blt_table_get_string, /* 53 */
    blt_table_set_string_rep, /* 54 */
    blt_table_set_string, /* 55 */
    blt_table_append_string, /* 56 */
    blt_table_set_bytes, /* 57 */
    blt_table_get_double, /* 58 */
    blt_table_set_double, /* 59 */
    blt_table_get_long, /* 60 */
    blt_table_set_long, /* 61 */
    blt_table_get_boolean, /* 62 */
    blt_table_set_boolean, /* 63 */
    blt_table_get_value, /* 64 */
    blt_table_set_value, /* 65 */
    blt_table_unset_value, /* 66 */
    blt_table_value_exists, /* 67 */
    blt_table_value_string, /* 68 */
    blt_table_value_bytes, /* 69 */
    blt_table_value_length, /* 70 */
    blt_table_tags_are_shared, /* 71 */
    blt_table_clear_row_tags, /* 72 */
    blt_table_clear_column_tags, /* 73 */
    blt_table_get_row_tags, /* 74 */
    blt_table_get_column_tags, /* 75 */
    blt_table_get_tagged_rows, /* 76 */
    blt_table_get_tagged_columns, /* 77 */
    blt_table_row_has_tag, /* 78 */
    blt_table_column_has_tag, /* 79 */
    blt_table_forget_row_tag, /* 80 */
    blt_table_forget_column_tag, /* 81 */
    blt_table_unset_row_tag, /* 82 */
    blt_table_unset_column_tag, /* 83 */
    blt_table_first_column, /* 84 */
    blt_table_next_column, /* 85 */
    blt_table_first_row, /* 86 */
    blt_table_next_row, /* 87 */
    blt_table_row_spec, /* 88 */
    blt_table_column_spec, /* 89 */
    blt_table_iterate_rows, /* 90 */
    blt_table_iterate_columns, /* 91 */
    blt_table_iterate_rows_objv, /* 92 */
    blt_table_iterate_columns_objv, /* 93 */
    blt_table_free_iterator_objv, /* 94 */
    blt_table_iterate_all_rows, /* 95 */
    blt_table_iterate_all_columns, /* 96 */
    blt_table_first_tagged_row, /* 97 */
    blt_table_first_tagged_column, /* 98 */
    blt_table_next_tagged_row, /* 99 */
    blt_table_next_tagged_column, /* 100 */
    blt_table_list_rows, /* 101 */
    blt_table_list_columns, /* 102 */
    blt_table_clear_row_traces, /* 103 */
    blt_table_clear_column_traces, /* 104 */
    blt_table_create_trace, /* 105 */
    blt_table_trace_column, /* 106 */
    blt_table_trace_row, /* 107 */
    blt_table_create_column_trace, /* 108 */
    blt_table_create_column_tag_trace, /* 109 */
    blt_table_create_row_trace, /* 110 */
    blt_table_create_row_tag_trace, /* 111 */
    blt_table_delete_trace, /* 112 */
    blt_table_create_notifier, /* 113 */
    blt_table_create_row_notifier, /* 114 */
    blt_table_create_row_tag_notifier, /* 115 */
    blt_table_create_column_notifier, /* 116 */
    blt_table_create_column_tag_notifier, /* 117 */
    blt_table_delete_notifier, /* 118 */
    blt_table_sort_init, /* 119 */
    blt_table_sort_rows, /* 120 */
    blt_table_sort_rows_subset, /* 121 */
    blt_table_sort_finish, /* 122 */
    blt_table_get_compare_proc, /* 123 */
    blt_table_get_row_map, /* 124 */
    blt_table_get_column_map, /* 125 */
    blt_table_set_row_map, /* 126 */
    blt_table_set_column_map, /* 127 */
    blt_table_restore, /* 128 */
    blt_table_file_restore, /* 129 */
    blt_table_register_format, /* 130 */
    blt_table_unset_keys, /* 131 */
    blt_table_get_keys, /* 132 */
    blt_table_set_keys, /* 133 */
    blt_table_key_lookup, /* 134 */
    blt_table_get_column_limits, /* 135 */
    Blt_InitHashTable, /* 136 */
    Blt_InitHashTableWithPool, /* 137 */
    Blt_DeleteHashTable, /* 138 */
    Blt_DeleteHashEntry, /* 139 */
    Blt_FirstHashEntry, /* 140 */
    Blt_NextHashEntry, /* 141 */
    Blt_HashStats, /* 142 */
    Blt_List_Init, /* 143 */
    Blt_List_Reset, /* 144 */
    Blt_List_Create, /* 145 */
    Blt_List_Destroy, /* 146 */
    Blt_List_CreateNode, /* 147 */
    Blt_List_DeleteNode, /* 148 */
    Blt_List_Append, /* 149 */
    Blt_List_Prepend, /* 150 */
    Blt_List_LinkAfter, /* 151 */
    Blt_List_LinkBefore, /* 152 */
    Blt_List_UnlinkNode, /* 153 */
    Blt_List_GetNode, /* 154 */
    Blt_List_DeleteNodeByKey, /* 155 */
    Blt_List_GetNthNode, /* 156 */
    Blt_List_Sort, /* 157 */
    Blt_Pool_Create, /* 158 */
    Blt_Pool_Destroy, /* 159 */
    Blt_Tags_Create, /* 160 */
    Blt_Tags_Destroy, /* 161 */
    Blt_Tags_Init, /* 162 */
    Blt_Tags_Reset, /* 163 */
    Blt_Tags_ItemHasTag, /* 164 */
    Blt_Tags_AddTag, /* 165 */
    Blt_Tags_AddItemToTag, /* 166 */
    Blt_Tags_ForgetTag, /* 167 */
    Blt_Tags_RemoveItemFromTag, /* 168 */
    Blt_Tags_ClearTagsFromItem, /* 169 */
    Blt_Tags_AppendTagsToChain, /* 170 */
    Blt_Tags_AppendTagsToObj, /* 171 */
    Blt_Tags_AppendAllTagsToObj, /* 172 */
    Blt_Tags_GetItemList, /* 173 */
    Blt_Tags_GetTable, /* 174 */
    Blt_Tree_GetKey, /* 175 */
    Blt_Tree_GetKeyFromNode, /* 176 */
    Blt_Tree_GetKeyFromInterp, /* 177 */
    Blt_Tree_CreateNode, /* 178 */
    Blt_Tree_CreateNodeWithId, /* 179 */
    Blt_Tree_DeleteNode, /* 180 */
    Blt_Tree_MoveNode, /* 181 */
    Blt_Tree_GetNode, /* 182 */
    Blt_Tree_FindChild, /* 183 */
    Blt_Tree_NextNode, /* 184 */
    Blt_Tree_PrevNode, /* 185 */
    Blt_Tree_FirstChild, /* 186 */
    Blt_Tree_LastChild, /* 187 */
    Blt_Tree_IsBefore, /* 188 */
    Blt_Tree_IsAncestor, /* 189 */
    Blt_Tree_PrivateValue, /* 190 */
    Blt_Tree_PublicValue, /* 191 */
    Blt_Tree_GetValue, /* 192 */
    Blt_Tree_ValueExists, /* 193 */
    Blt_Tree_SetValue, /* 194 */
    Blt_Tree_UnsetValue, /* 195 */
    Blt_Tree_AppendValue, /* 196 */
    Blt_Tree_ListAppendValue, /* 197 */
    Blt_Tree_GetArrayValue, /* 198 */
    Blt_Tree_SetArrayValue, /* 199 */
    Blt_Tree_UnsetArrayValue, /* 200 */
    Blt_Tree_AppendArrayValue, /* 201 */
    Blt_Tree_ListAppendArrayValue, /* 202 */
    Blt_Tree_ArrayValueExists, /* 203 */
    Blt_Tree_ArrayNames, /* 204 */
    Blt_Tree_GetValueByKey, /* 205 */
    Blt_Tree_SetValueByKey, /* 206 */
    Blt_Tree_UnsetValueByKey, /* 207 */
    Blt_Tree_AppendValueByKey, /* 208 */
    Blt_Tree_ListAppendValueByKey, /* 209 */
    Blt_Tree_ValueExistsByKey, /* 210 */
    Blt_Tree_FirstKey, /* 211 */
    Blt_Tree_NextKey, /* 212 */
    Blt_Tree_Apply, /* 213 */
    Blt_Tree_ApplyDFS, /* 214 */
    Blt_Tree_ApplyBFS, /* 215 */
    Blt_Tree_SortNode, /* 216 */
    Blt_Tree_Exists, /* 217 */
    Blt_Tree_Open, /* 218 */
    Blt_Tree_Close, /* 219 */
    Blt_Tree_Attach, /* 220 */
    Blt_Tree_GetFromObj, /* 221 */
    Blt_Tree_Size, /* 222 */
    Blt_Tree_CreateTrace, /* 223 */
    Blt_Tree_DeleteTrace, /* 224 */
    Blt_Tree_CreateEventHandler, /* 225 */
    Blt_Tree_DeleteEventHandler, /* 226 */
    Blt_Tree_RelabelNode, /* 227 */
    Blt_Tree_RelabelNodeWithoutNotify, /* 228 */
    Blt_Tree_NodeIdAscii, /* 229 */
    Blt_Tree_NodePath, /* 230 */
    Blt_Tree_NodeRelativePath, /* 231 */
    Blt_Tree_NodePosition, /* 232 */
    Blt_Tree_ClearTags, /* 233 */
    Blt_Tree_HasTag, /* 234 */
    Blt_Tree_AddTag, /* 235 */
    Blt_Tree_RemoveTag, /* 236 */
    Blt_Tree_ForgetTag, /* 237 */
    Blt_Tree_TagHashTable, /* 238 */
    Blt_Tree_TagTableIsShared, /* 239 */
    Blt_Tree_NewTagTable, /* 240 */
    Blt_Tree_FirstTag, /* 241 */
    Blt_Tree_DumpNode, /* 242 */
    Blt_Tree_Dump, /* 243 */
    Blt_Tree_DumpToFile, /* 244 */
    Blt_Tree_Restore, /* 245 */
    Blt_Tree_RestoreFromFile, /* 246 */
    Blt_Tree_Depth, /* 247 */
    Blt_Tree_RegisterFormat, /* 248 */
    Blt_Tree_RememberTag, /* 249 */
    Blt_VecMin, /* 250 */
    Blt_VecMax, /* 251 */
    Blt_AllocVectorId, /* 252 */
    Blt_SetVectorChangedProc, /* 253 */
    Blt_FreeVectorId, /* 254 */
    Blt_GetVectorById, /* 255 */
    Blt_NameOfVectorId, /* 256 */
    Blt_NameOfVector, /* 257 */
    Blt_VectorNotifyPending, /* 258 */
    Blt_CreateVector, /* 259 */
    Blt_CreateVector2, /* 260 */
    Blt_GetVector, /* 261 */
    Blt_GetVectorFromObj, /* 262 */
    Blt_VectorExists, /* 263 */
    Blt_ResetVector, /* 264 */
    Blt_ResizeVector, /* 265 */
    Blt_DeleteVectorByName, /* 266 */
    Blt_DeleteVector, /* 267 */
    Blt_ExprVector, /* 268 */
    Blt_InstallIndexProc, /* 269 */
    Blt_VectorExists2, /* 270 */
};

/* !END!: Do not edit above this line. */
