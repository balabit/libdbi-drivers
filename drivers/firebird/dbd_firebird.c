/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * libdbi - database independent abstraction layer for C.
 * Copyright (C) 2001-2004, David Parker and Mark Tobenkin.
 * http://libdbi.sourceforge.net
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * dbd_firebird.c: Firebird/Interbase database support
 * Copyright (C) 2004, Christian M. Stamgren <christian@stamgren.com>
 * http://libdbi-drivers.sourceforge.net
 *
 */

/**
 * Warning!! Warning!! Warning!!
 * This driver has a big gotha, You can't use dbi_result_numrows() 
 * This is because I don't know of a way to fetch the number of rows in a result set from firebird.
 * The only way to do it is to fetch all rows and count them..... 
 */


#ifdef  HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE /* we need asprintf */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <dbi/dbi.h>
#include <dbi/dbi-dev.h>
#include <dbi/dbd.h>


#include <ibase.h>
#include <gds.h>
#include "dbd_firebird.h"

static const dbi_info_t driver_info = {
        "Firebird",
	"Firebird/Interbase database support",
	"Christian M. Stamgren <christian@stamgren.com>",
	"http://libdbi-drivers.sourceforge.net",
	"dbd_firebird v" VERSION,
	__DATE__
};

#define MAXLEN 1024

#ifndef ISC_INT64_FORMAT
#define ISC_INT64_FORMAT        "ll"
#endif

static const char *custom_functions[] = {NULL}; 
static const char *reserved_words[] = FIREBIRD_RESERVED_WORDS;


void _translate_firebird_type(int field_type,  unsigned short *type, unsigned int *attribs);
void _get_field_info(dbi_result_t *result);
int _get_row_data(dbi_result_t *result, dbi_row_t *row, unsigned long long rowidx);
static unsigned long long return_generator_value(dbi_conn_t *conn, const char *sequence, int type);

void dbd_register_driver(const dbi_info_t **_driver_info, const char ***_custom_functions, 
			 const char ***_reserved_words) 
{
	*_driver_info = &driver_info;
	*_custom_functions = custom_functions;
	*_reserved_words = reserved_words;
}

int dbd_initialize(dbi_driver_t *driver) 
{
         return 0;
}


int dbd_connect(dbi_conn_t *conn) 
{
	char dpb_buffer[256], *dpb; 
	short dpb_length; 

        isc_db_handle db = NULL; /* database handle */
        isc_tr_handle trans = NULL; /* transaction handle */
	ibase_conn_t *iconn = (ibase_conn_t * ) malloc(sizeof(ibase_conn_t));

	const char *dbase =  dbi_conn_get_option(conn, "dbname");
	const char *username = dbi_conn_get_option(conn, "username");
	const char *password = dbi_conn_get_option(conn, "password");


	dpb = dpb_buffer;
	*dpb++ = isc_dpb_version1;
	*dpb++ = isc_dpb_num_buffers;
	*dpb++ = 1;
	*dpb++ = 90;
	dpb_length = dpb - dpb_buffer;
	
	isc_expand_dpb(&dpb, &dpb_length,
		       isc_dpb_user_name, username,
		       isc_dpb_password, password,
		       NULL);

	if ( isc_attach_database(iconn->status, 0, (char*)dbase, &db, dpb_length, dpb_buffer) ||
	     isc_start_transaction(iconn->status, &trans, 1, &db, 0, NULL)) {
		free(iconn);
		iconn = NULL;
		return -1;
	}

	iconn->trans = trans;
	iconn->db = db;
	conn->connection = (void *)iconn;

        if (dbase) conn->current_db = strdup(dbase);
	
	return 0;
}

int dbd_disconnect(dbi_conn_t *conn) 
{
	
	ibase_conn_t *iconn = conn->connection;
    
	if(iconn != NULL) {
		isc_commit_transaction(iconn->status, &(iconn->trans));
		isc_detach_database(iconn->status, &(iconn->db));
    
		free(iconn);
		iconn = NULL;
	}

	return 0;
}


int dbd_fetch_row(dbi_result_t *result, unsigned long long rownum) 
{
	dbi_row_t *row = NULL;
	
	if (result->result_state == NOTHING_RETURNED) return -1;
	if (result->result_state == ROWS_RETURNED) {
	        row = _dbd_row_allocate(result->numfields);
		if( _get_row_data(result, row, rownum) == 0 ) 
			return 0;
		
		_dbd_row_finalize(result, row, rownum);
	}
  
	return 1; 
}


int dbd_free_query(dbi_result_t *result) 
{
        ISC_STATUS_ARRAY status;
	ibase_stmt_t *istmt = result->result_handle;
	
	if(istmt != NULL) {
		isc_dsql_free_statement(status, &(istmt->stmt), DSQL_drop);
		free(istmt->osqlda);
		free(istmt);
	}

	istmt = NULL;
	return 0;
}


int dbd_goto_row(dbi_result_t *result, unsigned long long row) 
{	
	return 1;
}


int dbd_get_socket(dbi_conn_t *conn)
{
	return 0; 
}

const char *dbd_get_encoding(dbi_conn_t *conn)
{
	return NULL;
}

dbi_result_t *dbd_list_dbs(dbi_conn_t *conn, const char *pattern) 
{
	return NULL; 
}


dbi_result_t *dbd_list_tables(dbi_conn_t *conn, const char *db, const char *pattern) 
{

	if (pattern == NULL) {
		return  dbd_query(conn, "SELECT RDB$RELATION_NAME FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0");
	}
	else {
		dbi_result_t *res;
		char *sql_cmd;
		asprintf(&sql_cmd, "SELECT RDB$RELATION_NAME FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0 "
			           " AND RDB$RELATION_NAME LIKE '%s'", pattern);
		res = dbd_query(conn, sql_cmd);
		free(sql_cmd);
		return res;
	}
}


int dbd_quote_string(dbi_driver_t *driver, const char *orig, char *dest) 
{
	const char *worker = orig;
	register int i = 0, j = 0;
	int length = strlen(orig);

	for(i = 0; i < length; i++) {
		
		switch(worker[i]) {
		case '\'': 
			dest[j++] = '\'';
			break;
				
		}
		dest[j++] = worker[i];
	}

	return j;
}


dbi_result_t *dbd_query_null(dbi_conn_t *conn, const char unsigned *statement, unsigned long st_length) 
{	
	return NULL;
}


dbi_result_t *dbd_query(dbi_conn_t *conn, const char *statement) 
{

	XSQLVAR *var;
	XSQLDA  *sqlda; /* output SQLDA */
	isc_stmt_handle stmt = NULL; /* statement handle */
	ibase_stmt_t *res;
	dbi_result_t *result;
	static char stmt_info[] = { isc_info_sql_stmt_type };
	char info_buffer[20];
	short l;
	long statement_type;
	short num_cols, i;
	short length, alignment, type, offset;
	long buffer[MAXLEN];
	unsigned long long numrows = 0, affectedrows = 0;
	ibase_conn_t *iconn = conn->connection;

       if (isc_dsql_allocate_statement(iconn->status, &(iconn->db), &stmt)) {
	       return NULL;
       }
       
       sqlda = (XSQLDA *) malloc(XSQLDA_LENGTH (1));
       sqlda->sqln = 1;
       sqlda->version = 1;

       if (isc_dsql_prepare(iconn->status, &(iconn->trans), &stmt, 0, (char *)statement, 1, sqlda)) {
	       free(sqlda);
	       isc_dsql_free_statement(iconn->status, &stmt, DSQL_drop);
	       return NULL;
       }

       if (!isc_dsql_sql_info(iconn->status, &stmt, sizeof(stmt_info), stmt_info,
			      sizeof(info_buffer), info_buffer)) {
	       l = (short) isc_vax_integer((char *) info_buffer + 1, 2);
	       statement_type = isc_vax_integer((char *) info_buffer + 3, l); 
       }
       /* Execute a non-select statement.*/
       if (!sqlda->sqld) {

	       if (isc_dsql_execute(iconn->status, &(iconn->trans), &stmt, SQL_DIALECT_V6, NULL)) {
		       free(sqlda);
		       isc_dsql_free_statement(iconn->status, &stmt, DSQL_drop);
		       return NULL;
	       }
	       /* Commit DDL statements if that is what sql_info says */
	       if (iconn->trans && (statement_type == isc_info_sql_stmt_ddl)) {
		       if (isc_commit_transaction(iconn->status, &(iconn->trans))) {
			       free(sqlda);
			       isc_dsql_free_statement(iconn->status, &stmt, DSQL_drop);
			       return NULL;
		       }
	       }
       } else {
	       
	       /* Process select statements. */
	       
	       num_cols = sqlda->sqld;
	       numrows = 1; /*  Firebird  can't say how many rows there is, in this early stage. 
				We need to fetch all rows and count them :( */
	 
       
	       /* Need more room. */
	       if (sqlda->sqln < num_cols) {
		       sqlda = (XSQLDA *) realloc(sqlda, XSQLDA_LENGTH(num_cols));
		       sqlda->sqln = num_cols;
		       sqlda->version = 1;
		       
		       if (isc_dsql_describe(iconn->status, &stmt, SQL_DIALECT_V6, sqlda)) {
			       free(sqlda);
			       isc_dsql_free_statement(iconn->status, &stmt, DSQL_drop);
			       return NULL;
		       }
		       
	       }
	       for (var = sqlda->sqlvar, offset = 0, i = 0; i < num_cols; var++, i++) {
		       length = alignment = var->sqllen;
		       type = var->sqltype & ~1;
		       
		       if (type == SQL_TEXT)
			       alignment = 1;
		       else if (type == SQL_VARYING) {
			       length += sizeof (short) + 1;
			       alignment = sizeof (short);
		       }
		       
		       offset = FB_ALIGN(offset, alignment);
		       var->sqldata = (char *) buffer + offset;
		       offset += length;
		       offset = FB_ALIGN(offset, sizeof (short));
		       var->sqlind = (short*) ((char *) buffer + offset);
		       offset += sizeof  (short);
	       }
	       
	       if (isc_dsql_execute(iconn->status, &(iconn->trans), &stmt, SQL_DIALECT_V6, NULL)) {
		       free(sqlda);
		       isc_dsql_free_statement(iconn->status, &stmt, DSQL_drop);
		       return NULL;
	       }       
       }

       res = (ibase_stmt_t *)malloc(sizeof(ibase_stmt_t));
       res->stmt = stmt;
       res->osqlda = sqlda;
       res->rowcount = 1;
       
       result = _dbd_result_create(conn, (void *)res, numrows , affectedrows);
       _dbd_result_set_numfields(result, res->osqlda->sqld);
       _get_field_info(result);
       
       return result;
}


char *dbd_select_db(dbi_conn_t *conn, const char *db) 
{
	ibase_conn_t *iconn = conn->connection;

	if (!db || !*db) {
		return NULL;
	}

	if (iconn) {
		isc_commit_transaction(iconn->status, &(iconn->trans));
                isc_detach_database(iconn->status, &(iconn->db));
		if(conn->current_db) free(conn->current_db);
		free(iconn);
		iconn = NULL;
	}

	dbi_conn_set_option(conn, "dbname", db);
	if (dbd_connect(conn)) {
		return NULL;
	}

	return (char *)db; 
}

int dbd_geterror(dbi_conn_t *conn, int *errno, char **errstr) 
{
        ibase_conn_t *iconn = conn->connection;
        
	TEXT errbuf[MAXLEN];
	long sqlcode;

	if ( conn->connection == NULL) {
                *errstr = strdup("Unable to connect to database.");
		return 1;
	}
	
	 
	sqlcode = isc_sqlcode(iconn->status); 
	isc_sql_interprete(sqlcode, errbuf, sizeof(errbuf)); 
	*errstr = strdup(errbuf);
	  
	return 1;
}


unsigned long long dbd_get_seq_last(dbi_conn_t *conn, const char *sequence) 
{  
	return return_generator_value(conn, sequence, 0); //0 is currval
}

unsigned long long dbd_get_seq_next(dbi_conn_t *conn, const char *sequence) 
{		
	return return_generator_value(conn, sequence, 1); //1 is nextval
}


int dbd_ping(dbi_conn_t *conn) 
{
	char buf[100];
	ibase_conn_t *iconn = conn->connection;
        
	if (isc_database_info(iconn->status, &(iconn->db), 0, NULL, sizeof(buf), buf)) {
		free(iconn);
		if (conn->current_db ) free(conn->current_db);
		if(! dbd_connect(conn)) return 0;
	}

	return 1;
}

/* CORE FIREBIRD DATA FETCHING STUFF */
void _translate_firebird_type(int fieldtype, unsigned short *type, unsigned int *attribs) 
{
	unsigned int _type = 0;
	unsigned int _attribs = 0;
		
	switch (fieldtype) {
		
	  
	case SQL_TEXT:
		_type = DBI_TYPE_STRING;
	        _attribs |= DBI_STRING_FIXEDSIZE;
	case SQL_VARYING:
	        _type = DBI_TYPE_STRING;
		break;
	case SQL_SHORT:
	        _type = DBI_TYPE_INTEGER;
	        _attribs |= DBI_INTEGER_SIZE2;
		break;
	case SQL_LONG:
	        _type = DBI_TYPE_INTEGER;
	        _attribs |= DBI_INTEGER_SIZE3;
		break;
	case SQL_INT64:
	        _type = DBI_TYPE_INTEGER;
	        _attribs |= DBI_INTEGER_SIZE8;
		break;
	case SQL_FLOAT:
	        _type = DBI_TYPE_DECIMAL;
	        _attribs |= DBI_DECIMAL_SIZE4;
		break;
	case SQL_DOUBLE:
	        _type = DBI_TYPE_DECIMAL;
	        _attribs |= DBI_DECIMAL_SIZE8;
		break;
	case SQL_TIMESTAMP:
		_type = DBI_TYPE_DATETIME;
		break;
	case SQL_TYPE_TIME:
	        _type = DBI_TYPE_DATETIME;
	        _attribs |= DBI_DATETIME_TIME;
		break;
	case SQL_TYPE_DATE:
	        _type = DBI_TYPE_DATETIME;
		_attribs |= DBI_DATETIME_DATE;
		break;
	case SQL_BLOB:
	case SQL_ARRAY:
	        _type = DBI_TYPE_BINARY;
		break;
	}
	
	*type = _type;
	*attribs = _attribs;
}
 
void _get_field_info(dbi_result_t *result) 
{
	unsigned int idx = 0;
	unsigned short fieldtype;
	unsigned int fieldattribs;
	ibase_stmt_t *istmt = result->result_handle;

	while (idx < result->numfields) {
		_translate_firebird_type((istmt->osqlda->sqlvar[idx].sqltype & ~1), &fieldtype, &fieldattribs);
		_dbd_result_add_field(result, idx, istmt->osqlda->sqlvar[idx].sqlname, fieldtype, fieldattribs);
		idx++;
	}
}

int _get_row_data(dbi_result_t *result, dbi_row_t *row, unsigned long long rowidx) 
{
	int curfield = 0;
	XSQLVAR var;
	long fetch_stat = 0, blob_stat = 0;
	ISC_QUAD bid;
	isc_blob_handle blob_handle = NULL; /* Blob handle. */ 
	char blob_segment[80];
	unsigned short actual_seg_len;		
	struct tm times;
	char date_s[25];
	unsigned long sizeattrib;
	dbi_data_t *data = NULL;
	ibase_stmt_t *istmt = result->result_handle;
	ibase_conn_t *iconn = (ibase_conn_t *)result->conn->connection;

	fetch_stat = isc_dsql_fetch(iconn->status, &(istmt->stmt), SQL_DIALECT_V6, istmt->osqlda);

	if (fetch_stat != 0) {
		result->numrows_matched--;
		return 0;
	}

	while (curfield < result->numfields) {
		var = istmt->osqlda->sqlvar[curfield];
		data = &row->field_values[curfield];

		/**
		 * If the column holds a NULL value mark it as NULL 
		 */
		if ( (var.sqltype & 1) && ( *var.sqlind < 0)) {
			_set_field_flag( row, curfield, DBI_VALUE_NULL, 1);
			curfield++;
			continue;
		}
		
		
		switch ( result->field_types[curfield] ) {
		case DBI_TYPE_STRING: 
			if(result->field_attribs[curfield] & DBI_STRING_FIXEDSIZE) {
				data->d_string = strdup(var.sqldata);
				row->field_sizes[curfield] = (unsigned long long) var.sqllen;
			} else {
				vary_t *vary = NULL;
				vary = (vary_t *) var.sqldata;
				vary->vary_string[vary->vary_length] = '\0';
				data->d_string = strdup(vary->vary_string);
				row->field_sizes[curfield] = (unsigned long long) vary->vary_length;
			}
			break;
			
		case DBI_TYPE_INTEGER:
			sizeattrib = _isolate_attrib(result->field_attribs[curfield], DBI_INTEGER_SIZE1, DBI_INTEGER_SIZE8);
			switch (sizeattrib) {
			case DBI_INTEGER_SIZE1:
				
			case DBI_INTEGER_SIZE2:
				data->d_short = *(short *) var.sqldata; break;
			case DBI_INTEGER_SIZE3:
			case DBI_INTEGER_SIZE4:
				data->d_long = *(long *) var.sqldata; break;
			case DBI_INTEGER_SIZE8:
				data->d_longlong = *(ISC_INT64 *) var.sqldata; break;
			default:
				break;
			}
			break;
		case DBI_TYPE_DECIMAL:
			sizeattrib = _isolate_attrib(result->field_attribs[curfield], DBI_DECIMAL_SIZE4, DBI_DECIMAL_SIZE8);
			switch (sizeattrib) {
			case DBI_DECIMAL_SIZE4:
				data->d_float = *(float *) (var.sqldata); break;
			case DBI_DECIMAL_SIZE8:
				data->d_double = *(double *) (var.sqldata); break;
			default:
				break;
			}
			break;
		case DBI_TYPE_DATETIME:
			sizeattrib = _isolate_attrib(result->field_attribs[curfield], DBI_DATETIME_DATE, DBI_DATETIME_TIME);
			
			switch(sizeattrib) {
			case DBI_DATETIME_TIME: 
				isc_decode_sql_time((ISC_TIME *)var.sqldata, &times);
				sprintf(date_s, "%02d:%02d:%02d.%04d",
					times.tm_hour,
					times.tm_min,
					times.tm_sec,
					(*((ISC_TIME *)var.sqldata)) % 10000);
				break;
				
			case DBI_DATETIME_DATE:
				isc_decode_sql_date((ISC_DATE *)var.sqldata, &times);
				sprintf(date_s, "%04d-%02d-%02d",
					times.tm_year + 1900,
					times.tm_mon+1,
					times.tm_mday);
				break;
			default:
				isc_decode_timestamp((ISC_TIMESTAMP *)var.sqldata, &times);
				sprintf(date_s, "%04d-%02d-%02d %02d:%02d:%02d.%04d",
					times.tm_year + 1900,
					times.tm_mon+1,
					times.tm_mday,
					times.tm_hour,
					times.tm_min,
					times.tm_sec,
					((ISC_TIMESTAMP *)var.sqldata)->timestamp_time % 10000);
				break;	
			}
			data->d_datetime = _dbd_parse_datetime(date_s, sizeattrib);		
			break;
		
		case DBI_TYPE_BINARY:
			bid = *(ISC_QUAD *) var.sqldata;
			
			isc_open_blob2( iconn->status, &(iconn->db), &(iconn->trans), &blob_handle, &bid, 0, NULL );
			blob_stat = isc_get_segment( iconn->status, &blob_handle,  &actual_seg_len,  
						     sizeof(blob_segment), blob_segment  );

			data->d_string = malloc(sizeof(actual_seg_len));
			memcpy(data->d_string, blob_segment, actual_seg_len);
			row->field_sizes[curfield] = actual_seg_len;

			while (blob_stat == 0 || iconn->status[1] == isc_segment) { 
				blob_stat = isc_get_segment(iconn->status, &blob_handle, 
							    &actual_seg_len, 
							    sizeof(blob_segment), blob_segment); 

				data->d_string = realloc(data->d_string, 
							 row->field_sizes[curfield] + 
							 actual_seg_len);
				memcpy(data->d_string+row->field_sizes[curfield], 
				       blob_segment, actual_seg_len);
				row->field_sizes[curfield] += actual_seg_len;
			} 
			isc_close_blob(iconn->status, &blob_handle);
			break;
				
		default:
			break;
		}
		
		
		curfield++;
	}

	if( fetch_stat != 100L ) {
		result->rows = realloc(result->rows, (sizeof(dbi_row_t *) * (result->numrows_matched+1)));
		result->numrows_matched++;
	}

	return result->numrows_matched;
}

static unsigned long long return_generator_value(dbi_conn_t *conn, const char *sequence, int type)
{
	unsigned long long retval = 0;
	char *sql_cmd = NULL;
	dbi_result_t *result;
	ibase_stmt_t *istmt = NULL;
	ibase_conn_t *iconn = conn->connection;

	asprintf(&sql_cmd, "SELECT GEN_ID( %s ,%d ) FROM RDB$DATABASE",sequence, type );	
	result = dbd_query(conn, sql_cmd);

	istmt = result->result_handle;
	if(! isc_dsql_fetch(iconn->status, &(istmt->stmt), SQL_DIALECT_V6, istmt->osqlda) ) {
		retval = *(long *) istmt->osqlda->sqlvar[0].sqldata;
	}
	dbi_result_free(result);
	free(sql_cmd);
	return retval;
} 
