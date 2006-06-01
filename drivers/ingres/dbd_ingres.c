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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

static const char *custom_functions[] = {NULL};
static const char *reserved_words[] = INGRES_RESERVED_WORDS;

/* encoding strings, array is terminated by a pair of empty strings */
static const char encoding_hash[][16] = {
	"", "US-ASCII",
	"", ""
};

static II_PTR envHandle = NULL;

// quite arbitrary row count that we initially allocate, increasing as necessary.
// needed because Ingres can't provide a row count prior to fetching data.
#define INITIAL_ROWS	10
#define ROW_FACTOR		4 // multiply by this factor when we fill row array

#define SYS_CATALOGS	"iidbdb" // database name for system catalogs
#define NO_AUTOCOMMIT	"0" // mnemonic value for option

#define AUTOCOMMIT_ON(C) (((ingres_conn_t*)C->connection)->autoTranHandle != NULL)

typedef struct {
	II_PTR connHandle;
	II_PTR autoTranHandle;
	II_LONG sizeAdvise; // advised buffer size for blob (long) types
	dbi_conn_t *sysConn; // used for querying system catalogs
} ingres_conn_t;

typedef struct {
	II_PTR stmtHandle; // statement handle returned by associated query
	IIAPI_DESCRIPTOR *dataDesc; // filled by getDescriptor after query
} ingres_result_t;

/* forward declarations of internal functions */
static dbi_result_t *ingres_query(dbi_conn_t *conn, const char *statement, II_PTR *tranHandle);
static void ingres_classify_field(IIAPI_DESCRIPTOR *ds, unsigned short *type, unsigned int *attribs);
static int ingres_connect(dbi_conn_t *conn, const char *db, const char *autocommit);
static dbi_result_t *ingres_query(dbi_conn_t *conn, const char *statement, II_PTR *tranHandle);
void ingres_error(dbi_conn_t *conn, II_PTR errorHandle);

// months.gperf.c
struct month *in_word_set (register const char *str, register unsigned int len);

static IIAPI_STATUS ingres_wait(dbi_conn_t *conn, IIAPI_GENPARM *pgp){
	IIAPI_WAITPARM waitParm;

	for( waitParm.wt_timeout = -1 ; ! pgp->gp_completed ; )
		IIapi_wait(&waitParm);
	ingres_error(conn, pgp->gp_errorHandle);
	return pgp->gp_status;
}

static void ingres_close(II_PTR hdl) {
	IIAPI_STATUS status;
	IIAPI_CLOSEPARM closeParm = {{NULL, NULL}};

	if(hdl){
		closeParm.cl_stmtHandle = hdl;
		_verbose_handler(NULL, "closing stmtHandle %#x...\n", hdl);
		IIapi_close(&closeParm);
		ingres_wait(NULL, &closeParm.cl_genParm);
	}
}

static time_t ingres_date(char *raw){
	struct tm unixtime;
	char *p = raw, *q, sep;

	//_verbose_handler(NULL,"date: '%s'\n",raw);
	
	unixtime.tm_sec = unixtime.tm_min = unixtime.tm_hour = 0;
	unixtime.tm_isdst = -1;
	
	// parse Ingres default (US) date format: dd-mmm-yyyy [hh:mm:ss]
	//for(p = raw; *p && !isdigit(*p); ++p)
	//	;
	if(isdigit(*p)){
		// process day
		unixtime.tm_mday = atoi(p); //_verbose_handler(NULL,"day: %d ",unixtime.tm_mday);
		while(*p && isdigit(*p))
			++p;
		if(!*p){ _verbose_handler(NULL,"date ended after day??",raw); return 0; }
		sep = *p++; // skip separator

		// process month
		if(isdigit(*p)){
			unixtime.tm_mon = atoi(p)-1; /* months are 0 through 11 */
			//_verbose_handler(NULL,"month: %d ",unixtime.tm_mon);
			while(*p && *p != sep)
				++p;
		}else{
			q = p; // point to start of month name
			while(*p && *p != sep)
				++p;
			if(*p){
				*p = 0;
				unixtime.tm_mon = in_word_set(q,3)->index; // should work for long month names too
				//_verbose_handler(NULL,"month: %s -> %d ",q,unixtime.tm_mon);
				++p;
			}
		}
		if(!*p){ _verbose_handler(NULL,"date ended after month??",raw); return 0; }
		
		// process year
		unixtime.tm_year = atoi(p)-1900; //_verbose_handler(NULL,"year: %d\n",unixtime.tm_year);
		while(isdigit(*p))
			++p;

		// skip space following date
		while(isspace(*p))
			++p;

		// Ingres does not generate a time by itself, it's always preceded by a date.
		if(isdigit(*p)){ // time is present
			// process hours
			unixtime.tm_hour = atoi(p); //_verbose_handler(NULL,"hour: %d ",unixtime.tm_hour);
			while(isdigit(*p))
				++p;
			if(!*p){ _verbose_handler(NULL,"time ended after hour??",raw); return 0; }
			++p; // skip separator

			// process minutes
			unixtime.tm_min = atoi(p); //_verbose_handler(NULL,"min: %d ",unixtime.tm_min);
			while(isdigit(*p))
				++p;
			if(!*p){ _verbose_handler(NULL,"time ended after minute??",raw); return 0; }
			++p; // skip separator

			// process seconds
			unixtime.tm_sec = atoi(p); //_verbose_handler(NULL,"sec: %d\n",unixtime.tm_sec);
	
			/* check for a timezone suffix */
			//while(isdigit(*p) || isspace(*p))
			//	++p;
		}else if(*p)
			_verbose_handler(NULL,"bad time: '%s'",p);

		return timegm(&unixtime);
	}else
		_verbose_handler(NULL,"bad date: '%s'",raw);
	return 0;
}

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

	initParm.in_version = IIAPI_VERSION_3;
	initParm.in_timeout = -1;
	IIapi_initialize( &initParm );
	envHandle = initParm.in_envHandle;
	_verbose_handler(NULL, "initialize: envHandle=%#x\n",envHandle);
	return (initParm.in_status >= IIAPI_ST_SUCCESS 
			&& initParm.in_status < IIAPI_ST_ERROR) - 1;
}

int dbd_connect(dbi_conn_t *conn) {
	return ingres_connect(conn, NULL, dbi_conn_get_option(conn, "ingres_autocommit"));
}

#define ERRMAX 1024
void ingres_error(dbi_conn_t *conn, II_PTR errorHandle){
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
							 typestr[eiParm.ge_type], eiParm.ge_SQLSTATE, 
							 eiParm.ge_errorCode, eiParm.ge_message);
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

static int ingres_option_num(dbi_conn_t *conn, IIAPI_SETCONPRMPARM *psc, II_LONG id, char *name){
	const char *opt = dbi_conn_get_option(conn, name);

	if(opt){
		II_LONG val = atoi(opt);
		psc->sc_paramID = id;
		psc->sc_paramValue = &val; // works for Ingres 'long' and 'bool' types too, which are all int.
		IIapi_setConnectParam(psc);
		return ingres_wait(conn, &psc->sc_genParm) == IIAPI_ST_SUCCESS;
	}
	return 1;
}

static int ingres_option_str(dbi_conn_t *conn, IIAPI_SETCONPRMPARM *psc, II_LONG id, char *name){
	const char *opt = dbi_conn_get_option(conn, name);

	if(opt){
		psc->sc_paramID = id;
		psc->sc_paramValue = opt; // works for Ingres 'char' options too, of course
		IIapi_setConnectParam(psc);
		return ingres_wait(conn, &psc->sc_genParm) == IIAPI_ST_SUCCESS;
	}
	return 1;
}

static int ingres_connect(dbi_conn_t *conn, const char *db, const char *autocommit) {
	ingres_conn_t *iconn;
	IIAPI_SETCONPRMPARM scParm = {{NULL, NULL}};
	IIAPI_CONNPARM connParm = {{NULL, NULL}};
	IIAPI_AUTOPARM acParm = {{NULL, NULL}};
	IIAPI_STATUS status;

	scParm.sc_connHandle = NULL; // later, envHandle, but currently that causes connect to fail (?!)
    
	//_verbose_handler(NULL, "ingres_connect: envHandle=%#x\n",envHandle);

    // see OpenAPI reference for meaning of these options. Numeric codes in iiapi.h
	ingres_option_num(conn, &scParm, IIAPI_CP_CENTURY_BOUNDARY, "ingres_century_bdry"); // interpretation of 2-digit years
	ingres_option_num(conn, &scParm, IIAPI_CP_DATE_FORMAT, "ingres_date_format");
	ingres_option_str(conn, &scParm, IIAPI_CP_DECIMAL_CHAR, "ingres_decimal_char"); // seems to affect input format, not output?
	ingres_option_num(conn, &scParm, IIAPI_CP_EXCLUSIVE_LOCK, "ingres_excl_lock"); // application-exclusive lock on db
	ingres_option_str(conn, &scParm, IIAPI_CP_MONEY_SIGN, "ingres_money_sign"); // does not seem to affect result format?
	ingres_option_num(conn, &scParm, IIAPI_CP_MONEY_PRECISION, "ingres_money_prec");
	ingres_option_num(conn, &scParm, IIAPI_CP_MONEY_LORT, "ingres_money_lort");
	ingres_option_str(conn, &scParm, IIAPI_CP_NATIVE_LANG, "ingres_native_lang"); // language for server messages
	ingres_option_num(conn, &scParm, IIAPI_CP_NATIVE_LANG_CODE, "ingres_native_lang_code"); // language for server messages
	ingres_option_str(conn, &scParm, IIAPI_CP_NUMERIC_TREATMENT, "ingres_num_treatment"); // treatment of decimal literals
	ingres_option_str(conn, &scParm, IIAPI_CP_STRING_TRUNC, "ingres_string_trunc"); // overlong string behaviour
	ingres_option_str(conn, &scParm, IIAPI_CP_TIMEZONE, "ingres_timezone");

	connParm.co_target = db ? db : dbi_conn_get_option(conn, "dbname");
	connParm.co_connHandle = scParm.sc_connHandle;
	connParm.co_tranHandle = NULL;
	connParm.co_username = dbi_conn_get_option(conn, "username");
	connParm.co_password = dbi_conn_get_option(conn, "password");
	connParm.co_timeout = -1;

	IIapi_connect(&connParm);
	status = ingres_wait(conn, &connParm.co_genParm);

	if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR){
		_verbose_handler(conn, "connected, target='%s', API level=%d\n",
						 connParm.co_target, connParm.co_apiLevel);
		conn->connection = iconn = malloc(sizeof(ingres_conn_t));
		iconn->connHandle = connParm.co_connHandle;
		iconn->autoTranHandle = NULL;
		iconn->sizeAdvise = connParm.co_sizeAdvise;
		iconn->sysConn = NULL;
		if(!autocommit || atoi(autocommit)){ // enable auto-commit by default
			// set autocommit mode
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
		return 0;
    }
    return -1;
}

int dbd_disconnect(dbi_conn_t *conn) {
	ingres_conn_t *iconn = conn->connection;
	IIAPI_DISCONNPARM disconnParm = {{NULL, NULL}};
	IIAPI_AUTOPARM acParm = {{NULL, NULL}};
	IIAPI_STATUS status;
    
    if(iconn){
		if(iconn->sysConn)
			dbi_conn_close(iconn->sysConn);

		if(iconn->autoTranHandle){
			// exit autocommit mode
			acParm.ac_connHandle = iconn->connHandle;
			acParm.ac_tranHandle = iconn->autoTranHandle;
			IIapi_autocommit(&acParm);
			status = ingres_wait(conn, &acParm.ac_genParm);
			if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR)
				_verbose_handler(conn, "...exited autocommit\n");
			else
				_verbose_handler(conn, "...FAILED to exit autocommit\n");
		}

		disconnParm.dc_connHandle = iconn->connHandle;
		_verbose_handler(conn, "disconnecting...\n");
		IIapi_disconnect( &disconnParm );
		ingres_wait(conn, &disconnParm.dc_genParm);

		free(conn->connection);
		conn->connection = NULL;
	}
	return 0;
}

static int ingres_results(dbi_result_t *result){
	ingres_result_t *pres = result->result_handle;
	IIAPI_STATUS status;
	IIAPI_GETCOLPARM gcParm = {{NULL, NULL}};
	IIAPI_DESCRIPTOR *desc = pres->dataDesc;
	IIAPI_DATAVALUE *databuf;
	IIAPI_CONVERTPARM convParm;
	dbi_row_t *row;
	dbi_data_t *data;
	dbi_row_t **resized;
	int i, j, retval = 0, len;
	unsigned long count,limit;
	char *val;

	// no random access, we have to fetch row data sequentially using getColumns
	// therefore, grab all rows now.

	// set up a destination block for each field
	databuf = calloc(result->numfields, sizeof(IIAPI_DATAVALUE));
	if(!databuf) return 0;
	for(i = 0; i < result->numfields; ++i){
		databuf[i].dv_length = desc[i].ds_length;
		if(!(databuf[i].dv_value = malloc(desc[i].ds_length+1)))
			goto err;
	}

	gcParm.gc_stmtHandle = pres->stmtHandle;
	gcParm.gc_rowCount = 1;
	gcParm.gc_columnCount = result->numfields;
	gcParm.gc_columnData = databuf;
	gcParm.gc_rowsReturned = 0;
	gcParm.gc_moreSegments = 0;
	_verbose_handler(result->conn,"result columnCount=%d, stmtHandle=%#x\n", 
					 gcParm.gc_columnCount, gcParm.gc_stmtHandle);
    
	limit = result->numrows_matched;
	for(count = 0; ; ++count){
	
		if(count == limit){
			limit *= ROW_FACTOR;
			_verbose_handler(result->conn,"reallocating to new row limit %d\n",limit);
			if( (resized = realloc(result->rows, limit*sizeof(dbi_row_t*))) )
				result->rows = resized;
			else{
				_verbose_handler(result->conn,"can't expand row array; aborting\n");
				break;
			}
		}
	
		_verbose_handler(result->conn,"fetching row %d\n",count);
		IIapi_getColumns( &gcParm );
		status = ingres_wait(result->conn, &gcParm.gc_genParm);
		if ( status == IIAPI_ST_NO_DATA ){ // normal completion of fetch
			retval = 1;
			break; 
		}else if ( status > IIAPI_ST_NO_DATA ){
			_verbose_handler(result->conn,"getColumns returned status %d; aborting\n",status);
			break;
		}else if(gcParm.gc_moreSegments){
			_verbose_handler(result->conn,"long/blob columns not yet supported; aborting\n");
			break;
		}

		if((row = _dbd_row_allocate(result->numfields))){
			for(i = 0, data = row->field_values; i < result->numfields; ++i, ++data){
				row->field_sizes[i] = 0;
				if(databuf[i].dv_null){
					_set_field_flag(row, i, DBI_VALUE_NULL, 1);
					//_verbose_handler(result->conn,"  [%d] is NULL\n",i);
				}else
					switch(desc[i].ds_dataType){
					//case IIAPI_HNDL_TYPE: // can't do anything with this
					// 'national character sets' -- UTF-16 strings
					case IIAPI_NCHA_TYPE:
					case IIAPI_NVCH_TYPE:
					case IIAPI_LNVCH_TYPE:
					case IIAPI_DEC_TYPE:
					case IIAPI_MNY_TYPE:
					case IIAPI_DTE_TYPE:
						// convert to string first
						convParm.cv_srcDesc = desc[i];
						convParm.cv_srcValue = databuf[i];
						convParm.cv_dstDesc.ds_dataType = IIAPI_CHA_TYPE;
						convParm.cv_dstDesc.ds_nullable = FALSE;
						convParm.cv_dstDesc.ds_length = desc[i].ds_length + desc[i].ds_precision + 32; // include plenty of slop
						convParm.cv_dstDesc.ds_precision =
						convParm.cv_dstDesc.ds_scale = 0;
						convParm.cv_dstDesc.ds_columnType = IIAPI_COL_TUPLE;
						convParm.cv_dstValue.dv_length = convParm.cv_dstDesc.ds_length;
						convParm.cv_dstValue.dv_value = val = malloc(convParm.cv_dstValue.dv_length+1);
						IIapi_convertData(&convParm);
						len = convParm.cv_dstValue.dv_length;
						if(convParm.cv_status > IIAPI_ST_SUCCESS){
							_verbose_handler(result->conn,"could not convertData from column type %d\n",desc[i].ds_dataType);
							free(val);
							break; // should do something more drastic
						}else if(desc[i].ds_dataType == IIAPI_DTE_TYPE){
							val[len] = 0;
							data->d_datetime = ingres_date(val);
							//_verbose_handler(result->conn,"  [%d] date string %d bytes\n", i,len);
							free(val);
							break;
						}
						// strip trailing blanks from converted value,
						// since we had to overestimate the dst field width
						while(len && val[len-1] == ' ')
							--len;							
						val[len] = 0;
						data->d_string = val; // use converted data block
						row->field_sizes[i] = len;
						//_verbose_handler(result->conn,"  [%d] converted string %d bytes (desc %d bytes)\n",i,len,desc[i].ds_length);
						break;
					// the blob types aren't implemented by fetch yet (TODO)
					//case IIAPI_LBYTE_TYPE:
					//case IIAPI_LVCH_TYPE:
					//case IIAPI_LTXT_TYPE:
					// variable length (first 2 bytes define length)
					case IIAPI_VCH_TYPE:
					case IIAPI_VBYTE_TYPE:
					case IIAPI_TXT_TYPE:
						//for(j=0;j<databuf[i].dv_length;++j)
						//	_verbose_handler(result->conn,"%02x ",((unsigned char*)databuf[i].dv_value)[j]);
						//_verbose_handler(result->conn,"\n");
						// assume length (first 2 bytes of datum) is host native (short)
						row->field_sizes[i] = len = *(short*)databuf[i].dv_value;
						if((data->d_string = malloc(len+1))){
							memcpy(data->d_string, (char*)databuf[i].dv_value + 2, len);
							data->d_string[len] = 0; // NUL-terminate it, in case someone wants to pretend it's a string
						}
						//_verbose_handler(result->conn,"  [%d] variable size %d bytes (desc %d bytes)\n",i,len,desc[i].ds_length);
						break;
					// fixed string/binary types
					case IIAPI_BYTE_TYPE:
					case IIAPI_CHR_TYPE:
					case IIAPI_CHA_TYPE:
					case IIAPI_LOGKEY_TYPE:
					case IIAPI_TABKEY_TYPE:
						row->field_sizes[i] = len = databuf[i].dv_length;
						data->d_string = databuf[i].dv_value; // just copy pointer to the fetched block
						databuf[i].dv_value = malloc(desc[i].ds_length+1); // replace block for future rows
						data->d_string[len] = 0; // NUL-terminate the string
						//_verbose_handler(result->conn,"  [%d] fixed size %d bytes (desc %d bytes)\n",i,databuf[i].dv_length,desc[i].ds_length);
						break;
					// these are returned in native format, all sizes
					case IIAPI_INT_TYPE:
					case IIAPI_FLT_TYPE:
						// getColumns already copied data
						//_verbose_handler(result->conn,"  [%d] copying %d bytes\n",i,databuf[i].dv_length);
						memcpy(&row->field_values[i], databuf[i].dv_value, databuf[i].dv_length);
						break;
					default:
						_verbose_handler(result->conn,"can't handle column type = %d\n",desc[i].ds_dataType);
					}
			}
			_dbd_row_finalize(result, row, count);
		}else{
			_verbose_handler(result->conn,"failed to allocate row; aborting\n");
			break;
		}
	}
	
	// Don't need this handle any more.
	ingres_close(pres->stmtHandle);
	pres->stmtHandle = NULL;

	result->numrows_matched = count;
	// resize row array to match actual count (FIXME: work out why this crashes)
	//if( (resized = realloc(result->rows, count*sizeof(dbi_row_t*))) )
	//	result->rows = resized;

err: // clean up allocations
	for(i = 0; i < result->numfields; ++i)
		if(databuf[i].dv_value)
			free(databuf[i].dv_value);
	free(databuf);
	
	return retval;
}

int dbd_fetch_row(dbi_result_t *result, unsigned long long rowidx) {
	return result->result_state == ROWS_RETURNED; /* 0 on error, 1 on successful fetchrow */
}

int dbd_free_query(dbi_result_t *result) {
	if(result->result_handle){
		ingres_close(((ingres_result_t*)result->result_handle)->stmtHandle);
		free(result->result_handle);
		result->result_handle = NULL;
	}
	return 0;
}

int dbd_goto_row(dbi_result_t *result, unsigned long long rowidx) {
	return 1;
}

int dbd_get_socket(dbi_conn_t *conn){
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

static int ingres_commit(dbi_conn_t *conn, II_PTR tranHandle){
	IIAPI_COMMITPARM cmParm = {{NULL, NULL}};
	
    cmParm.cm_tranHandle = tranHandle;
	IIapi_commit(&cmParm);
    return ingres_wait(conn, &cmParm.cm_genParm) == IIAPI_ST_SUCCESS;
}

static int ingres_rollback(dbi_conn_t *conn, II_PTR tranHandle){
	IIAPI_ROLLBACKPARM rbParm = {{NULL, NULL}};
	
    rbParm.rb_tranHandle = tranHandle;
    rbParm.rb_savePointHandle = NULL;
	IIapi_rollback(&rbParm);
    return ingres_wait(conn, &rbParm.rb_genParm) == IIAPI_ST_SUCCESS;
}

static dbi_result_t *ingres_sys_query(dbi_conn_t *conn, const char *sql) {
	II_PTR tranHandle;
	dbi_result_t *res = NULL;
	ingres_conn_t *iconn = conn->connection;
	
	if(!iconn->sysConn){
		// note that we need to keep the system connection around,
		// because closing it prematurely will kill result sets
		// FIXME: could use _disjoin_from_conn()
		iconn->sysConn = dbi_conn_new(driver_info.name);
		if(ingres_connect(iconn->sysConn, SYS_CATALOGS, NO_AUTOCOMMIT) < 0){
			_verbose_handler(conn,"can't connect to '%s'\n",SYS_CATALOGS);
			return NULL;
		}
	}
	res = ingres_query(iconn->sysConn, sql, &tranHandle);
	_verbose_handler(conn,"ingres_sys_query: tranHandle=%#x\n",tranHandle);
	if(!res) _verbose_handler(conn,"no result for '%s'??\n",sql);
	ingres_rollback(iconn->sysConn, tranHandle);
	return res;
}

char *dbd_get_engine_version(dbi_conn_t *conn, char *versionstring) {
	char *str = "0";
	dbi_result_t *res = ingres_query(conn, "SELECT dbmsinfo('_version')", NULL);
	if(res && dbi_result_next_row(res)){
		strncpy(versionstring, dbi_result_get_string_idx(res,1), VERSIONSTRING_LENGTH);
		versionstring[VERSIONSTRING_LENGTH-1] = 0; // make sure of NUL-termination
		str = versionstring;
	}
	if(res) dbi_result_free(res);
	return str;
}

dbi_result_t *dbd_list_dbs(dbi_conn_t *conn, const char *pattern) {
	static char *select = "SELECT database_name FROM iidatabase_info";
	char *sql = select;
	dbi_result_t *res = NULL;
	if(!pattern || asprintf(&sql, "%s WHERE database_name LIKE '%s'",select,pattern) > 0){
		res = ingres_sys_query(conn, sql);
		if(pattern) free(sql);
	}
	return res;
}

dbi_result_t *dbd_list_tables(dbi_conn_t *conn, const char *db, const char *pattern) {
	static char *select = "SELECT table_name FROM iitables WHERE table_name NOT LIKE 'ii%'";
	char *sql = select;
	dbi_result_t *res = NULL;
	II_PTR tranHandle = NULL;
	dbi_conn_t *newconn = dbi_conn_new(driver_info.name);
	
	if(ingres_connect(newconn, db, NO_AUTOCOMMIT) < 0)
		_verbose_handler(conn,"dbd_list_tables: can't connect to '%s'\n",db);
	else{
		if(!pattern || asprintf(&sql, "%s AND table_name LIKE '%s'",select,pattern) > 0){
			res = ingres_query(newconn, sql, &tranHandle);
			if(pattern) free(sql);
		}
		if(res) // remove result set from connection's list
			;// dbi_result_disjoin(res); FIXME: this crashes us later for no discernible reason
		else
			_verbose_handler(conn,"dbd_list_tables: no result for '%s'??\n",sql);
		ingres_rollback(newconn, tranHandle);
		//dbi_conn_close(newconn); // FIXME: until disjoin works, don't close; leave connection around (bad)
	}
	return res;
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

// note transaction handle is returned in tranHandle parameter, if not NULL
// this is only really of use for non-Autocommit connections (such as system catalog access).
static dbi_result_t *ingres_query(dbi_conn_t *conn, const char *statement, II_PTR *tranHandle) {
	ingres_conn_t *iconn = conn->connection;
	dbi_result_t *res = NULL;
	ingres_result_t *pres = NULL;
	int i, matchedRows, affectedRows;
	IIAPI_QUERYPARM	queryParm = {{NULL, NULL}};
	IIAPI_GETQINFOPARM gqParm = {{NULL, NULL}};
	IIAPI_GETDESCRPARM gdParm = {{NULL, NULL}};
	IIAPI_DESCRIPTOR *pdesc;
	IIAPI_STATUS status;

	if(!iconn || !iconn->connHandle){
		_verbose_handler(conn,"whoa, query attempted without a connection\n");
		return NULL;
	}

	queryParm.qy_connHandle = iconn->connHandle;
	queryParm.qy_queryType = IIAPI_QT_QUERY;
	queryParm.qy_queryText = statement;
	queryParm.qy_parameters = FALSE;
	queryParm.qy_tranHandle = iconn->autoTranHandle; // will be NULL if we're not using autocommit
	queryParm.qy_stmtHandle = NULL;
	IIapi_query( &queryParm );
	status = ingres_wait(conn, &queryParm.qy_genParm);
	if(tranHandle) *tranHandle = queryParm.qy_tranHandle;

	if(status == IIAPI_ST_NO_DATA)
		_verbose_handler(conn,"(IIAPI_ST_NO_DATA: expected data, but none returned)\n");

	if (status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_NO_DATA){

		gdParm.gd_stmtHandle = queryParm.qy_stmtHandle;
		IIapi_getDescriptor(&gdParm);
    	status = ingres_wait(conn, &gdParm.gd_genParm);
		if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR){
			// if descriptorCount is zero, no data can be expected
			// create result struct anyway

			if(!gdParm.gd_descriptorCount){
				// fetch the number of affected rows
				gqParm.gq_stmtHandle = queryParm.qy_stmtHandle;
				IIapi_getQueryInfo(&gqParm);
				status = ingres_wait(conn, &gqParm.gq_genParm);

				if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR && (gqParm.gq_mask & IIAPI_GQ_ROW_COUNT)){
					affectedRows = gqParm.gq_rowCount;
					_verbose_handler(conn,"getQueryInfo: row count = %d\n",affectedRows);
				}else
					affectedRows = 0;
				
				res = _dbd_result_create(conn, NULL, 0, affectedRows);
				_verbose_handler(conn,"no descriptors\n");
				ingres_close(queryParm.qy_stmtHandle);
			}else{
				_verbose_handler(conn,"new result set, stmtHandle = %#x\n",queryParm.qy_stmtHandle);
				pres = malloc(sizeof(ingres_result_t));
				pres->stmtHandle = queryParm.qy_stmtHandle;
				pres->dataDesc = gdParm.gd_descriptor;
				
				res = _dbd_result_create(conn, pres, INITIAL_ROWS, 0);
				_dbd_result_set_numfields(res, gdParm.gd_descriptorCount);
				
				// fetch column information
				for(i = 0, pdesc = gdParm.gd_descriptor; i < gdParm.gd_descriptorCount; ++i, ++pdesc){
					unsigned short type;
					unsigned int attribs;
					ingres_classify_field(&gdParm.gd_descriptor[i], &type, &attribs);
					_verbose_handler(conn,"field %d: '%s' dataType=%d nullable=%d"
						" length=%d precision=%d scale=%d dbiType=%d dbiAttr=%#x\n",
						i,pdesc->ds_columnName,pdesc->ds_dataType,pdesc->ds_nullable,
						pdesc->ds_length,pdesc->ds_precision,pdesc->ds_scale,type,attribs);
					_dbd_result_add_field(res, i, pdesc->ds_columnName, type, attribs);
				}
				
				// we have no choice but to fetch all result data now, because
				// that is the only way we can report the correct row count
				// as a property of the result set.
				ingres_results(res);
			}
		}
	}
	// must have been an error.
	if(!res)
		ingres_close(queryParm.qy_stmtHandle);
	return res;
}

dbi_result_t *dbd_query(dbi_conn_t *conn, const char *statement) {
	return ingres_query(conn, statement, NULL);
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
	
	if (ingres_connect(conn, db, dbi_conn_get_option(conn, "ingres_autocommit")))
		return NULL;
	
	return db;
}

int dbd_geterror(dbi_conn_t *conn, int *errno, char **errstr) {
	/* put error number into errno, error string into errstr
	 * return 0 if error, 1 if errno filled, 2 if errstr filled, 3 if both errno and errstr filled */
	// for now, just assume errstr was set up by a ingres_error call
	// FIXME: we really need to be able to stash an errorhandle instead (after each API call), and use it here
	return 2;
}

unsigned long long dbd_get_seq_last(dbi_conn_t *conn, const char *sequence) {
	// This function cannot be used in autocommit mode.

	// in particular, it cannot tell you anything about the sequence value used
	// in a previous INSERT(NEXT VALUE FOR ...) statement, because other transactions
	// may have used the same sequence since then.
	// Ingres won't allow this function in an unrelated transaction.

	// To use sequences to identify INSERTed rows, first obtain the NEXT VALUE
	// using the dbi function (which will be an individual transaction), 
	// then use that sequence value directly in a separate INSERT transaction.

	long seq = 0;
	char *sql;
	dbi_result_t *res;

	asprintf(&sql, "SELECT CURRENT VALUE FOR %s", sequence);
	res = ingres_query(conn, sql, NULL);
	if(res && dbi_result_next_row(res))
		seq = dbi_result_get_int_idx(res,1);
	else if(AUTOCOMMIT_ON(conn))
		_verbose_handler(conn,"dbi_conn_sequence_last() can't work in autocommit mode."
						 " See libdbd-ingres driver docs for a workaround.\n");
	if(res) dbi_result_free(res);
	return seq;
	
	return 0;
}

unsigned long long dbd_get_seq_next(dbi_conn_t *conn, const char *sequence) {
	long seq = 0;
	char *sql;
	dbi_result_t *res;
	II_PTR tran;

	asprintf(&sql, "SELECT NEXT VALUE FOR %s", sequence);
	res = ingres_query(conn, sql, &tran);
	if(res && dbi_result_next_row(res))
		seq = dbi_result_get_int_idx(res,1);
	if(res) dbi_result_free(res);
	//if(!AUTOCOMMIT_ON(conn))  // you probably don't want this hack
	//	ingres_commit(conn, tran);
	return seq;
}

int dbd_ping(dbi_conn_t *conn) {
	long test = 0;
	dbi_result_t *res;

	res = ingres_query(conn, "SELECT 1", NULL);
	if(res && dbi_result_next_row(res))
		test = dbi_result_get_int_idx(res,1);
	if(res) dbi_result_free(res);
	return test;
}

/* CORE DATA FETCHING STUFF */

void ingres_classify_field(IIAPI_DESCRIPTOR *ds, unsigned short *type, unsigned int *attribs) {
	*type = 0;
	*attribs = 0;

	switch(ds->ds_dataType){
	//IIAPI_HNDL_TYPE
	// integer
	case IIAPI_INT_TYPE:
		*type = DBI_TYPE_INTEGER; // always signed
		switch(ds->ds_length){
		case 1: *attribs = DBI_INTEGER_SIZE1; break;
		case 2: *attribs = DBI_INTEGER_SIZE2; break;
		case 4: *attribs = DBI_INTEGER_SIZE4; break;
		case 8: *attribs = DBI_INTEGER_SIZE8; break;
		}
		break;
	// float
	case IIAPI_FLT_TYPE:
		*type = DBI_TYPE_DECIMAL;
		switch(ds->ds_length){
		case 4: *attribs = DBI_DECIMAL_SIZE4; break;
		case 8: *attribs = DBI_DECIMAL_SIZE8; break;
		}
		break;
	case IIAPI_LOGKEY_TYPE:
	case IIAPI_TABKEY_TYPE:
	// variable length binary string
	case IIAPI_LBYTE_TYPE:
	case IIAPI_VBYTE_TYPE:
	// fixed length binary string
	case IIAPI_BYTE_TYPE:
		*type = DBI_TYPE_BINARY;
		break;
	// variable length char string; segment length is first two bytes of datum
	case IIAPI_VCH_TYPE:
	case IIAPI_LVCH_TYPE:
	case IIAPI_TXT_TYPE: // deprecated
	// fixed length char string
	case IIAPI_CHR_TYPE: // deprecated
	case IIAPI_CHA_TYPE:
	case IIAPI_LTXT_TYPE:
	// fixed length 'national character set' UTF-16 (wide) char string (TODO)
	case IIAPI_NCHA_TYPE:
	// variable length UTF-16 string; first 2 bytes are length
	case IIAPI_NVCH_TYPE:
	case IIAPI_LNVCH_TYPE:
	// Ingres-native types, must be converted
	case IIAPI_DEC_TYPE: // packed decimal; use convertData or formatData
	case IIAPI_MNY_TYPE: // Ingres money; use convertData or formatData
		*type = DBI_TYPE_STRING; //TODO
		break;
	case IIAPI_DTE_TYPE: // Ingres date; use convertData or formatData
		*type = DBI_TYPE_DATETIME;
		break;
	}
}
