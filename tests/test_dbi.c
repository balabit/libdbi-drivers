#include <stdio.h>
#include <dbi/dbi.h>
#include <time.h>

#define QUERY_LEN 512


int main(int argc, char **argv) {
	dbi_driver driver;
	dbi_conn conn;
	dbi_result result;

	char driverdir[256];
	char drivername[64];
	char dbname[64];
	char initial_dbname[64];
	char dbdir[256];
	char username[64];
	char password[64];
	char hostname[256];
	char query[QUERY_LEN+1];
	const char *errmsg;
	int numdrivers;

	printf("\nlibdbi-drivers test program: $Id$\n"
	       "Library version: %s\n\n", dbi_version());
	
	printf("libdbi driver directory? [%s] ", DBI_DRIVER_DIR);
	fgets(driverdir, 256, stdin);
	if (driverdir[0] == '\n') strncpy(driverdir, DBI_DRIVER_DIR, 255), driverdir[255] = '\0';
	else driverdir[strlen(driverdir)-1] = '\0';
	
	numdrivers = dbi_initialize(driverdir);
	
	if (numdrivers < 0) {
		printf("Unable to initialize libdbi! Make sure you specified a valid driver directory.\n");
		dbi_shutdown();
		return 1;
	}
	else if (numdrivers == 0) {
		printf("Initialized libdbi, but no drivers were found!\n");
		dbi_shutdown();
		return 1;
	}
	
	driver = NULL;
	printf("Available drivers (%d): ", numdrivers);
	while ((driver = dbi_driver_list(driver)) != NULL) {
		printf("%s ", dbi_driver_get_name(driver));
	}
	driver = NULL;
	drivername[0] = '\n';

	while (drivername[0] == '\n') {
		printf("\ntest which driver? ");
		fgets(drivername, 64, stdin);
	}
	drivername[strlen(drivername)-1] = '\0';
	
	if (!strcmp(drivername, "mysql") || !strcmp(drivername, "pgsql")) {
		printf("\ndatabase administrator name? ");
		fgets(username, 64, stdin);
		if (*username == '\n') {
			*username = '\0';
		}
		else {
			username[strlen(username)-1] = '\0';
		}

		printf("\ndatabase administrator password? ");
		fgets(password, 64, stdin);
		if (*password == '\n') {
			*password = '\0';
		}
		else {
			password[strlen(password)-1] = '\0';
		}
	}	  
	if(!strcmp(drivername, "sqlite")) {
		printf("database directory? [.] ");
		fgets(dbdir, 256, stdin);
		if (dbdir[0] == '\n') {
			dbdir[0] = '.';
			dbdir[1] = '\0';
		}
		else {
			dbdir[strlen(dbdir)-1] = '\0';
		}
	} else {
		printf("\ndatabase hostname? [(blank for local socket if possible)] ");
		fgets(hostname, 256, stdin);
		if (*hostname == '\n') {
			if (!strcmp(drivername, "pgsql") || !strcmp(drivername, "msql")) {
				*hostname = '\0';
			} else {
				strcpy(hostname, "localhost");
			}
		} else {
			hostname[strlen(hostname)-1] = '\0';
			if (!strcmp(drivername, "pgsql")) {
				if (!strcmp(hostname, "localhost")) {
					*hostname = '\0';
				} 
			}
		}
	}

	printf("database name? [libdbitest] ");
	fgets(dbname, 64, stdin);
	if (dbname[0] == '\n') {
		strcpy(dbname, "libdbitest");
	}
	else {
		dbname[strlen(dbname)-1] = '\0';
	}
	
	if ((conn = dbi_conn_new(drivername)) == NULL) {
		printf("Can't instantiate '%s' driver into a dbi_conn!\n", drivername);
		dbi_shutdown();
		return 1;
	}

	driver = dbi_conn_get_driver(conn);
	
	printf("\nPlugin information:\n-------------------\n");
	printf("\tName:       %s\n"
	       "\tFilename:   %s\n"
	       "\tDesc:       %s\n"
	       "\tMaintainer: %s\n"
	       "\tURL:        %s\n"
	       "\tVersion:    %s\n"
	       "\tCompiled:   %s\n", 
	       dbi_driver_get_name(driver), dbi_driver_get_filename(driver), 
	       dbi_driver_get_description(driver), dbi_driver_get_maintainer(driver), 
	       dbi_driver_get_url(driver), dbi_driver_get_version(driver), 
	       dbi_driver_get_date_compiled(driver));
	
	if (!strcmp(drivername, "mysql") || !strcmp(drivername, "pgsql")) {
		dbi_conn_set_option(conn, "host", hostname);
		dbi_conn_set_option(conn, "username", username);
		dbi_conn_set_option(conn, "password", password);
	
	} else if (!strcmp(drivername, "msql")) {
		if( *hostname) {
			dbi_conn_set_option(conn, "host", hostname);
		}
	} else { /* sqlite */
		dbi_conn_set_option(conn, "sqlite_dbdir", dbdir);
	}

	if (!strcmp(drivername, "mysql")) {
		strcpy(initial_dbname, "mysql");
	}
	else if (!strcmp(drivername, "pgsql")) {
		strcpy(initial_dbname, "template1");
	}
	else if (!strcmp(drivername, "sqlite")) {
		strcpy(initial_dbname, dbname);
	}
	else { /* msql */
		strcpy(initial_dbname, dbname);
	}
	
	dbi_conn_set_option(conn, "dbname", initial_dbname);
	

	if (dbi_conn_connect(conn) < 0) {
		dbi_conn_error(conn, &errmsg);
		printf("\nUnable to connect! Error message: %s\n", errmsg);
		dbi_shutdown();
		return 1;
	}
	
	printf("\nSuccessfully connected!\nTest 1: List databases: \n");
	
	if ((result = dbi_conn_get_db_list(conn, NULL)) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("AAH! Can't get database list! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	}
	printf("\tGot result, try to access rows\n\t\t");
	while (dbi_result_next_row(result)) {
		const char *databasename = NULL;
		databasename = dbi_result_get_string_idx(result, 1);
		printf("%s ", databasename);
	}
	
	dbi_result_free(result);
	
	printf("\nTest 2: Create database %s: \n", dbname);
	
	if (!strcmp(drivername, "sqlite") || !strcmp(drivername, "msql")) {
		printf("\tThis is a no-op with the sqlite/msql drivers.\n");
	}
	else {
		if ((result = dbi_conn_queryf(conn, "CREATE DATABASE %s", dbname)) == NULL) {
			printf("\tDatabase not created.\n");
			fflush(stdout);
			dbi_conn_error(conn, &errmsg);
			printf("\tDarn! Can't create database! Error message: %s\n", errmsg);
			dbi_conn_close(conn);
			dbi_shutdown();
			return 1;
		}
		printf("\tOk.\n");
		fflush(stdout);
		dbi_result_free(result);
	}

	printf("\nTest 3: Select database: \n");

	if (dbi_conn_select_db(conn, dbname)) {
		dbi_conn_error(conn, &errmsg);
		printf("\tUh-oh! Can't select database! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	} else {
		printf("\tOk.\n");
	}
	
	printf("\nTest 4: Create table: \n");
	
	if (!strcmp(drivername, "mysql")) {
	  snprintf(query, QUERY_LEN, "CREATE TABLE test_datatypes ( the_char TINYINT, the_uchar TINYINT, the_short SMALLINT, the_ushort SMALLINT, the_long INT, the_ulong INT, the_longlong BIGINT, the_ulonglong BIGINT, the_float FLOAT4, the_double FLOAT8, the_string VARCHAR(255), the_datetime DATETIME, the_date DATE, the_time TIME)");
	}
	else if (!strcmp(drivername, "pgsql")) {
	  snprintf(query, QUERY_LEN, "CREATE TABLE test_datatypes ( the_char SMALLINT, the_uchar SMALLINT, the_short SMALLINT, the_ushort SMALLINT, the_long INT, the_ulong INT, the_longlong BIGINT, the_ulonglong BIGINT, the_float FLOAT4, the_double FLOAT8, the_string VARCHAR(255), the_datetime TIMESTAMP, the_date DATE, the_time TIME)");
	} 
	else if (!strcmp(drivername, "msql")) {
	  snprintf(query, QUERY_LEN, "CREATE TABLE test_datatypes ( the_char INT8, the_uchar UINT8, the_short INT16, the_ushort UINT16, the_long INT, the_ulong UINT, the_longlong INT64, the_ulonglong UINT64, the_float REAL, the_string CHAR(255), the_date DATE, the_time TIME)");		
	}
	else {
	  snprintf(query, QUERY_LEN, "CREATE TABLE test_datatypes ( the_char CHAR, the_uchar CHAR, the_short SMALLINT, the_ushort SMALLINT, the_long INT, the_ulong INT, the_longlong BIGINT, the_ulonglong BIGINT, the_float FLOAT4, the_double FLOAT8, the_string VARCHAR(255), the_datetime DATETIME, the_date DATE, the_time TIME)");
	}
	if ((result = dbi_conn_query(conn, query)) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("\tAAH! Can't create table! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	} else {
		printf("\tOk.\n");
	}
	dbi_result_free(result);
	
	printf("\nTest 5: List tables: \n");
	
	if ((result = dbi_conn_get_table_list(conn, dbname, NULL)) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("\tOops! Can't get table list! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	}
	printf("\tGot result, try to access rows\n\t\t");
	while (dbi_result_next_row(result)) {
		const char *tablename = NULL;
		tablename = dbi_result_get_string_idx(result, 1);
		printf("%s ", tablename);
	}
	
	dbi_result_free(result);
	
	printf("\nTest 6: Insert row: \n");

	if(!strcmp(drivername, "msql")) {
		snprintf(query, QUERY_LEN, "INSERT INTO test_datatypes VALUES (-127, 127, -32767, 32767, -2147483647, 2147483647, -9223372036854775807,9223372036854775807, 3.402823466E+38, 'this is a test', '11-jul-1977', '23:59:59')");
	} else {
		snprintf(query, QUERY_LEN, "INSERT INTO test_datatypes VALUES (-127, 127, -32768, 32767, -2147483648, 2147483647, -9223372036854775808, 9223372036854775807, 3.402823466E+38, 1.7976931348623157E+307, 'this is a test', '2001-12-31 23:59:59', '2001-12-31', '23:59:59')");
	}
	if ((result = dbi_conn_query(conn, query)) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("\tAAH! Can't insert data! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	} else {
		printf("\tOk.\n");
	}
	dbi_result_free(result);

	printf("\nTest 7: Retrieve data: \n");
	
	if ((result = dbi_conn_query(conn, "SELECT * from test_datatypes")) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("\tAAH! Can't get read data! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	}
	printf("\tGot result, try to access rows\n");
	while (dbi_result_next_row(result)) {
		signed char the_char;
		unsigned char the_uchar;
		short the_short;
		unsigned short the_ushort;
		long the_long;
		unsigned long the_ulong;
		long long the_longlong;
		unsigned long long the_ulonglong;
		float the_float;
		double the_double;
		const char* the_string;
/* 		const unsigned char* the_binary; */
		time_t the_datetime;
		time_t the_date_dt;
		time_t the_time_dt;
		struct tm* ptr_tm;
		struct tm* ptr_tm_date;
		struct tm* ptr_tm_time;
		const char *the_date;
		const char *the_time;
		int year_dt;
		int mon_dt;
		int day_dt;
		int hour_dt;
		int min_dt;
		int sec_dt;
		int year;
		int mon;
		int day;
		int hour;
		int min;
		int sec;

		dbi_error_flag errflag;

		the_char = dbi_result_get_char(result, "the_char");
		errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
		if (errflag) {
		  printf("the_char errflag=%d\n", errflag);
		}

		the_uchar = dbi_result_get_uchar(result, "the_uchar");
		errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
		if (errflag) {
		  printf("the_uchar errflag=%d\n", errflag);
		}

		the_short = dbi_result_get_short(result, "the_short");
		errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
		if (errflag) {
		  printf("the_short errflag=%d\n", errflag);
		}

		the_ushort = dbi_result_get_ushort(result, "the_ushort");
		errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
		if (errflag) {
		  printf("the_ushort errflag=%d\n", errflag);
		}

		the_long = dbi_result_get_long(result, "the_long");
		errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
		if (errflag) {
		  printf("the_long errflag=%d\n", errflag);
		}

		the_ulong = dbi_result_get_ulong(result, "the_ulong");
		errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
		if (errflag) {
		  printf("the_ulong errflag=%d\n", errflag);
		}

		the_longlong = dbi_result_get_longlong(result, "the_longlong");
		errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
		if (errflag) {
		  printf("the_longlong errflag=%d\n", errflag);
		}

		the_ulonglong = dbi_result_get_ulonglong(result, "the_ulonglong");
		errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
		if (errflag) {
		  printf("the_ulonglong errflag=%d\n", errflag);
		}

		the_float = dbi_result_get_float(result, "the_float");
		errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
		if (errflag) {
		  printf("the_float errflag=%d\n", errflag);
		}

		if(!strcmp(drivername, "msql")) {
			printf("the_double: test skipped for this driver.\n");
		}
		else {
			the_double = dbi_result_get_double(result, "the_double");
			errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
			if (errflag) {
				printf("the_double errflag=%d\n", errflag);
			}
		}
		the_string = dbi_result_get_string(result, "the_string");
		errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
		if (errflag) {
		  printf("the_string errflag=%d\n", errflag);
		}

		if(!strcmp(drivername, "msql")) {
			printf("the_datetime: test skipped for this driver.\n");
		}
		else {
			the_datetime = dbi_result_get_datetime(result, "the_datetime");
			errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
			if (errflag) {
				printf("the_datetime errflag=%d\n", errflag);
			}
		}
		if(!strcmp(drivername, "msql")) {
			the_date = dbi_result_get_string(result, "the_date");
			errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
			if (errflag) {
				printf("the_date errflag=%d\n", errflag);
			}
			
			the_time = dbi_result_get_string(result, "the_time");
			errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
			if (errflag) {
				printf("the_time errflag=%d\n", errflag);
			}
			
			printf("the_char: in:-127 out:%d<<\nthe_uchar: in:127 out:%u<<\nthe_short: in:-32767 out:%hd<<\nthe_ushort: in:32767 out:%hu<<\nthe_long: in:-2147483647 out:%ld<<\nthe_ulong: in:2147483647 out:%lu<<\nthe_longlong: in:-9223372036854775807 out:%qd<<\nthe_ulonglong: in:9223372036854775807 out:%qu<<\nthe_float: in:3.402823466E+38 out:%e<<\nthe_string: in:\'this is a test\' out:\'%s\'<<\nthe_date: in:\'11-jul-1977\' out: %s<<\nthe_time: in:\'23:59:59\' out: %s<<", (signed int)the_char, (unsigned int)the_uchar, the_short, the_ushort, the_long, the_ulong, the_longlong, the_ulonglong, the_float, the_string, the_date, the_time);
			
		}
		else {

		the_date_dt = dbi_result_get_datetime(result, "the_date");
		errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
		if (errflag) {
		  printf("the_date errflag=%d\n", errflag);
		}
			
		the_time_dt = dbi_result_get_datetime(result, "the_time");
		errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
		if (errflag) {
		  printf("the_time errflag=%d\n", errflag);
		}
			
		ptr_tm = localtime(&the_datetime);
		year_dt = ptr_tm->tm_year+1900;
		mon_dt = ptr_tm->tm_mon+1;
		day_dt = ptr_tm->tm_mday;
		hour_dt = ptr_tm->tm_hour;
		min_dt = ptr_tm->tm_min;
		sec_dt = ptr_tm->tm_sec;

		ptr_tm_date = localtime(&the_date_dt);
		year = ptr_tm_date->tm_year+1900;
		mon = ptr_tm_date->tm_mon+1;
		day = ptr_tm_date->tm_mday;

		ptr_tm_time = localtime(&the_time_dt);
		hour = ptr_tm_time->tm_hour;
		min = ptr_tm_time->tm_min;
		sec = ptr_tm_time->tm_sec;

		printf("the_char: in:-127 out:%d<<\nthe_uchar: in:127 out:%u<<\nthe_short: in:-32768 out:%hd<<\nthe_ushort: in:32767 out:%hu<<\nthe_long: in:-2147483648 out:%ld<<\nthe_ulong: in:2147483647 out:%lu<<\nthe_longlong: in:-9223372036854775808 out:%qd<<\nthe_ulonglong: in:9223372036854775807 out:%qu<<\nthe_float: in:3.402823466E+38 out:%e<<\nthe_double: in:1.7976931348623157E+307 out:%e\nthe_string: in:\'this is a test\' out:\'%s\'<<\nthe_datetime: in:\'2001-12-31 23:59:59\' out:%d-%d-%d %d:%d:%d\nthe_date: in:\'2001-12-31\' out:%d-%d-%d\nthe_time: in:\'23:59:59\' out:%d:%d:%d\n", (signed int)the_char, (unsigned int)the_uchar, the_short, the_ushort, the_long, the_ulong, the_longlong, the_ulonglong, the_float, the_double, the_string, year_dt, mon_dt, day_dt, hour_dt, min_dt, sec_dt, year, mon, day, hour, min, sec);

		}
	}
	dbi_result_free(result);
	
	printf("\nTest 8: Drop table: \n");
	
	if ((result = dbi_conn_query(conn, "DROP TABLE test_datatypes")) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("\tAAH! Can't drop table! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	} else {
		printf("\tOk.\n");
	}
	dbi_result_free(result);
	
	printf("\nTest 9: Drop database: \n");
	
	if (!strcmp(drivername, "sqlite")) {
		char dbpath[256];

		strcpy(dbpath, dbdir);
		if (dbpath[strlen(dbpath)-1] != '/') {
			strcat(dbpath, "/");
		}
		strcat(dbpath, dbname);
		if (unlink(dbpath)) {
			printf("AAH! Can't delete database file!\n");
			dbi_conn_close(conn);
			dbi_shutdown();
			return 1;
		}
	} else if (!strcmp(drivername, "msql")) {
		printf("\tThis is a no-op with the mSQL driver.\n");
		
	}
	else {
		if (dbi_conn_select_db(conn, initial_dbname)) {
			dbi_conn_error(conn, &errmsg);
			printf("Uh-oh! Can't select database! Error message: %s\n", errmsg);
			dbi_conn_close(conn);
			dbi_shutdown();
			return 1;
		}

		if ((result = dbi_conn_queryf(conn, "DROP DATABASE %s", dbname)) == NULL) {
			dbi_conn_error(conn, &errmsg);
			printf("\tAAH! Can't drop database! Error message: %s\n", errmsg);
			dbi_conn_close(conn);
			dbi_shutdown();
			return 1;
		} else {
			printf("Ok.\n");
		}
		
		dbi_result_free(result);
	}
	
	printf("\n\n");
	printf("SUCCESS! All done, disconnecting and shutting down libdbi. Have a nice day.\n\n");
	
	dbi_conn_close(conn);
	dbi_shutdown();
	
	return 0;
}
