/*
 * libdbi-drivers - database drivers for libdbi, the database
 * independent abstraction layer for C.

 * Copyright (C) 2001-2008, David Parker, Mark Tobenkin, Markus Hoenicka
 * http://libdbi-drivers.sourceforge.net
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

#include <stdio.h>
#include <string.h>
#include <dbi/dbi.h>
#include <dbi/dbi-dev.h> /* need this to access custom functions */
#include <time.h>
#include <unistd.h>
#include <limits.h>

#ifdef __MINGW32__
#include <windows.h>
#define sleep(seconds) Sleep((seconds)*1000)
#endif

#define QUERY_LEN 1024

/* this is defined by the Makefile and passed via -D */
/* #define DBDIR /usr/local/var/lib/libdbi */

/* structure definitions */
struct CONNINFO {
  char driverdir[256];
  char drivername[64];
  char dbname[64];
  char initial_dbname[64];
  char dbdir[256];
  char username[64];
  char password[64];
  char hostname[256];
  char version[64];
  int timeout;
};

struct TABLEINFO {
  int have_double;
  int have_longlong;
  int have_ulonglong;
  int have_datetime;
  int have_datetime_tz;
  int have_time_tz;
};

/* the dbi instance for the recallable interface */
dbi_inst dbi_instance;

/* switch for recallable (0) vs. legacy (!=0) interface */
int n_legacy = 0;

/* some test data */
char string_to_quote[] = "Can \'we\' \"quote\" this properly?";
char string_to_escape[] = "Can \'we\' \"escape\" this properly?";
char numstring[] = "-54321";

unsigned char binary_to_quote[] = {'A', 'B', '\0', 'C', '\'', 'D'};
size_t binary_to_quote_length = 6;

const char default_dbdir[] = DBDIR;

/* prototypes */
void init_tinfo(struct TABLEINFO* ptr_tinfo);
int init_db(struct CONNINFO* ptr_cinfo);
int ask_for_conninfo(struct CONNINFO* ptr_cinfo);
int set_driver_options(struct CONNINFO* ptr_cinfo, dbi_conn conn, const char* encoding, const char* db);
int test_list_db(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int test_create_db(struct CONNINFO* ptr_cinfo, dbi_conn conn, const char* encoding);
int test_select_db(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int test_create_table(struct CONNINFO* ptr_cinfo, struct TABLEINFO* ptr_tinfo, dbi_conn conn);
int test_list_tables(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int test_insert_row(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int test_retrieve_data(struct CONNINFO* ptr_cinfo, struct TABLEINFO* ptr_tinfo, dbi_conn conn);
int test_retrieve_zero_rows(struct CONNINFO* ptr_cinfo, struct TABLEINFO* ptr_tinfo, dbi_conn conn);
int test_retrieve_data_as(struct CONNINFO* ptr_cinfo, struct TABLEINFO* ptr_tinfo, dbi_conn conn);
int test_drop_table(dbi_conn conn);
int test_drop_db(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int test_error_messages(struct CONNINFO* ptr_cinfo, dbi_conn conn, int n);
int test_custom_function(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int test_custom_function_parameters(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int my_dbi_initialize(const char *driverdir, dbi_inst *Inst);
void my_dbi_shutdown(dbi_inst Inst);
dbi_driver my_dbi_driver_list(dbi_driver Current, dbi_inst Inst);
dbi_conn my_dbi_conn_new(const char *name, dbi_inst Inst);



int main(int argc, char **argv) {
  dbi_driver driver;
  dbi_conn conn;
  int testnumber = 1;
  const char *errmsg;
  unsigned int dbengine_version;
  char versionstring[VERSIONSTRING_LENGTH];

  struct CONNINFO cinfo;
  struct TABLEINFO tinfo;

  /* initialize structure */
  init_tinfo(&tinfo);

  if (ask_for_conninfo(&cinfo)) {
    exit(1);
  }

  if ((conn = my_dbi_conn_new(cinfo.drivername, dbi_instance)) == NULL) {
    printf("Can't instantiate '%s' driver into a dbi_conn!\n", cinfo.drivername);
    my_dbi_shutdown(dbi_instance);
    return 1;
  }

  driver = dbi_conn_get_driver(conn);
	
  printf("\nLibrary information:\n--------------------\n");
  printf("\tCurrent:        %d\n"
	 "\tRevision:       %d\n"
	 "\tAge:            %d\n"
	 "\tVersion string: %s\n",
	 LIBDBI_LIB_CURRENT,
	 LIBDBI_LIB_REVISION,
	 LIBDBI_LIB_AGE,
	 dbi_version());

  printf("\nDriver information:\n-------------------\n");
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
	
  if (set_driver_options(&cinfo, conn, "", NULL)) {
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* some drivers need an existing database to connect to */
  if (init_db(&cinfo)) {
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  if (dbi_conn_connect(conn) < 0) {
    dbi_conn_error(conn, &errmsg);
    printf("\nUnable to connect! Error message: %s\n", errmsg);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }
  
  dbengine_version = dbi_conn_get_engine_version(conn);
  dbi_conn_get_engine_version_string(conn, versionstring);

  printf("\nSuccessfully connected!\n\tUsing database engine version %d (numeric) and %s (string)\n", dbengine_version, versionstring);

  /* Test: list available databases */
  printf("\nTest %d: List databases: \n", testnumber++);
	
  if (test_list_db(&cinfo, conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* Test: custom function */
  printf("\n\nTest %d: Run driver-specific function: \n", testnumber++, cinfo.dbname);
	
  if (test_custom_function(&cinfo, conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  
  /* Test: create database */
  printf("\nTest %d: Create database %s using default encoding: \n", testnumber++, cinfo.dbname);
	
  if (test_create_db(&cinfo, conn, NULL)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* Test: select database */
  printf("\nTest %d: Select database: \n", testnumber++);

  if (test_select_db(&cinfo, conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* Test: get encoding */
  printf("\nTest %d: Get encoding: \n", testnumber++);
	
  printf("\tThe database encoding appears to be: %s\n", dbi_conn_get_encoding(conn));


  /* Test: create table */
  printf("\nTest %d: Create table: \n", testnumber++);
	
  if (test_create_table(&cinfo, &tinfo, conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* Test: list tables */
  printf("\nTest %d: List tables: \n", testnumber++);
	
  if (test_list_tables(&cinfo, conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* Test: custom function parameters*/
  printf("\n\nTest %d: Run driver-specific function parameters: \n", testnumber++, cinfo.dbname);
            
  if (test_custom_function_parameters(&cinfo, conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }  
  
  /* Test: insert row */
  printf("\nTest %d: Insert row: \n", testnumber++);

  if (test_insert_row(&cinfo, conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* Test: retrieve data */
  printf("\nTest %d: Retrieve data: \n", testnumber++);
	
  if (test_retrieve_data(&cinfo, &tinfo, conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* Test: run a query which returns zero rows */
  printf("\nTest %d: Retrieve zero rows: \n", testnumber++);
	
  if (test_retrieve_zero_rows(&cinfo, &tinfo, conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }


  /* Test: retrieve data as string or integer */
  printf("\nTest %d: Retrieve data as string or integer: \n", testnumber++);
	
  if (test_retrieve_data_as(&cinfo, &tinfo, conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* Test: drop table */
  printf("\nTest %d: Drop table: \n", testnumber++);
	
  if (test_drop_table(conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* Test: drop database */
  printf("\nTest %d: Drop database:\n", testnumber++);
	
  if (test_drop_db(&cinfo, conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* Test: test character encoding conversion */
  printf("\nTest %d: Character encoding conversion:\n", testnumber++);
	
  if (test_convert_encoding(&cinfo, conn)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }


  /* Test: test error message handling */
  printf("\nTest %d: Error message handling:\n", testnumber++);

  /* supposed to fail */
  if (test_error_messages(&cinfo, conn, 1)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* supposed to fail. Check whether error message is overwritten
     appropriately */
  if (test_error_messages(&cinfo, conn, 2)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* supposed to succeed. Check whether error message is reset */
  if (test_error_messages(&cinfo, conn, 0)) {
    dbi_conn_close(conn);
    my_dbi_shutdown(dbi_instance);
    exit(1);
  }

  /* we're done with this connection */
  dbi_conn_close(conn);
  conn = NULL;

  if ((!strcmp(cinfo.drivername, "mysql") && dbengine_version > 40100)
      ||!strcmp(cinfo.drivername, "pgsql")) {

    printf("\nNow run a couple of tests related to character encodings\n"
    	   "The previous tests used the default encoding, if any. Now we try to connect using UTF-8 and create an UTF-8 database\n");
    if ((conn = my_dbi_conn_new(cinfo.drivername, dbi_instance)) == NULL) {
      printf("Can't instantiate '%s' driver into a dbi_conn!\n", cinfo.drivername);
      my_dbi_shutdown(dbi_instance);
      return 1;
    }

    driver = dbi_conn_get_driver(conn);
	
    if (set_driver_options(&cinfo, conn, "UTF-8", NULL)) {
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }

    if (dbi_conn_connect(conn) < 0) {
      dbi_conn_error(conn, &errmsg);
      printf("\nUnable to connect! Error message: %s\n", errmsg);
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }
	
    printf("\nSuccessfully connected using an UTF-8 encoding!\n");
	
    /* Test: create UTF-8 database */
    printf("\nTest %d: Create database %s with encoding UTF-8: \n", testnumber++, cinfo.dbname);
	
    if (test_create_db(&cinfo, conn, "UTF-8")) {
      dbi_conn_close(conn);
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }

    /* Test: select UTF-8 database */
    printf("\nTest %d: Select UTF-8 database: \n", testnumber++);

    if (test_select_db(&cinfo, conn)) {
      dbi_conn_close(conn);
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }

    /* Test: get encoding of UTF-8 database */
    printf("\nTest %d: Get encoding of UTF-8 database: \n", testnumber++);
	
    printf("The database encoding appears to be: %s\n", dbi_conn_get_encoding(conn));

    /* Test: drop UTF-8 database */
    printf("\nTest %d: Drop UTF-8 database: \n", testnumber++);
	
    if (test_drop_db(&cinfo, conn)) {
      dbi_conn_close(conn);
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }


    /* we're done with this connection */
    dbi_conn_close(conn);
    conn = NULL;

    /* repeat test for latin1 encoding */
    printf("\nNow repeat this test with a ISO-8859-1 encoding\n");
    if ((conn = my_dbi_conn_new(cinfo.drivername, dbi_instance)) == NULL) {
      printf("Can't instantiate '%s' driver into a dbi_conn!\n", cinfo.drivername);
      my_dbi_shutdown(dbi_instance);
      return 1;
    }

    driver = dbi_conn_get_driver(conn);
	
    if (set_driver_options(&cinfo, conn, "ISO-8859-1", NULL)) {
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }

    if (dbi_conn_connect(conn) < 0) {
      dbi_conn_error(conn, &errmsg);
      printf("\nUnable to connect! Error message: %s\n", errmsg);
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }
	
    printf("\nSuccessfully connected using a ISO-8859-1 encoding!\n");
	
    /* Test: create ISO-8859-1 database */
    printf("\nTest %d: Create database %s with encoding ISO-8859-1: \n", testnumber++, cinfo.dbname);
	
    if (test_create_db(&cinfo, conn, "ISO-8859-1")) {
      dbi_conn_close(conn);
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }

    /* Test: select ISO-8859-1 database */
    printf("\nTest %d: Select ISO-8859-1 database: \n", testnumber++);

    if (test_select_db(&cinfo, conn)) {
      dbi_conn_close(conn);
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }

    /* Test: get encoding of ISO-8859-1 database */
    printf("\nTest %d: Get encoding of ISO-8859-1 database: \n", testnumber++);
	
    printf("The database encoding appears to be: %s\n", dbi_conn_get_encoding(conn));

    /* we're done with this connection */
    dbi_conn_close(conn);
    conn = NULL;

    /* now make a connection to the existing database using a different
       encoding */
    printf("\nAttempt to connect to the existing ISO-8859-1 database using an UTF-8 encoding\n");
    if ((conn = my_dbi_conn_new(cinfo.drivername, dbi_instance)) == NULL) {
      printf("Can't instantiate '%s' driver into a dbi_conn!\n", cinfo.drivername);
      my_dbi_shutdown(dbi_instance);
      return 1;
    }

    driver = dbi_conn_get_driver(conn);
	
    if (set_driver_options(&cinfo, conn, "UTF-8", "libdbitest")) {
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }

    if (dbi_conn_connect(conn) < 0) {
      dbi_conn_error(conn, &errmsg);
      printf("\nUnable to connect! Error message: %s\n", errmsg);
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }
	
    printf("\nSuccessfully connected to ISO-8859-1 database using UTF-8 encoding!\n");
	
    /* Test: get encoding of ISO-8859-1 database */
    printf("\nTest %d: Get encoding of ISO-8859-1 database using UTF-8 encoding: \n", testnumber++);
	
    printf("The database encoding appears to be: %s\n", dbi_conn_get_encoding(conn));

    /* we're done with this connection */
    dbi_conn_close(conn);
    conn = NULL;

    /* now make a connection to the existing database using the "auto"
       encoding */
    printf("\nAttempt to connect to the existing ISO-8859-1 database using the \"auto\" encoding feature\n");

    if ((conn = my_dbi_conn_new(cinfo.drivername, dbi_instance)) == NULL) {
      printf("Can't instantiate '%s' driver into a dbi_conn!\n", cinfo.drivername);
      my_dbi_shutdown(dbi_instance);
      return 1;
    }

    driver = dbi_conn_get_driver(conn);
	
    if (set_driver_options(&cinfo, conn, "auto", "libdbitest")) {
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }

    if (dbi_conn_connect(conn) < 0) {
      dbi_conn_error(conn, &errmsg);
      printf("\nUnable to connect! Error message: %s\n", errmsg);
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }
	
    printf("\nSuccessfully connected to ISO-8859-1 database using \"auto\" encoding!\n");
	
    /* Test: get encoding of ISO-8859-1 database */
    printf("\nTest %d: Get encoding of ISO-8859-1 database using \"auto\" encoding: \n", testnumber++);
	
    printf("The database encoding appears to be: %s\n", dbi_conn_get_encoding(conn));

    /* Test: drop ISO-8859-1 database */
    printf("\nTest %d: Drop ISO-8859-1 database: \n", testnumber++);
	
    if (test_drop_db(&cinfo, conn)) {
      dbi_conn_close(conn);
      my_dbi_shutdown(dbi_instance);
      exit(1);
    }
  }
	
  printf("\n\n");
  printf("SUCCESS! All done, disconnecting and shutting down libdbi. Have a nice day.\n\n");
	
  if (conn) {
    dbi_conn_close(conn);
  }

  my_dbi_shutdown(dbi_instance);
	
  exit (0);
}

/* ******************************************************************* */
/* end of main program                                                 */
/* ******************************************************************* */

/* returns 0 on success, 1 on error */
int init_db(struct CONNINFO* ptr_cinfo) {
  char command[1024];
  /* Debian hack: the interactive client is called isql-fb here */
  /* believe it or not. Debian, always touted for its stability and
     no-surprises upgrades even between major versions decided
     somewhere between 3.0 and 4.0 to change a dash to an underscore
     to confuse us */
  int boolean = access("/etc/debian-version", F_OK) & access("/etc/debian_version", F_OK);

  if (!strcmp(ptr_cinfo->drivername, "firebird")) {
    if (!*(ptr_cinfo->hostname)) {
      snprintf(command, 1024, 
	       "echo \"CREATE DATABASE \'%s/%s\';\""
	       "| %s -e -pas %s "
	       "-u %s -sql_dialect 3", ptr_cinfo->dbdir, 
	       ptr_cinfo->dbname,  
	       ( boolean ? "isql" : "isql-fb"), 
	       ptr_cinfo->password, ptr_cinfo->username);
    }
    else { /* remote */
      snprintf(command, 1024, 
	       "echo \"CREATE DATABASE \'%s:%s/%s\';\""
	       "| %s -e -pas %s "
	       "-u %s -sql_dialect 3", ptr_cinfo->hostname, ptr_cinfo->dbdir, 
	       ptr_cinfo->dbname,  
	       ( boolean ? "isql" : "isql-fb"), 
	       ptr_cinfo->password, ptr_cinfo->username);
    }
    if (system(command)) {
      fprintf(stderr, "Could not create initial database\n");
      return 1;
    }
/*     snprintf(command, 1024, "sudo chmod 666 %s/%s", ptr_cinfo->dbdir, ptr_cinfo->dbname); */
/*     if (system(command)) { */
/*       fprintf(stderr, "Could not set database permissions\n"); */
/*       return 1; */
/*     } */
  }
  
  return 0;
}

/* returns 0 on success, 1 on error */
int test_custom_function(struct CONNINFO* ptr_cinfo, dbi_conn conn) {
  dbi_conn_t *myconn = conn;

  /* attempt to call a trivial function of the client library */
  if (!strcmp(ptr_cinfo->drivername, "firebird")) {
    fprintf(stderr, "\tnot yet implemented for this driver\n");
    return 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "freetds")) {
    fprintf(stderr, "\tnot yet implemented for this driver\n");
    return 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "ingres")) {
    fprintf(stderr, "\tnot yet implemented for this driver\n");
    return 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "msql")) {
    fprintf(stderr, "\tnot yet implemented for this driver\n");
    return 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "mysql")) {
    int protocol;
    unsigned int (*custom_function)(void*);

    if ((custom_function = dbi_driver_specific_function(dbi_conn_get_driver(conn), "mysql_get_proto_info")) != NULL) {
      protocol = custom_function(myconn->connection);
      printf("\tmysql_get_proto_info returned: %d\n", protocol);
      return 0;
    }
    else {
      printf("\tD'uh! Cannot run custom function\n");
      return 1;
    }
  }
  else if (!strcmp(ptr_cinfo->drivername, "oracle")) {
    fprintf(stderr, "not yet implemented\n");
    return 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "pgsql")) {
    int protocol;
    int (*custom_function)(void*);

    if ((custom_function = dbi_driver_specific_function(dbi_conn_get_driver(conn), "PQprotocolVersion")) != NULL) {
      protocol = custom_function(myconn->connection);
      printf("\tPQprotocolVersion returned: %d\n", protocol);
      return 0;
    }
    else {
      printf("\tD'uh! Cannot run custom function\n");
      return 1;
    }
  }
  else if (!strcmp(ptr_cinfo->drivername, "sqlite")) {
    const char* version;
    const char* (*custom_function)(void);

    if ((custom_function = dbi_driver_specific_function(dbi_conn_get_driver(conn), "sqlite_libversion")) != NULL) {
      version = custom_function();
      printf("\tsqlite_libversion returned: %s\n", version);
      return 0;
    }
    else {
      printf("\tD'uh! Cannot run custom function\n");
      return 1;
    }
  }
  else if (!strcmp(ptr_cinfo->drivername, "sqlite3")) {
    const char* version;
    const char* (*custom_function)(void);

    if ((custom_function = dbi_driver_specific_function(dbi_conn_get_driver(conn), "sqlite3_libversion")) != NULL) {
      version = custom_function();
      printf("\tsqlite3_libversion returned: %s\n", version);
      return 0;
    }
    else {
      printf("\tD'uh! Cannot run custom function\n");
      return 1;
    }
  }
}

/* returns 0 on success, 1 on error */
int test_custom_function_parameters(struct CONNINFO* ptr_cinfo, dbi_conn conn) {
  dbi_conn_t *myconn = conn;

  /* attempt to call a trivial function of the client library */
  if (!strcmp(ptr_cinfo->drivername, "firebird")) {
    fprintf(stderr, "\tnot yet implemented for this driver\n");
    return 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "freetds")) {
    fprintf(stderr, "\tnot yet implemented for this driver\n");
    return 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "ingres")) {
    fprintf(stderr, "\tnot yet implemented for this driver\n");
    return 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "msql")) {
    fprintf(stderr, "\tnot yet implemented for this driver\n");
    return 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "mysql")) {
    fprintf(stderr, "\tnot yet implemented for this driver\n");
    return 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "oracle")) {
    fprintf(stderr, "not yet implemented\n");
    return 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "pgsql")) {
    int res;
    int count = 30;
    const char *error = NULL;
    const char *errmsg = NULL;
    int (*custom_function_copy)(void*, const char*, int) = NULL;
    int (*custom_function_end)(void*, const char*) = NULL;  
    char* (*custom_function_error)(void*) = NULL;
    dbi_result result;   
    
    const char *query = "COPY batch FROM STDIN";
    const char *query_data1 = "1\t2\t\"A little job\"\n";
    const char *query_data2 = "2\t3\t\"Other little job\"\n";
    const char *table_batch = "CREATE TABLE batch ( id int, jobid int, jobname varchar)";
    
    // Start copy 
    
    if ((result = dbi_conn_query(conn, table_batch)) == NULL) {
       dbi_conn_error(conn, &errmsg);
       printf("\tAAH! Can't create table! Error message: %s\n", errmsg);
       return 1;
    } else {
       printf("\tOk.\n");
    }
    dbi_result_free(result);
    
    if ((result = dbi_conn_query(conn, query)) == NULL) {
       dbi_conn_error(conn, &errmsg);
       printf("\tAAH! Can't query! Error message: %s\n", errmsg);
       return 1;
    } else {
       printf("\tOk %s.\n", query);
    }
    dbi_result_free(result);
    
    // Start copy insert
    // Insert data two times
    if ((custom_function_copy = dbi_driver_specific_function(dbi_conn_get_driver(conn), "PQputCopyData")) != NULL) {
       
       printf("\tPQputCopyData %s\n", query_data1);
       res = custom_function_copy(myconn->connection, query_data1, strlen(query_data1));
       printf("\tPQputCopyData returned: %d\n", res);
              
       if (res <= 0) {
          printf("\tD'uh! PQputCopyData error\n");
          return 1;
       }
       
       printf("\tPQputCopyData %s\n", query_data2);
       res = custom_function_copy(myconn->connection, query_data2, strlen(query_data2));
       printf("\tPQputCopyData returned: %d\n", res);
       
       if (res <= 0) {
          printf("\tD'uh! PQputCopyData error\n");
          return 1;
       }
    }
                
    // End data
    if ((custom_function_end = dbi_driver_specific_function(dbi_conn_get_driver(conn), "PQputCopyEnd")) != NULL) {
       do { 
          res = custom_function_end(myconn->connection, error);         
       } while (res == 0 && --count > 0);
    }
    
    if (res <= 0) {
       printf("\tD'uh! PQputCopyEnd error\n");
       return 1;
    }
    printf("\tPQputCopyEnd returned: %d\n\tError: %d %s\n", res, dbi_conn_error(conn, &errmsg), errmsg);
    if ((custom_function_error = dbi_driver_specific_function(dbi_conn_get_driver(conn), "PQerrorMessage")) != NULL) {
       printf("\tPQerrorMessage returned %s\n", custom_function_error(myconn->connection));
    }
    
    if ((result = dbi_conn_query(conn, "SELECT * from batch")) == NULL) {
        dbi_conn_error(conn, &errmsg);
        printf("\tAAH! Can't get read data! Error message: %s\n", errmsg);
        return 1;
      }

    printf("\tGot result, %d rows, try to access rows\n", dbi_result_get_numrows(result));

    while (dbi_result_next_row(result)) {
        const char *errmsg = NULL;
        long the_long_one = 0;
        long the_long_two = 0;
        const char* the_string;
        
        dbi_error_flag errflag;

        /* first retrieve the values */
        the_long_one = dbi_result_get_long(result, "id");
        errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
        if (errflag) {
          printf("the_int_one errflag=%s\n", errmsg);
        }

        the_long_two = dbi_result_get_long(result, "jobid");
        errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
        if (errflag) {
          printf("the_int_two errflag=%s\n", errmsg);
        }
        
        the_string = dbi_result_get_string(result, "jobname");
        errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
        if (errflag) {
          printf("the_stringr errflag=%s\n", errmsg);
        }
                
        printf("\tResult: the_long_one: %d the_long_two: %d the_string: %s\n",
              the_long_one,
              the_long_two,
              the_string);
    }        
    
    return 0;        
  }
  else if (!strcmp(ptr_cinfo->drivername, "sqlite")) {
     fprintf(stderr, "\tnot yet implemented for this driver\n");
     return 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "sqlite3")) {
     fprintf(stderr, "\tnot yet implemented for this driver\n");
     return 0;
  }
  else {
     printf("\tD'uh! Cannot run custom function\n");
     return 1;
  }
}

/* returns 0 on success, 1 on error */
int ask_for_conninfo(struct CONNINFO* ptr_cinfo) {
  int numdrivers;
  char interface[16];
  dbi_driver driver;

  fprintf(stderr, "\nlibdbi-drivers test program: $Id$\n\n");
  
  fprintf(stderr, "test recallable (r) or legacy (l) libdbi interface? [r] ");
  fgets(interface, 16, stdin);
  if (*interface == '\n'
      || *interface == 'r') {
    n_legacy = 0;
  }
  else {
    n_legacy = 1;
  }

  fprintf(stderr, "libdbi driver directory? [%s] ", DBI_DRIVER_DIR);
  fgets(ptr_cinfo->driverdir, 256, stdin);
  if ((ptr_cinfo->driverdir)[0] == '\n') {
    strncpy(ptr_cinfo->driverdir, DBI_DRIVER_DIR, 255), (ptr_cinfo->driverdir)[255] = '\0';
  }
  else {
    (ptr_cinfo->driverdir)[strlen(ptr_cinfo->driverdir)-1] = '\0';
  }
	
  numdrivers = my_dbi_initialize(ptr_cinfo->driverdir, &dbi_instance);
	
  if (numdrivers < 0) {
    fprintf(stderr, "Unable to initialize libdbi! Make sure you specified a valid driver directory.\n");
    my_dbi_shutdown(dbi_instance);
    return 1;
  }
  else if (numdrivers == 0) {
    fprintf(stderr, "Initialized libdbi, but no drivers were found!\n");
    my_dbi_shutdown(dbi_instance);
    return 1;
  }
	
  driver = NULL;
  fprintf(stderr, "%d drivers available: ", numdrivers);
  while ((driver = my_dbi_driver_list(driver, dbi_instance)) != NULL) {
    fprintf(stderr, "%s ", dbi_driver_get_name(driver));
  }
  driver = NULL;
  (ptr_cinfo->drivername)[0] = '\n';

  while ((ptr_cinfo->drivername)[0] == '\n') {
    fprintf(stderr, "\ntest which driver? ");
    fgets(ptr_cinfo->drivername, 64, stdin);
  }
  (ptr_cinfo->drivername)[strlen(ptr_cinfo->drivername)-1] = '\0';
	
  if (!strcmp(ptr_cinfo->drivername, "mysql")
      || !strcmp(ptr_cinfo->drivername, "pgsql")
      || !strcmp(ptr_cinfo->drivername, "firebird")
      || !strcmp(ptr_cinfo->drivername, "freetds")
     ) {
    fprintf(stderr, "\ndatabase administrator name? ");
    fgets(ptr_cinfo->username, 64, stdin);
    if (*(ptr_cinfo->username) == '\n') {
      *(ptr_cinfo->username) = '\0';
    }
    else {
      (ptr_cinfo->username)[strlen(ptr_cinfo->username)-1] = '\0';
    }

    fprintf(stderr, "\ndatabase administrator password? ");
    fgets(ptr_cinfo->password, 64, stdin);
    if (*(ptr_cinfo->password) == '\n') {
      *(ptr_cinfo->password) = '\0';
    }
    else {
      (ptr_cinfo->password)[strlen(ptr_cinfo->password)-1] = '\0';
    }
  }	  
  if(!strcmp(ptr_cinfo->drivername, "sqlite")
     || !strcmp(ptr_cinfo->drivername, "sqlite3")
     || !strcmp(ptr_cinfo->drivername, "firebird")) {
    fprintf(stderr, "database directory? [DEFAULT] ");
    fgets(ptr_cinfo->dbdir, 256, stdin);
    if ((ptr_cinfo->dbdir)[0] == '\n') {
      (ptr_cinfo->dbdir)[0] = '\0';
    }
    else {
      (ptr_cinfo->dbdir)[strlen(ptr_cinfo->dbdir)-1] = '\0';
    }
    if (!strcmp(ptr_cinfo->drivername, "firebird")
	&& (ptr_cinfo->dbdir)[0] == '\0') {
      strcpy(ptr_cinfo->dbdir, default_dbdir);
      strcat(ptr_cinfo->dbdir, "/firebird");
    }
  }

  if (!strcmp(ptr_cinfo->drivername, "firebird")
      || !strcmp(ptr_cinfo->drivername, "mysql")
      || !strcmp(ptr_cinfo->drivername, "pgsql")
      || !strcmp(ptr_cinfo->drivername, "freetds")
     ) {
    fprintf(stderr, "\ndatabase hostname? [(blank for local socket if possible)] ");
    fgets(ptr_cinfo->hostname, 256, stdin);
    if (*(ptr_cinfo->hostname) == '\n') {
      if (!strcmp(ptr_cinfo->drivername, "pgsql")
	  || !strcmp(ptr_cinfo->drivername, "firebird")
	  || !strcmp(ptr_cinfo->drivername, "msql")) {
	*(ptr_cinfo->hostname) = '\0';
      } 
      else {
	strcpy(ptr_cinfo->hostname, "localhost");
      }
    }
    else {
      (ptr_cinfo->hostname)[strlen(ptr_cinfo->hostname)-1] = '\0';
      if (!strcmp(ptr_cinfo->drivername, "pgsql")) {
	if (!strcmp(ptr_cinfo->hostname, "localhost")) {
	  *(ptr_cinfo->hostname) = '\0';
	} 
      }
    }
  }

  if (!strcmp(ptr_cinfo->drivername, "freetds")) {
    fprintf(stderr, "database version? ");
    fgets(ptr_cinfo->version, 64, stdin);
    (ptr_cinfo->version)[strlen(ptr_cinfo->version)-1] = '\0';
  }

  fprintf(stderr, "database name? [libdbitest] ");
  fgets(ptr_cinfo->dbname, 64, stdin);
  if ((ptr_cinfo->dbname)[0] == '\n') {
    strcpy(ptr_cinfo->dbname, "libdbitest");
  }
  else {
    (ptr_cinfo->dbname)[strlen(ptr_cinfo->dbname)-1] = '\0';
  }

  /* use default timeout for now */
  ptr_cinfo->timeout = 10;

  return 0;
}

/* always returns 0 */
int set_driver_options(struct CONNINFO* ptr_cinfo, dbi_conn conn, const char* encoding, const char* db) {
  if (!strcmp(ptr_cinfo->drivername, "mysql")
      || !strcmp(ptr_cinfo->drivername, "pgsql")
      || !strcmp(ptr_cinfo->drivername, "freetds")
     ) {
    dbi_conn_set_option(conn, "host", ptr_cinfo->hostname);
    dbi_conn_set_option(conn, "username", ptr_cinfo->username);
    dbi_conn_set_option(conn, "password", ptr_cinfo->password);

    if (!strcmp(ptr_cinfo->drivername, "freetds") && strlen(ptr_cinfo->version)){
      dbi_conn_set_option(conn, "freetds_version", ptr_cinfo->version);
    }
  } 
  else if (!strcmp(ptr_cinfo->drivername, "msql")) {
    if( *(ptr_cinfo->hostname)) {
      dbi_conn_set_option(conn, "host", ptr_cinfo->hostname);
    }
  }
  else if (!strcmp(ptr_cinfo->drivername, "sqlite3")) { 
    if (*(ptr_cinfo->dbdir)) {
      dbi_conn_set_option(conn, "sqlite3_dbdir", ptr_cinfo->dbdir);
    }
  }
  else  if (!strcmp(ptr_cinfo->drivername, "sqlite")){
    if (*(ptr_cinfo->dbdir)) {
      dbi_conn_set_option(conn, "sqlite_dbdir", ptr_cinfo->dbdir);
    }
  }
  else  if (!strcmp(ptr_cinfo->drivername, "firebird")){
    if (*(ptr_cinfo->dbdir)) {
      dbi_conn_set_option(conn, "firebird_dbdir", ptr_cinfo->dbdir);
    }
    dbi_conn_set_option(conn, "host", ptr_cinfo->hostname);
    dbi_conn_set_option(conn, "username", ptr_cinfo->username);
    dbi_conn_set_option(conn, "password", ptr_cinfo->password);
  }

  if (!strcmp(ptr_cinfo->drivername, "mysql")) {
    strcpy(ptr_cinfo->initial_dbname, "mysql");
    
    if (encoding && *encoding) {
      dbi_conn_set_option(conn, "encoding", encoding);
    }
  }
  else if (!strcmp(ptr_cinfo->drivername, "pgsql")) {
    strcpy(ptr_cinfo->initial_dbname, "template1");
    if (encoding && *encoding) {
      dbi_conn_set_option(conn, "encoding", encoding);
    }
  }
  else if (!strcmp(ptr_cinfo->drivername, "sqlite")
		|| !strcmp(ptr_cinfo->drivername, "sqlite3")) {
    strcpy(ptr_cinfo->initial_dbname, ptr_cinfo->dbname);
    if (encoding && *encoding) {
      dbi_conn_set_option(conn, "encoding", encoding);
    }
  }
  else if (!strcmp(ptr_cinfo->drivername, "ingres")
		|| !strcmp(ptr_cinfo->drivername, "firebird")){
    strcpy(ptr_cinfo->initial_dbname, ptr_cinfo->dbname);
  }
  else if (!strcmp(ptr_cinfo->drivername, "freetds")) {
    strcpy(ptr_cinfo->initial_dbname, "master");
    if (encoding && *encoding) {
      dbi_conn_set_option(conn, "encoding", encoding);
    }
  }
	
  dbi_conn_set_option(conn, "dbname", db && *db ? db : ptr_cinfo->initial_dbname);
  dbi_conn_set_option_numeric(conn, "timeout", ptr_cinfo->timeout);
  dbi_conn_set_option_numeric(conn, "LogQueries", 1);

  return 0;
}

/* returns 0 on success, 1 on error */
int test_list_db(struct CONNINFO* ptr_cinfo, dbi_conn conn) {
  const char *errmsg;
  dbi_result result;

  /* currently not implemented in firebird */
  if (!strcmp(ptr_cinfo->drivername, "firebird")) {
    printf("\tnot yet implemented\n");
    return 0;
  }

  if ((result = dbi_conn_get_db_list(conn, NULL)) == NULL) {
    dbi_conn_error(conn, &errmsg);
    printf("AAH! Can't get database list! Error message: %s\n", errmsg);
    return 1;
  }

  printf("\tGot result, try to access rows\n\t\t");
  while (dbi_result_next_row(result)) {
    const char *databasename = NULL;
    databasename = dbi_result_get_string_idx(result, 1);
    printf("%s ", databasename);
  }
	
  dbi_result_free(result);
  
  return 0;
}

/* returns 0 on success, 1 on error */
int test_create_db(struct CONNINFO* ptr_cinfo, dbi_conn conn, const char* encoding) {
  const char *errmsg;
  dbi_result result;
  char my_enc[32];

  if (!strcmp(ptr_cinfo->drivername, "sqlite")
      || !strcmp(ptr_cinfo->drivername, "sqlite3")
      || !strcmp(ptr_cinfo->drivername, "firebird")
      || !strcmp(ptr_cinfo->drivername, "msql")
      || !strcmp(ptr_cinfo->drivername, "ingres")) {
    printf("\tThis is a no-op with the sqlite/msql/firebird/ingres drivers.\n");
  }
  else {
    if (encoding && *encoding) {
      if (!strcmp(ptr_cinfo->drivername, "mysql")) {
	result = dbi_conn_queryf(conn, "CREATE DATABASE %s CHARACTER SET %s", ptr_cinfo->dbname, (!strcmp(encoding, "UTF-8")) ? "utf8":"latin1");
      }
      else if (!strcmp(ptr_cinfo->drivername, "pgsql")) {
	result = dbi_conn_queryf(conn, "CREATE DATABASE %s WITH ENCODING = '%s'", ptr_cinfo->dbname, (!strcmp(encoding, "UTF-8")) ? "UNICODE":"LATIN1");
      }
      else {
	result = dbi_conn_queryf(conn, "CREATE DATABASE %s", ptr_cinfo->dbname);
      }
    }
    else {
      /* use default encoding */
      result = dbi_conn_queryf(conn, "CREATE DATABASE %s", ptr_cinfo->dbname);
    }

    if (result == NULL) {
      printf("\tDatabase not created.\n");
      fflush(stdout);
      dbi_conn_error(conn, &errmsg);
      printf("\tDarn! Can't create database! Error message: %s\n", errmsg);
      return 1;
    }
    printf("\tOk.\n");
    fflush(stdout);
    dbi_result_free(result);
  }
  return 0;
}

/* returns 0 on success, 1 on error */
int test_select_db(struct CONNINFO* ptr_cinfo, dbi_conn conn) {
  const char *errmsg;

  if (dbi_conn_select_db(conn, ptr_cinfo->dbname)) {
    dbi_conn_error(conn, &errmsg);
    printf("\tUh-oh! Can't select database! Error message: %s\n", errmsg);
    return 1;
  }
  else {
    printf("\tOk.\n");
  }
  return 0;
}

/* returns 0 on success, 1 on error */
int test_create_table(struct CONNINFO* ptr_cinfo, struct TABLEINFO* ptr_tinfo, dbi_conn conn) {
  char query[QUERY_LEN+1];
  const char *errmsg;
  dbi_result result;

  if (!strcmp(ptr_cinfo->drivername, "firebird")) {
        snprintf(query, QUERY_LEN, "CREATE TABLE test_datatypes ( "
		 "the_char SMALLINT,"
		 "the_uchar SMALLINT,"
		 "the_short SMALLINT, "
		 "the_ushort SMALLINT,"
		 "the_long INTEGER,"
		 "the_ulong INTEGER, "
		 "the_float FLOAT,"
		 "the_double DOUBLE PRECISION, "
		 "the_driver_string VARCHAR(255),"
		 "the_conn_quoted_string VARCHAR(255),"
		 "the_conn_quoted_string_copy VARCHAR(255),"
		 "the_conn_escaped_string VARCHAR(255),"
		 "the_conn_escaped_string_copy VARCHAR(255),"
		 "the_numstring VARCHAR(255),"
		 "the_empty_string VARCHAR(255),"
		 "the_null_string VARCHAR(255), "
		 "the_binary_quoted_string BLOB,"
		 "the_binary_escaped_string BLOB,"
		 "the_datetime TIMESTAMP, "
		 "the_date DATE,"
		 "the_time TIME,"
		 "id INTEGER NOT NULL PRIMARY KEY)");
    ptr_tinfo->have_double = 1;
    ptr_tinfo->have_datetime = 1;
  }
  else if (!strcmp(ptr_cinfo->drivername, "freetds")) {
     snprintf(query, QUERY_LEN, "CREATE TABLE test_datatypes ( "
	      "the_char TINYINT,"
	      "the_uchar TINYINT,"
	      "the_short SMALLINT,"
	      "the_ushort SMALLINT,"
	      "the_long INT,"
	      "the_ulong INT,"
	      "the_longlong BIGINT,"
	      "the_ulonglong BIGINT,"
	      "the_float REAL,"
	      "the_double FLOAT,"
	      "the_driver_string VARCHAR(255),"
	      "the_conn_quoted_string VARCHAR(255),"
	      "the_conn_quoted_string_copy VARCHAR(255),"
	      "the_conn_escaped_string VARCHAR(255),"
	      "the_conn_escaped_string_copy VARCHAR(255),"
	      "the_numstring VARCHAR(255),"
	      "the_empty_string VARCHAR(255),"
	      "the_null_string VARCHAR(255),"
	      "the_binary_quoted_string IMAGE,"
	      "the_binary_escaped_string IMAGE,"
	      "the_datetime DATETIME,"
	      "the_date DATETIME,"
	      "the_time DATETIME,"
	      "id INT IDENTITY,"
	      "CONSTRAINT tr_test_datatypes PRIMARY KEY (id))");
    ptr_tinfo->have_double = 1;
    ptr_tinfo->have_longlong = 1;
    ptr_tinfo->have_ulonglong = 1;
    ptr_tinfo->have_datetime = 1;
  } 
  else if (!strcmp(ptr_cinfo->drivername, "ingres")) {
     snprintf(query, QUERY_LEN, "CREATE TABLE test_datatypes ( "
	      "the_char TINYINT,"
	      "the_uchar TINYINT,"
	      "the_short SMALLINT,"
	      "the_ushort SMALLINT,"
	      "the_long INT,"
	      "the_ulong INT,"
	      "the_longlong BIGINT,"
	      "the_ulonglong BIGINT,"
	      "the_float FLOAT4,"
	      "the_double FLOAT,"
	      "the_decimal DECIMAL(12,4),"
	      "the_money MONEY,"
	      "the_character CHAR(50),"
	      "the_byte BYTE(50),"
	      "the_driver_string VARCHAR(255),"
	      "the_conn_quoted_string VARCHAR(255),"
	      "the_conn_quoted_string_copy VARCHAR(255),"
	      "the_conn_escaped_string VARCHAR(255),"
	      "the_conn_escaped_string_copy VARCHAR(255),"
	      "the_numstring VARCHAR(255),"
	      "the_empty_string VARCHAR(255),"
	      "the_null_string VARCHAR(255),"
	      "the_binary_string BLOB,"
	      "the_binary_copy BLOB,"
	      "the_datetime DATE,"
	      "the_date DATE,"
	      "the_time DATE,"
	      "id INT NOT NULL CONSTRAINT id_key PRIMARY KEY)");
    ptr_tinfo->have_double = 1;
    ptr_tinfo->have_longlong = 1;
    ptr_tinfo->have_ulonglong = 1;
    ptr_tinfo->have_datetime = 1;
  } 
  else if (!strcmp(ptr_cinfo->drivername, "msql")) {
    snprintf(query, QUERY_LEN, "CREATE TABLE test_datatypes ( "
	     "the_char INT8,"
	     "the_uchar UINT8,"
	     "the_short INT16,"
	     "the_ushort UINT16,"
	     "the_long INT,"
	     "the_ulong UINT,"
	     "the_longlong INT64,"
	     "the_ulonglong UINT64,"
	     "the_float REAL,"
	     "the_driver_string CHAR(255),"
	     "the_conn_quoted_string CHAR(255),"
	     "the_conn_quoted_string_copy CHAR(255),"
	     "the_conn_escaped_string CHAR(255),"
	     "the_conn_escaped_string_copy CHAR(255),"
	     "the_numstring CHAR(255),"
	     "the_empty_string VARCHAR(255),"
	     "the_null_string VARCHAR(255),"
	     "the_date DATE,"
	     "the_time TIME,"
	     "the_time_tz TIME,"
	     "id INT)");		
    ptr_tinfo->have_longlong = 1;
    ptr_tinfo->have_ulonglong = 1;
    ptr_tinfo->have_time_tz = 1;
  }
  else if (!strcmp(ptr_cinfo->drivername, "mysql")) {
    snprintf(query, QUERY_LEN, "CREATE TABLE test_datatypes ( "
	     "the_char TINYINT,"
	     "the_uchar TINYINT,"
	     "the_short SMALLINT,"
	     "the_ushort SMALLINT,"
	     "the_long INT,"
	     "the_ulong INT,"
	     "the_longlong BIGINT,"
	     "the_ulonglong BIGINT,"
	     "the_float FLOAT4,"
	     "the_double FLOAT8,"
	     "the_driver_string VARCHAR(255),"
	     "the_conn_quoted_string VARCHAR(255),"
	     "the_conn_quoted_string_copy VARCHAR(255),"
	     "the_conn_escaped_string VARCHAR(255),"
	     "the_conn_escaped_string_copy VARCHAR(255),"
	     "the_numstring VARCHAR(255),"
	     "the_empty_string VARCHAR(255),"
	     "the_null_string VARCHAR(255),"
	     "the_binary_quoted_string BLOB,"
	     "the_binary_escaped_string BLOB,"
	     "the_datetime DATETIME,"
	     "the_datetime_tz DATETIME,"
	     "the_date DATE,"
	     "the_time TIME,"
	     "the_time_tz TIME,"
	     "id INT AUTO_INCREMENT,"
	     "PRIMARY KEY (id))");
    ptr_tinfo->have_double = 1;
    ptr_tinfo->have_longlong = 1;
    ptr_tinfo->have_ulonglong = 1;
    ptr_tinfo->have_datetime = 1;
    ptr_tinfo->have_datetime_tz = 0;
    ptr_tinfo->have_time_tz = 0;
  }
  else if (!strcmp(ptr_cinfo->drivername, "pgsql")) {
    /* PostgreSQL does not have a 1-byte integer, use smallint
       instead. This will raise a warning when retrieving the value */
    snprintf(query, QUERY_LEN, "CREATE TABLE test_datatypes ( "
	     "the_char SMALLINT,"
	     "the_uchar SMALLINT,"
	     "the_short SMALLINT,"
	     "the_ushort SMALLINT,"
	     "the_long INT,"
	     "the_ulong INT,"
	     "the_longlong BIGINT,"
	     "the_ulonglong BIGINT,"
	     "the_float FLOAT4,"
	     "the_double FLOAT8,"
	     "the_driver_string VARCHAR(255),"
	     "the_conn_quoted_string VARCHAR(255),"
	     "the_conn_quoted_string_copy VARCHAR(255),"
	     "the_conn_escaped_string VARCHAR(255),"
	     "the_conn_escaped_string_copy VARCHAR(255),"
	     "the_numstring VARCHAR(255),"
	     "the_empty_string VARCHAR(255),"
	     "the_null_string VARCHAR(255),"
	     "the_binary_quoted_string BYTEA,"
	     "the_binary_escaped_string BYTEA,"
	     "the_datetime TIMESTAMP,"
	     "the_datetime_tz TIMESTAMP WITH TIME ZONE,"
	     "the_date DATE,"
	     "the_time TIME,"
	     "the_time_tz TIME WITH TIME ZONE,"
	     "id SERIAL PRIMARY KEY)");
    ptr_tinfo->have_double = 1;
    ptr_tinfo->have_longlong = 1;
    ptr_tinfo->have_ulonglong = 1;
    ptr_tinfo->have_datetime = 1;
    ptr_tinfo->have_datetime_tz = 1;
    ptr_tinfo->have_time_tz = 1;
  } 
  else if (!strcmp(ptr_cinfo->drivername, "sqlite")
	   || !strcmp(ptr_cinfo->drivername, "sqlite3")){
    snprintf(query, QUERY_LEN, "CREATE TABLE test_datatypes ( "
	     "the_char CHAR,"
	     "the_uchar CHAR,"
	     "the_short SMALLINT,"
	     "the_ushort SMALLINT,"
	     "the_long INT,"
	     "the_ulong INT,"
	     "the_longlong BIGINT,"
	     "the_ulonglong BIGINT,"
	     "the_float FLOAT4,"
	     "the_double FLOAT8,"
	     "the_driver_string VARCHAR(255),"
	     "the_conn_quoted_string VARCHAR(255),"
	     "the_conn_quoted_string_copy VARCHAR(255),"
	     "the_conn_escaped_string VARCHAR(255),"
	     "the_conn_escaped_string_copy VARCHAR(255),"
	     "the_numstring VARCHAR(255),"
	     "the_empty_string VARCHAR(255),"
	     "the_null_string VARCHAR(255),"
	     "the_binary_quoted_string BLOB,"
	     "the_binary_escaped_string BLOB,"
	     "the_datetime DATETIME,"
	     "the_datetime_tz DATETIME,"
	     "the_date DATE,"
	     "the_time TIME,"
	     "the_time_tz TIME,"
	     "id INTEGER AUTO INCREMENT)");
    ptr_tinfo->have_double = 1;
    ptr_tinfo->have_longlong = 1;
    ptr_tinfo->have_ulonglong = 1;
    ptr_tinfo->have_datetime = 1;
    ptr_tinfo->have_datetime_tz = 1;
    ptr_tinfo->have_time_tz = 1;
  }


  if ((result = dbi_conn_query(conn, query)) == NULL) {
    dbi_conn_error(conn, &errmsg);
    printf("\tAAH! Can't create table! Error message: %s\n", errmsg);
    return 1;
  }
  else {
    printf("\tOk.\n");
  }
  dbi_result_free(result);
  
  if(!strcmp(ptr_cinfo->drivername, "ingres"))
  	dbi_result_free(dbi_conn_query(conn, "CREATE SEQUENCE test_datatypes_id_seq"));

  return 0;
}

/* returns 0 on success, 1 on error */
int test_list_tables(struct CONNINFO* ptr_cinfo, dbi_conn conn) {
  const char *errmsg;
  dbi_result result;

  if ((result = dbi_conn_get_table_list(conn, ptr_cinfo->dbname, NULL)) == NULL) {
    dbi_conn_error(conn, &errmsg);
    printf("\tOops! Can't get table list! Error message: %s\n", errmsg);
    return 1;
  }
  printf("\tGot result, try to access rows\n\t\t");
  while (dbi_result_next_row(result)) {
    const char *tablename = NULL;
    tablename = dbi_result_get_string_idx(result, 1);
    printf("%s ", tablename);
  }
	
  dbi_result_free(result);
  return 0;
}

/* returns 0 on success, 1 on error */
int test_insert_row(struct CONNINFO* ptr_cinfo, dbi_conn conn) {
  char query[QUERY_LEN+1];
  char *my_string_to_quote = NULL;
  char *my_string_to_escape = NULL;
  char *driver_quoted_string = NULL;
  char *quoted_string = NULL;
  char *escaped_string = NULL;
  unsigned char* quoted_binary = NULL;
  unsigned char* escaped_binary = NULL;
  size_t quoted_string_length;
  size_t escaped_string_length;
  size_t quoted_binary_length;
  size_t escaped_binary_length;
  const char *errmsg;
  unsigned long long n_last_id;
  unsigned long long n_next_id;
  dbi_result result;

  /* strings which are quoted or escaped in place must be allocated */
  my_string_to_quote = strdup(string_to_quote);
  my_string_to_escape = strdup(string_to_escape);

  if (!my_string_to_quote || !my_string_to_escape) {
    return 1;
  }

  /* driver function (deprecated) */
  dbi_driver_quote_string_copy(dbi_conn_get_driver(conn), string_to_quote, &driver_quoted_string);

  /* conn quote functions */
  dbi_conn_quote_string(conn, &my_string_to_quote);
  quoted_string_length = dbi_conn_quote_string_copy(conn, string_to_quote, &quoted_string);
  quoted_binary_length = dbi_conn_quote_binary_copy(conn, binary_to_quote, binary_to_quote_length, &quoted_binary);

  /* conn escape functions */
  dbi_conn_escape_string(conn, &my_string_to_escape);
  escaped_string_length = dbi_conn_escape_string_copy(conn, string_to_escape, &escaped_string);
  escaped_binary_length = dbi_conn_escape_binary_copy(conn, binary_to_quote, binary_to_quote_length, &escaped_binary);

  if(!strcmp(ptr_cinfo->drivername, "firebird")) {
    snprintf(query, QUERY_LEN, "INSERT INTO test_datatypes ("
	     "the_char,"
	     "the_uchar,"
	     "the_short, "
	     "the_ushort,"
	     "the_long,"
	     "the_ulong,"
	     "the_float,"
	     "the_double, "
	     "the_driver_string,"
	     "the_conn_quoted_string,"
	     "the_conn_quoted_string_copy,"
	     "the_conn_escaped_string,"
	     "the_conn_escaped_string_copy,"
	     "the_numstring,"
	     "the_empty_string,"
	     "the_null_string,"
	     "the_binary_quoted_string,  "
	     "the_binary_escaped_string,  "
	     "the_datetime,"
	     "the_date,"
	     "the_time,"
	     "id) "
	     "VALUES ("
	     "-127,"
	     "127,"
	     "-32768,"
	     "32767,"
	     "-2147483648,"
	     "2147483647, "
	     "3.4e+37,"
	     "1.7e+307,"
	     "%s,"
	     "%s,"
	     "%s,"
	     "'%s',"
	     "'%s',"
	     "'%s',"
	     "'',"
	     "NULL,"
	     "%s, "
	     "'%s', "
 	     "'2001-12-31 23:59:59',"
	     "'2001-12-31',"
	     "'23:59:59',"
	     "1)",
	     driver_quoted_string, 
	     my_string_to_quote,
	     quoted_string,
	     my_string_to_escape,
	     escaped_string,
	     numstring,
	     quoted_binary,
	     escaped_binary);
  } 
  else if (!strcmp(ptr_cinfo->drivername, "freetds")){
  /* 
   * For test one byte data type use TINYINT
   * this is unsigned type and by insert replace 
   * -127 to binary equivalent 129
   */
    snprintf(query, QUERY_LEN, "INSERT INTO test_datatypes ("
	     "the_char,"
	     "the_uchar,"
	     "the_short,"
	     "the_ushort,"
	     "the_long,"
	     "the_ulong,"
	     "the_longlong,"
	     "the_ulonglong,"
	     "the_float,"
	     "the_double,"
	     "the_driver_string,"
	     "the_conn_quoted_string,"
	     "the_conn_quoted_string_copy,"
	     "the_conn_escaped_string,"
	     "the_conn_escaped_string_copy,"
	     "the_numstring,"
	     "the_empty_string,"
	     "the_null_string,"
	     "the_binary_quoted_string,"
	     "the_binary_escaped_string,"
	     "the_datetime,"
	     "the_date,"
	     "the_time) VALUES ("
	     "127,"
	     "127,"
	     "-32768,"
	     "32767,"
	     "-2147483648,"
	     "2147483647,"
	     "-9223372036854775807,"
	     "9223372036854775807,"
	     "3.402823466E+38,"
	     "1.7976931348623157E+307,"
	     "%s,"
	     "%s,"
	     "%s,"
	     "'%s',"
	     "'%s',"
	     "'%s',"
	     "'',"
	     "NULL,"
	     "%s,"
	     "'%s',"
	     "'2001-12-31 23:59:59',"
	     "'2001-12-31',"
	     "'23:59:59')",
	     driver_quoted_string, 
	     my_string_to_quote,
	     quoted_string,
	     my_string_to_escape,
	     escaped_string,
	     numstring,
	     quoted_binary,
	     escaped_binary);
  } 
  else if(!strcmp(ptr_cinfo->drivername, "ingres")) {
    snprintf(query, QUERY_LEN, "INSERT INTO test_datatypes VALUES ("
	     "-127,"
	     "127,"
	     "-32768,"
	     "32767,"
	     "-2147483648,"
	     "2147483647, "
	     "-9223372036854775807,"
	     "9223372036854775807,"
	     "3.4e+37,"
	     "1.7e+307,"
	     "'1234.5678',"
	     "'$567.89',"
	     "'char column',"
	     "X'07ff656667',"
	     "%s,"
	     "%s,"
	     "%s,"
	     "'%s',"
	     "'%s',"
	     "'%s',"
	     "'',"
	     "NULL,"
	     "%s,"
	     "'%s',"
 	     "'31-dec-2001 23:59:59',"
	     "'31-dec-2001',"
	     "'23:59:59',"
	     "NEXT VALUE FOR test_datatypes_id_seq)",
	     driver_quoted_string, 
	     my_string_to_quote,
	     quoted_string,
	     my_string_to_escape,
	     escaped_string,
	     numstring,
	     quoted_binary,
	     escaped_binary);
  }
  else if(!strcmp(ptr_cinfo->drivername, "msql")) {
    snprintf(query, QUERY_LEN, "INSERT INTO test_datatypes VALUES ("
	     "-127,"
	     "127,"
	     "-32767,"
	     "32767,"
	     "-2147483647,"
	     "2147483647,"
	     "-9223372036854775807,"
	     "9223372036854775807,"
	     "3.402823466E+38,"
	     "%s,"
	     "%s,"
	     "%s,"
	     "'%s',"
	     "'%s',"
	     "'%s',"
	     "'',"
	     "NULL,"
	     "'11-jul-1977',"
	     "'23:59:59',"
	     "NULL)",
	     driver_quoted_string,
	     my_string_to_quote,
	     quoted_string,
	     my_string_to_escape,
	     escaped_string,
	     numstring);
  }
  else { /* mysql, pgsql, sqlite, sqlite3 */
    snprintf(query, QUERY_LEN, "INSERT INTO test_datatypes ("
	     "the_char,"
	     "the_uchar,"
	     "the_short,"
	     "the_ushort,"
	     "the_long,"
	     "the_ulong,"
	     "the_longlong,"
	     "the_ulonglong,"
	     "the_float,"
	     "the_double,"
	     "the_driver_string,"
	     "the_conn_quoted_string,"
	     "the_conn_quoted_string_copy,"
	     "the_conn_escaped_string,"
	     "the_conn_escaped_string_copy,"
	     "the_numstring,"
	     "the_empty_string,"
	     "the_null_string,"
	     "the_binary_quoted_string,"
	     "the_binary_escaped_string,"
	     "the_datetime,"
	     "the_datetime_tz,"
	     "the_date,"
	     "the_time,"
	     "the_time_tz) VALUES ("
	     "-127,"
	     "127,"
	     "-32768,"
	     "32767,"
	     "-2147483648,"
	     "2147483647,"
	     "-9223372036854775807,"
	     "9223372036854775807,"
	     "3.402823466E+38,"
	     "1.7976931348623157E+307,"
	     "%s,"
	     "%s,"
	     "%s,"
	     "'%s',"
	     "'%s',"
	     "'%s',"
	     "'',"
	     "NULL,"
	     "%s,"
	     "'%s',"
	     "'2001-12-31 23:59:59',"
	     "'2001-12-31 23:59:59 -10:00',"
	     "'2001-12-31',"
	     "'23:59:59',"
	     "'23:59:59-10:00')",
	     driver_quoted_string, 
	     my_string_to_quote,
	     quoted_string,
	     my_string_to_escape,
	     escaped_string,
	     numstring,
	     quoted_binary,
	     escaped_binary);
  }

  free(my_string_to_quote);
  free(my_string_to_escape);
  if (driver_quoted_string) {
    free(driver_quoted_string);
  }
  if (quoted_string) {
    free(quoted_string);
  }
  if (escaped_string) {
    free(escaped_string);
  }
  if (quoted_binary) {
    free(quoted_binary);
  }
  if (escaped_binary) {
    free(escaped_binary);
  }

  if ((result = dbi_conn_query(conn, query)) == NULL) {
    dbi_conn_error(conn, &errmsg);
    printf("\tAAH! Can't insert data! Error message: %s\n", errmsg);
    return 1;
  }
  else {
    printf("\tOk.\n");
  }
  dbi_result_free(result);

  /* check autoincrement column values */
  if (!strcmp(ptr_cinfo->drivername, "pgsql")
   || !strcmp(ptr_cinfo->drivername, "ingres")) {
    n_last_id = dbi_conn_sequence_last(conn, "test_datatypes_id_seq");
    n_next_id = dbi_conn_sequence_next(conn, "test_datatypes_id_seq");
  } else if (!strcmp(ptr_cinfo->drivername, "firebird")) {
    //We have not setup any sequences yet?!?
    return 0;
  }
  else {
    n_last_id = dbi_conn_sequence_last(conn, NULL);
    n_next_id = dbi_conn_sequence_next(conn, NULL);
  }

  printf("\tLength of quoted binary: %d\n\tLast ID: %llu, next ID %llu\n", quoted_binary_length, n_last_id, n_next_id);

  return 0;
}

/* returns 0 on success, 1 on error */
int test_retrieve_data(struct CONNINFO* ptr_cinfo, struct TABLEINFO* ptr_tinfo, dbi_conn conn) {
  const char *errmsg;
  dbi_result result;

  if ((result = dbi_conn_query(conn, "SELECT * from test_datatypes")) == NULL) {
    dbi_conn_error(conn, &errmsg);
    printf("\tAAH! Can't get read data! Error message: %s\n", errmsg);
    return 1;
  }

  printf("\tGot result, try to access rows\n");

  while (dbi_result_next_row(result)) {
    const char *errmsg = NULL;
    signed char the_char = 0;
    unsigned char the_uchar = 0;
    short the_short = 0;
    unsigned short the_ushort = 0;
    long the_long = 0;
    unsigned long the_ulong = 0;
    long long the_longlong = 0;
    unsigned long long the_ulonglong = 0;
    float the_float = 0;
    double the_double = 0;
    const char* the_driver_string;
    const char* the_quoted_string;
    const char* the_quoted_string_copy;
    const char* the_escaped_string;
    const char* the_escaped_string_copy;
    const char* the_empty_string;
    const char* the_null_string;
    const unsigned char* the_quoted_binary;
    unsigned char* the_escaped_binary_copy;
    time_t the_datetime;
    time_t the_datetime_tz;
    time_t the_date_dt;
    time_t the_date_dt_tz;
    time_t the_time_dt;
    time_t the_time_dt_tz;
    struct tm* ptr_tm;
    struct tm* ptr_tm_date;
    struct tm* ptr_tm_time;
    const char *the_date;
    const char *the_time;
    int year_dt = 0;
    int mon_dt = 0;
    int day_dt = 0;
    int hour_dt = 0;
    int min_dt = 0;
    int sec_dt = 0;
    int year_dt_tz = 0;
    int mon_dt_tz = 0;
    int day_dt_tz = 0;
    int hour_dt_tz = 0;
    int min_dt_tz = 0;
    int sec_dt_tz = 0;
    int year = 0;
    int mon = 0;
    int day = 0;
    int hour = 0;
    int min = 0;
    int sec = 0;
    int hour_tz = 0;
    int min_tz = 0;
    int sec_tz = 0;
    int i;
    unsigned int the_driver_string_length = 0;
    unsigned int the_quoted_string_length = 0;
    unsigned int the_quoted_string_copy_length = 0;
    unsigned int the_escaped_string_length = 0;
    unsigned int the_escaped_string_copy_length = 0;
    unsigned int the_empty_string_length = 0;
    unsigned int the_null_string_length = 0;
    unsigned int the_quoted_binary_length = 0;
    unsigned int the_escaped_binary_length = 0;

    dbi_error_flag errflag;

    /* first retrieve the values */
    the_char = dbi_result_get_char(result, "the_char");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_char errflag=%s\n", errmsg);
    }

    the_uchar = dbi_result_get_uchar(result, "the_uchar");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_uchar errflag=%s\n", errmsg);
    }

    the_short = dbi_result_get_short(result, "the_short");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_short errflag=%s\n", errmsg);
    }

    the_ushort = dbi_result_get_ushort(result, "the_ushort");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_ushort errflag=%s\n", errmsg);
    }

    /* the _long function (instead of _int) is used here intentionally
       to verify that the "deprecated" message shows up during
       compilation */
    the_long = dbi_result_get_long(result, "the_long");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_long errflag=%s\n", errmsg);
    }

    the_ulong = dbi_result_get_ulong(result, "the_ulong");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_ulong errflag=%s\n", errmsg);
    }


    if (!ptr_tinfo->have_longlong) {
      printf("the_longlong: test skipped for this driver.\n");
    }
    else {
      the_longlong = dbi_result_get_longlong(result, "the_longlong");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_longlong errflag=%s\n", errmsg);
      }
    }

    if (!ptr_tinfo->have_ulonglong) {
      printf("the_ulonglong: test skipped for this driver.\n");
    }
    else {
      the_ulonglong = dbi_result_get_ulonglong(result, "the_ulonglong");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_ulonglong errflag=%s\n", errmsg);
      }
    }

    the_float = dbi_result_get_float(result, "the_float");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_float errflag=%s\n", errmsg);
    }

    if(!ptr_tinfo->have_double) {
      printf("the_double: test skipped for this driver.\n");
    }
    else {
      the_double = dbi_result_get_double(result, "the_double");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_double errflag=%s\n", errmsg);
      }
    }
    the_driver_string = dbi_result_get_string(result, "the_driver_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_driver_string errflag=%s\n", errmsg);
    }
    the_quoted_string = dbi_result_get_string(result, "the_conn_quoted_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_conn_quoted_string errflag=%s\n", errmsg);
    }

    the_quoted_string_copy = dbi_result_get_string_copy(result, "the_conn_quoted_string_copy");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_conn_quoted_string_copy errflag=%s\n", errmsg);
    }

    the_escaped_string = dbi_result_get_string(result, "the_conn_escaped_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_conn_escaped_string errflag=%s\n", errmsg);
    }

    the_escaped_string_copy = dbi_result_get_string_copy(result, "the_conn_escaped_string_copy");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_conn_escaped_string_copy errflag=%s\n", errmsg);
    }

    the_empty_string = dbi_result_get_string(result, "the_empty_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_empty_string errflag=%s\n", errmsg);
    }

    the_null_string = dbi_result_get_string(result, "the_null_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_null_string errflag=%s\n", errmsg);
    }

    printf("\tthis should cause a column type mismatch...\n");
    dbi_result_get_string(result, "the_long");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("type mismatch errflag=%s\n", errmsg);
    }

    printf("\tthis should cause a bad name error...\n");
    dbi_result_get_string(result, "the_nonexistent");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("bad name errflag=%s\n",errmsg);
    }

    the_quoted_binary = dbi_result_get_binary(result, "the_binary_quoted_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_binary_string errflag=%s\n", errmsg);
    }

    the_escaped_binary_copy = dbi_result_get_binary_copy(result, "the_binary_escaped_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_binary_copy errflag=%s\n", errmsg);
    }
        
    if(!ptr_tinfo->have_datetime) {
      printf("the_datetime: test skipped for this driver.\n");
    }
    else {
      the_datetime = dbi_result_get_datetime(result, "the_datetime");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_datetime errflag=%s\n", errmsg);
      }
      ptr_tm = gmtime(&the_datetime);
      year_dt = ptr_tm->tm_year+1900;
      mon_dt = ptr_tm->tm_mon+1;
      day_dt = ptr_tm->tm_mday;
      hour_dt = ptr_tm->tm_hour;
      min_dt = ptr_tm->tm_min;
      sec_dt = ptr_tm->tm_sec;
    }

    if(!ptr_tinfo->have_datetime_tz) {
      printf("the_datetime_tz: test skipped for this driver.\n");
    }
    else {
      the_datetime_tz = dbi_result_get_datetime(result, "the_datetime_tz");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_datetime_tz errflag=%s\n", errmsg);
      }
      ptr_tm = gmtime(&the_datetime_tz);
      year_dt_tz = ptr_tm->tm_year+1900;
      mon_dt_tz = ptr_tm->tm_mon+1;
      day_dt_tz = ptr_tm->tm_mday;
      hour_dt_tz = ptr_tm->tm_hour;
      min_dt_tz = ptr_tm->tm_min;
      sec_dt_tz = ptr_tm->tm_sec;
    }

    /* then retrieve the field lengths */
    the_driver_string_length = dbi_result_get_field_length(result, "the_driver_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_driver_string_length errflag=%s\n", errmsg);
    }

    the_quoted_string_length = dbi_result_get_field_length(result, "the_conn_quoted_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_conn_quoted_string_length errflag=%s\n", errmsg);
    }

    the_escaped_string_length = dbi_result_get_field_length(result, "the_conn_escaped_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_conn_escaped_string_length errflag=%s\n", errmsg);
    }

    the_empty_string_length = dbi_result_get_field_length(result, "the_empty_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_empty_string_length errflag=%s\n", errmsg);
    }

    the_quoted_binary_length = dbi_result_get_field_length(result, "the_binary_quoted_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_binary_quoted_string_length errflag=%s\n", errmsg);
    }

    the_escaped_binary_length = dbi_result_get_field_length(result, "the_binary_escaped_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_binary_escaped_string_length errflag=%s\n", errmsg);
    }

    if(!strcmp(ptr_cinfo->drivername, "msql")) {
      the_date = dbi_result_get_string(result, "the_date");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_date errflag=%s\n", errmsg);
      }
			
      the_time = dbi_result_get_string(result, "the_time");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_time errflag=%s\n", errmsg);
      }
			
      printf("the_char: in:-127 out:%d<<\nthe_uchar: in:127 out:%u<<\n"
	     "the_short: in:-32767 out:%hd<<\nthe_ushort: in:32767 out:%hu<<\n"
	     "the_long: in:-2147483647 out:%ld<<\nthe_ulong: in:2147483647 out:%lu<<\n"
	     "the_longlong: in:-9223372036854775807 out:%lld<<\nthe_ulonglong: in:9223372036854775807 out:%llu<<\n"
	     "the_float: in:3.402823466E+38 out:%e<<\n"
	     "the_driver_string: in:\'%s\' out:\'%s\'<<\n"
	     "the_quoted_string: in:\'%s\' out:\'%s\'<<\n"
	     "the_quoted_string_copy: in:\'%s\' out:\'%s\'<<\n"
	     "the_escaped_string: in:\'%s\' out:\'%s\'<<\n"
	     "the_escaped_string_copy: in:\'%s\' out:\'%s\'<<\n"
	     "the_empty_string: out:\'%s\'\n"
	     "the_null_string: out:\'%s\'\n"
	     "the_date: in:\'11-jul-1977\' out: %s<<\n"
	     "the_time: in:\'23:59:59\' out: %s<<", 
	     (signed int)the_char,
	     (unsigned int)the_uchar, 
	     the_short,
	     the_ushort,
	     the_long,
	     the_ulong,
	     the_longlong,
	     the_ulonglong, 
	     the_float,
	     string_to_quote,
	     the_driver_string,
	     string_to_quote, 
	     the_quoted_string,
	     string_to_quote, 
	     the_quoted_string_copy,
	     string_to_escape, 
	     the_escaped_string,
	     string_to_escape, 
	     the_escaped_string_copy,
	     the_empty_string,
	     the_null_string,
	     the_date,
	     the_time);

    }
    else { /* not msql */

      the_date_dt = dbi_result_get_datetime(result, "the_date");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_date errflag=%s\n", errmsg);
      }
			
      the_time_dt = dbi_result_get_datetime(result, "the_time");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_time errflag=%s\n", errmsg);
      }

      if (!ptr_tinfo->have_time_tz) {
	printf("time_tz: test skipped for this driver\n");
      }
      else {
	the_time_dt_tz = dbi_result_get_datetime(result, "the_time_tz");
	errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
	if (errflag) {
	  printf("the_time_tz errflag=%s\n", errmsg);
	}
/* 	printf("type went to %d, attribs went to %d\n", dbi_result_get_field_type(result, "the_time_tz"), dbi_result_get_field_attribs(result, "the_time_tz")); */
	ptr_tm_time = gmtime(&the_time_dt_tz);
	hour_tz = ptr_tm_time->tm_hour;
	min_tz = ptr_tm_time->tm_min;
	sec_tz = ptr_tm_time->tm_sec;
      }
			
      ptr_tm_date = gmtime(&the_date_dt);
      year = ptr_tm_date->tm_year+1900;
      mon = ptr_tm_date->tm_mon+1;
      day = ptr_tm_date->tm_mday;
	
      ptr_tm_time = gmtime(&the_time_dt);
      hour = ptr_tm_time->tm_hour;
      min = ptr_tm_time->tm_min;
      sec = ptr_tm_time->tm_sec;

      if (!strcmp(ptr_cinfo->drivername, "firebird")) {
	printf("the_short: in:-32768 out:%hd<<\n"
	       "the_ushort: in:32767 out:%hu<<\n"
	       "the_long: in:-2147483648 out:%ld<<\n"
	       "the_ulong: in:2147483647 out:%lu<<\n"
	       "the_float: in:3.4E+37 out:%e<<\n"
	       "the_double: in:1.7E+307 out:%e\n"
	       "the_driver_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_quoted_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_quoted_string_copy: in:\'%s\' out:\'%s\'<<\n"
	       "the_escaped_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_escaped_string_copy: in:\'%s\' out:\'%s\'<<\n"
	       "the_empty_string: out:\'%s\'<<\n"
	       "the_null_string: out:\'%s\'\n"
	       "the_datetime: in:\'2001-12-31 23:59:59\' out:%d-%d-%d %d:%d:%d\n"
	       "the_date: in:\'2001-12-31\' out:%d-%d-%d\n"
	       "the_time: in:\'23:59:59\' out:%d:%d:%d\n",
	       the_short,
	       the_ushort,
	       the_long,
	       the_ulong,
	       the_float,
	       the_double, 
	       string_to_quote,
	       the_driver_string,
	       string_to_quote, 
	       the_quoted_string,
	       string_to_quote, 
	       the_quoted_string_copy,
	       string_to_escape, 
	       the_escaped_string,
	       string_to_escape, 
	       the_escaped_string_copy,
	       the_empty_string,
	       the_null_string, 
	       year_dt,
	       mon_dt,
	       day_dt,
	       hour_dt,
	       min_dt,
	       sec_dt,
	       year,
	       mon,
	       day,
	       hour,
	       min,
	       sec);
      }
      else {
	printf("the_char: in:-127 out:%d<<\n"
	       "the_uchar: in:127 out:%u<<\n"
	       "the_short: in:-32768 out:%hd<<\n"
	       "the_ushort: in:32767 out:%hu<<\n"
	       "the_long: in:-2147483648 out:%ld<<\n"
	       "the_ulong: in:2147483647 out:%lu<<\n"
	       "the_longlong: in:-9223372036854775807 out:%lld<<\n"
	       "the_ulonglong: in:9223372036854775807 out:%llu<<\n"
	       "the_float: in:3.402823466E+38 out:%e<<\n"
	       "the_double: in:1.7976931348623157E+307 out:%e<<\n"
	       "the_driver_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_quoted_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_quoted_string_copy: in:\'%s\' out:\'%s\'<<\n"
	       "the_escaped_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_escaped_string_copy: in:\'%s\' out:\'%s\'<<\n"
	       "the_empty_string: out:\'%s\'<<\n"
	       "the_null_string: out:\'%s\'\n"
	       "the_datetime: in:\'2001-12-31 23:59:59\' out:%d-%d-%d %d:%d:%d\n"
	       "the_datetime_tz: in:\'2001-12-31 23:59:59 -10:00\' out:%d-%d-%d %d:%d:%d\n"
	       "the_date: in:\'2001-12-31\' out:%d-%d-%d\n"
	       "the_time: in:\'23:59:59\' out:%d:%d:%d\n"
	       "the_time_tz: in:\'23:59:59-10:00\' out:%d:%d:%d\n",
	       (signed int)the_char,
	       (unsigned int)the_uchar,
	       the_short,
	       the_ushort, 
	       the_long,
	       the_ulong,
	       the_longlong,
	       the_ulonglong,
	       the_float,
	       the_double, 
	       string_to_quote,
	       the_driver_string,
	       string_to_quote, 
	       the_quoted_string,
	       string_to_quote, 
	       the_quoted_string_copy,
	       string_to_escape, 
	       the_escaped_string,
	       string_to_escape, 
	       the_escaped_string_copy,
	       the_empty_string,
	       the_null_string,
	       year_dt,
	       mon_dt,
	       day_dt,
	       hour_dt,
	       min_dt,
	       sec_dt, 
	       year_dt_tz,
	       mon_dt_tz,
	       day_dt_tz,
	       hour_dt_tz,
	       min_dt_tz,
	       sec_dt_tz, 
	       year,
	       mon,
	       day,
	       hour,
	       min,
	       sec,
	       hour_tz,
	       min_tz,
	       sec_tz);
      }
    }
    
    printf("the_quoted_binary: in: ");
    for (i = 0; i < binary_to_quote_length; i++) {
      printf("%d-", binary_to_quote[i]);
    }

    printf(" out: ");
    for (i = 0; i < the_quoted_binary_length; i++) {
      printf("%d-", the_quoted_binary[i]);
    }

    printf("<<\n");

    printf("the_escaped_binary_copy: in: ");
    for (i = 0; i < binary_to_quote_length; i++) {
      printf("%d-", binary_to_quote[i]);
    }

    printf(" out: ");
    for (i = 0; i < the_escaped_binary_length; i++) {
      printf("%d-", the_escaped_binary_copy[i]);
    }

    printf("<<\n");

    printf("\n\nfield lengths:\n"
    	   "the_driver_string_length %d\n"
	   "the_quoted_string_length %d\n"
    	   "the_empty_string_length %d\n"
	   "the_null_string_length %d\n"
    	   "the_quoted_binary_length %d\n"
	   "the_escaped_binary_length %d\n\n", 
	   the_driver_string_length,
	   the_quoted_string_length,
	   the_empty_string_length,
	   the_null_string_length,
	   the_quoted_binary_length,
	   the_escaped_binary_length);

  } /* end while */

  /* todo: check field sizes */
  dbi_result_free(result);

  return 0;
}

/* returns 0 on success, 1 on error */
int test_retrieve_zero_rows(struct CONNINFO* ptr_cinfo, struct TABLEINFO* ptr_tinfo, dbi_conn conn) {
  const char *errmsg;
  dbi_result result;
  int numfields;

  if ((result = dbi_conn_query(conn, "SELECT * from test_datatypes WHERE 0=1")) == NULL) {
    dbi_conn_error(conn, &errmsg);
    printf("\tAAH! Can't get read data! Error message: %s\n", errmsg);
    return 1;
  }

  numfields = dbi_result_get_numfields(result);

  if (numfields == DBI_FIELD_ERROR) {
    dbi_conn_error(conn, &errmsg);
    printf("\tAAH! Can't access data! Error message: %s\n", errmsg);
    return 1;
  }

  printf("\ttest_datatypes contains %d columns\n", numfields);

  return 0;
}

/* returns 0 on success, 1 on error */
int test_retrieve_data_as(struct CONNINFO* ptr_cinfo, struct TABLEINFO* ptr_tinfo, dbi_conn conn) {
  const char *errmsg;
  dbi_result result;

  if ((result = dbi_conn_query(conn, "SELECT * from test_datatypes")) == NULL) {
    dbi_conn_error(conn, &errmsg);
    printf("\tAAH! Can't get read data! Error message: %s\n", errmsg);
    return 1;
  }

  printf("\tGot result, try to access rows\n");

  while (dbi_result_next_row(result)) {
    const char *errmsg = NULL;
    char* the_char_as_string = NULL;
    char* the_uchar_as_string = NULL;
    char* the_short_as_string = NULL;
    char* the_ushort_as_string = NULL;
    char* the_long_as_string = NULL;
    char* the_ulong_as_string = NULL;
    char* the_longlong_as_string = NULL;
    char* the_ulonglong_as_string = NULL;
    char* the_float_as_string = NULL;
    char* the_double_as_string = NULL;
    char* the_string_as_string = NULL;
    char* the_numstring_as_string = NULL;
    char* the_empty_string_as_string = NULL;
    char* the_null_string_as_string = NULL;
    char* the_binary_as_string = NULL;
    char* the_date_as_string = NULL;
    char* the_time_as_string = NULL;
    char* the_datetime_as_string = NULL;
    long long the_char_as_ll = 0;
    long long the_uchar_as_ll = 0;
    long long the_short_as_ll = 0;
    long long the_ushort_as_ll = 0;
    long long the_long_as_ll = 0;
    long long the_ulong_as_ll = 0;
    long long the_longlong_as_ll = 0;
    long long the_ulonglong_as_ll = 0;
    long long the_float_as_ll = 0;
    long long the_double_as_ll = 0;
    long long the_string_as_ll = 0;
    long long the_numstring_as_ll = 0;
    long long the_empty_string_as_ll = 0;
    long long the_null_string_as_ll = 0;
    long long the_binary_as_ll = 0;
    long long the_date_as_ll = 0;
    long long the_time_as_ll = 0;
    long long the_datetime_as_ll = 0;

    dbi_error_flag errflag;

    /* first retrieve the values */
    the_char_as_string = dbi_result_get_as_string_copy(result, "the_char");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_char errflag=%s\n", errmsg);
    }

    the_char_as_ll = dbi_result_get_as_longlong(result, "the_char");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_char errflag=%s\n", errmsg);
    }

    the_uchar_as_string = dbi_result_get_as_string_copy(result, "the_uchar");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_uchar errflag=%s\n", errmsg);
    }

    the_uchar_as_ll = dbi_result_get_as_longlong(result, "the_uchar");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_uchar errflag=%s\n", errmsg);
    }

    the_short_as_string = dbi_result_get_as_string_copy(result, "the_short");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_short errflag=%s\n", errmsg);
    }

    the_short_as_ll = dbi_result_get_as_longlong(result, "the_short");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_short errflag=%s\n", errmsg);
    }

    the_ushort_as_string = dbi_result_get_as_string_copy(result, "the_ushort");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_ushort errflag=%s\n", errmsg);
    }

    the_ushort_as_ll = dbi_result_get_as_longlong(result, "the_ushort");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_ushort errflag=%s\n", errmsg);
    }

    the_long_as_string = dbi_result_get_as_string_copy(result, "the_long");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_long errflag=%s\n", errmsg);
    }

    the_long_as_ll = dbi_result_get_as_longlong(result, "the_long");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_long errflag=%s\n", errmsg);
    }

    the_ulong_as_string = dbi_result_get_as_string_copy(result, "the_ulong");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_ulong errflag=%s\n", errmsg);
    }

    the_ulong_as_ll = dbi_result_get_as_longlong(result, "the_ulong");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_ulong errflag=%s\n", errmsg);
    }


    if (!ptr_tinfo->have_longlong) {
      printf("the_longlong: test skipped for this driver.\n");
    }
    else {
      the_longlong_as_string = dbi_result_get_as_string_copy(result, "the_longlong");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_longlong errflag=%s\n", errmsg);
      }
      the_longlong_as_ll = dbi_result_get_as_longlong(result, "the_longlong");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_longlong errflag=%s\n", errmsg);
      }
    }

    if (!ptr_tinfo->have_ulonglong) {
      printf("the_ulonglong: test skipped for this driver.\n");
    }
    else {
      the_ulonglong_as_string = dbi_result_get_as_string_copy(result, "the_ulonglong");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_ulonglong errflag=%s\n", errmsg);
      }
      the_ulonglong_as_ll = dbi_result_get_as_longlong(result, "the_ulonglong");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_ulonglong errflag=%s\n", errmsg);
      }
    }

    the_float_as_string = dbi_result_get_as_string_copy(result, "the_float");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_float errflag=%s\n", errmsg);
    }

    the_float_as_ll = dbi_result_get_as_longlong(result, "the_float");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_float errflag=%s\n", errmsg);
    }

    if(!ptr_tinfo->have_double) {
      printf("the_double: test skipped for this driver.\n");
    }
    else {
      the_double_as_string = dbi_result_get_as_string_copy(result, "the_double");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_double errflag=%s\n", errmsg);
      }
      the_double_as_ll = dbi_result_get_as_longlong(result, "the_double");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_double errflag=%s\n", errmsg);
      }
    }

    the_string_as_string = dbi_result_get_as_string_copy(result, "the_conn_quoted_string_copy");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_conn_quoted_string_copy errflag=%s\n", errmsg);
    }

    the_string_as_ll = dbi_result_get_as_longlong(result, "the_conn_quoted_string_copy");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_conn_quoted_string_copy errflag=%s\n", errmsg);
    }

    the_numstring_as_string = dbi_result_get_as_string_copy(result, "the_numstring");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_conn_quoted_string_copy errflag=%s\n", errmsg);
    }

    the_numstring_as_ll = dbi_result_get_as_longlong(result, "the_numstring");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_conn_quoted_string_copy errflag=%s\n", errmsg);
    }

    the_empty_string_as_string = dbi_result_get_as_string_copy(result, "the_empty_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_empty_string errflag=%s\n", errmsg);
    }

    the_empty_string_as_ll = dbi_result_get_as_longlong(result, "the_empty_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_empty_string errflag=%s\n", errmsg);
    }

    the_null_string_as_string = dbi_result_get_as_string_copy(result, "the_null_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_null_string errflag=%s\n", errmsg);
    }

    the_null_string_as_ll = dbi_result_get_as_longlong(result, "the_null_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_null_string errflag=%s\n", errmsg);
    }

    the_binary_as_string = dbi_result_get_as_string_copy(result, "the_binary_quoted_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_binary_string errflag=%s\n", errmsg);
    }

    the_binary_as_ll = dbi_result_get_as_longlong(result, "the_binary_quoted_string");
    errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
    if (errflag) {
      printf("the_binary_string errflag=%s\n", errmsg);
    }

    if(!ptr_tinfo->have_datetime) {
      printf("the_datetime: test skipped for this driver.\n");
    }
    else {
      the_datetime_as_string = dbi_result_get_as_string_copy(result, "the_datetime");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_datetime errflag=%s\n", errmsg);
      }
      the_datetime_as_ll = dbi_result_get_as_longlong(result, "the_datetime");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_datetime errflag=%s\n", errmsg);
      }
    }

    if(!strcmp(ptr_cinfo->drivername, "msql")) {
      the_date_as_string = dbi_result_get_as_string_copy(result, "the_date");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_date errflag=%s\n", errmsg);
      }
			
      the_date_as_ll = dbi_result_get_as_longlong(result, "the_date");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_date errflag=%s\n", errmsg);
      }
			
      the_time_as_string = dbi_result_get_as_string_copy(result, "the_time");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_time errflag=%s\n", errmsg);
      }

      the_time_as_ll = dbi_result_get_as_longlong(result, "the_time");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_time errflag=%s\n", errmsg);
      }
			
      printf("the_char_as_string: in:-127 out:%s<<\n"
	     "the_uchar_as_string: in:127 out:%s<<\n"
	     "the_short_as_string: in:-32767 out:%s<<\n"
	     "the_ushort_as_string: in:32767 out:%s<<\n"
	     "the_long_as_string: in:-2147483647 out:%s<<\n"
	     "the_ulong_as_string: in:2147483647 out:%s<<\n"
	     "the_longlong_as_string: in:-9223372036854775807 out:%s<<\n"
	     "the_ulonglong_as_string: in:9223372036854775807 out:%s<<\n"
	     "the_float_as_string: in:3.402823466E+38 out:%s<<\n"
	     "the_string_as_string: in:\'%s\' out:\'%s\'<<\n"
	     "the_numstring_as_string: in:\'%s\' out:\'%s\'<<\n"
	     "the_empty_string_as_string: out:\'%s\'\n"
	     "the_null_string_as_string: out:\'%s\'\n"
	     "the_date_as_string: in:\'11-jul-1977\' out: %s<<\n"
	     "the_time_as_string: in:\'23:59:59\' out: %s<<", 
	     "the_char_as_ll: in:-127 out:%lld<<\n"
	     "the_uchar_as_ll: in:127 out:%lld<<\n"
	     "the_short_as_ll: in:-32767 out:%lld<<\n"
	     "the_ushort_as_ll: in:32767 out:%lld<<\n"
	     "the_long_as_ll: in:-2147483647 out:%lld<<\n"
	     "the_ulong_as_ll: in:2147483647 out:%lld<<\n"
	     "the_longlong_as_ll: in:-9223372036854775807 out:%lld<<\n"
	     "the_ulonglong_as_ll: in:9223372036854775807 out:%lld<<\n"
	     "the_float_as_ll: in:3.402823466E+38 out:%lld<<\n"
	     "the_string_as_ll: in:\'%s\' out:%lld<<\n"
	     "the_numstring_as_ll: in:\'%s\' out:%lld<<\n"
	     "the_empty_string_as_ll: out:%lld\n"
	     "the_null_string_as_ll: out:%lld\n"
	     "the_date_as_ll: in:\'11-jul-1977\' out: %lld<<\n"
	     "the_time_as_ll: in:\'23:59:59\' out: %lld<<", 
	     the_char_as_string,
	     the_uchar_as_string, 
	     the_short_as_string,
	     the_ushort_as_string,
	     the_long_as_string,
	     the_ulong_as_string,
	     the_longlong_as_string,
	     the_ulonglong_as_string, 
	     the_float_as_string,
	     string_to_quote,
	     the_string_as_string,
	     numstring,
	     the_numstring_as_string,
	     the_empty_string_as_string,
	     the_null_string_as_string,
	     the_date_as_string,
	     the_time_as_string,
	     the_char_as_string,
	     the_uchar_as_ll, 
	     the_short_as_ll,
	     the_ushort_as_ll,
	     the_long_as_ll,
	     the_ulong_as_ll,
	     the_longlong_as_ll,
	     the_ulonglong_as_ll, 
	     the_float_as_ll,
	     string_to_quote,
	     the_string_as_ll,
	     numstring,
	     the_numstring_as_ll,
	     the_empty_string_as_ll,
	     the_null_string_as_ll,
	     the_date_as_ll,
	     the_time_as_ll);

    }
    else { /* not msql */
      the_date_as_string = dbi_result_get_as_string_copy(result, "the_date");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_date errflag=%s\n", errmsg);
      }
			
      the_date_as_ll = dbi_result_get_as_longlong(result, "the_date");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_date errflag=%s\n", errmsg);
      }
			
      the_time_as_string = dbi_result_get_as_string_copy(result, "the_time");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_time errflag=%s\n", errmsg);
      }

      the_time_as_ll = dbi_result_get_as_longlong(result, "the_time");
      errflag = dbi_conn_error(dbi_result_get_conn(result), &errmsg);
      if (errflag) {
	printf("the_time errflag=%s\n", errmsg);
      }

      if (!strcmp(ptr_cinfo->drivername, "firebird")) {
	printf("the_short_as_string: in:-32768 out:%s<<\n"
	       "the_ushort_as_string: in:32767 out:%s<<\n"
	       "the_long_as_string: in:-2147483648 out:%s<<\n"
	       "the_ulong_as_string: in:2147483647 out:%s<<\n"
	       "the_float_as_string: in:3.4E+37 out:%s<<\n"
	       "the_double_as_string: in:1.7E+307 out:%s\n"
	       "the_string_as_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_numstring_as_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_empty_string_as_string: out:\'%s\'<<\n"
	       "the_null_string_as_string: out:\'%s\'\n"
	       "the_datetime_as_string: in:\'2001-12-31 23:59:59\' out:%s\n"
	       "the_date_as_string: in:\'2001-12-31\' out:%s\n"
	       "the_time_as_string: in:\'23:59:59\' out:%s\n",
	       "the_short_as_ll: in:-32768 out:%ld<<\n"
	       "the_ushort_as_ll: in:32767 out:%ld<<\n"
	       "the_long_as_ll: in:-2147483648 out:%ld<<\n"
	       "the_ulong_as_ll: in:2147483647 out:%ld<<\n"
	       "the_float_as_ll: in:3.4E+37 out:%ld<<\n"
	       "the_double_as_ll: in:1.7E+307 out:%ld\n"
	       "the_string_as_ll: in:\'%s\' out:%ld<<\n"
	       "the_numstring_as_ll: in:\'%s\' out:%ld<<\n"
	       "the_empty_string_as_ll: out:%ld<<\n"
	       "the_null_string_as_ll: out:%ld\n"
	       "the_datetime_as_ll: in:\'2001-12-31 23:59:59\' out:%ld\n"
	       "the_date_as_ll: in:\'2001-12-31\' out:%ld\n"
	       "the_time_as_ll: in:\'23:59:59\' out:%ld\n",
	       the_short_as_string,
	       the_ushort_as_string,
	       the_long_as_string,
	       the_ulong_as_string,
	       the_float_as_string,
	       the_double_as_string, 
	       string_to_quote,
	       the_string_as_string,
	       numstring, 
	       the_numstring_as_string,
	       the_empty_string_as_string,
	       the_null_string_as_string, 
	       the_datetime_as_string,
	       the_date_as_string,
	       the_time_as_string,
	       the_short_as_ll,
	       the_ushort_as_ll,
	       the_long_as_ll,
	       the_ulong_as_ll,
	       the_float_as_ll,
	       the_double_as_ll, 
	       string_to_quote,
	       the_string_as_ll,
	       numstring, 
	       the_numstring_as_ll,
	       the_empty_string_as_ll,
	       the_null_string_as_ll, 
	       the_datetime_as_ll,
	       the_date_as_ll,
	       the_time_as_ll);
      }
      else {
	printf("the_char_as_string: in:-127 out:%s<<\n"
	       "the_uchar_as_string: in:127 out:%s<<\n"
	       "the_short_as_string: in:-32768 out:%s<<\n"
	       "the_ushort_as_string: in:32767 out:%s<<\n"
	       "the_long_as_string: in:-2147483648 out:%s<<\n"
	       "the_ulong_as_string: in:2147483647 out:%s<<\n"
	       "the_longlong_as_string: in:-9223372036854775807 out:%s<<\n"
	       "the_ulonglong_as_string: in:9223372036854775807 out:%s<<\n"
	       "the_float_as_string: in:3.402823466E+38 out:%s<<\n"
	       "the_double_as_string: in:1.7976931348623157E+307 out:%s<<\n"
	       "the_string_as_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_numstring_as_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_empty_string_as_string: out:\'%s\'<<\n"
	       "the_null_string_as_string: out:\'%s\'\n"
	       "the_datetime_as_string: in:\'2001-12-31 23:59:59\' out:\'%s\'\n"
	       "the_date_as_string: in:\'2001-12-31\' out:\'%s\'\n"
	       "the_time_as_string: in:\'23:59:59\' out:\'%s\'\n"
	       "the_char_as_ll: in:-127 out:%lld<<\n"
	       "the_uchar_as_ll: in:127 out:%lld<<\n"
	       "the_short_as_ll: in:-32768 out:%lld<<\n"
	       "the_ushort_as_ll: in:32767 out:%lld<<\n"
	       "the_long_as_ll: in:-2147483648 out:%lld<<\n"
	       "the_ulong_as_ll: in:2147483647 out:%lld<<\n"
	       "the_longlong_as_ll: in:-9223372036854775807 out:%lld<<\n"
	       "the_ulonglong_as_ll: in:9223372036854775807 out:%lld<<\n"
	       "the_float_as_ll: in:3.402823466E+38 out:%lld<<\n"
	       "the_double_as_ll: in:1.7976931348623157E+307 out:%lld<<\n"
	       "the_string_as_ll: in:\'%s\' out:%lld<<\n"
	       "the_numstring_as_ll: in:\'%s\' out:%lld<<\n"
	       "the_empty_string_as_ll: out:%lld<<\n"
	       "the_null_string_as_ll: out:%lld\n"
	       "the_datetime_as_ll: in:\'2001-12-31 23:59:59\' out:%lld\n"
	       "the_date_as_ll: in:\'2001-12-31\' out:%lld\n"
	       "the_time_as_ll: in:\'23:59:59\' out:%lld\n",
	       the_char_as_string,
	       the_uchar_as_string,
	       the_short_as_string,
	       the_ushort_as_string, 
	       the_long_as_string,
	       the_ulong_as_string,
	       the_longlong_as_string,
	       the_ulonglong_as_string,
	       the_float_as_string,
	       the_double_as_string, 
	       string_to_quote,
	       the_string_as_string,
	       numstring, 
	       the_numstring_as_string,
	       the_empty_string_as_string,
	       the_null_string_as_string,
	       the_datetime_as_string,
	       the_date_as_string,
	       the_time_as_string,
	       the_char_as_ll,
	       the_uchar_as_ll,
	       the_short_as_ll,
	       the_ushort_as_ll, 
	       the_long_as_ll,
	       the_ulong_as_ll,
	       the_longlong_as_ll,
	       the_ulonglong_as_ll,
	       the_float_as_ll,
	       the_double_as_ll, 
	       string_to_quote,
	       the_string_as_ll,
	       numstring, 
	       the_numstring_as_ll,
	       the_empty_string_as_ll,
	       the_null_string_as_ll,
	       the_datetime_as_ll,
	       the_date_as_ll,
	       the_time_as_ll);
      } /* end if firebird */
    } /* end if msql */
    if (the_char_as_string) {
      free(the_char_as_string);
    }
    if (the_uchar_as_string) {
      free(the_uchar_as_string);
    }
    if (the_short_as_string) {
      free(the_short_as_string);
    }
    if (the_ushort_as_string) {
      free(the_ushort_as_string);
    }
    if (the_long_as_string) {
      free(the_long_as_string);
    }
    if (the_ulong_as_string) {
      free(the_ulong_as_string);
    }
    if (the_longlong_as_string) {
      free(the_longlong_as_string);
    }
    if (the_ulonglong_as_string) {
      free(the_ulonglong_as_string);
    }
    if (the_float_as_string) {
      free(the_float_as_string);
    }
    if (the_double_as_string) {
      free(the_double_as_string);
    }
    if (the_string_as_string) {
      free(the_string_as_string);
    }
    if (the_numstring_as_string) {
      free(the_numstring_as_string);
    }
    if (the_empty_string_as_string) {
      free(the_empty_string_as_string);
    }
    if (the_null_string_as_string) {
      free(the_null_string_as_string);
    }
    if (the_binary_as_string) {
      free(the_binary_as_string);
    }
    if (the_date_as_string) {
      free(the_date_as_string);
    }
    if (the_time_as_string) {
      free(the_time_as_string);
    }
    if (the_datetime_as_string) {
      free(the_datetime_as_string);
    }
  } /* end while */

  dbi_result_free(result);

  return 0;
}

/* returns 0 on success, 1 on error */
int test_drop_table(dbi_conn conn) {
  const char *errmsg;
  dbi_result result;

  if ((result = dbi_conn_query(conn, "DROP TABLE test_datatypes")) == NULL) {
    dbi_conn_error(conn, &errmsg);
    printf("\tAAH! Can't drop table! Error message: %s\n", errmsg);
    return 1;
  }
  else {
    printf("\tOk.\n");
  }
  dbi_result_free(result);
  return 0;
}

/* returns 0 on success, 1 on error */
int test_drop_db(struct CONNINFO* ptr_cinfo, dbi_conn conn) {
  const char *errmsg;
  dbi_result result;

  if (!strcmp(ptr_cinfo->drivername, "sqlite")
   || !strcmp(ptr_cinfo->drivername, "sqlite3")) {
    char dbpath[_POSIX_PATH_MAX];

    /* need this break to grant some time for db unlocking */
    printf("...hang on a second...\n");
    sleep(3);

    if (*(ptr_cinfo->dbdir)) {
      strcpy(dbpath, ptr_cinfo->dbdir);
    }
    else {
      if (!strcmp(ptr_cinfo->drivername, "sqlite")) {
	snprintf(dbpath, _POSIX_PATH_MAX, "%s/sqlite", default_dbdir);
      }
      else {
	snprintf(dbpath, _POSIX_PATH_MAX, "%s/sqlite3", default_dbdir);
      }
    }
    if (dbpath[strlen(dbpath)-1] != '/') {
      strcat(dbpath, "/");
    }
    strcat(dbpath, ptr_cinfo->dbname);

    if (unlink(dbpath)) {
      printf("AAH! Can't delete database file!\n");
      return 1;
    }
    printf("\tOk.\n");
  }
  else if (!strcmp(ptr_cinfo->drivername, "msql") 
  		|| !strcmp(ptr_cinfo->drivername, "firebird")
  		|| !strcmp(ptr_cinfo->drivername, "ingres")) {
    printf("\tThis is a no-op with the mSQL/Firebird/Ingres driver.\n");
		
  }
  
  else {
    if (dbi_conn_select_db(conn, ptr_cinfo->initial_dbname)) {
      dbi_conn_error(conn, &errmsg);
      printf("Uh-oh! Can't select database! Error message: %s\n", errmsg);
      return 1;
    }

    /* need this break to grant some time for db unlocking */
    printf("...hang on a second...\n");
    sleep(3);

    if ((result = dbi_conn_queryf(conn, "DROP DATABASE %s", 
				  ptr_cinfo->dbname)) == NULL) {
      dbi_conn_error(conn, &errmsg);
      printf("\tAAH! Can't drop database %s<< connected to database %s! Error message: %s\n", ptr_cinfo->dbname, ptr_cinfo->initial_dbname, errmsg);
      return 1;
    }
    else {
      printf("Ok.\n");
    }
		
    dbi_result_free(result);
  }
  return 0;
}

/* returns 0 on success, 1 on error */
int test_convert_encoding(struct CONNINFO* ptr_cinfo, dbi_conn conn) {
  char dbencoding[32] = "";

  if (!strcmp(ptr_cinfo->drivername, "mysql")) {
    strcpy(dbencoding, "latin1");
  }
  else if (!strcmp(ptr_cinfo->drivername, "pgsql")) {
    strcpy(dbencoding, "LATIN1");
  }

  printf("\tIANA encoding UTF-8 is known to the engine as %s\n", dbi_driver_encoding_from_iana(dbi_conn_get_driver(conn), "UTF-8"));

  if (*dbencoding) {
    printf("Engine encoding %s is known at IANA as %s\n", dbencoding, dbi_driver_encoding_to_iana(dbi_conn_get_driver(conn), dbencoding));
  }
  return 0;
}

/* returns 0 on success, 1 on error */
int test_error_messages(struct CONNINFO* ptr_cinfo, dbi_conn conn, int n) {
  char query[QUERY_LEN+1];
  const char *errmsg;
  int n_result;
  dbi_result result;

  if (n) {
    printf("\tThis is supposed to fail...\n");
  
    snprintf(query, QUERY_LEN, "%d SYNTAX ERROR", n);

    if ((result = dbi_conn_query(conn, query)) == NULL) {
      n_result = dbi_conn_error(conn, &errmsg);
      printf("\tHeck! The error number is %d; the message reads: %s\n", n_result, errmsg);
      return 0; /* an error is expected */
    }
    else {
      n_result = dbi_conn_error(conn, &errmsg);
      printf("\tOk. In this case, this is an error. The error number is %d; the message reads: %s\n", result, errmsg);
      dbi_result_free(result);
      return 1; /* lack of an error is an error here ... */
    }
  }
  else { /* n=0 */
    if (strcmp(ptr_cinfo->drivername, "firebird")) {
      printf("\tThis is supposed to succeed...\n");
  
      snprintf(query, QUERY_LEN, "SELECT %d", n);

      if ((result = dbi_conn_query(conn, query)) == NULL) {
	n_result = dbi_conn_error(conn, &errmsg);
	printf("\tHeck! The error number is %d; the message reads: %s\n", n_result, errmsg);
	return 1; /* there should be no error */
      }
      else {
	n_result = dbi_conn_error(conn, &errmsg);
	printf("\tOk. The error number is %d; the message reads: %s\n", n_result, errmsg);
	dbi_result_free(result);
	return 0; /* as expected */
      }
    }
    else {
      /* can't find a SQL statement that firebird executes willingly
	 without data */
      return 0;
    }
  }
}

/* fill tableinfo structure with suitable start values */
void init_tinfo(struct TABLEINFO* ptr_tinfo) {
  ptr_tinfo->have_double = 0;
  ptr_tinfo->have_longlong = 0;
  ptr_tinfo->have_ulonglong = 0;
  ptr_tinfo->have_datetime = 0;
  ptr_tinfo->have_datetime_tz = 0;
  ptr_tinfo->have_time_tz = 0;
}

/* convenience wrappers for recallable vs. legacy libdbi interface */
int my_dbi_initialize(const char *driverdir, dbi_inst *Inst) {
  if (n_legacy) {
    return dbi_initialize(driverdir);
  }
  else {
    return dbi_initialize_r(driverdir, Inst);
  }
}

void my_dbi_shutdown(dbi_inst Inst) {
  if (n_legacy) {
    dbi_shutdown();
  }
  else {
    dbi_shutdown_r(Inst);
  }
}

dbi_driver my_dbi_driver_list(dbi_driver Current, dbi_inst Inst) {
  if (n_legacy) {
    return dbi_driver_list(Current);
  }
  else {
    return dbi_driver_list_r(Current, Inst);
  }
}

dbi_conn my_dbi_conn_new(const char *name, dbi_inst Inst) {
  if (n_legacy) {
    return dbi_conn_new(name);
  }
  else {
    return dbi_conn_new_r(name, Inst);
  }
}
