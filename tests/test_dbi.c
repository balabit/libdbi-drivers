#include <stdio.h>
#include <dbi/dbi.h>

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
	const char *errmsg;
	int numdrivers;

	printf("\nlibdbi-drivers test program: $Id$\nLibrary version: %s\n\n", dbi_version());
	
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

	if (!strcmp(drivername, "mysql") || !strcmp(drivername, "pgsql")
	    || !strcmp(drivername, "msql")) {
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

	  printf("\ndatabase hostname? [(blank for local socket if possible)] ");
	  fgets(hostname, 256, stdin);
	  if (*hostname == '\n') {
	    if (!strcmp(drivername, "pgsql")) {
	      *hostname = '\0';
	    }
	    else {
	      strcpy(hostname, "localhost");
	    }
	  }
	  else {
	    hostname[strlen(hostname)-1] = '\0';
	    if (!strcmp(drivername, "pgsql")) {
	      if (!strcmp(hostname, "localhost")) {
		*hostname = '\0';
	      }
	    }
	  }
	}
	else { /* sqlite */
	  printf("database directory? [.] ");
	  fgets(dbdir, 256, stdin);
	  if (dbdir[0] == '\n') {
	    dbdir[0] = '.';
	    dbdir[1] = '\0';
	  }
	  else {
	    dbdir[strlen(dbdir)-1] = '\0';
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
		   "\tCompiled:   %s\n", dbi_driver_get_name(driver), dbi_driver_get_filename(driver), dbi_driver_get_description(driver), dbi_driver_get_maintainer(driver), dbi_driver_get_url(driver), dbi_driver_get_version(driver), dbi_driver_get_date_compiled(driver));

	if (!strcmp(drivername, "mysql") || !strcmp(drivername, "pgsql")
	    || !strcmp(drivername, "msql")) {
	  dbi_conn_set_option(conn, "host", hostname);
	  dbi_conn_set_option(conn, "username", username);
	  dbi_conn_set_option(conn, "password", password);
	}
	else { /* sqlite */
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
	printf("got result, try to access rows\n");
	while (dbi_result_next_row(result)) {
		const char *databasename = NULL;
		databasename = dbi_result_get_string_idx(result, 1);
		printf("%s ", databasename);
	}

	dbi_result_free(result);

	printf("\nTest 2: Create database %s: \n", dbname);

	if (!strcmp(drivername, "sqlite")) {
	  printf("This is a no-op with the sqlite driver\n");
	}
	else {
	  if ((result = dbi_conn_queryf(conn, "CREATE DATABASE %s", dbname)) == NULL) {
	  printf("database not created\n");
	  fflush(stdout);
	    dbi_conn_error(conn, &errmsg);
	    printf("Darn! Can't create database! Error message: %s\n", errmsg);
	    dbi_conn_close(conn);
	    dbi_shutdown();
	    return 1;
	  }
	  printf("database created ok\n");
	  fflush(stdout);
	  dbi_result_free(result);
	}

	printf("\nTest 3: Select database: \n");

	if (dbi_conn_select_db(conn, dbname)) {
	    dbi_conn_error(conn, &errmsg);
	    printf("Uh-oh! Can't select database! Error message: %s\n", errmsg);
	    dbi_conn_close(conn);
	    dbi_shutdown();
	    return 1;
	}

	printf("\nTest 4: Create table: \n");

	if ((result = dbi_conn_query(conn, "CREATE TABLE test_address (street VARCHAR(64), number INTEGER, state CHAR(2), city VARCHAR(64), zip INTEGER)")) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("AAH! Can't create table! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	}

	dbi_result_free(result);

	printf("\nTest 5: List tables: \n");
	
	if ((result = dbi_conn_get_table_list(conn, dbname, NULL)) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("Oops! Can't get table list! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	}
	printf("got result, try to access rows\n");
	while (dbi_result_next_row(result)) {
		const char *tablename = NULL;
		tablename = dbi_result_get_string_idx(result, 1);
		printf("%s ", tablename);
	}

	dbi_result_free(result);

	printf("\nTest 6: Insert row: \n");

	if ((result = dbi_conn_query(conn, "INSERT INTO test_address VALUES ('Spellman','5946','TX','Houston','77096')")) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("AAH! Can't insert data! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	}

	dbi_result_free(result);

	printf("\nTest 7: Retrieve data: \n");

	if ((result = dbi_conn_query(conn, "SELECT * from test_address")) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("AAH! Can't get read data! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	}
	printf("got result, try to access rows\n");
	while (dbi_result_next_row(result)) {
		const char *street = NULL;
		int number = 0;
		const char *state = NULL;
		const char *city = NULL;
		int zip = 0;
		street = dbi_result_get_string(result, "street");
		number = dbi_result_get_long(result, "number");
		state = dbi_result_get_string(result, "state");
		city = dbi_result_get_string(result, "city");
		zip = dbi_result_get_long(result, "zip");
		printf("I used to live in %d %s, %s, %s %d\n", number, street, city, state, zip);
	}

	dbi_result_free(result);

	printf("\nTest 8: Drop table: \n");

	if ((result = dbi_conn_query(conn, "DROP TABLE test_address")) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("AAH! Can't drop table! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
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
	    printf("AAH! Can't drop database! Error message: %s\n", errmsg);
	    dbi_conn_close(conn);
	    dbi_shutdown();
	    return 1;
	  }

	  dbi_result_free(result);
	}

	printf("\n\n");
	printf("SUCCESS! All done, disconnecting and shutting down libdbi. Have a nice day.\n\n");

	dbi_conn_close(conn);
	dbi_shutdown();

	return 0;
}
