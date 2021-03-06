/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * bltDataTableSqlite.c --
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

#include <bltInt.h>

#ifndef NO_DATATABLE

#include "config.h"
#ifdef HAVE_LIBSQLITE

#ifdef HAVE_MEMORY_H
  #include <memory.h>
#endif /* HAVE_MEMORY_H */

#include <tcl.h>
#include <bltDataTable.h>
#include <bltAlloc.h>
#include <bltSwitch.h>
#include <sqlite3.h>

DLLEXPORT extern Tcl_AppInitProc blt_table_sqlite_init;
DLLEXPORT extern Tcl_AppInitProc blt_table_sqlite_safe_init;

/*
 * Format       Import          Export
 * csv          file/data       file/data
 * tree         data            data
 * vector       data            data
 * xml          file/data       file/data
 * sqlite       file            file
 */

/*
 * ImportArgs --
 */
typedef struct {
    Tcl_Obj *fileObjPtr;                /* File to read. */
    Tcl_Obj *queryObjPtr;               /* If non-NULL, query to make. */
} ImportArgs;

static Blt_SwitchSpec importSwitches[] = 
{
    {BLT_SWITCH_OBJ, "-file",  "fileName", (char *)NULL,
        Blt_Offset(ImportArgs, fileObjPtr), 0, 0},
    {BLT_SWITCH_OBJ, "-query", "string", (char *)NULL,
        Blt_Offset(ImportArgs, queryObjPtr), 0, 0},
    {BLT_SWITCH_END}
};

/*
 * ExportArgs --
 */
typedef struct {
    BLT_TABLE_ITERATOR ri, ci;
    unsigned int flags;
    Tcl_Obj *fileObjPtr;
    Tcl_Obj *tableObjPtr;
    const char *tableName;
} ExportArgs;

#define EXPORT_ROWLABELS        (1<<0)

static Blt_SwitchFreeProc ColumnIterFreeProc;
static Blt_SwitchParseProc ColumnIterSwitchProc;
static Blt_SwitchCustom columnIterSwitch = {
    ColumnIterSwitchProc, NULL, ColumnIterFreeProc, 0,
};
static Blt_SwitchFreeProc RowIterFreeProc;
static Blt_SwitchParseProc RowIterSwitchProc;
static Blt_SwitchCustom rowIterSwitch = {
    RowIterSwitchProc, NULL, RowIterFreeProc, 0,
};

static Blt_SwitchSpec exportSwitches[] = 
{
    {BLT_SWITCH_CUSTOM, "-columns",   "columns" ,(char *)NULL,
        Blt_Offset(ExportArgs, ci),   0, 0, &columnIterSwitch},
    {BLT_SWITCH_OBJ, "-file", "fileName", (char *)NULL,
        Blt_Offset(ExportArgs, fileObjPtr), 0, 0},
    {BLT_SWITCH_CUSTOM, "-rows",      "rows", (char *)NULL,
        Blt_Offset(ExportArgs, ri),   0, 0, &rowIterSwitch},
    {BLT_SWITCH_BITS_NOARG, "-rowlabels",  "", (char *)NULL,
        Blt_Offset(ExportArgs, flags), 0, EXPORT_ROWLABELS},
    {BLT_SWITCH_OBJ, "-table", "tableName", (char *)NULL,
        Blt_Offset(ExportArgs, tableObjPtr), 0, 0},
    {BLT_SWITCH_END}
};

static BLT_TABLE_EXPORT_PROC ExportSqliteProc;

#define DEF_CLIENT_FLAGS (CLIENT_MULTI_STATEMENTS|CLIENT_MULTI_RESULTS)

static BLT_TABLE_IMPORT_PROC ImportSqliteProc;

/*
 *---------------------------------------------------------------------------
 *
 * ColumnIterFreeProc --
 *
 *      Free the storage associated with the -columns switch.
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
ColumnIterFreeProc(ClientData clientData, char *record, int offset, int flags)
{
    BLT_TABLE_ITERATOR *iterPtr = (BLT_TABLE_ITERATOR *)(record + offset);

    blt_table_free_iterator_objv(iterPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * ColumnIterSwitchProc --
 *
 *      Convert a Tcl_Obj representing an offset in the table.
 *
 * Results:
 *      The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ColumnIterSwitchProc(ClientData clientData, Tcl_Interp *interp,
                     const char *switchName, Tcl_Obj *objPtr, char *record,
                     int offset, int flags)
{
    BLT_TABLE_ITERATOR *iterPtr = (BLT_TABLE_ITERATOR *)(record + offset);
    BLT_TABLE table;
    Tcl_Obj **objv;
    int objc;

    table = clientData;
    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
        return TCL_ERROR;
    }
    if (blt_table_iterate_columns_objv(interp, table, objc, objv, iterPtr)
        != TCL_OK) {
        return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 *---------------------------------------------------------------------------
 *
 * RowIterFreeProc --
 *
 *      Free the storage associated with the -rows switch.
 *
 * Results:
 *      None.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
RowIterFreeProc(ClientData clientData, char *record, int offset, int flags)
{
    BLT_TABLE_ITERATOR *iterPtr = (BLT_TABLE_ITERATOR *)(record + offset);

    blt_table_free_iterator_objv(iterPtr);
}

/*
 *---------------------------------------------------------------------------
 *
 * RowIterSwitchProc --
 *
 *      Convert a Tcl_Obj representing an offset in the table.
 *
 * Results:
 *      The return value is a standard TCL result.
 *
 *---------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
RowIterSwitchProc(ClientData clientData, Tcl_Interp *interp,
                     const char *switchName, Tcl_Obj *objPtr, char *record,
                     int offset, int flags)
{
    BLT_TABLE_ITERATOR *iterPtr = (BLT_TABLE_ITERATOR *)(record + offset);
    BLT_TABLE table;
    Tcl_Obj **objv;
    int objc;

    table = clientData;
    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
        return TCL_ERROR;
    }
    if (blt_table_iterate_rows_objv(interp, table, objc, objv, iterPtr)
        != TCL_OK) {
        return TCL_ERROR;
    }
    return TCL_OK;
}

static BLT_TABLE_COLUMN_TYPE
SqliteTypeToColumnType(int type) 
{
    switch(type) {
    case SQLITE_BLOB:                   /* 4 */
        return TABLE_COLUMN_TYPE_BLOB;
    case SQLITE_INTEGER:                /* 1 */
        return TABLE_COLUMN_TYPE_LONG;
    case SQLITE_FLOAT:                  /* 2 */
        return TABLE_COLUMN_TYPE_DOUBLE;
    case SQLITE_NULL:                   /* 0 */
        return TABLE_COLUMN_TYPE_STRING;
    case SQLITE_TEXT:                   /* 3 */
    default: 
        return TABLE_COLUMN_TYPE_STRING;
    }
}

static int
SqliteConnect(Tcl_Interp *interp, const char *fileName, sqlite3 **connPtr)
{
    sqlite3  *conn;                     /* Connection handler. */

    if (sqlite3_open(fileName, &conn) != SQLITE_OK) {
        Tcl_AppendResult(interp, "can't open sqlite database", "\"",
                fileName, "\": ", sqlite3_errmsg(conn), (char *)NULL);
        sqlite3_close(conn);
        return TCL_ERROR;
    }
    *connPtr = conn;
    return TCL_OK;
}

static void
SqliteDisconnect(sqlite3 *conn) 
{
    sqlite3_close(conn);
}

static int
SqliteImportLabel(Tcl_Interp *interp, BLT_TABLE table, BLT_TABLE_COLUMN col,
                  sqlite3_stmt *stmt, int index)
{
    const char *label;
    int type;
                    
    label = (char *)sqlite3_column_name(stmt, index);
    if (blt_table_set_column_label(interp, table, col, label) != TCL_OK) { 
        return TCL_ERROR;
    }
    type = sqlite3_column_type(stmt, index);
    type = SqliteTypeToColumnType(type);
    if (blt_table_set_column_type(interp, table, col, type) != TCL_OK) {
        return TCL_ERROR;
    }
    return TCL_OK;
}

static int
SqliteImportRow(Tcl_Interp *interp, BLT_TABLE table, sqlite3_stmt *stmt,
                long numColumns, BLT_TABLE_COLUMN *cols, long index)
{
    BLT_TABLE_ROW row;
    int i;
    
    /* First check that there are enough rows in the table to accomodate
     * the new data. Add more if necessary. */
    if (index >= blt_table_num_rows(table)) {
        if (blt_table_extend_rows(interp, table, 1, &row) != TCL_OK) {
            return TCL_ERROR;
        }
    }
    row = blt_table_row(table, index);
    for (i = 0; i < numColumns; i++) {
        int type;
            
        type = sqlite3_column_type(stmt, i);
        switch (type) {
        case SQLITE_INTEGER: 
            {
                long lval;
                
                lval = sqlite3_column_int64(stmt, i);
                if (blt_table_set_long(interp, table, row, cols[i], lval)
                    != TCL_OK) {
                    return TCL_ERROR;
                }
            }
            break;
        case SQLITE_FLOAT: 
            {
                double dval;
                
                dval = sqlite3_column_double(stmt, i);
                if (blt_table_set_double(interp, table, row, cols[i], dval)
                    != TCL_OK) {
                    return TCL_ERROR;
                }
            }
            break;
        case SQLITE_NULL:
            break;                          /* Ignore empty cells. */
        default:
        case SQLITE_BLOB: 
        case SQLITE_TEXT:
            {
                const char *sval;
                int length;
                
                sval = (const char *)sqlite3_column_text(stmt, i);
                length = sqlite3_column_bytes(stmt, i);
                if (blt_table_set_string_rep(interp, table, row, cols[i], sval,
                        length) != TCL_OK) {
                    return TCL_ERROR;
                }
            }
        }
    }
    return TCL_OK;
}

static int
SqliteImport(Tcl_Interp *interp, BLT_TABLE table, sqlite3 *conn, 
             ImportArgs *argsPtr) 
{
    BLT_TABLE_COLUMN *cols;
    const char *query, *left;
    int initialized, length, result;
    long numColumns, count;
    sqlite3_stmt *stmt;
    

    stmt = NULL;
    query = Tcl_GetStringFromObj(argsPtr->queryObjPtr, &length);
    if (sqlite3_prepare_v2(conn, query, length, &stmt, &left) != SQLITE_OK) {
        Tcl_AppendResult(interp, "error in query \"", query, "\": ", 
                         sqlite3_errmsg(conn), (char *)NULL);
        return TCL_ERROR;
    }
    if (stmt == NULL) {
        Tcl_AppendResult(interp, "empty statement in query \"", query, "\": ", 
                         sqlite3_errmsg(conn), (char *)NULL);
        return TCL_ERROR;
    }
    if ((left != NULL) && (left[0] != '\0')) {
        Tcl_AppendResult(interp, "extra statements follow query \"", left,
                         "\": ", sqlite3_errmsg(conn), (char *)NULL);
        return TCL_ERROR;
    }
    numColumns = sqlite3_column_count(stmt);
    cols = Blt_Malloc(sizeof(BLT_TABLE_COLUMN) * numColumns);
    if (cols == NULL) {
        Tcl_AppendResult(interp, "can't allocate ", Blt_Itoa(numColumns),
                         " column slots.", (char *)NULL);
        goto error;
    }
    if (blt_table_extend_columns(interp, table, numColumns, cols) != TCL_OK) {
        goto error;                     /* Can't create new columns. */
    }
    initialized = FALSE;
    result = SQLITE_OK;
    count = 0;
    do {
        result = sqlite3_step(stmt);
        if ((result == SQLITE_OK) || (result == SQLITE_ROW)) {
            int i;
            
            if (!initialized) {
                /* After the first step, step the column labels and type.  */
                for (i = 0; i < numColumns; i++) {
                    if (SqliteImportLabel(interp, table, cols[i], stmt, i)
                        != TCL_OK) {
                        goto error;
                    }
                }
                initialized = TRUE;
            }
            if (SqliteImportRow(interp, table, stmt, numColumns, cols, count)
                != TCL_OK) {
                goto error;
            }
        } else if (result != SQLITE_DONE) {
            Tcl_AppendResult(interp, "step failed \": ", 
                             sqlite3_errmsg(conn), (char *)NULL);
            goto error;
        }
    } while ((result == SQLITE_OK) || (result == SQLITE_ROW)) ;
    Blt_Free(cols);
    sqlite3_finalize(stmt);
    return TCL_OK;
 error:
    if (stmt != NULL) {
        sqlite3_finalize(stmt);
    }
    if (cols != NULL) {
        Blt_Free(cols);
    }
    return TCL_ERROR;
}

static int
SqliteCreateTable(Tcl_Interp *interp, sqlite3 *conn, BLT_TABLE table,
                  ExportArgs *argsPtr)
{
    BLT_TABLE_COLUMN col;
    Blt_DBuffer dbuffer;
    const char *query;
    int first;
    int length;
    int result;
    sqlite3_stmt *stmt;
    
    dbuffer = Blt_DBuffer_Create();
    Blt_DBuffer_Format(dbuffer, "DROP TABLE IF EXISTS %s; CREATE TABLE %s (",
                       argsPtr->tableName, argsPtr->tableName);
    if (argsPtr->flags & EXPORT_ROWLABELS) {
        Blt_DBuffer_Format(dbuffer, "_rowId TEXT, ");
    }        
    first = TRUE;
    for (col = blt_table_first_tagged_column(&argsPtr->ci); col != NULL; 
         col = blt_table_next_tagged_column(&argsPtr->ci)) {
        int type;
        const char *label;
        
        type = blt_table_column_type(col);
        label = blt_table_column_label(col);
        if (!first) {
            Blt_DBuffer_Format(dbuffer, ", ");
        }
        Blt_DBuffer_Format(dbuffer, "[%s] ", label);
        switch(type) {
        case TABLE_COLUMN_TYPE_BOOLEAN:
        case TABLE_COLUMN_TYPE_LONG:
            Blt_DBuffer_Format(dbuffer, "INTEGER");     break;
        case TABLE_COLUMN_TYPE_DOUBLE:
            Blt_DBuffer_Format(dbuffer, "REAL");        break;
        case TABLE_COLUMN_TYPE_BLOB:
            Blt_DBuffer_Format(dbuffer, "BLOB");        break;
        default:
        case TABLE_COLUMN_TYPE_TIME:
        case TABLE_COLUMN_TYPE_STRING:
            Blt_DBuffer_Format(dbuffer, "TEXT");        break;
        }
        first = FALSE;
    }
    Blt_DBuffer_Format(dbuffer, ");"); 
    query = (const char *)Blt_DBuffer_String(dbuffer);
    length = Blt_DBuffer_Length(dbuffer);
    result = sqlite3_prepare_v2(conn, query, length, &stmt, NULL);
    if (result != SQLITE_OK) {
        Tcl_AppendResult(interp, "error in table create \"", query, "\": ", 
                         sqlite3_errmsg(conn), (char *)NULL);
    }
    Blt_DBuffer_Destroy(dbuffer);
    if (result != SQLITE_OK) {
        goto error;
    }
    do {
        result = sqlite3_step(stmt);
    } while (result == SQLITE_OK);
    sqlite3_finalize(stmt);
    assert(result == SQLITE_DONE);
    return TCL_OK;
 error:
    if (stmt != NULL) {
        sqlite3_finalize(stmt);
    }
    return TCL_ERROR;
}

static int
SqliteExportValues(Tcl_Interp *interp, sqlite3 *conn, BLT_TABLE table,
                   ExportArgs *argsPtr)
{
    BLT_TABLE_COLUMN col;
    BLT_TABLE_ROW row;
    Blt_DBuffer dbuffer, dbuffer2;
    const char *query;
    int length, result;
    int count;                          /* sqlite3 parameter index. */
    sqlite3_stmt *stmt;
    
    dbuffer = Blt_DBuffer_Create();
    dbuffer2 = Blt_DBuffer_Create();
    Blt_DBuffer_Format(dbuffer, "INSERT INTO %s (", argsPtr->tableName);
    Blt_DBuffer_Format(dbuffer2, "(");
    count = 1;                          /* sqlite3 parameter indices start
                                         * from 1. */
    if (argsPtr->flags & EXPORT_ROWLABELS) {
        Blt_DBuffer_Format(dbuffer, "_rowId TEXT ");
        Blt_DBuffer_Format(dbuffer2, "?%d", count);
        count++;
    }        
    for (col = blt_table_first_tagged_column(&argsPtr->ci); col != NULL;
         col = blt_table_next_tagged_column(&argsPtr->ci)) {
        const char *label;
        
        label = blt_table_column_label(col);
        if (count > 1) {
            Blt_DBuffer_Format(dbuffer, ", ");
            Blt_DBuffer_Format(dbuffer2, ", ");
        }
        Blt_DBuffer_Format(dbuffer, "[%s]", label);
        Blt_DBuffer_Format(dbuffer2, "?%d", count);
        count++;
    }
    Blt_DBuffer_Format(dbuffer2, ");");
    Blt_DBuffer_Format(dbuffer, ") values ");
    Blt_DBuffer_Concat(dbuffer, dbuffer2);
    Blt_DBuffer_Destroy(dbuffer2);
    query = Blt_DBuffer_String(dbuffer);
    length = Blt_DBuffer_Length(dbuffer);
    result = sqlite3_prepare_v2(conn, query, length, &stmt, NULL);

    if (result != SQLITE_OK) {
        Tcl_AppendResult(interp, "error in insert statment \"", query, "\": ", 
                         sqlite3_errmsg(conn), (char *)NULL);
        Blt_DBuffer_Destroy(dbuffer);
        goto error;
    }
    if (stmt == NULL) {
        Tcl_AppendResult(interp, "empty statement in query \"", query, "\": ", 
                         sqlite3_errmsg(conn), (char *)NULL);
        Blt_DBuffer_Destroy(dbuffer);
        goto error;
    }
    Blt_DBuffer_Destroy(dbuffer);
    for (row = blt_table_first_tagged_row(&argsPtr->ri); row != NULL; 
         row = blt_table_next_tagged_row(&argsPtr->ri)) {
        int count;                      /* sqlite3 result set index. */
        
        count = 1;                      /* sqlite3 parameter indices start
                                         * from 1. */
        if (argsPtr->flags & EXPORT_ROWLABELS) {
            const char *label;
                    
            label = blt_table_row_label(row);
            sqlite3_bind_text(stmt, count, label, -1, NULL);
            count++;
        }
        for (col = blt_table_first_tagged_column(&argsPtr->ci); col != NULL;
             col = blt_table_next_tagged_column(&argsPtr->ci)) {
            if (!blt_table_value_exists(table, row, col)) {
                sqlite3_bind_null(stmt, count);
            } else {
                int type;
            
                type = blt_table_column_type(col);
                switch(type) {
                case TABLE_COLUMN_TYPE_LONG:
                case TABLE_COLUMN_TYPE_BOOLEAN:
                    {
                        long lval;
                        
                        lval = blt_table_get_long(interp, table, row, col, 0);
                        sqlite3_bind_int64(stmt, count, lval);
                    }
                    break;
                case TABLE_COLUMN_TYPE_DOUBLE:
                    {
                        double dval;
                        
                        dval = blt_table_get_double(interp, table, row, col);
                        sqlite3_bind_double(stmt, count, dval);
                    }
                    break;
                default:
                case TABLE_COLUMN_TYPE_STRING:
                    {
                        const char *sval;
                        
                        sval = blt_table_get_string(table, row, col);
                        sqlite3_bind_text(stmt, count, sval, -1, NULL);
                    }
                    break;
                }
            }
            count++;
        }
        do {
            result = sqlite3_step(stmt);
        } while (result == SQLITE_OK);
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
    return TCL_OK;
 error:
    if (stmt != NULL) {
        sqlite3_finalize(stmt);
    }
    return TCL_ERROR;
}

static int
ImportSqliteProc(BLT_TABLE table, Tcl_Interp *interp, int objc, 
                Tcl_Obj *const *objv)
{
    ImportArgs args;
    sqlite3 *conn;
    const char *fileName;
    int result;
    
    memset(&args, 0, sizeof(args));
    if (Blt_ParseSwitches(interp, importSwitches, objc - 3, objv + 3, 
                &args, BLT_SWITCH_DEFAULTS) < 0) {
        return TCL_ERROR;
    }
    if (args.fileObjPtr == NULL) {
        Tcl_AppendResult(interp, "-file switch is required.", (char *)NULL);
        return TCL_ERROR;
    }
    if (args.queryObjPtr == NULL) {
        Tcl_AppendResult(interp, "no -query switch found.", (char *)NULL);
        return TCL_ERROR;
    }
    conn = NULL;                          /* Suppress compiler warning. */
    fileName = Tcl_GetString(args.fileObjPtr);
    result = SqliteConnect(interp, fileName, &conn);
    if (result == TCL_OK) {
        result = SqliteImport(interp, table, conn, &args);
    }
    SqliteDisconnect(conn);
    Blt_FreeSwitches(importSwitches, &args, 0);
    return result;
}

static int
ExportSqliteProc(BLT_TABLE table, Tcl_Interp *interp, int objc, 
                Tcl_Obj *const *objv)
{
    ExportArgs args;
    sqlite3 *conn;
    int result;
    const char *fileName;
    
    if ((blt_table_num_rows(table) == 0) ||
        (blt_table_num_columns(table) == 0)) {
        return TCL_OK;                         /* Empty table. */
    }
    memset(&args, 0, sizeof(args));
    rowIterSwitch.clientData = table;
    columnIterSwitch.clientData = table;
    blt_table_iterate_all_rows(table, &args.ri);
    blt_table_iterate_all_columns(table, &args.ci);
    if (Blt_ParseSwitches(interp, exportSwitches, objc - 3, objv + 3, 
                &args, BLT_SWITCH_DEFAULTS) < 0) {
        return TCL_ERROR;
    }
    if (args.tableObjPtr != NULL) {
        args.tableName = Tcl_GetString(args.tableObjPtr);
    } else {
        args.tableName = "bltDataTable";
    }
    if (args.fileObjPtr == NULL) {
        Tcl_AppendResult(interp, "-file switch is required.", (char *)NULL);
        return TCL_ERROR;
    }
    conn = NULL;                          /* Suppress compiler warning. */
    fileName = Tcl_GetString(args.fileObjPtr);
    result = SqliteConnect(interp, fileName, &conn);
    if (result == TCL_OK) {
        result = SqliteCreateTable(interp, conn, table, &args);
    }
    if (result == TCL_OK) {
        result = SqliteExportValues(interp, conn, table, &args);
    }
    SqliteDisconnect(conn);
    Blt_FreeSwitches(exportSwitches, &args, 0);
    return result;
}

int 
blt_table_sqlite_init(Tcl_Interp *interp)
{
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION_COMPILED, PKG_ANY) == NULL) {
        return TCL_ERROR;
    };
#endif
#ifdef USE_BLT_STUBS
    if (Blt_InitTclStubs(interp, BLT_VERSION, PKG_EXACT) == NULL) {
        return TCL_ERROR;
    };
#else
    if (Tcl_PkgRequire(interp, "blt_tcl", BLT_VERSION, PKG_EXACT) == NULL) {
        return TCL_ERROR;
    }
#endif    
    if (Tcl_PkgProvide(interp, "blt_datatable_sqlite", BLT_VERSION) != TCL_OK) { 
        return TCL_ERROR;
    }
    return blt_table_register_format(interp,
        "sqlite",               /* Name of format. */
        ImportSqliteProc,       /* Import procedure. */
        ExportSqliteProc);      /* Export procedure. */

}

int 
blt_table_sqlite_safe_init(Tcl_Interp *interp)
{
    return blt_table_sqlite_init(interp);
}

#endif /* HAVE_LIBSQLITE */
#endif /* NO_DATATABLE */

