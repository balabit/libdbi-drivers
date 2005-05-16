/*
 * libdbi - database independent abstraction layer for C.
 * Copyright (C) 2001-2002, David Parker and Mark Tobenkin.
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
 * dbd_mysql.c: MySQL database support (using libmysqlclient)
 * Copyright (C) 2001-2002, Mark Tobenkin <mark@brentwoodradio.com>
 * http://libdbi.sourceforge.net
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
#include <time.h>

#include <dbi/dbi.h>
#include <dbi/dbi-dev.h>
#include <dbi/dbd.h>

#include <mysql/mysql.h>
#include "dbd_mysql.h"

static const dbi_info_t driver_info = {
	"mysql",
	"MySQL database support (using libmysqlclient)",
	"Mark M. Tobenkin <mark@brentwoodradio.com>",
	"http://libdbi-drivers.sourceforge.net",
	"dbd_mysql v" VERSION,
	__DATE__
};

static const char *custom_functions[] = {NULL}; // TODO
static const char *reserved_words[] = MYSQL_RESERVED_WORDS;

/* encoding strings, array is terminated by empty string */
static const char mysql_encoding_hash[][16] = {
  /* MySQL, www.iana.org */
  "ascii", "US-ASCII",
  "ujis", "EUC-JP",
  "euc_kr", "EUC-KR", 
  "euckr", "EUC-KR", 
  "utf8", "UTF-8",
  "latin1", "ISO-8859-1",
  "latin2", "ISO-8859-2",
  "latin5", "ISO-8859-9",
  "latin7", "ISO-8859-13",
  "greek", "ISO-8859-7",
  "hebrew", "ISO-8859-8",
  "koi8_ru", "KOI8-R",
  "koi8r", "KOI8-R",
  "koi8_ukr","KOI8-U",
  "koi8u","KOI8-U",
  "cp1251","windows-1251",
  "win1250","windows-1250",
  "cp1250","windows-1250",
  "cp1256","windows-1256",
  "win1257","windows-1257",
  "cp1257","windows-1257",
  "cp850","IBM850",
  "cp852","IBM852",
  "dos","IBM865",
  "cp866","IBM866",
  "big5","Big5",
  "gb2312","GB2312",
  "sjis","Shift_JIS",
  "danish","ISO646-NO",
  "hungarian","ISO646-HU",
  "gbk","GBK",
  "ucs2","ISO-10646-UCS-2",
  "dec8","DEC-MCS",
  "tis620","TIS-620",
  "hp8","hp-roman8",
  ""
};

/* forward declarations of local functions */
void _translate_mysql_type(enum enum_field_types fieldtype, unsigned short *type, unsigned int *attribs);
void _get_field_info(dbi_result_t *result);
void _get_row_data(dbi_result_t *result, dbi_row_t *row, unsigned long long rowidx);
const char* _encoding_mysql2iana(const char *penc);
const char* _encoding_iana2mysql(const char *ienc);



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
	
	return 0;
}

int dbd_connect(dbi_conn_t *conn) {
	MYSQL *mycon;
	char* sql_cmd;

	const char *host = dbi_conn_get_option(conn, "host");
	const char *username = dbi_conn_get_option(conn, "username");
	const char *password = dbi_conn_get_option(conn, "password");
	const char *dbname = dbi_conn_get_option(conn, "dbname");
	const char *encoding = dbi_conn_get_option(conn, "encoding");
	int port = dbi_conn_get_option_numeric(conn, "port");
	/* mysql specific options */
	const char *unix_socket = dbi_conn_get_option(conn, "mysql_unix_socket");
	int compression = dbi_conn_get_option_numeric(conn, "mysql_compression");

	int _compression = (compression > 0) ? CLIENT_COMPRESS : 0;
	
	mycon = mysql_init(NULL);
	if (!mycon) {
		return -1;
	}
	else if (!mysql_real_connect(mycon, host, username, password, dbname, port, unix_socket, _compression)) {
		conn->connection = (void *)mycon; // still need this set so _error_handler can grab information
		_error_handler(conn, DBI_ERROR_DBD);
		mysql_close(mycon);
		conn->connection = NULL; // myconn no longer valid
		return -2;
	}
	else {
		conn->connection = (void *)mycon;
		if (dbname) conn->current_db = strdup(dbname);
	}
	
	if (encoding && *encoding) {
	  /* set connection encoding */
	  if (!strcmp(encoding, "auto")) {
	    encoding = dbd_get_encoding(conn);
	    if (encoding) {
	      asprintf(&sql_cmd, "SET NAMES '%s'", encoding);
/* 	      printf("SET NAMES '%s'", encoding); */
	      dbd_query(conn, sql_cmd);
	      free(sql_cmd);
	    }
/* 	    else { */
/* 	      printf("could not retrieve encoding\n"); */
/* 	    } */
	    /* else: do nothing, use default */
	  }
	  else {
	    asprintf(&sql_cmd, "SET NAMES '%s'", _encoding_iana2mysql(encoding));
/*  	    printf("SET NAMES '%s'", _encoding_iana2mysql(encoding)); */
	    dbd_query(conn, sql_cmd);
	    free(sql_cmd);
	  }
/* 	  printf("set encoding to %s<<\n", encoding); */
	}
/* 	else { */
/* 	  printf("leave encoding alone\n"); */
/* 	} */

	return 0;
}

int dbd_disconnect(dbi_conn_t *conn) {
	if (conn->connection) mysql_close((MYSQL *)conn->connection);
	return 0;
}

int dbd_fetch_row(dbi_result_t *result, unsigned long long rownum) {
	dbi_row_t *row = NULL;

	if (result->result_state == NOTHING_RETURNED) return -1;
	
	if (result->result_state == ROWS_RETURNED) {
		/* get row here */
		row = _dbd_row_allocate(result->numfields);
		_get_row_data(result, row, rownum);
		_dbd_row_finalize(result, row, rownum);
	}
	
	return 1; /* 0 on error, 1 on successful fetchrow */
}

int dbd_free_query(dbi_result_t *result) {
	if (result->result_handle) mysql_free_result((MYSQL_RES *)result->result_handle);
	return 0;
}

int dbd_goto_row(dbi_result_t *result, unsigned long long row) {
	// XXX TODO: kosherize this, handle efficient queries.
	mysql_data_seek((MYSQL_RES *)result->result_handle, row);
	return 1;
}

int dbd_get_socket(dbi_conn_t *conn){
	MYSQL *mycon = (MYSQL*)conn->connection;

	if (!mycon) return -1;

	return (int)mycon->net.fd;
}

const char *dbd_get_encoding(dbi_conn_t *conn){
	MYSQL *mycon = (MYSQL*)conn->connection;
	const char* my_enc = NULL;
	const char* iana_enc = NULL;
	dbi_result dbires = NULL;
	dbi_result dbires1 = NULL;
	const char* schemastring;
	const char* encodingopt;
	char* encodingstart = NULL;
	char* sql_cmd;
	int i = 0;

	if (!mycon) return NULL;

	/* quite a mess. Set the dbi option "encoding" to "auto" to get
	   a behaviour similar to PostgreSQL where the default connection
	   encoding is identical to the database encoding. If you do not 
	   use this option, or if you explicitly requested a particular
	   connection encoding, this encoding will be returned
	   [it has been set in dbd_connect()]
	*/

	encodingopt = dbi_conn_get_option(conn, "encoding");
	if (encodingopt && !strcmp(encodingopt, "auto")) {
	  asprintf(&sql_cmd, "SHOW CREATE DATABASE %s", conn->current_db);
/* 	  printf("SHOW CREATE DATABASE %s", conn->current_db); */

	  dbires = dbi_conn_query(conn, sql_cmd);
	  free(sql_cmd);

	  if (dbires && dbi_result_next_row(dbires)) {
	    schemastring = dbi_result_get_string_idx(dbires, 2);

	    if (schemastring && *schemastring) {
	      my_enc = strstr(schemastring, "CHARACTER SET");
/*   	      printf("schemastring:%s<<enc:%s<<\n", schemastring, my_enc); */
	      if (my_enc) {
		my_enc += 14; /* set to start of encoding name */	    }
	    }
	  }
	}

	if (!my_enc) {
	  /* use connection encoding instead */
	  asprintf(&sql_cmd, "SHOW VARIABLES LIKE '%s'", "character_set_connection");
/* 	  printf("SHOW CREATE DATABASE %s", conn->current_db); */

	  dbires1 = dbi_conn_query(conn, sql_cmd);
	  free(sql_cmd);

	  if (dbires1 && dbi_result_next_row(dbires1)) {
	    my_enc = dbi_result_get_string_idx(dbires1, 2);
	  }

/* 	  my_enc = mysql_character_set_name(mycon); */
/*   	  printf("mysql claims enc:%s<<\n", my_enc); */
	  if (!my_enc) {
	    if (dbires) {
	      dbi_result_free(dbires);
	    }
	    if (dbires1) {
	      dbi_result_free(dbires1);
	    }

	    return NULL;
	  }
	}


	iana_enc = _encoding_mysql2iana(my_enc);
	
	if (dbires) {
	  dbi_result_free(dbires);
	}

	if (dbires1) {
	  dbi_result_free(dbires1);
	}

	return iana_enc;
}

const char* _encoding_mysql2iana(const char *menc) {
  int i = 0;

  /* loop over all even entries in hash and compare to menc */
  while (*mysql_encoding_hash[i]) {
    if (!strncmp(mysql_encoding_hash[i], menc, strlen(mysql_encoding_hash[i]))) {
      /* return corresponding odd entry */
      return mysql_encoding_hash[i+1];
    }
    i+=2;
  }

  /* don't know how to translate, return original encoding */
  return menc;
}

const char* _encoding_iana2mysql(const char *ienc) {
  int i = 0;

  /* loop over all odd entries in hash and compare to ienc */
  while (*mysql_encoding_hash[i+1]) {
    if (!strcmp(mysql_encoding_hash[i+1], ienc)) {
      /* return corresponding even entry */
      return mysql_encoding_hash[i];
    }
    i+=2;
  }

  /* don't know how to translate, return original encoding */
  return ienc;
}

dbi_result_t *dbd_list_dbs(dbi_conn_t *conn, const char *pattern) {
	dbi_result_t *res;
	char *sql_cmd;

	if (pattern == NULL) {
		return dbd_query(conn, "SHOW DATABASES");
	}
	else {
		asprintf(&sql_cmd, "SHOW DATABASES LIKE '%s'", pattern);
		res = dbd_query(conn, sql_cmd);
		free(sql_cmd);
		return res;
	}
}

dbi_result_t *dbd_list_tables(dbi_conn_t *conn, const char *db, const char *pattern) {
	dbi_result_t *res;
	char *sql_cmd;

	if (db == NULL || db[0] == '\0') {
		return dbd_query(conn, "SHOW TABLES");
	}

	if (pattern == NULL) {
		asprintf(&sql_cmd, "SHOW TABLES FROM %s", db);
		res = dbd_query(conn, sql_cmd);
		free(sql_cmd);
		return res;
	}
	else {
		asprintf(&sql_cmd, "SHOW TABLES FROM %s LIKE '%s'", db, pattern);
		res = dbd_query(conn, sql_cmd);
		free(sql_cmd);
		return res;
	}
}

int dbd_quote_string(dbi_driver_t *driver, const char *orig, char *dest) {
	/* foo's -> 'foo\'s' */
	unsigned int len;
	
	strcpy(dest, "'");
	len = mysql_escape_string(dest+1, orig, strlen(orig));	
	strcat(dest, "'");
	
	return len+2;
}

int dbd_conn_quote_string(dbi_conn_t *conn, const char *orig, char *dest) {
	/* foo's -> 'foo\'s' */
	unsigned int len;
	MYSQL *mycon = (MYSQL*)conn->connection;
	
	/* todo: use mysql_real_escape_string */
	strcpy(dest, "'");
	len = mysql_real_escape_string(mycon, dest+1, orig, strlen(orig));	
	strcat(dest, "'");
	
	return len+2;
}

dbi_result_t *dbd_query(dbi_conn_t *conn, const char *statement) {
	/* allocate a new dbi_result_t and fill its applicable members:
	 * 
	 * result_handle, numrows_matched, and numrows_changed.
	 * everything else will be filled in by DBI */
	
	dbi_result_t *result;
	MYSQL_RES *res;
	
	if (mysql_query((MYSQL *)conn->connection, statement)) {
		return NULL;
	}
	
	res = mysql_store_result((MYSQL *)conn->connection);
	
	/* if res is null, the query was something that doesn't return rows (like an INSERT) */
	result = _dbd_result_create(conn, (void *)res, (res ? mysql_num_rows(res) : 0), mysql_affected_rows((MYSQL *)conn->connection));

	if (res) {
	  _dbd_result_set_numfields(result, mysql_num_fields((MYSQL_RES *)result->result_handle));
	  _get_field_info(result);
	}

	return result;
}

dbi_result_t *dbd_query_null(dbi_conn_t *conn, const unsigned char *statement, unsigned long st_length) {
	dbi_result_t *result;
	MYSQL_RES *res;
	
	if (mysql_real_query((MYSQL *)conn->connection, statement, st_length)) {
		return NULL;
	}
	
	res = mysql_store_result((MYSQL *)conn->connection);
	
	/* if res is null, the query was something that doesn't return rows (like an INSERT) */
	result = _dbd_result_create(conn, (void *)res, (res ? mysql_num_rows(res) : 0), mysql_affected_rows((MYSQL *)conn->connection));

	if (res) {
	  _dbd_result_set_numfields(result, mysql_num_fields((MYSQL_RES *)result->result_handle));
	  _get_field_info(result);
	}

	return result;
}

char *dbd_select_db(dbi_conn_t *conn, const char *db) {
	if (mysql_select_db((MYSQL *)conn->connection, db)) {
		_error_handler(conn, DBI_ERROR_DBD);
		return "";
	}

	if (conn->current_db) {
	  free(conn->current_db);
	}
	conn->current_db = strdup(db);
	return (char *)db;
}

int dbd_geterror(dbi_conn_t *conn, int *errno, char **errstr) {
	/* put error number into errno, error string into errstr
	 * return 0 if error, 1 if errno filled, 2 if errstr filled, 3 if both errno and errstr filled */

	if (strcmp("",mysql_error((MYSQL *)conn->connection)) == 0) {
		return -1;
	}
	
	*errno = mysql_errno((MYSQL *)conn->connection);
	*errstr = strdup(mysql_error((MYSQL *)conn->connection));
	return 3;
}

unsigned long long dbd_get_seq_last(dbi_conn_t *conn, const char *sequence) {
	return mysql_insert_id((MYSQL *)conn->connection);
}

unsigned long long dbd_get_seq_next(dbi_conn_t *conn, const char *sequence) {
	return 0;
}

int dbd_ping(dbi_conn_t *conn) {
	MYSQL *mysql = (MYSQL *)conn->connection;

	if (mysql_ping(mysql) == 0) {
		// server is alive and kicking
		return 1;
	}

	return 0;
}

/* CORE MYSQL DATA FETCHING STUFF */

void _translate_mysql_type(enum enum_field_types fieldtype, unsigned short *type, unsigned int *attribs) {
	unsigned int _type = 0;
	unsigned int _attribs = 0;

	switch (fieldtype) {
		case FIELD_TYPE_TINY:
			_type = DBI_TYPE_INTEGER;
			_attribs |= DBI_INTEGER_SIZE1;
			break;
		case FIELD_TYPE_YEAR:
			_attribs |= DBI_INTEGER_UNSIGNED;
		case FIELD_TYPE_SHORT:
			_type = DBI_TYPE_INTEGER;
			_attribs |= DBI_INTEGER_SIZE2;
			break;
		case FIELD_TYPE_INT24:
			_type = DBI_TYPE_INTEGER;
			_attribs |= DBI_INTEGER_SIZE3;
			break;
		case FIELD_TYPE_LONG:
			_type = DBI_TYPE_INTEGER;
			_attribs |= DBI_INTEGER_SIZE4;
			break;
		case FIELD_TYPE_LONGLONG:
			_type = DBI_TYPE_INTEGER;
			_attribs |= DBI_INTEGER_SIZE8;
			break;
			
		case FIELD_TYPE_FLOAT:
			_type = DBI_TYPE_DECIMAL;
			_attribs |= DBI_DECIMAL_SIZE4;
			break;
		case FIELD_TYPE_DOUBLE:
			_type = DBI_TYPE_DECIMAL;
			_attribs |= DBI_DECIMAL_SIZE8;
			break;
			
		case FIELD_TYPE_DATE: /* TODO parse n stuph to native DBI unixtime type. for now, string */
			_type = DBI_TYPE_DATETIME;
			_attribs |= DBI_DATETIME_DATE;
			break;
		case FIELD_TYPE_TIME:
			_type = DBI_TYPE_DATETIME;
			_attribs |= DBI_DATETIME_TIME;
			break;
		case FIELD_TYPE_DATETIME:
		case FIELD_TYPE_TIMESTAMP:
			_type = DBI_TYPE_DATETIME;
			_attribs |= DBI_DATETIME_DATE;
			_attribs |= DBI_DATETIME_TIME;
			break;
			
		case FIELD_TYPE_DECIMAL: /* decimal is actually a string, has arbitrary precision, no floating point rounding */
		case FIELD_TYPE_ENUM:
		case FIELD_TYPE_SET:
		case FIELD_TYPE_VAR_STRING:
		case FIELD_TYPE_STRING:
			_type = DBI_TYPE_STRING;
			break;
			
		case FIELD_TYPE_TINY_BLOB:
		case FIELD_TYPE_MEDIUM_BLOB:
		case FIELD_TYPE_LONG_BLOB:
		case FIELD_TYPE_BLOB:
			_type = DBI_TYPE_BINARY;
			break;
			
		default:
			_type = DBI_TYPE_STRING;
			break;
	}
	
	*type = _type;
	*attribs = _attribs;
}

void _get_field_info(dbi_result_t *result) {
	unsigned int idx = 0;
	MYSQL_FIELD *field;
	unsigned short fieldtype;
	unsigned int fieldattribs;

	field = mysql_fetch_fields((MYSQL_RES *)result->result_handle);
	
	while (idx < result->numfields) {
		_translate_mysql_type(field[idx].type, &fieldtype, &fieldattribs);
		if ((fieldtype == DBI_TYPE_INTEGER) && (field[idx].flags & UNSIGNED_FLAG)) fieldattribs |= DBI_INTEGER_UNSIGNED;
		_dbd_result_add_field(result, idx, field[idx].name, fieldtype, fieldattribs);
		idx++;
	}
}

void _get_row_data(dbi_result_t *result, dbi_row_t *row, unsigned long long rowidx) {
	MYSQL_RES *_res = result->result_handle;
	MYSQL_ROW _row;
	
	int curfield = 0;
	char *raw = NULL;
	unsigned long *strsizes = NULL;
	unsigned long sizeattrib;
	dbi_data_t *data;	

	_row = mysql_fetch_row(_res);
	strsizes = mysql_fetch_lengths(_res);

	while (curfield < result->numfields) {
		raw = _row[curfield];
		data = &row->field_values[curfield];
		
		row->field_sizes[curfield] = 0;
		/* this will be set to the string size later on if the field is indeed a string */

		if ((strsizes[curfield] == 0) && (raw == NULL)) {
		        _set_field_flag( row, curfield, DBI_VALUE_NULL, 1);
			curfield++;
			continue;
		}
		
		switch (result->field_types[curfield]) {
			case DBI_TYPE_INTEGER:
				sizeattrib = _isolate_attrib(result->field_attribs[curfield], DBI_INTEGER_SIZE1, DBI_INTEGER_SIZE8);
				switch (sizeattrib) {
					case DBI_INTEGER_SIZE1:
						data->d_char = (char) atol(raw); break;
					case DBI_INTEGER_SIZE2:
						data->d_short = (short) atol(raw); break;
					case DBI_INTEGER_SIZE3:
					case DBI_INTEGER_SIZE4:
						data->d_long = (long) atol(raw); break;
					case DBI_INTEGER_SIZE8:
						data->d_longlong = (long long) atoll(raw); break;
					default:
						break;
				}
				break;
			case DBI_TYPE_DECIMAL:
				sizeattrib = _isolate_attrib(result->field_attribs[curfield], DBI_DECIMAL_SIZE4, DBI_DECIMAL_SIZE8);
				switch (sizeattrib) {
					case DBI_DECIMAL_SIZE4:
						data->d_float = (float) strtod(raw, NULL); break;
					case DBI_DECIMAL_SIZE8:
						data->d_double = (double) strtod(raw, NULL); break;
					default:
						break;
				}
				break;
			case DBI_TYPE_STRING:
				data->d_string = strdup(raw);
				row->field_sizes[curfield] = (unsigned long long) strsizes[curfield];
				break;
			case DBI_TYPE_BINARY:
				row->field_sizes[curfield] = (unsigned long long) strsizes[curfield];
				data->d_string = malloc(strsizes[curfield]+1); // one extra char for libdbi's null
				if (!data->d_string) {
					break;
				}
				memcpy(data->d_string, raw, strsizes[curfield]);
				data->d_string[strsizes[curfield]] = '\0'; // manually null-terminate the data so C string casting still works
				if (dbi_conn_get_option_numeric(result->conn, "mysql_include_trailing_null") == 1) {
					row->field_sizes[curfield]++; // the extra null we added was actually part of the original data
				}
				break;
			case DBI_TYPE_DATETIME:
				sizeattrib = _isolate_attrib(result->field_attribs[curfield], DBI_DATETIME_DATE, DBI_DATETIME_TIME);
				data->d_datetime = _dbd_parse_datetime(raw, sizeattrib);
				break;
				
			default:
				data->d_string = strdup(raw);
				row->field_sizes[curfield] = (unsigned long long) strsizes[curfield];
				break;
		}
		
		curfield++;
	}
}


