
typedef struct ibase_conn_s
{
  isc_db_handle db;             /* database handle */
  isc_tr_handle trans;          /* transaction handle */
  ISC_STATUS_ARRAY status;
} ibase_conn_t;

typedef struct ibase_stmt_s
{
  XSQLDA  *osqlda;                 /* output SQLDA */
  isc_stmt_handle stmt;            /* statement handle */
  unsigned long long rowcount;
} ibase_stmt_t;

typedef struct vary_s {
  short vary_length;
  char vary_string[1];
} vary_t;



#define FIREBIRD_RESERVED_WORDS { \
        "", \
        NULL }
