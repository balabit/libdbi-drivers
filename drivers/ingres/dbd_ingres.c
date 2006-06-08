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

#define AUTOCOMMIT_ON(C) (((ingres_conn_t*)C->connection)->autocommit)

#define SAVE_ERROR(C,E) ingres_error(E, dbi_verbosity>1, &((ingres_conn_t*)C->connection)->errorCode, \
									 &((ingres_conn_t*)C->connection)->errorMsg)
#define DEBUG_ERROR(E) ingres_error(E, dbi_verbosity>2, NULL, NULL)
#define DRIVER_ERROR(C,E) _dbd_internal_error_handler(C,E,0)

#define PRINT_VERBOSE if(dbi_verbosity>1) _verbose_handler
#define PRINT_DEBUG   if(dbi_verbosity>2) _verbose_handler

#define IS_BLOB(T) ( (T) == IIAPI_LVCH_TYPE  || (T) == IIAPI_LBYTE_TYPE \
				  || (T) == IIAPI_LNVCH_TYPE || (T) == IIAPI_LTXT_TYPE )

typedef struct {
	II_PTR connHandle;
	II_PTR currTran;
	II_LONG sizeAdvise; // advised buffer size for blob (long) types
	dbi_conn_t *sysConn; // used for querying system catalogs
	II_LONG errorCode;
	char *errorMsg;
	int autocommit;
} ingres_conn_t;

typedef struct {
	II_PTR stmtHandle; // statement handle returned by associated query
	IIAPI_DESCRIPTOR *dataDesc; // filled by getDescriptor after query
} ingres_result_t;

/* forward declarations of internal functions */
static dbi_result_t *ingres_query(dbi_conn_t *conn, const char *statement);
static void ingres_classify_field(IIAPI_DESCRIPTOR *ds, unsigned short *type, unsigned int *attribs);
static int ingres_connect(dbi_conn_t *conn, const char *db, const char *autocommit);
void ingres_error(II_PTR errorHandle, int print, int *errno, char **errmsg);

// months.gperf.c
struct month *in_word_set (register const char *str, register unsigned int len);

static IIAPI_STATUS ingres_wait(dbi_conn_t *conn, IIAPI_GENPARM *pgp){
	IIAPI_WAITPARM waitParm;

	for( waitParm.wt_timeout = -1 ; ! pgp->gp_completed ; )
		IIapi_wait(&waitParm);
	return pgp->gp_status;
}

static void ingres_close(dbi_conn_t *conn, II_PTR hdl) {
	IIAPI_STATUS status;
	IIAPI_CLOSEPARM closeParm = {{NULL, NULL}};

	if(hdl){
		closeParm.cl_stmtHandle = hdl;
		PRINT_DEBUG(conn, "closing stmtHandle %#x...\n", hdl);
		IIapi_close(&closeParm);
		ingres_wait(conn, &closeParm.cl_genParm);
		DEBUG_ERROR(closeParm.cl_genParm.gp_errorHandle);
	}
}

static time_t ingres_date(char *raw){
	struct tm unixtime;
	char *p = raw, *q, sep;

	//PRINT_VERBOSE(NULL,"date: '%s'\n",raw);
	
	unixtime.tm_sec = unixtime.tm_min = unixtime.tm_hour = 0;
	unixtime.tm_isdst = -1;
	
	// parse Ingres default (US) date format: dd-mmm-yyyy [hh:mm:ss]
	//for(p = raw; *p && !isdigit(*p); ++p)
	//	;
	if(isdigit(*p)){
		// process day
		unixtime.tm_mday = atoi(p);
		while(*p && isdigit(*p))
			++p;
		if(!*p){ _verbose_handler(NULL,"date ended after day??",raw); return 0; }
		sep = *p++; // skip separator

		// process month
		if(isdigit(*p)){
			unixtime.tm_mon = atoi(p)-1; /* months are 0 through 11 */
			while(*p && *p != sep)
				++p;
		}else{
			q = p; // point to start of month name
			while(*p && *p != sep)
				++p;
			if(*p){
				*p = 0;
				unixtime.tm_mon = in_word_set(q,3)->index; // should work for long month names too
				++p;
			}
		}
		if(!*p){ _verbose_handler(NULL,"date ended after month??",raw); return 0; }
		
		// process year
		unixtime.tm_year = atoi(p)-1900;

		PRINT_DEBUG(NULL,"ingres_date: parsed date day=%d mon=%d yr=%d\n", unixtime.tm_mday, unixtime.tm_mon, unixtime.tm_year);

		while(isdigit(*p))
			++p;
		// skip space following date
		while(*p == ' ')
			++p;

		// Ingres does not generate a time by itself, it's always preceded by a date.
		if(isdigit(*p)){ // time is present
			// process hours
			unixtime.tm_hour = atoi(p);
			while(isdigit(*p))
				++p;
			if(!*p){ _verbose_handler(NULL,"time ended after hour??",raw); return 0; }
			++p; // skip separator

			// process minutes
			unixtime.tm_min = atoi(p);
			while(isdigit(*p))
				++p;
			if(!*p){ _verbose_handler(NULL,"time ended after minute??",raw); return 0; }
			++p; // skip separator

			// process seconds
			unixtime.tm_sec = atoi(p); 
			
			PRINT_DEBUG(NULL,"ingres_date: parsed time %02d:%02d:%02d\n", unixtime.tm_hour, unixtime.tm_min, unixtime.tm_sec);
	
			/* check for a timezone suffix */
			//while(isdigit(*p) || *p == ' ')
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
	PRINT_DEBUG(NULL, "initialize: envHandle=%#x\n",envHandle);
	return (initParm.in_status >= IIAPI_ST_SUCCESS 
			&& initParm.in_status < IIAPI_ST_ERROR) - 1;
}

int dbd_connect(dbi_conn_t *conn) {
	return ingres_connect(conn, NULL, dbi_conn_get_option(conn, "ingres_autocommit"));
}

void ingres_error(II_PTR errorHandle, int print, int *errno, char **errmsg){
	static char *typestr[] = {"0","ERROR","WARNING","MESSAGE"};
	char *p, buf[0x200];
	int count, n, size=0x200;
	IIAPI_GETEINFOPARM eiParm;

	if(errmsg){
		if(*errmsg) free(*errmsg);
		*errmsg = malloc(size);
	}
	if( (eiParm.ge_errorHandle = errorHandle) ){
		for(count = 0;;){
			IIapi_getErrorInfo(&eiParm);
			if(eiParm.ge_status == IIAPI_ST_SUCCESS){
				n = snprintf(buf, sizeof(buf), "%s SQLSTATE:%s Code:%06X  %s\n",
							 typestr[eiParm.ge_type], eiParm.ge_SQLSTATE, 
							 eiParm.ge_errorCode, eiParm.ge_message);
				/*if(print)*/ PRINT_VERBOSE(NULL, buf);
				if(errno) *errno = eiParm.ge_errorCode; // store last error
				if(errmsg){
					if(count+n >= size){
						size += n + 0x200;
						if( (p = realloc(*errmsg, size)) )
							*errmsg = p;
					}else
						p = *errmsg;
					if(p){
						memcpy(p+count, buf, n);
						p[n] = 0;
						count += n;
					}
				}
			}else
				break;
		}
	}else{
		if(errmsg) *errmsg = NULL;
		if(errno) *errno = 0;
	}
}

static int ingres_option_num(dbi_conn_t *conn, IIAPI_SETCONPRMPARM *psc, II_LONG id, char *name){
	const char *opt = dbi_conn_get_option(conn, name);
	IIAPI_STATUS status;
	
	if(opt){
		II_LONG val = atoi(opt);
		psc->sc_paramID = id;
		psc->sc_paramValue = &val; // works for Ingres 'long' and 'bool' types too, which are all int.
		IIapi_setConnectParam(psc);
		status = ingres_wait(conn, &psc->sc_genParm);
		DEBUG_ERROR(psc->sc_genParm.gp_errorHandle);
		return status == IIAPI_ST_SUCCESS;
	}
	return 1;
}

static int ingres_option_str(dbi_conn_t *conn, IIAPI_SETCONPRMPARM *psc, II_LONG id, char *name){
	const char *opt = dbi_conn_get_option(conn, name);
	IIAPI_STATUS status;

	if(opt){
		psc->sc_paramID = id;
		psc->sc_paramValue = opt; // works for Ingres 'char' options too, of course
		IIapi_setConnectParam(psc);
		status = ingres_wait(conn, &psc->sc_genParm);
		DEBUG_ERROR(psc->sc_genParm.gp_errorHandle);
		return status == IIAPI_ST_SUCCESS;
	}
	return 1;
}

static int ingres_connect(dbi_conn_t *conn, const char *db, const char *autocommit) {
	ingres_conn_t *iconn;
	IIAPI_SETCONPRMPARM scParm = {{NULL, NULL}};
	IIAPI_CONNPARM connParm = {{NULL, NULL}};
	IIAPI_AUTOPARM acParm = {{NULL, NULL}};
	IIAPI_STATUS status;

	// we need this structure whether connection succeeds or not,
	// at least to track the error handle.
	conn->connection = iconn = malloc(sizeof(ingres_conn_t));
	iconn->connHandle = NULL;
	iconn->currTran = NULL;
	iconn->sysConn = NULL;
	iconn->errorCode = 0;
	iconn->errorMsg = NULL;
		
	scParm.sc_connHandle = NULL; // later, envHandle, but currently that causes connect to fail (?!)
    
	PRINT_DEBUG(NULL, "ingres_connect: envHandle=%#x\n",envHandle);

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
	SAVE_ERROR(conn, connParm.co_genParm.gp_errorHandle);
	if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR){
		PRINT_VERBOSE(conn, "connected to '%s', API level=%d, BLOB sizeAdvise=%d\n",
					  connParm.co_target, connParm.co_apiLevel, connParm.co_sizeAdvise);
		iconn->connHandle = connParm.co_connHandle;
		iconn->sizeAdvise = connParm.co_sizeAdvise;
		if((iconn->autocommit = !autocommit || atoi(autocommit))){ // enable auto-commit by default
			// set autocommit mode
			acParm.ac_connHandle = connParm.co_connHandle;
			acParm.ac_tranHandle = NULL;
			IIapi_autocommit(&acParm);
			status = ingres_wait(conn, &acParm.ac_genParm);
			DEBUG_ERROR(acParm.ac_genParm.gp_errorHandle);
			if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR){
				// stash the autocommit transaction handle
				iconn->currTran = acParm.ac_tranHandle;
				PRINT_VERBOSE(conn, "...enabled autocommit, tranHandle=%#x\n", acParm.ac_tranHandle);
			}else
				PRINT_VERBOSE(conn, "...FAILED to enable autocommit\n");
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

		if(iconn->autocommit){
			// exit autocommit mode
			acParm.ac_connHandle = iconn->connHandle;
			acParm.ac_tranHandle = iconn->currTran;
			IIapi_autocommit(&acParm);
			status = ingres_wait(conn, &acParm.ac_genParm);
			DEBUG_ERROR(acParm.ac_genParm.gp_errorHandle);
			if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR)
				PRINT_VERBOSE(conn, "...exited autocommit\n");
			else
				PRINT_VERBOSE(conn, "...FAILED to exit autocommit\n");
		}

		disconnParm.dc_connHandle = iconn->connHandle;
		PRINT_VERBOSE(conn, "disconnecting...\n");
		IIapi_disconnect( &disconnParm );
		ingres_wait(conn, &disconnParm.dc_genParm);
		DEBUG_ERROR(disconnParm.dc_genParm.gp_errorHandle);
		free(conn->connection);
		conn->connection = NULL;
	}
	return 0;
}

static int ingres_field(dbi_result_t *result, dbi_row_t *row, dbi_data_t *data, 
						 int idx, IIAPI_DESCRIPTOR *pdesc, IIAPI_DATAVALUE *pdataval)
{
	IIAPI_CONVERTPARM convParm;
	int j, len;
	unsigned long count,limit;
	char *val;
	
	switch(pdesc->ds_dataType){
	//case IIAPI_HNDL_TYPE: // can't do anything with this
	// 'national character sets' -- UTF-16 strings
	case IIAPI_NCHA_TYPE:
	case IIAPI_NVCH_TYPE:
	case IIAPI_LNVCH_TYPE:
	case IIAPI_DEC_TYPE:
	case IIAPI_MNY_TYPE:
	case IIAPI_DTE_TYPE:
		// convert to string first
		convParm.cv_srcDesc = *pdesc;
		convParm.cv_srcValue = *pdataval;
		convParm.cv_dstDesc.ds_dataType = IIAPI_CHA_TYPE;
		convParm.cv_dstDesc.ds_nullable = FALSE;
		convParm.cv_dstDesc.ds_length = pdesc->ds_length + pdesc->ds_precision + 32; // include plenty of slop
		convParm.cv_dstDesc.ds_precision =
		convParm.cv_dstDesc.ds_scale = 0;
		convParm.cv_dstDesc.ds_columnType = IIAPI_COL_TUPLE;
		convParm.cv_dstValue.dv_length = convParm.cv_dstDesc.ds_length;
		convParm.cv_dstValue.dv_value = val = malloc(convParm.cv_dstValue.dv_length+1);
		IIapi_convertData(&convParm);
		len = convParm.cv_dstValue.dv_length;
		if(convParm.cv_status > IIAPI_ST_SUCCESS){
			_verbose_handler(result->conn,"could not convertData from column type %d\n",pdesc->ds_dataType);
			return 0;
		}else if(pdesc->ds_dataType == IIAPI_DTE_TYPE){
			val[len] = 0;
			data->d_datetime = ingres_date(val);
			PRINT_DEBUG(result->conn,"  [%d] date string %d bytes\n", idx,len);
			break;
		}
		// strip leading and trailing blanks from converted value
		while(len && val[len-1] == ' ')
			--len;
		for(j = 0; j < len && val[j] == ' '; ++j)
			;
		len -= j;
		row->field_sizes[idx] = len;
		data->d_string = malloc(len+1); // use converted data block
		memcpy(data->d_string, val+j, len);
		data->d_string[len] = 0;
		free(val);
		PRINT_DEBUG(result->conn,"  [%d] converted string %d bytes (desc %d bytes)\n",idx,len,pdesc->ds_length);
		break;
	//case IIAPI_LBYTE_TYPE: // these are handled in ingres_results()
	//case IIAPI_LVCH_TYPE:
	//case IIAPI_LTXT_TYPE:
	// variable length (first 2 bytes define length)
	case IIAPI_VCH_TYPE:
	case IIAPI_VBYTE_TYPE:
	case IIAPI_TXT_TYPE:
		//for(j=0;j<pdataval->dv_length;++j)
		//	_verbose_handler(result->conn,"%02x ",((unsigned char*)pdataval->dv_value)[j]);
		//_verbose_handler(result->conn,"\n");
		// assume length (first 2 bytes of datum) is host native (short)
		row->field_sizes[idx] = len = *(unsigned short*)pdataval->dv_value;
		if((data->d_string = malloc(len+1))){
			memcpy(data->d_string, (char*)pdataval->dv_value + 2, len);
			data->d_string[len] = 0; // NUL-terminate it, in case someone wants to pretend it's a string
		}
		PRINT_DEBUG(result->conn,"  [%d] variable size %d bytes (desc %d bytes)\n",idx,len,pdesc->ds_length);
		break;
	// fixed string/binary types
	case IIAPI_BYTE_TYPE:
	case IIAPI_CHR_TYPE:
	case IIAPI_CHA_TYPE:
	case IIAPI_LOGKEY_TYPE:
	case IIAPI_TABKEY_TYPE:
		row->field_sizes[idx] = len = pdataval->dv_length;
		data->d_string = pdataval->dv_value; // just copy pointer to the fetched block
		pdataval->dv_value = malloc(pdesc->ds_length+1); // replace block for future rows
		data->d_string[len] = 0; // NUL-terminate the string
		PRINT_DEBUG(result->conn,"  [%d] fixed size %d bytes (desc %d bytes)\n",idx,pdataval->dv_length,pdesc->ds_length);
		break;
	// these are returned in native format, all sizes
	case IIAPI_INT_TYPE:
	case IIAPI_FLT_TYPE:
		// getColumns already copied data
		PRINT_DEBUG(result->conn,"  [%d] copying %d bytes\n",idx,pdataval->dv_length);
		memcpy(data, pdataval->dv_value, 8); // constant permits compile-time optimisation
		break;
	default:
		_verbose_handler(result->conn,"ingres_field(): can't handle column type = %d\n",pdesc->ds_dataType);
		return 0;
	}
	return 1;
}

static int ingres_results(dbi_result_t *result){
	ingres_result_t *pres = result->result_handle;
	IIAPI_STATUS status;
	IIAPI_GETCOLPARM gcParm = {{NULL, NULL}};
	IIAPI_DESCRIPTOR *desc = pres->dataDesc;
	IIAPI_DATAVALUE *databuf;
	dbi_row_t *row;
	dbi_row_t **resized;
	int fieldidx, lastidx, i, j, len, isblob, cols, retval = 0;
	unsigned long count,limit,bloblen,blobmax;
	char *p, *blob;

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
    
	limit = result->numrows_matched;
	for(count = 0; ; ++count){
	
		if(count == limit){
			limit *= ROW_FACTOR;
			PRINT_DEBUG(result->conn,"reallocating to new row limit %d\n",limit);
			if( (resized = realloc(result->rows, limit*sizeof(dbi_row_t*))) )
				result->rows = resized;
			else{
				DRIVER_ERROR(result->conn,"can't expand row array; aborting\n");
				break;
			}
		}
	
		PRINT_VERBOSE(result->conn,"fetching row %d\n",count);

		if((row = _dbd_row_allocate(result->numfields))){

			for(fieldidx = 0; fieldidx < result->numfields; ){
				// skip over non-BLOB fields
				for(lastidx = fieldidx; lastidx < result->numfields 
										&& !IS_BLOB(desc[lastidx].ds_dataType); ++lastidx)
					;

				isblob = IS_BLOB(desc[fieldidx].ds_dataType);
				if(isblob) ++lastidx;
				gcParm.gc_columnCount = cols = lastidx - fieldidx;
				gcParm.gc_columnData = &databuf[fieldidx];
				
				if(isblob){
					bloblen = 0;
					blob = malloc(blobmax = desc[fieldidx].ds_length);
				}
				do{
					IIapi_getColumns( &gcParm );
					status = ingres_wait(result->conn, &gcParm.gc_genParm);
					if ( status == IIAPI_ST_NO_DATA ){ // normal completion of fetch
						retval = 1;
						goto rowsdone; 
					}else if ( status > IIAPI_ST_NO_DATA ){
						SAVE_ERROR(result->conn, gcParm.gc_genParm.gp_errorHandle);
						_verbose_handler(result->conn,"getColumns returned status %d; aborting\n",status);
						goto rowsdone; 
					}
					PRINT_DEBUG(result->conn,"getColumns fieldidx=%d columnCount=%d isblob=%d more=%d\n",
								fieldidx,gcParm.gc_columnCount,isblob,gcParm.gc_moreSegments);
					if(isblob){
						// append returned segment to blob
						len = *(unsigned short*)databuf[fieldidx].dv_value;
						if(bloblen + len > blobmax){
							// the idea here is to reduce the number of reallocs (typically memcopies).
							// means, e.g., a 1MB BLOB can be fetched in 9 doublings (reallocs)
							// otherwise, if we just added a segment size each time, we would realloc over 500 times!
							blobmax *= 2;
							if(p = realloc(blob, blobmax)){
								blob = p;
								PRINT_DEBUG(result->conn, "resized blob to %d bytes\n", blobmax);
							}else{ // returned data for BLOB column will be short!
								DRIVER_ERROR(result->conn, "could not resize blob; aborting\n");
								goto rowsdone;
							}
						}
						memcpy(blob + bloblen, (char*)databuf[fieldidx].dv_value + 2, len);
						bloblen += len;
						PRINT_DEBUG(result->conn, "  [%d] BLOB segment %d bytes\n", fieldidx, len);
					}
				}while(gcParm.gc_moreSegments);
				
				// process the column(s) we got
				for(i = fieldidx; i < lastidx; ++i){
					row->field_sizes[i] = 0;
					if(databuf[i].dv_null){
						_set_field_flag(row, i, DBI_VALUE_NULL, 1);
						PRINT_DEBUG(result->conn,"  [%d] is NULL\n",i);
					}else if(isblob){
						if(p = realloc(blob, bloblen)) // shrink block to actual length
							blob = p;
						row->field_values[fieldidx].d_string = blob;
						row->field_sizes[fieldidx] = bloblen;
						PRINT_DEBUG(result->conn,"  [%d] is BLOB of %d bytes\n",i,bloblen);
					}else
						ingres_field(result, row, &row->field_values[i], i, &desc[i], &databuf[i]);
				}
						
				fieldidx += cols;
			}

			_dbd_row_finalize(result, row, count);
		}else{
			DRIVER_ERROR(result->conn, "failed to allocate row; aborting\n");
			break;
		}
	}
rowsdone:
	// Don't need this handle any more.
	ingres_close(result->conn, pres->stmtHandle);
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
		ingres_close(result->conn, ((ingres_result_t*)result->result_handle)->stmtHandle);
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
	IIAPI_STATUS status;
	
	PRINT_DEBUG(conn, "COMMIT tranHandle=%#x\n", tranHandle);
    cmParm.cm_tranHandle = tranHandle;
	IIapi_commit(&cmParm);
    status = ingres_wait(conn, &cmParm.cm_genParm);
	DEBUG_ERROR(cmParm.cm_genParm.gp_errorHandle);
    return status == IIAPI_ST_SUCCESS;
}

static int ingres_rollback(dbi_conn_t *conn, II_PTR tranHandle){
	IIAPI_ROLLBACKPARM rbParm = {{NULL, NULL}};
	IIAPI_STATUS status;
	
	PRINT_DEBUG(conn, "ROLLBACK tranHandle=%#x\n", tranHandle);
    rbParm.rb_tranHandle = tranHandle;
    rbParm.rb_savePointHandle = NULL;
	IIapi_rollback(&rbParm);
    status = ingres_wait(conn, &rbParm.rb_genParm);
	DEBUG_ERROR(rbParm.rb_genParm.gp_errorHandle);
    return status == IIAPI_ST_SUCCESS;
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
	res = ingres_query(iconn->sysConn, sql);
	if(!res) _verbose_handler(conn,"no result for '%s'??\n",sql);
	ingres_rollback(iconn->sysConn, ((ingres_conn_t*)iconn->sysConn->connection)->currTran);
	return res;
}

char *dbd_get_engine_version(dbi_conn_t *conn, char *versionstring) {
	char *str = "0";
	dbi_result_t *res = ingres_query(conn, "SELECT dbmsinfo('_version')");
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
			res = ingres_query(newconn, sql);
			if(pattern) free(sql);
		}
		if(res) // remove result set from connection's list
			;// dbi_result_disjoin(res); FIXME: this crashes us later for no discernible reason
		else
			_verbose_handler(conn,"dbd_list_tables: no result for '%s'??\n",sql);
		ingres_rollback(newconn, ((ingres_conn_t*)newconn->connection)->currTran);
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

// note for non-Autocommit connections (such as system catalog access),
// transaction handle is returned in tranHandle parameter, if not NULL
static dbi_result_t *ingres_query(dbi_conn_t *conn, const char *statement) {
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
		DRIVER_ERROR(conn, "whoa, query attempted without a connection\n");
		return NULL;
	}
	
	if(!strncasecmp(statement, "COMMIT", 6)){
		if(ingres_commit(conn, iconn->currTran))
			iconn->currTran = NULL;
		return NULL;
	}else if(!strncasecmp(statement, "ROLLBACK", 8)){
		if(ingres_rollback(conn, iconn->currTran))
			iconn->currTran = NULL;
		return NULL;
	}

	queryParm.qy_connHandle = iconn->connHandle;
	queryParm.qy_queryType = IIAPI_QT_QUERY;
	queryParm.qy_queryText = statement;
	queryParm.qy_parameters = FALSE;
	queryParm.qy_tranHandle = iconn->currTran;
	queryParm.qy_stmtHandle = NULL;
	PRINT_DEBUG(conn, "query tranHandle IN=%#x\n", queryParm.qy_tranHandle);
	IIapi_query( &queryParm );
	status = ingres_wait(conn, &queryParm.qy_genParm);
	SAVE_ERROR(conn, queryParm.qy_genParm.gp_errorHandle);

	iconn->currTran = queryParm.qy_tranHandle;
	PRINT_DEBUG(conn, "query tranHandle OUT=%#x\n", queryParm.qy_tranHandle);

	if(status == IIAPI_ST_NO_DATA)
		_verbose_handler(conn,"(IIAPI_ST_NO_DATA: expected data, but none returned)\n");

	if (status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_NO_DATA){

		gdParm.gd_stmtHandle = queryParm.qy_stmtHandle;
		IIapi_getDescriptor(&gdParm);
    	status = ingres_wait(conn, &gdParm.gd_genParm);
    	SAVE_ERROR(conn, gdParm.gd_genParm.gp_errorHandle);
		if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR){
			// if descriptorCount is zero, no data can be expected
			// create result struct anyway

			if(!gdParm.gd_descriptorCount){
				// fetch the number of affected rows
				gqParm.gq_stmtHandle = queryParm.qy_stmtHandle;
				IIapi_getQueryInfo(&gqParm);
				status = ingres_wait(conn, &gqParm.gq_genParm);
				SAVE_ERROR(conn, gqParm.gq_genParm.gp_errorHandle);
				if(status >= IIAPI_ST_SUCCESS && status < IIAPI_ST_ERROR && (gqParm.gq_mask & IIAPI_GQ_ROW_COUNT)){
					affectedRows = gqParm.gq_rowCount;
					PRINT_VERBOSE(conn,"getQueryInfo: row count = %d\n",affectedRows);
				}else
					affectedRows = 0;
				
				res = _dbd_result_create(conn, NULL, 0, affectedRows);
				PRINT_VERBOSE(conn,"no descriptors\n");
				ingres_close(conn, queryParm.qy_stmtHandle);
			}else{
				PRINT_VERBOSE(conn,"new result set, stmtHandle = %#x\n",queryParm.qy_stmtHandle);
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
					PRINT_VERBOSE(conn,"field %d: '%s' dataType=%d nullable=%d"
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
		ingres_close(conn, queryParm.qy_stmtHandle);
	return res;
}

dbi_result_t *dbd_query(dbi_conn_t *conn, const char *statement) {
	return ingres_query(conn, statement);
}

dbi_result_t *dbd_query_null(dbi_conn_t *conn, const unsigned char *statement, size_t st_length) {
	DRIVER_ERROR(conn, "dbd_query_null() not implemented\n"); //FIXME: use internal error handler (fix other instances too!)
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
	if(conn && conn->connection && ((ingres_conn_t*)conn->connection)->errorMsg){
		*errno = ((ingres_conn_t*)conn->connection)->errorCode;
		*errstr = strdup( ((ingres_conn_t*)conn->connection)->errorMsg );
		return 3;
	}
	return 0;
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
	res = ingres_query(conn, sql);
	free(sql);
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

	asprintf(&sql, "SELECT NEXT VALUE FOR %s", sequence);
	res = ingres_query(conn, sql);
	free(sql);
	if(res && dbi_result_next_row(res))
		seq = dbi_result_get_int_idx(res,1);
	if(res) dbi_result_free(res);
	return seq;
}

int dbd_ping(dbi_conn_t *conn) {
	long test = 0;
	dbi_result_t *res;

	res = ingres_query(conn, "SELECT 1");
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
