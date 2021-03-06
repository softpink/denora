
/*
 *
 * (c) 2004-2013 Denora Team
 * Contact us at info@denorastats.org
 *
 * Please read COPYING and README for furhter details.
 *
 * Based on the original code of Anope by Anope Team.
 * Based on the original code of Thales by Lucas.
 *
 *
 *
 */
#include "denora.h"

char *rdb_errmsg;

/*************************************************************************/

int rdb_init()
{
	int res = 0;
#ifdef USE_MYSQL
	if (sqltype == SQL_MYSQL)
	{
		res = db_mysql_init(0);
	}
	SET_SEGV_LOCATION();
	if (res)
	{
		db_connect();
	}
#else
	alog(LOG_DEBUG, "MySQL not found while compiling, SQL queries are disabled");
	denora->do_sql = 0;
	res = 1;
#endif

	return res;
}

/*************************************************************************/

int rdb_close()
{
	denora->do_sql = 0;
#ifdef USE_MYSQL
	if (sqltype == SQL_MYSQL)
	{
		db_mysql_close(0);
#ifdef USE_THREADS
		db_mysql_close(1);
#endif
		return 1;
	}
#endif
	return 0;
}

/*************************************************************************/

int rdb_clear_table(char *table)
{
#if defined(USE_MYSQL)
	static char buf[1024];

	SET_SEGV_LOCATION();

	if (KeepUserTable && !stricmp(table, UserTable))
	{
		ircsnprintf(buf, sizeof(buf), "UPDATE %s SET online='N'",
		            table);
	}
	else if (KeepServerTable && !stricmp(table, ServerTable))
	{
		ircsnprintf(buf, sizeof(buf), "UPDATE %s SET online='N'",
		            table);
	}
	else
	{
		ircsnprintf(buf, sizeof(buf), "TRUNCATE TABLE %s", table);
	}
#ifdef USE_MYSQL
	if (sqltype == SQL_MYSQL)
		return db_mysql_query(buf,0);
#endif

#else
	USE_VAR(table);
#endif
	return 0;
}

/*************************************************************************/

int rdb_direct_query(char *query, int con)
{
#if !defined(USE_MYSQL)
	USE_VAR(query);
	USE_VAR(con);
#endif

	SET_SEGV_LOCATION();

	if (!denora->do_sql)
	{
		return -1;
	}
#ifdef USE_MYSQL
	if (sqltype == SQL_MYSQL)
	{
		return db_mysql_query(query, con);
		/* 
		 * Threaded calls may return results but are probably way too late.
		 * So lets clean them up in order to free results and save memory
		 */
		if (con == 1)
			dbMySQLPrepareForQuery(con);
	}
#endif
	return 0;
}

/*************************************************************************/

/* send an SQL query to the database */
int rdb_query(int i, const char *fmt, ...)
{
	va_list args;
	char buf[NET_BUFSIZE];      /* should be enough for long queries (don't underestimate :o)) */
	int res;

	if (!denora->do_sql)
	{
		return 0;
	}

	if (BadPtr(fmt))
	{
		return 0;
	}

	va_start(args, fmt);
	ircvsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);


#ifdef USE_THREADS
	if (UseThreading)
	{
		if (i == QUERY_HIGH)
		{
			res = rdb_direct_query(buf, 0);
		}
		else
		{
			/*
			 * This should be: qp = AddQueueEntry(qp, buf);
			 * However this will SEGV denora, and we can't have that for now... *MIST!*
			 */
			qp = AddQueueEntry(NULL, buf);
			res = 1;
		}
	}
	else
	{
		res = rdb_direct_query(buf,0);
	}
#else
	i = 0; /* hack to avoid compiler warning that we are not using the varabile */
	res = rdb_direct_query(buf,0);
#endif
	return res;
}

/*************************************************************************/

/* escape the string */
char *rdb_escape(char *ch)
{
	char *ret = NULL;
#if defined(USE_MYSQL)
	char *result = NULL;
#ifdef USE_MYSQL
	if (sqltype == SQL_MYSQL)
	{
		result = db_mysql_quote(ch);
	}
#endif
	if (result)
	{
		ret = sstrdup(result);
		free(result);
	}
#else
	USE_VAR(ch);
#endif
	return ret;
}

/*************************************************************************/

int rdb_insertid()
{
	SET_SEGV_LOCATION();

#ifdef USE_MYSQL
	if (sqltype == SQL_MYSQL)
	{
		return mysql_insert_id(mysql);
	}
#endif
	return 0;
}

/*************************************************************************/

char *rdb_error_msg()
{
	if (rdb_errmsg)
		free(rdb_errmsg);
	rdb_errmsg = NULL;
#ifdef USE_MYSQL
	if (sqltype == SQL_MYSQL)
	{
		rdb_errmsg = sstrdup(mysql_error(mysql));
	}
#endif
	return rdb_errmsg;
}

/*************************************************************************/

int rdb_check_table(char *table)
{
#ifdef USE_MYSQL
	int res = 1;
	MYSQL_RES *my_res;

	rdb_query(QUERY_HIGH, "SHOW TABLES LIKE '%s';", table);
	my_res = mysql_store_result(mysql);

	if (my_res)
	{
		if (mysql_num_rows(my_res) == 0)
		{
			res = 0;
		}
		mysql_free_result(my_res);
	}
	else
	{
		res = 0;
	}
	if (!res)
	{
		alog(LOG_DEBUG, "Table %s was not found", table);
	}
	return res;
#else
	USE_VAR(table);
#endif
	return 0;
}

/*************************************************************************/
