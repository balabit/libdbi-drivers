#include <stdio.h>
#include <string.h>
#include <dbi/dbi.h>
#include <time.h>
#include <unistd.h>

#define QUERY_LEN 1024

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
};

char string_to_quote[] = "Can \'we\' \"quote\" this properly?";

unsigned char binary_to_quote[] = {'A', 'B', '\0', 'C', '\'', 'D'};
size_t binary_to_quote_length = 6;

int init_db(struct CONNINFO* ptr_cinfo);
int ask_for_conninfo(struct CONNINFO* ptr_cinfo);
int set_driver_options(struct CONNINFO* ptr_cinfo, dbi_conn conn, const char* encoding, const char* db);
int test_list_db(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int test_create_db(struct CONNINFO* ptr_cinfo, dbi_conn conn, const char* encoding);
int test_select_db(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int test_create_table(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int test_list_tables(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int test_insert_row(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int test_retrieve_data(struct CONNINFO* ptr_cinfo, dbi_conn conn);
int test_drop_table(dbi_conn conn);
int test_drop_db(struct CONNINFO* ptr_cinfo, dbi_conn conn);

int main(int argc, char **argv) {
  dbi_driver driver;
  dbi_conn conn;
  const char *errmsg;
  unsigned int dbengine_version;
  char versionstring[VERSIONSTRING_LENGTH];

  struct CONNINFO cinfo;

  if (ask_for_conninfo(&cinfo)) {
    exit(1);
  }

  if ((conn = dbi_conn_new(cinfo.drivername)) == NULL) {
    printf("Can't instantiate '%s' driver into a dbi_conn!\n", cinfo.drivername);
    dbi_shutdown();
    return 1;
  }

  driver = dbi_conn_get_driver(conn);
	
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
    dbi_shutdown();
    exit(1);
  }

  /* some drivers need an existing database to connect to */
  if (init_db(&cinfo)) {
    dbi_shutdown();
    exit(1);
  }

  if (dbi_conn_connect(conn) < 0) {
    dbi_conn_error(conn, &errmsg);
    printf("\nUnable to connect! Error message: %s\n", errmsg);
    dbi_shutdown();
    exit(1);
  }
  
  dbengine_version = dbi_conn_get_engine_version(conn);
  dbi_conn_get_engine_version_string(conn, versionstring);

  printf("\nSuccessfully connected!\n\tUsing database engine version %d (numeric) and %s (string)\n", dbengine_version, versionstring);

  /* Test 1: list available databases */
  printf("\nTest 1: List databases: \n");
	
  if (test_list_db(&cinfo, conn)) {
    dbi_conn_close(conn);
    dbi_shutdown();
    exit(1);
  }

  /* Test 2: create database */
  printf("\nTest 2: Create database %s using default encoding: \n", cinfo.dbname);
	
  if (test_create_db(&cinfo, conn, NULL)) {
    dbi_conn_close(conn);
    dbi_shutdown();
    exit(1);
  }

  /* Test 3: select database */
  printf("\nTest 3: Select database: \n");

  if (test_select_db(&cinfo, conn)) {
    dbi_conn_close(conn);
    dbi_shutdown();
    exit(1);
  }

  /* Test 4: get encoding */
  printf("\nTest 4: Get encoding: \n");
	
  printf("\tThe database encoding appears to be: %s\n", dbi_conn_get_encoding(conn));


  /* Test 5: create table */
  printf("\nTest 5: Create table: \n");
	
  if (test_create_table(&cinfo, conn)) {
    dbi_conn_close(conn);
    dbi_shutdown();
    exit(1);
  }

  /* Test 6: list tables */
  printf("\nTest 6: List tables: \n");
	
  if (test_list_tables(&cinfo, conn)) {
    dbi_conn_close(conn);
    dbi_shutdown();
    exit(1);
  }

  /* Test 7: insert row */
  printf("\nTest 7: Insert row: \n");

  if (test_insert_row(&cinfo, conn)) {
    dbi_conn_close(conn);
    dbi_shutdown();
    exit(1);
  }

  /* Test 8: retrieve data */
  printf("\nTest 8: Retrieve data: \n");
	
  if (test_retrieve_data(&cinfo, conn)) {
    dbi_conn_close(conn);
    dbi_shutdown();
    exit(1);
  }

  /* Test 9: drop table */
  printf("\nTest 9: Drop table: \n");
	
  if (test_drop_table(conn)) {
    dbi_conn_close(conn);
    dbi_shutdown();
    exit(1);
  }

  /* Test 10: drop database */
  printf("\nTest 10: Drop database:\n");
	
  if (test_drop_db(&cinfo, conn)) {
    dbi_conn_close(conn);
    dbi_shutdown();
    exit(1);
  }

  /* Test 11: test character encoding conversion */
  printf("\nTest 11: Character encoding conversion:\n");
	
  if (test_convert_encoding(&cinfo, conn)) {
    dbi_conn_close(conn);
    dbi_shutdown();
    exit(1);
  }


  /* we're done with this connection */
  dbi_conn_close(conn);
  conn = NULL;

  if ((!strcmp(cinfo.drivername, "mysql") && dbengine_version > 40100)
      ||!strcmp(cinfo.drivername, "pgsql")) {

    printf("\nNow run a couple of tests related to character encodings\nThe previous tests used the default encoding, if any. Now we try to connect using UTF-8 and create an UTF-8 database\n");
    if ((conn = dbi_conn_new(cinfo.drivername)) == NULL) {
      printf("Can't instantiate '%s' driver into a dbi_conn!\n", cinfo.drivername);
      dbi_shutdown();
      return 1;
    }

    driver = dbi_conn_get_driver(conn);
	
    if (set_driver_options(&cinfo, conn, "UTF-8", NULL)) {
      dbi_shutdown();
      exit(1);
    }

    if (dbi_conn_connect(conn) < 0) {
      dbi_conn_error(conn, &errmsg);
      printf("\nUnable to connect! Error message: %s\n", errmsg);
      dbi_shutdown();
      exit(1);
    }
	
    printf("\nSuccessfully connected using an UTF-8 encoding!\n");
	
    /* Test 12: create UTF-8 database */
    printf("\nTest 12: Create database %s with encoding UTF-8: \n", cinfo.dbname);
	
    if (test_create_db(&cinfo, conn, "UTF-8")) {
      dbi_conn_close(conn);
      dbi_shutdown();
      exit(1);
    }

    /* Test 13: select UTF-8 database */
    printf("\nTest 13: Select UTF-8 database: \n");

    if (test_select_db(&cinfo, conn)) {
      dbi_conn_close(conn);
      dbi_shutdown();
      exit(1);
    }

    /* Test 14: get encoding of UTF-8 database */
    printf("\nTest 14: Get encoding of UTF-8 database: \n");
	
    printf("The database encoding appears to be: %s\n", dbi_conn_get_encoding(conn));

    /* Test 15: drop UTF-8 database */
    printf("\nTest 15: Drop UTF-8 database: \n");
	
    if (test_drop_db(&cinfo, conn)) {
      dbi_conn_close(conn);
      dbi_shutdown();
      exit(1);
    }


    /* we're done with this connection */
    dbi_conn_close(conn);
    conn = NULL;

    /* repeat test for latin1 encoding */
    printf("\nNow repeat this test with a ISO-8859-1 encoding\n");
    if ((conn = dbi_conn_new(cinfo.drivername)) == NULL) {
      printf("Can't instantiate '%s' driver into a dbi_conn!\n", cinfo.drivername);
      dbi_shutdown();
      return 1;
    }

    driver = dbi_conn_get_driver(conn);
	
    if (set_driver_options(&cinfo, conn, "ISO-8859-1", NULL)) {
      dbi_shutdown();
      exit(1);
    }

    if (dbi_conn_connect(conn) < 0) {
      dbi_conn_error(conn, &errmsg);
      printf("\nUnable to connect! Error message: %s\n", errmsg);
      dbi_shutdown();
      exit(1);
    }
	
    printf("\nSuccessfully connected using a ISO-8859-1 encoding!\n");
	
    /* Test 16: create ISO-8859-1 database */
    printf("\nTest 16: Create database %s with encoding ISO-8859-1: \n", cinfo.dbname);
	
    if (test_create_db(&cinfo, conn, "ISO-8859-1")) {
      dbi_conn_close(conn);
      dbi_shutdown();
      exit(1);
    }

    /* Test 17: select ISO-8859-1 database */
    printf("\nTest 17: Select ISO-8859-1 database: \n");

    if (test_select_db(&cinfo, conn)) {
      dbi_conn_close(conn);
      dbi_shutdown();
      exit(1);
    }

    /* Test 18: get encoding of ISO-8859-1 database */
    printf("\nTest 18: Get encoding of ISO-8859-1 database: \n");
	
    printf("The database encoding appears to be: %s\n", dbi_conn_get_encoding(conn));

    /* we're done with this connection */
    dbi_conn_close(conn);
    conn = NULL;

    /* now make a connection to the existing database using a different
       encoding */
    printf("\nAttempt to connect to the existing ISO-8859-1 database using an UTF-8 encoding\n");
    if ((conn = dbi_conn_new(cinfo.drivername)) == NULL) {
      printf("Can't instantiate '%s' driver into a dbi_conn!\n", cinfo.drivername);
      dbi_shutdown();
      return 1;
    }

    driver = dbi_conn_get_driver(conn);
	
    if (set_driver_options(&cinfo, conn, "UTF-8", "libdbitest")) {
      dbi_shutdown();
      exit(1);
    }

    if (dbi_conn_connect(conn) < 0) {
      dbi_conn_error(conn, &errmsg);
      printf("\nUnable to connect! Error message: %s\n", errmsg);
      dbi_shutdown();
      exit(1);
    }
	
    printf("\nSuccessfully connected to ISO-8859-1 database using UTF-8 encoding!\n");
	
    /* Test 19: get encoding of ISO-8859-1 database */
    printf("\nTest 19: Get encoding of ISO-8859-1 database using UTF-8 encoding: \n");
	
    printf("The database encoding appears to be: %s\n", dbi_conn_get_encoding(conn));

    /* we're done with this connection */
    dbi_conn_close(conn);
    conn = NULL;

    /* now make a connection to the existing database using the "auto"
       encoding */
    printf("\nAttempt to connect to the existing ISO-8859-1 database using the \"auto\" encoding feature\n");

    if ((conn = dbi_conn_new(cinfo.drivername)) == NULL) {
      printf("Can't instantiate '%s' driver into a dbi_conn!\n", cinfo.drivername);
      dbi_shutdown();
      return 1;
    }

    driver = dbi_conn_get_driver(conn);
	
    if (set_driver_options(&cinfo, conn, "auto", "libdbitest")) {
      dbi_shutdown();
      exit(1);
    }

    if (dbi_conn_connect(conn) < 0) {
      dbi_conn_error(conn, &errmsg);
      printf("\nUnable to connect! Error message: %s\n", errmsg);
      dbi_shutdown();
      exit(1);
    }
	
    printf("\nSuccessfully connected to ISO-8859-1 database using \"auto\" encoding!\n");
	
    /* Test 20: get encoding of ISO-8859-1 database */
    printf("\nTest 20: Get encoding of ISO-8859-1 database using \"auto\" encoding: \n");
	
    printf("The database encoding appears to be: %s\n", dbi_conn_get_encoding(conn));

    /* Test 21: drop ISO-8859-1 database */
    printf("\nTest 21: Drop ISO-8859-1 database: \n");
	
    if (test_drop_db(&cinfo, conn)) {
      dbi_conn_close(conn);
      dbi_shutdown();
      exit(1);
    }
  }
	
  printf("\n\n");
  printf("SUCCESS! All done, disconnecting and shutting down libdbi. Have a nice day.\n\n");
	
  if (conn) {
    dbi_conn_close(conn);
  }

  dbi_shutdown();
	
  exit (0);
}

/* ******************************************************************* */
/* end of main program                                                 */
/* ******************************************************************* */

/* returns 0 on success, 1 on error */
int init_db(struct CONNINFO* ptr_cinfo) {
  char command[1024];
    /* Debian hack: the interactive client is called isql-fb here */
  int boolean = access("/etc/debian-version", F_OK);

  if (!strcmp(ptr_cinfo->drivername, "firebird")) {
    snprintf(command, 1024, 
	     "echo \"CREATE DATABASE \'%s:%s/%s\' user '%s' password '%s';\""
	     "| %s -e -pas %s "
	     "-u %s -sql_dialect 3", ptr_cinfo->hostname, ptr_cinfo->dbdir, 
	     ptr_cinfo->dbname, ptr_cinfo->username, ptr_cinfo->password, 
	     ( boolean ? "isql" : "isql-fb"), 
	     ptr_cinfo->password, ptr_cinfo->username);
    if (system(command)) {
      fprintf(stderr, "Could not create initial database\n");
      return 1;
    }
    snprintf(command, 1024, "sudo chmod 666 %s/%s", ptr_cinfo->dbdir, ptr_cinfo->dbname);
    if (system(command)) {
      fprintf(stderr, "Could not set database permissions\n");
      return 1;
    }
  }
  
  return 0;
}

/* returns 0 on success, 1 on error */
int ask_for_conninfo(struct CONNINFO* ptr_cinfo) {
  int numdrivers;
  dbi_driver driver;

  fprintf(stderr, "\nlibdbi-drivers test program: $Id$\n"
	 "Library version: %s\n\n", dbi_version());
	
  fprintf(stderr, "libdbi driver directory? [%s] ", DBI_DRIVER_DIR);
  fgets(ptr_cinfo->driverdir, 256, stdin);
  if ((ptr_cinfo->driverdir)[0] == '\n') {
    strncpy(ptr_cinfo->driverdir, DBI_DRIVER_DIR, 255), (ptr_cinfo->driverdir)[255] = '\0';
  }
  else {
    (ptr_cinfo->driverdir)[strlen(ptr_cinfo->driverdir)-1] = '\0';
  }
	
  numdrivers = dbi_initialize(ptr_cinfo->driverdir);
	
  if (numdrivers < 0) {
    fprintf(stderr, "Unable to initialize libdbi! Make sure you specified a valid driver directory.\n");
    dbi_shutdown();
    return 1;
  }
  else if (numdrivers == 0) {
    fprintf(stderr, "Initialized libdbi, but no drivers were found!\n");
    dbi_shutdown();
    return 1;
  }
	
  driver = NULL;
  fprintf(stderr, "%d drivers available: ", numdrivers);
  while ((driver = dbi_driver_list(driver)) != NULL) {
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
    fprintf(stderr, "database directory? [.] ");
    fgets(ptr_cinfo->dbdir, 256, stdin);
    if ((ptr_cinfo->dbdir)[0] == '\n') {
      (ptr_cinfo->dbdir)[0] = '.';
      (ptr_cinfo->dbdir)[1] = '\0';
    }
    else {
      (ptr_cinfo->dbdir)[strlen(ptr_cinfo->dbdir)-1] = '\0';
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
    dbi_conn_set_option(conn, "sqlite3_dbdir", ptr_cinfo->dbdir);
  }
  else  if (!strcmp(ptr_cinfo->drivername, "sqlite")){
    dbi_conn_set_option(conn, "sqlite_dbdir", ptr_cinfo->dbdir);
  }
  else  if (!strcmp(ptr_cinfo->drivername, "firebird")){
    dbi_conn_set_option(conn, "firebird_dbdir", ptr_cinfo->dbdir);
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
  else if (!strcmp(ptr_cinfo->drivername, "mysql")){
    strcpy(ptr_cinfo->initial_dbname, ptr_cinfo->dbname);
  }
  else if (!strcmp(ptr_cinfo->drivername, "firebird")){
    strcpy(ptr_cinfo->initial_dbname, ptr_cinfo->dbname);
  }
  else if (!strcmp(ptr_cinfo->drivername, "freetds")) {
    strcpy(ptr_cinfo->initial_dbname, "master");
    if (encoding && *encoding) {
      dbi_conn_set_option(conn, "encoding", encoding);
    }
  }
	
  if (db && *db) {
    dbi_conn_set_option(conn, "dbname", db);
  }
  else {
    dbi_conn_set_option(conn, "dbname", ptr_cinfo->initial_dbname);
  }

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
      || !strcmp(ptr_cinfo->drivername, "msql")) {
    printf("\tThis is a no-op with the sqlite/msql/firebird drivers.\n");
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
int test_create_table(struct CONNINFO* ptr_cinfo, dbi_conn conn) {
  char query[QUERY_LEN+1];
  const char *errmsg;
  dbi_result result;

  if (!strcmp(ptr_cinfo->drivername, "mysql")) {
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
	     "the_conn_string VARCHAR(255),"
	     "the_empty_string VARCHAR(255),"
	     "the_null_string VARCHAR(255),"
	     "the_binary_string BLOB,"
	     "the_datetime DATETIME,"
	     "the_datetime_tz DATETIME,"
	     "the_date DATE,"
	     "the_time TIME,"
	     "the_time_tz TIME,"
	     "id INT AUTO_INCREMENT,"
	     "PRIMARY KEY (id))");
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
	     "the_conn_string VARCHAR(255),"
	     "the_empty_string VARCHAR(255),"
	     "the_null_string VARCHAR(255),"
	     "the_binary_string BYTEA,"
	     "the_datetime TIMESTAMP,"
	     "the_datetime_tz TIMESTAMP WITH TIME ZONE,"
	     "the_date DATE,"
	     "the_time TIME,"
	     "the_time_tz TIME WITH TIME ZONE,"
	     "id SERIAL PRIMARY KEY)");
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
	     "the_conn_string CHAR(255),"
	     "the_empty_string VARCHAR(255),"
	     "the_null_string VARCHAR(255),"
	     "the_date DATE,"
	     "the_time TIME,"
	     "the_time_tz TIME,"
	     "id INT)");		
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
	     "the_conn_string VARCHAR(255),"
	     "the_empty_string VARCHAR(255),"
	     "the_null_string VARCHAR(255),"
	     "the_binary_string BLOB,"
	     "the_datetime DATETIME,"
	     "the_datetime_tz DATETIME,"
	     "the_date DATE,"
	     "the_time TIME,"
	     "the_time_tz TIME,"
	     "id INTEGER AUTO INCREMENT)");
  }
  else if (!strcmp(ptr_cinfo->drivername, "firebird")) {
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
		 "the_conn_string VARCHAR(255),"
		 "the_empty_string VARCHAR(255),"
		 "the_null_string VARCHAR(255), "
		 "the_binary_string BLOB,"
		 "the_datetime TIMESTAMP, "
		 "the_datetime_tz TIMESTAMP, "
		 "the_date DATE,"
		 "the_time TIME,"
		 "the_time_tz TIME,"
		 "id INTEGER NOT NULL PRIMARY KEY)");

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
	      "the_conn_string VARCHAR(255),"
	      "the_empty_string VARCHAR(255),"
	      "the_null_string VARCHAR(255),"
	      "the_binary_string IMAGE,"
	      "the_datetime DATETIME,"
	      "the_date DATETIME,"
	      "the_time DATETIME,"
	      "id INT IDENTITY,"
	      "CONSTRAINT tr_test_datatypes PRIMARY KEY (id))");
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
  char *driver_quoted_string = NULL;
  char *conn_quoted_string = NULL;
  unsigned char* quoted_binary = NULL;
  size_t quoted_binary_length;
  const char *errmsg;
  unsigned long long n_last_id;
  unsigned long long n_next_id;
  dbi_result result;

  dbi_driver_quote_string_copy(dbi_conn_get_driver(conn), string_to_quote, &driver_quoted_string);
  dbi_conn_quote_string_copy(conn, string_to_quote, &conn_quoted_string);
  quoted_binary_length = dbi_conn_quote_binary_copy(conn, binary_to_quote, binary_to_quote_length, &quoted_binary);
  if(!strcmp(ptr_cinfo->drivername, "msql")) {
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
	     "'',"
	     "NULL,"
	     "'11-jul-1977',"
	     "'23:59:59',"
	     "NULL)", driver_quoted_string, conn_quoted_string);
  } else if (!strcmp(ptr_cinfo->drivername, "freetds")){
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
	     "the_conn_string,"
	     "the_empty_string,"
	     "the_null_string,"
	     "the_binary_string,"
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
	     "'',"
	     "NULL,"
	     "%s,"
	     "'2001-12-31 23:59:59',"
	     "'2001-12-31',"
	     "'23:59:59')", driver_quoted_string, conn_quoted_string, quoted_binary);
  } 
  else if(!strcmp(ptr_cinfo->drivername, "firebird")) {
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
	     "the_conn_string,"
	     "the_empty_string,"
	     "the_null_string,"
	     "the_binary_string,  "
	     "the_datetime,"
	     "the_date, the_time,"
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
	     "'',"
	     "NULL,"
	     "%s, "
 	     "'2001-12-31 23:59:59',"
	     "'2001-12-31',"
	     "'23:59:59',"
	     "1)", driver_quoted_string, 
	     conn_quoted_string, quoted_binary);
  }
else {
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
	     "the_conn_string,"
	     "the_empty_string,"
	     "the_null_string,"
	     "the_binary_string,"
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
	     "'',"
	     "NULL,"
	     "%s,"
	     "'2001-12-31 23:59:59',"
	     "'2001-12-31 23:59:59 -10:00',"
	     "'2001-12-31',"
	     "'23:59:59',"
	     "'23:59:59-10:00')", driver_quoted_string, conn_quoted_string, quoted_binary);
  }

  if (driver_quoted_string) {
    free(driver_quoted_string);
  }

  if (conn_quoted_string) {
    free(conn_quoted_string);
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
  if (!strcmp(ptr_cinfo->drivername, "pgsql")) {
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
int test_retrieve_data(struct CONNINFO* ptr_cinfo, dbi_conn conn) {
  const char *errmsg;
  dbi_result result;

  if ((result = dbi_conn_query(conn, "SELECT * from test_datatypes")) == NULL) {
    dbi_conn_error(conn, &errmsg);
    printf("\tAAH! Can't get read data! Error message: %s\n", errmsg);
    return 1;
  }

  printf("\tGot result, try to access rows\n");

  while (dbi_result_next_row(result)) {
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
    const char* the_conn_string;
    const char* the_empty_string;
    const char* the_null_string;
    const unsigned char* the_binary_string;
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
    unsigned int the_conn_string_length = 0;
    unsigned int the_empty_string_length = 0;
    unsigned int the_null_string_length = 0;
    unsigned int the_binary_string_length = 0;

    dbi_error_flag errflag;

    /* first retrieve the values */
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


    if( !strcmp(ptr_cinfo->drivername, "firebird")) {
      printf("the_ulonlong: test skipped for this driver.\n");
    } else {
      the_longlong = dbi_result_get_longlong(result, "the_longlong");
      errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
      if (errflag) {
	printf("the_longlong errflag=%d\n", errflag);
      }
    }

    if( !strcmp(ptr_cinfo->drivername, "firebird")) {
      printf("the_ulonlong: test skipped for this driver.\n");
    } else {
      the_ulonglong = dbi_result_get_ulonglong(result, "the_ulonglong");
      errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
      if (errflag) {
	printf("the_ulonglong errflag=%d\n", errflag);
      }
    }
    the_float = dbi_result_get_float(result, "the_float");
    errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
    if (errflag) {
      printf("the_float errflag=%d\n", errflag);
    }

    if(!strcmp(ptr_cinfo->drivername, "msql")) {
      printf("the_double: test skipped for this driver.\n");
    }
    else {
      the_double = dbi_result_get_double(result, "the_double");
      errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
      if (errflag) {
	printf("the_double errflag=%d\n", errflag);
      }
    }
    the_driver_string = dbi_result_get_string(result, "the_driver_string");
    errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
    if (errflag) {
      printf("the_driver_string errflag=%d\n", errflag);
    }
    the_conn_string = dbi_result_get_string(result, "the_conn_string");
    errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
    if (errflag) {
      printf("the_conn_string errflag=%d\n", errflag);
    }

    the_empty_string = dbi_result_get_string(result, "the_empty_string");
    errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
    if (errflag) {
      printf("the_empty_string errflag=%d\n", errflag);
    }

    the_null_string = dbi_result_get_string(result, "the_null_string");
    errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
    if (errflag) {
      printf("the_null_string errflag=%d\n", errflag);
    }

    the_binary_string = dbi_result_get_binary(result, "the_binary_string");
    errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
    if (errflag) {
      printf("the_binary_string errflag=%d\n", errflag);
    }

    if(!strcmp(ptr_cinfo->drivername, "msql")) {
      printf("the_datetime: test skipped for this driver.\n");
    }
    else {
      the_datetime = dbi_result_get_datetime(result, "the_datetime");
      errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
      if (errflag) {
	printf("the_datetime errflag=%d\n", errflag);
      }

      the_datetime_tz = dbi_result_get_datetime(result, "the_datetime_tz");
      errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
      if (errflag) {
	printf("the_datetime_tz errflag=%d\n", errflag);
      }
    }

    /* then retrieve the field lengths */
    the_driver_string_length = dbi_result_get_field_length(result, "the_driver_string");
    errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
    if (errflag) {
      printf("the_driver_string_length errflag=%d\n", errflag);
    }

    the_conn_string_length = dbi_result_get_field_length(result, "the_conn_string");
    errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
    if (errflag) {
      printf("the_conn_string_length errflag=%d\n", errflag);
    }

    the_empty_string_length = dbi_result_get_field_length(result, "the_empty_string");
    errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
    if (errflag) {
      printf("the_empty_string_length errflag=%d\n", errflag);
    }

    the_binary_string_length = dbi_result_get_field_length(result, "the_binary_string");
    errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
    if (errflag) {
      printf("the_binary_string_size errflag=%d\n", errflag);
    }


    if(!strcmp(ptr_cinfo->drivername, "msql")) {
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
			
      printf("the_char: in:-127 out:%d<<\nthe_uchar: in:127 out:%u<<\nthe_short: in:-32767 out:%hd<<\nthe_ushort: in:32767 out:%hu<<\nthe_long: in:-2147483647 out:%ld<<\nthe_ulong: in:2147483647 out:%lu<<\nthe_longlong: in:-9223372036854775807 out:%lld<<\nthe_ulonglong: in:9223372036854775807 out:%llu<<\nthe_float: in:3.402823466E+38 out:%e<<\nthe_driver_string: in:\'%s\' out:\'%s\'<<\nthe_conn_string: in:\'%s\' out:\'%s\'<<\nthe_empty_string: out:\'%s\'\nthe_null_string: out:\'%s\'\nthe_date: in:\'11-jul-1977\' out: %s<<\nthe_time: in:\'23:59:59\' out: %s<<", (signed int)the_char, (unsigned int)the_uchar, the_short, the_ushort, the_long, the_ulong, the_longlong, the_ulonglong, the_float, string_to_quote, the_driver_string, string_to_quote, the_conn_string, the_empty_string, the_null_string, the_date, the_time);

    }
    else {

      ptr_tm = gmtime(&the_datetime);
      year_dt = ptr_tm->tm_year+1900;
      mon_dt = ptr_tm->tm_mon+1;
      day_dt = ptr_tm->tm_mday;
      hour_dt = ptr_tm->tm_hour;
      min_dt = ptr_tm->tm_min;
      sec_dt = ptr_tm->tm_sec;
	
      ptr_tm = gmtime(&the_datetime_tz);
      year_dt_tz = ptr_tm->tm_year+1900;
      mon_dt_tz = ptr_tm->tm_mon+1;
      day_dt_tz = ptr_tm->tm_mday;
      hour_dt_tz = ptr_tm->tm_hour;
      min_dt_tz = ptr_tm->tm_min;
      sec_dt_tz = ptr_tm->tm_sec;
	
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
			
      the_time_dt_tz = dbi_result_get_datetime(result, "the_time_tz");
      errflag = dbi_conn_error_flag(dbi_result_get_conn(result));
      if (errflag) {
	printf("the_time_tz errflag=%d\n", errflag);
/* 	printf("type went to %d, attribs went to %d\n", dbi_result_get_field_type(result, "the_time_tz"), dbi_result_get_field_attribs(result, "the_time_tz")); */
      }
			
      ptr_tm_date = gmtime(&the_date_dt);
      year = ptr_tm_date->tm_year+1900;
      mon = ptr_tm_date->tm_mon+1;
      day = ptr_tm_date->tm_mday;
	
      ptr_tm_time = gmtime(&the_time_dt);
      hour = ptr_tm_time->tm_hour;
      min = ptr_tm_time->tm_min;
      sec = ptr_tm_time->tm_sec;

      ptr_tm_time = gmtime(&the_time_dt_tz);
      hour_tz = ptr_tm_time->tm_hour;
      min_tz = ptr_tm_time->tm_min;
      sec_tz = ptr_tm_time->tm_sec;

      if (!strcmp(ptr_cinfo->drivername, "firebird")) {
	printf("the_short: in:-32768 out:%hd<<\n"
	       "the_ushort: in:32767 out:%hu<<\n"
	       "the_long: in:-2147483648 out:%ld<<\n"
	       "the_ulong: in:2147483647 out:%lu<<\n"
	       "the_float: in:3.4E+37 out:%e<<\n"
	       "the_double: in:1.7E+307 out:%e\n"
	       "the_driver_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_conn_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_empty_string: out:\'%s\'<<\n"
	       "the_null_string: out:\'%s\'\n"
	       "the_datetime: in:\'2001-12-31 23:59:59\' out:%d-%d-%d %d:%d:%d\n"
	       "the_date: in:\'2001-12-31\' out:%d-%d-%d\n"
	       "the_time: in:\'23:59:59\' out:%d:%d:%d\n",
	       the_short, the_ushort, the_long, the_ulong, the_float, the_double, string_to_quote, the_driver_string, string_to_quote, the_conn_string, the_empty_string, the_null_string, year_dt, mon_dt, day_dt, hour_dt, min_dt, sec_dt, year, mon, day, hour, min, sec);
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
	       "the_double: in:1.7976931348623157E+307 out:%e\n"
	       "the_driver_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_conn_string: in:\'%s\' out:\'%s\'<<\n"
	       "the_empty_string: out:\'%s\'<<\n"
	       "the_null_string: out:\'%s\'\n"
	       "the_datetime: in:\'2001-12-31 23:59:59\' out:%d-%d-%d %d:%d:%d\n"
	       "the_datetime_tz: in:\'2001-12-31 23:59:59 -10:00\' out:%d-%d-%d %d:%d:%d\n"
	       "the_date: in:\'2001-12-31\' out:%d-%d-%d\n"
	       "the_time: in:\'23:59:59\' out:%d:%d:%d\n"
	       "the_time_tz: in:\'23:59:59-10:00\' out:%d:%d:%d\n",
	       (signed int)the_char, (unsigned int)the_uchar, the_short, the_ushort, the_long, the_ulong, the_longlong, the_ulonglong, the_float, the_double, string_to_quote, the_driver_string, string_to_quote, the_conn_string, the_empty_string, the_null_string, year_dt, mon_dt, day_dt, hour_dt, min_dt, sec_dt, year_dt_tz, mon_dt_tz, day_dt_tz, hour_dt_tz, min_dt_tz, sec_dt_tz, year, mon, day, hour, min, sec, hour_tz, min_tz, sec_tz);
      }
    }
    
    printf("the_binary_string: in: ");
    for (i = 0; i < binary_to_quote_length; i++) {
      printf("%d-", binary_to_quote[i]);
    }

    printf(" out: ");
    for (i = 0; i < the_binary_string_length; i++) {
      printf("%d-", the_binary_string[i]);
    }

    printf("<<\n");

    printf("\n\nfield lengths:\nthe_driver_string_length %d\nthe_conn_string_length %d\nthe_empty_string_length %d\nthe_null_string_length %d\nthe_binary_string_length %d\n\n", 
	   the_driver_string_length,
	   the_conn_string_length,
	   the_empty_string_length,
	   the_null_string_length,
	   the_binary_string_length);

  } /* end while */

  /* todo: check field sizes */
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
    char dbpath[256];

    /* need this break to grant some time for db unlocking */
    printf("...hang on a second...\n");
    sleep(3);

    strcpy(dbpath, ptr_cinfo->dbdir);
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
  else if (!strcmp(ptr_cinfo->drivername, "msql") || !strcmp(ptr_cinfo->drivername, "firebird")) {
    printf("\tThis is a no-op with the mSQL/Firebird driver.\n");
		
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
