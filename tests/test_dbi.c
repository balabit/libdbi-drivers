#include <stdio.h>
#include <dbi/dbi.h>

int main(int argc, char **argv) {
	dbi_driver driver;
	dbi_conn conn;
	dbi_result result;

	char driverdir[256];
	char drivername[64];
	char dbname[64];
	char dbdir[256];
	const char *errmsg;
	int numdrivers;

	printf("\nlibdbi test program: $Id$\nLibrary version: %s\n\n", dbi_version());
	
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
	printf("Available drivers (%d) (this test works only with sqlite, so choose wisely): ", numdrivers);
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

	printf("database name? [libdbitest] ");
	fgets(dbname, 64, stdin);
	if (dbname[0] == '\n') strncpy(dbname, "libdbitest", 63), dbname[63] = '\0';
	else dbname[strlen(dbname)-1] = '\0';
	
	printf("database directory? [.] ");
	fgets(dbdir, 256, stdin);
	if (dbdir[0] == '\n') {
	  dbdir[0] = '.';
	  dbdir[1] = '\0';
	}
	else {
	  dbdir[strlen(dbdir)-1] = '\0';
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

	dbi_conn_set_option(conn, "dbname", dbname);
	dbi_conn_set_option(conn, "sqlite_dbdir", dbdir);

	if (dbi_conn_connect(conn) < 0) {
		dbi_conn_error(conn, &errmsg);
		printf("\nUnable to connect! Error message: %s\n", errmsg);
		dbi_shutdown();
		return 1;
	}

	printf("\nSuccessfully connected!\nTest 1: List tables: \n");
	
	if ((result = dbi_conn_get_table_list(conn, dbname, NULL)) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("AAH! Can't get table list! Error message: %s\n", errmsg);
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

	printf("\nTest 2: List databases: \n");

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

	printf("\nTest 3: Create table: \n");

	if ((result = dbi_conn_query(conn, "CREATE TABLE test_address (street VARCHAR(64), number INTEGER, state CHAR(2), city VARCHAR(64), zip INTEGER)")) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("AAH! Can't create table! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	}

	dbi_result_free(result);

	printf("\nTest 4: Insert row: \n");

	if ((result = dbi_conn_query(conn, "INSERT INTO test_address VALUES ('Spellman','5946','TX','Houston','77096')")) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("AAH! Can't insert data! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	}

	dbi_result_free(result);

	printf("\nTest 5: Retrieve data: \n");

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

	printf("\nTest 6: Drop table: \n");

	if ((result = dbi_conn_query(conn, "DROP TABLE test_address")) == NULL) {
		dbi_conn_error(conn, &errmsg);
		printf("AAH! Can't drop table! Error message: %s\n", errmsg);
		dbi_conn_close(conn);
		dbi_shutdown();
		return 1;
	}

	dbi_result_free(result);

	printf("\n\n");
	printf("SUCCESS! All done, disconnecting and shutting down libdbi. Have a nice day.\n\n");

	dbi_conn_close(conn);
	dbi_shutdown();

	return 0;
}
