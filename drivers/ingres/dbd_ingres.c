/*
 * libdbi - database independent abstraction layer for C.
 * Ingres driver Copyright (C) 2006 Toby Thain <qu1j0t3 at sourceforge.net>
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
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE /* we need asprintf */

#ifndef HAVE_ATOLL
long long atoll(const char *str);
#endif

#ifndef HAVE_STRTOLL
long long strtoll(const char *nptr, char **endptr, int base);
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> /* for isdigit() */

#include <dbi/dbi.h>
#include <dbi/dbi-dev.h>
#include <dbi/dbd.h>

#include "dbd_ingres.h"
#include "iiapi.h"

static const dbi_info_t driver_info = {
	"ingres",
	"Ingres database support",
	"Toby Thain <qu1j0t3@sourceforge.net>",
	"http://libdbi-drivers.sourceforge.net",
	"dbd_ingres v" VERSION,
	__DATE__
};

static const char *custom_functions[] = {NULL}; // TODO
static const char *reserved_words[] = INGRES_RESERVED_WORDS;

/* encoding strings, array is terminated by a pair of empty strings */
static const char encoding_hash[][16] = {
  "SQL_ASCII", "US-ASCII",
  "", ""
};

typedef struct {
	II_PTR connHandle;
	II_PTR autoTranHandle;
	II_PTR lastStmtHandle;
} ingres_conn_t;

/* forward declarations of internal functions */
void ingres_classify_field(IIAPI_DESCRIPTOR *ds, unsigned short *type, unsigned int *attribs);
void _get_field_info(dbi_result_t *result);
void _get_row_data(dbi_result_t *result, dbi_row_t *row, unsigned long long rowidx);
int _dbd_real_connect(dbi_conn_t *conn, const char *db);
void _dbd_geterror(dbi_conn_t *conn, II_PTR errorHandle);

/* real code starts here */
void dbd_register_driver(const dbi_info_t **_driver_info, const char ***_custom_functions, const char ***_reserved_words) {
	/* this is the first function called after the driver module is loaded into memory */
	*_driver_info = &driver_info;
	*_custom_functions = custom_functions;
	*_reserved_words = reserved_words;
}

int dbd_initialize(dbi_driver_t *driver) {
	/* perform any database-specific server initialization.
	 * this is called right after dbd_register_driver().
	 * return -1 on error, 0 on success. if -1 is returned, the driver will not
	 * be added to the list of available drivers. */
    IIAPI_INITPARM  initParm;

    initParm.in_version = IIAPI_VERSION_1; 
    initParm.in_timeout = -1;
    IIapi_initialize( &initParm );
	return (initParm.in_status >= IIAPI_ST_SUCCESS 
			&& initParm.in_status < IIAPI_ST_ERROR) - 1;
}

int dbd_connect(dbi_conn_t *conn) {
  return _dbd_real_connect(conn, NULL);
}

#define ERRMAX 1024
void _dbd_geterror(dbi_conn_t *conn, II_PTR errorHandle){
	static char *typestr[] = {"0","ERROR","WARNING","MESSAGE"};
	char *msg, *p, buf[0x100];
	int count, n;
	IIAPI_GETEINFOPARM eiParm;

	if( errorHandle /*&& (msg = malloc(ERRMAX))*/ ){
		eiParm.ge_errorHandle = errorHandle;
		for(p = msg, count = 0;;){
			IIapi_getErrorInfo(&eiParm);
			if(eiParm.ge_status == IIAPI_ST_SUCCESS){
				n = snprintf(buf, sizeof(buf), "%s-%s-%08X  %s\n",
						typestr[eiParm.ge_type], eiParm.ge_SQLSTATE, eiParm.ge_errorCode, eiParm.ge_message);
				_verbose_handler(conn, buf);
				//n = strlen(eiParm.ge_message);
				/*if(count+n < ERRMAX){
					memcpy(p, buf, n);
					p += n;
					count += n;
					*p++ = '\n';
				}*/
			}else
				break;
		}
		/*if(conn){
			if(conn->error_message) free(conn->error_message);
			conn->error_message = msg;
		}*/
	}
}

IIAPI_STATUS ingres_wait(dbi_conn_t *conn, IIAPI_GENPARM *pgp){
    IIAPI_WAITPARM	waitParm;

    for( waitParm.wt_timeout = -1 ; ! pgp->gp_completed ; )
		IIapi_wait(&waitParm);
	_dbd_geterror(conn, pgp->gp_errorHandle);
	return pgp->gp_status;
}

int _dbd_real_connect(dbi_conn_t *conn, const char *db) {
	ingres_conn_t *iconn;
    IIAPI_CONNPARM connParm;
    IIAPI_AUTOPARM acParm;
    IIAPI_STATUS status;
    char *autocommit = dbi_conn_get_option(conn, "ingres_autocommit");
    int ok, ac = autocommit ? atoi(autocommit) : 1; // enable auto-commit by default

    connParm.co_genParm.gp_callback = NULL;
    connParm.co_genParm.gp_closure = NULL;
    connParm.co_target = dbi_conn_get_option(conn, "dbname");
    connParm.co_connHandle = NULL;
    connParm.co_tranHandle = NULL;
    connParm.co_username = dbi_conn_get_option(conn, "username");
    connParm.co_password = dbi_conn_get_option(conn, "password");
    connParm.co_timeout = -1;

    IIapi_connect(&connParm);
    status = ingres_wait(conn, &connParm.co_genParm);

    conn->connection = iconn = malloc(sizeof(ingres_conn_t));
    iconn->connHandle = connParm.co_connHandle;
    iconn->autoTranHandle = NULL;
    iconn->lastStmtHandle = NULL;
    ok = status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR;
	if(ok && ac){
    	// set autocommit mode
    	acParm.ac_genParm.gp_callback = NULL;
    	acParm.ac_genParm.gp_closure = NULL;
    	acParm.ac_connHandle = connParm.co_connHandle;
    	acParm.ac_tranHandle = NULL;
    	IIapi_autocommit(&acParm);
    	status = ingres_wait(conn, &acParm.ac_genParm);
		if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR){
			// stash the autocommit transaction handle
			iconn->autoTranHandle = acParm.ac_tranHandle;
    		_verbose_handler(conn, "...enabled autocommit\n");
		}else
			_verbose_handler(conn, "...FAILED to enable autocommit\n");
    }

    return ok - 1;
}

int dbd_disconnect(dbi_conn_t *conn) {
	ingres_conn_t *iconn = conn->connection;
    IIAPI_DISCONNPARM disconnParm;
    IIAPI_AUTOPARM acParm;
    IIAPI_STATUS status;
    
    ingres_close(iconn->lastStmtHandle);
    
    if(iconn->autoTranHandle){
		// exit autocommit mode
		acParm.ac_genParm.gp_callback = NULL;
		acParm.ac_genParm.gp_closure = NULL;
		acParm.ac_connHandle = iconn->connHandle;
		acParm.ac_tranHandle = iconn->autoTranHandle;
		IIapi_autocommit(&acParm);
		status = ingres_wait(conn, &acParm.ac_genParm);
		if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR)
			_verbose_handler(conn, "...exited autocommit\n");
		else
			_verbose_handler(conn, "...FAILED to exit autocommit\n");
	}
			
    disconnParm.dc_genParm.gp_callback = NULL;
    disconnParm.dc_genParm.gp_closure = NULL;
    disconnParm.dc_connHandle = iconn->connHandle;
    _verbose_handler(conn, "disconnecting...\n");
    IIapi_disconnect( &disconnParm );
    ingres_wait(conn, &disconnParm.dc_genParm);
    free(conn->connection);
    conn->connection = NULL;
	return 0;
}

int dbd_fetch_row(dbi_result_t *result, unsigned long long rowidx) {
	dbi_row_t *row = NULL;

	if (result->result_state == NOTHING_RETURNED) return 0;
	
	if (result->result_state == ROWS_RETURNED) {
		/* get row here */
		row = _dbd_row_allocate(result->numfields);
		_get_row_data(result, row, rowidx);
		_dbd_row_finalize(result, row, rowidx);
	}
	
	return 1; /* 0 on error, 1 on successful fetchrow */
}

int ingres_close(II_PTR hdl) {
    IIAPI_STATUS status;
	IIAPI_CLOSEPARM closeParm;

	if(hdl){
		closeParm.cl_genParm.gp_callback = NULL;
		closeParm.cl_genParm.gp_closure = NULL;
		closeParm.cl_stmtHandle = hdl;
	    fputs("closing stmtHandle...\n", stderr);
		IIapi_close(&closeParm);
		status = ingres_wait(NULL, &closeParm.cl_genParm);
		return status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR;
	}
	return 0;
}

int dbd_free_query(dbi_result_t *result) {
	return 0;
}

int dbd_goto_row(dbi_result_t *result, unsigned long long rowidx) {
	/* libpq doesn't have to do anything, the row index is specified when
	 * fetching fields */
	return 1;
}

int dbd_get_socket(dbi_conn_t *conn)
{
	return 0; // we don't use socket
}

const char *dbd_get_encoding(dbi_conn_t *conn){
	  return NULL; // TODO
}

const char* dbd_encoding_to_iana(const char *db_encoding) {
  /* don't know how to translate, return original encoding */
  return db_encoding; //TODO
}

const char* dbd_encoding_from_iana(const char *iana_encoding) {
  /* don't know how to translate, return original encoding */
  return iana_encoding; //TODO
}

char *dbd_get_engine_version(dbi_conn_t *conn, char *versionstring) {
	// TODO: obtain this from iidbcapabilities catalog, INGRES/SQL_LEVEL
	*versionstring = 0;
	return versionstring;
}

dbi_result_t *dbd_list_dbs(dbi_conn_t *conn, const char *pattern) {
	// obtain from iidatabase_info catalog
/* FIXME: need to connect to iidbdb first
	if (pattern == NULL) {
		return dbd_query(conn, "SELECT database_name FROM iidatabase_info");
	}
	else {
		asprintf(&sql_cmd, "SELECT database_name FROM iidatabase_info WHERE datname LIKE '%s'", pattern);
		res = dbd_query(conn, sql_cmd);
		free(sql_cmd);
		return res;
	}
*/
	return NULL; // TODO
}

dbi_result_t *dbd_list_tables(dbi_conn_t *conn, const char *db, const char *pattern) {
	return NULL; // TODO
}

size_t dbd_quote_string(dbi_driver_t *driver, const char *orig, char *dest) {
	// Ingres quotes ' by doubling it
	char *p = dest,*q;
	*p++ = '\'';
	for(q=orig;*q;)
		if(*q == '\''){
			*p++ = '\'';
			*p++ = '\'';
			++q;
		}else
			*p++ = *q++;
	*p++ = '\'';
	*p = 0;
	return p-dest;
}

size_t dbd_conn_quote_string(dbi_conn_t *conn, const char *orig, char *dest) {
  return dbd_quote_string(conn->driver, orig, dest);
}

size_t dbd_quote_binary(dbi_conn_t *conn, const unsigned char* orig, size_t from_length, unsigned char **ptr_dest) {
	static char *hex = "0123456789ABCDEF";
	size_t to_length = from_length*2 + 4;
	unsigned char *p = malloc(to_length), *q;
	if(*ptr_dest = p){
		*p++ = 'X';
		*p++ = '\'';
		for(q=orig;from_length--;){
			*p++ = hex[(*q) >> 4];
			*p++ = hex[(*q) & 0xf];
			++q;
		}
		*p++ = '\'';
		*p = 0;
		return to_length;
	}
	return DBI_LENGTH_ERROR;
}

dbi_result_t *dbd_query(dbi_conn_t *conn, const char *statement) {
	ingres_conn_t *iconn = conn->connection;
	dbi_result_t *res = NULL;
	int i;
    IIAPI_QUERYPARM	queryParm;
    IIAPI_GETQINFOPARM gqParm;
	IIAPI_GETDESCRPARM gdParm;
	IIAPI_STATUS status;
    
    ingres_close(iconn->lastStmtHandle);
    
    queryParm.qy_genParm.gp_callback = NULL;
    queryParm.qy_genParm.gp_closure = NULL;
    queryParm.qy_connHandle = iconn->connHandle;
    queryParm.qy_queryType = IIAPI_QT_QUERY;
    queryParm.qy_queryText = statement;
    queryParm.qy_parameters = FALSE;
    queryParm.qy_tranHandle = iconn->autoTranHandle;
    queryParm.qy_stmtHandle = NULL;
    IIapi_query( &queryParm );
    status = ingres_wait(conn, &queryParm.qy_genParm);
   
	// status could be IIAPI_ST_NO_DATA here, which we treat as harmless.
	// libdbi docs say only return NULL in case of error. (An empty SELECT is fine.)
	if(status == IIAPI_ST_NO_DATA)
		_verbose_handler(conn,"(IIAPI_ST_NO_DATA: expected data, but none returned)\n");
	if (status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR){
    	// fetch the number of affected rows using getQueryInfo
		// This doesn't distinguish rows matched vs. rows changed,
		// so we could unnecessarily create space for a result set that will
		// never arrive. To find out if data really is available,
		// follow up with a call to getDescriptor
    	gqParm.gq_genParm.gp_callback = NULL;
    	gqParm.gq_genParm.gp_closure = NULL;
    	gqParm.gq_stmtHandle = queryParm.qy_stmtHandle;
    	IIapi_getQueryInfo(&gqParm);
	    status = ingres_wait(conn, &gqParm.gq_genParm);
		
		if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR && !(gqParm.gq_mask & IIAPI_GQ_ROW_COUNT))
			_verbose_handler(conn,"(no row count available)\n");
		if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR && (gqParm.gq_mask & IIAPI_GQ_ROW_COUNT)){
			_verbose_handler(conn,"gq_rowCount = %d\n",gqParm.gq_rowCount);
			gdParm.gd_genParm.gp_callback = NULL;
			gdParm.gd_genParm.gp_closure = NULL;
			gdParm.gd_stmtHandle = queryParm.qy_stmtHandle;
			IIapi_getDescriptor(&gdParm);
	    	status = ingres_wait(conn, &gqParm.gq_genParm);
			if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR){
				// if descriptorCount is zero, no data can be expected
				// create result struct anyway
				if(!gdParm.gd_descriptorCount)
					_verbose_handler(conn,"gd_descriptorCount is ZERO\n");
    			res = _dbd_result_create(conn, gqParm.gq_stmtHandle, 
    					gdParm.gd_descriptorCount ? gqParm.gq_rowCount : 0, gqParm.gq_rowCount);
    			for(i = 0; i < gdParm.gd_descriptorCount; ++i){
    				unsigned short type;
    				unsigned int attribs;
    				ingres_classify_field(&gdParm.gd_descriptor[i], &type, &attribs);
					_verbose_handler(conn,"field %d: '%s' ds_dataType=%d dbiType=%d dbiAttribs=%#x\n",
						i,gdParm.gd_descriptor[i].ds_columnName,gdParm.gd_descriptor[i].ds_dataType,type,attribs);
    				_dbd_result_add_field(res, i, gdParm.gd_descriptor[i].ds_columnName, type, attribs);
    			}
    		}
		}
	}
	
	// stash statement handle, we need to close this before issuing another query
	iconn->lastStmtHandle = queryParm.qy_stmtHandle;
	return res;
}

dbi_result_t *dbd_query_null(dbi_conn_t *conn, const unsigned char *statement, size_t st_length) {
	_verbose_handler(conn, "dbd_query_null() not implemented\n");
	return NULL;
}

const char *dbd_select_db(dbi_conn_t *conn, const char *db) {
/* Ingres doesn't support switching databases without reconnecting */
	if (!db || !*db)
		return NULL;
	
	if (conn->connection)
		dbd_disconnect(conn);
	
	if (_dbd_real_connect(conn, db))
		return NULL;
	
	return db;
}

int dbd_geterror(dbi_conn_t *conn, int *errno, char **errstr) {
	/* put error number into errno, error string into errstr
	 * return 0 if error, 1 if errno filled, 2 if errstr filled, 3 if both errno and errstr filled */
	// for now, just assume errstr was set up by a _dbd_geterror call
	// FIXME: we really need to be able to stash an errorhandle instead (after each API call), and use it here
	return 2;
}

unsigned long long dbd_get_seq_last(dbi_conn_t *conn, const char *sequence) {
	unsigned long long seq_last = 0;
	char *sql_cmd;
	char *rawdata;
	dbi_result_t *result;

	asprintf(&sql_cmd, "SELECT CURRENT VALUE FOR %s", sequence);
	if (!sql_cmd) return 0;
	result = dbd_query(conn, sql_cmd);
	free(sql_cmd);

	if (result) {
		// TODO
		dbi_result_free((dbi_result)result);
	}

	return seq_last;
}

unsigned long long dbd_get_seq_next(dbi_conn_t *conn, const char *sequence) {
	unsigned long long seq_next = 0;
	char *sql_cmd;
	char *rawdata;
	dbi_result_t *result;

	asprintf(&sql_cmd, "SELECT NEXT VALUE FOR %s", sequence);
	if (!sql_cmd) return 0;
	result = dbd_query(conn, sql_cmd);
	free(sql_cmd);

	if (result) {	
		// TODO
		dbi_result_free((dbi_result)result);
	}

	return seq_next;
}

int dbd_ping(dbi_conn_t *conn) {
	// TODO
	return 0;
}

/* CORE DATA FETCHING STUFF */

void ingres_classify_field(IIAPI_DESCRIPTOR *ds, unsigned short *type, unsigned int *attribs) {
	unsigned int _type = 0;
	unsigned int _attribs = 0;

	switch(ds->ds_dataType){
	//IIAPI_HNDL_TYPE
	// integer
	case IIAPI_INT_TYPE:
		_type = DBI_TYPE_INTEGER; // always signed
		switch(ds->ds_length){
		case 1: _attribs = DBI_INTEGER_SIZE1; break;
		case 2: _attribs = DBI_INTEGER_SIZE2; break;
		case 4: _attribs = DBI_INTEGER_SIZE4; break;
		case 8: _attribs = DBI_INTEGER_SIZE8; break;
		}
		break;
	// float
	case IIAPI_FLT_TYPE:
		_type = DBI_TYPE_DECIMAL;
		switch(ds->ds_length){
		case 4: _attribs = DBI_DECIMAL_SIZE4; break;
		case 8: _attribs = DBI_DECIMAL_SIZE8; break;
		}
		break;
	// fixed length binary string
	case IIAPI_BYTE_TYPE:
		_type = DBI_TYPE_BINARY;
		break;
	// fixed length char string
	case IIAPI_CHR_TYPE: // deprecated
	case IIAPI_CHA_TYPE:
	case IIAPI_LTXT_TYPE:
	case IIAPI_LOGKEY_TYPE:
	case IIAPI_TABKEY_TYPE:
		_type = DBI_TYPE_STRING;
		break;
	// variable length char string; segment length is first two bytes of datum
	case IIAPI_VCH_TYPE:
	case IIAPI_LVCH_TYPE:
	case IIAPI_TXT_TYPE: // deprecated
		_type = DBI_TYPE_STRING;
		break;
	// variable length binary string
	case IIAPI_LBYTE_TYPE:
	case IIAPI_VBYTE_TYPE:
		_type = DBI_TYPE_BINARY;
		break;
	// fixed length 'national character set' UTF-16 (wide) char string (TODO)
	case IIAPI_NCHA_TYPE: 
		break; //TODO
	// variable length UTF-16 string; first 2 bytes are length
	case IIAPI_NVCH_TYPE:
	case IIAPI_LNVCH_TYPE:
		break; //TODO
	// Ingres-native types, must be converted
	case IIAPI_DEC_TYPE: // packed decimal; use convertData or formatData
		break;
	case IIAPI_DTE_TYPE: // Ingres date; use convertData or formatData
		_type = DBI_TYPE_DATETIME;
		break;
	case IIAPI_MNY_TYPE: // Ingres money; use convertData or formatData
		break;
	}
	*type = _type;
	*attribs = _attribs;
}

void _get_field_info(dbi_result_t *result) {
/*
	unsigned int idx = 0;
	unsigned int pgOID = 0;
	char *fieldname;
	unsigned short fieldtype;
	unsigned int fieldattribs;
	
	while (idx < result->numfields) {
		//pgOID = PQftype((PGresult *)result->result_handle, idx);
		//fieldname = PQfname((PGresult *)result->result_handle, idx);
		ingres_classify_field(pgOID, &fieldtype, &fieldattribs);
		_dbd_result_add_field(result, idx, fieldname, fieldtype, fieldattribs);
		idx++;
	}
*/
}

void _get_row_data(dbi_result_t *result, dbi_row_t *row, unsigned long long rowidx) {
/*
	unsigned int curfield = 0;
	char *raw = NULL;
	size_t strsize = 0;
	unsigned int sizeattrib;
	dbi_data_t *data;
	unsigned char *temp = NULL;
	size_t unquoted_length;

	while (curfield < result->numfields) {
		//raw = PQgetvalue((PGresult *)result->result_handle, rowidx, curfield);
		data = &row->field_values[curfield];

		row->field_sizes[curfield] = 0; // will be set to strlen later on for strings
		
		//if (PQgetisnull((PGresult *)result->result_handle, rowidx, curfield) == 1)
		{
		        _set_field_flag( row, curfield, DBI_VALUE_NULL, 1);
			curfield++;
			continue;
		}
		
		switch (result->field_types[curfield]) {
		case DBI_TYPE_INTEGER:
			switch (result->field_attribs[curfield], DBI_INTEGER_SIZEMASK) {
			case DBI_INTEGER_SIZE1:
				data->d_char = atol(raw); break;
			case DBI_INTEGER_SIZE2:
				data->d_short = atol(raw); break;
			case DBI_INTEGER_SIZE3:
			case DBI_INTEGER_SIZE4:
				data->d_long = atol(raw); break;
			case DBI_INTEGER_SIZE8:
				data->d_longlong = atoll(raw); break;
			default:
				break;
			}
			break;
		case DBI_TYPE_DECIMAL:
			switch (result->field_attribs[curfield] & DBI_DECIMAL_SIZEMASK) {
			case DBI_DECIMAL_SIZE4:
				data->d_float = strtod(raw, NULL); break;
			case DBI_DECIMAL_SIZE8:
				data->d_double = strtod(raw, NULL); break;
			default:
				break;
			}
			break;
		case DBI_TYPE_STRING:
			break;
		case DBI_TYPE_BINARY:	
			break;
			
		case DBI_TYPE_DATETIME:
			sizeattrib = result->field_attribs[curfield] & (DBI_DATETIME_DATE|DBI_DATETIME_TIME);
			data->d_datetime = _dbd_parse_datetime(raw, sizeattrib);
			break;
			
		default:
			break;
		}
		
		curfield++;
	}
*/
}

