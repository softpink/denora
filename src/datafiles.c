
/* Database file handling routines.
 *
 * (c) 2004-2014 Denora Team
 * Contact us at info@denorastats.org
 *
 * Please read COPYING and README for furhter details.
 *
 * Based on the original code of Anope by Anope Team.
 * Based on the original code of Thales by Lucas.
 * Based on code from Thilo Schulz.
 *
 *
 *
 */

#include "denora.h"

static int curday = 0;

/*************************************************************************/

int new_read_db_entry(char **key, char **value, FILE * fptr)
{
	char *string = *key;
	int character;
	int i = 0;

	**key = '\0';
	**value = '\0';
	

	while (1)
	{
		if ((character = fgetc(fptr)) == EOF)   /* a problem occurred reading the file */
		{
			if (ferror(fptr))
			{
				return DB_READ_ERROR;   /* error! */
			}
			return DB_EOF_ERROR;	/* end of file */
		}
		else if (character == BLOCKEND)	 /* END OF BLOCK */
		{
			return DB_READ_BLOCKEND;
		}
		else if (character == VALUEEND)	 /* END OF VALUE */
		{
			string[i] = '\0';   /* end of value */
			return DB_READ_SUCCESS;
		}
		else if (character == SEPARATOR)	/* END OF KEY */
		{
			string[i] = '\0';   /* end of key */
			string = *value;    /* beginning of value */
			i = 0;	      /* start with the first character of our value */
		}
		else
		{
			if ((i == (MAXKEYLEN - 1)) && (string == *key))     /* max key length reached, continuing with value */
			{
				string[i] = '\0';       /* end of key */
				string = *value;	/* beginning of value */
				i = 0;	  /* start with the first character of our value */
			}
			else if ((i == (MAXVALLEN - 1)) && (string == *value))      /* max value length reached, returning */
			{
				string[i] = '\0';
				return DB_READ_SUCCESS;
			}
			else
			{
				string[i] = character;  /* read string (key or value) */
				i++;
			}
		}
	}
}

/*************************************************************************/

int new_write_db_entry(const char *key, DenoraDBFile * dbptr,
		       const char *fmt, ...)
{
	char string[MAXKEYLEN + MAXVALLEN + 2], value[MAXVALLEN];   /* safety byte :P */
	va_list ap;
	unsigned int length;

	if (!dbptr)
	{
		return DB_WRITE_ERROR;
	}
	

	va_start(ap, fmt);
	ircvsnprintf(value, MAXVALLEN, fmt, ap);
	va_end(ap);

	if (!stricmp(value, "(null)"))
	{
		return DB_WRITE_NOVAL;
	}
	
	ircsnprintf(string, MAXKEYLEN + MAXVALLEN + 1, "%s%c%s", key,
		    SEPARATOR, value);
	length = strlen(string);
	string[length] = VALUEEND;
	length++;
	if (fwrite(string, 1, length, dbptr->fptr) < length)
	{
		alog(LOG_DEBUG, "debug: Error writing to %s", dbptr->filename);
		remove(dbptr->filename);
		rename(dbptr->temp_name, dbptr->filename);
		filedb_close(dbptr, NULL, NULL);
		dbptr = NULL;
		return DB_WRITE_ERROR;
	}
	
	return DB_WRITE_SUCCESS;
}

/*************************************************************************/

int new_write_db_endofblock(DenoraDBFile * dbptr)
{
	if (!dbptr)
	{
		return DB_WRITE_ERROR;
	}
	

	if (fputc(BLOCKEND, dbptr->fptr) == EOF)
	{
		alog(LOG_DEBUG, "debug: Error writing to %s", dbptr->filename);
		filedb_close(dbptr, NULL, NULL);
		return DB_WRITE_ERROR;
	}
	

	return DB_WRITE_SUCCESS;
}

/*************************************************************************/

void fill_db_ptr(DenoraDBFile * dbptr, int version, int core_version,
		 char *service, char *filename)
{
	char buffer[PATH_MAX];
	char buf[BUFSIZE];
	char tempbuf[BUFSIZE];

	int failgetcwd = 0;
	*buffer = '\0';
	*buf = '\0';
	*tempbuf = '\0';


#ifdef _WIN32
	if (_getcwd(buffer, PATH_MAX) == NULL)
	{
#else
	if (getcwd(buffer, PATH_MAX) == NULL)
	{
#endif
		alog(LOG_DEBUG, "debug: Unable to set Current working directory");
		failgetcwd = 1;
	}

	dbptr->db_version = version;
	dbptr->core_db_version = core_version;
	
	if (!BadPtr(service))
	{
		dbptr->service = sstrdup(service);
	}
	else
	{
		dbptr->service = sstrdup("");
	}

#ifndef _WIN32
	if (failgetcwd)
	{
		/* path failed just get the file name in there */
		dbptr->filename = sstrdup(filename);
		ircsnprintf(tempbuf, BUFSIZE, "%s.temp", filename);
	}
	else
	{
		ircsnprintf(buf, BUFSIZE, "%s/%s", buffer, filename);
		dbptr->filename = sstrdup(buf);
		ircsnprintf(tempbuf, BUFSIZE, "%s.temp", buf);
	}
	dbptr->temp_name = sstrdup(tempbuf);
#else
	if (failgetcwd)
	{
		/* path failed just get the file name in there */
		dbptr->filename = sstrdup(filename);
		ircsnprintf(tempbuf, BUFSIZE, "%s.temp", filename);
	}
	else
	{
		ircsnprintf(buf, BUFSIZE, "%s\\%s", buffer, filename);
		dbptr->filename = sstrdup(buf);
		ircsnprintf(tempbuf, BUFSIZE, "%s.temp", buf);
	}
	dbptr->temp_name = sstrdup(tempbuf);
#endif
	return;
}

/*************************************************************************/

/**
 * Renames a database
 *
 * @param name Database to name
 * @param ext Extention
 * @return void
 */
static void rename_database(char *name, char *ext)
{

	char destpath[PATH_MAX];

	ircsnprintf(destpath, sizeof(destpath), "backups/%s.%s", name, ext);
	if (rename(name, destpath) != 0)
	{
		alog(LOG_NORMAL, "Backup of %s failed.", name);
		denora_cmd_global(s_StatServ, "WARNING! Backup of %s failed.",
				  name);
	}
}

/*************************************************************************/

/**
 * Removes old databases
 *
 * @return void
 */
static void remove_backups(void)
{

	char ext[9];
	char path[PATH_MAX];

	time_t t;
	struct tm tm;

	time(&t);
	t -= (60 * 60 * 24 * KeepBackupsFor);
#ifdef _WIN32
	localtime_s(&tm, &t);
#else
	tm = *localtime(&t);
#endif
	strftime(ext, sizeof(ext), "%Y%m%d", &tm);

	ircsnprintf(path, sizeof(path), "backups/%s.%s", ChannelDB, ext);
	unlink(path);

	ircsnprintf(path, sizeof(path), "backups/%s.%s", ServerDB, ext);
	unlink(path);

	ircsnprintf(path, sizeof(path), "backups/%s.%s", ChannelStatsDB, ext);
	unlink(path);

}

/*************************************************************************/

/**
 * Handles database backups.
 *
 * @return void
 */
void backup_databases(void)
{

	time_t t;
	struct tm tm;

	if (!KeepBackups)
	{
		return;
	}

	time(&t);
	tm = *localtime(&t);

	if (!curday)
	{
		curday = tm.tm_yday;
		return;
	}

	if (curday != tm.tm_yday)
	{

		char ext[9];

		send_event(EVENT_DB_BACKUP, 1, EVENT_START);
		alog(LOG_DEBUG, "Backing up databases");

		remove_backups();

		curday = tm.tm_yday;
		strftime(ext, sizeof(ext), "%Y%m%d", &tm);

		rename_database(ChannelDB, ext);
		rename_database(ServerDB, ext);
		rename_database(ChannelStatsDB, ext);
		send_event(EVENT_DB_BACKUP, 1, EVENT_STOP);
	}
}

/*************************************************************************/

void ModuleDatabaseBackup(char *dbname)
{

	time_t t;
	struct tm tm;

	if (!KeepBackups)
	{
		return;
	}

	time(&t);
	tm = *localtime(&t);

	if (!curday)
	{
		curday = tm.tm_yday;
		return;
	}

	if (curday != tm.tm_yday)
	{

		char ext[9];

		alog(LOG_DEBUG, "Backing up %s", dbname);

		ModuleRemoveBackups(dbname);

		curday = tm.tm_yday;
		strftime(ext, sizeof(ext), "%Y%m%d", &tm);

		rename_database(dbname, ext);
	}
}

/*************************************************************************/

void ModuleRemoveBackups(char *dbname)
{
	char ext[9];
	char path[PATH_MAX];

	time_t t;
	struct tm tm;

	time(&t);
	t -= (60 * 60 * 24 * KeepBackupsFor);
#ifdef _WIN32
	localtime_s(&tm, &t);
#else
	tm = *localtime(&t);
#endif
	strftime(ext, sizeof(ext), "%Y%m%d", &tm);

	ircsnprintf(path, sizeof(path), "backups/%s.%s", dbname, ext);
	unlink(path);
}

/*************************************************************************/

DenoraDBFile *filedb_open(char *db, int type, char **key, char **value)
{
	DenoraDBFile *dbptr = calloc(1, sizeof(DenoraDBFile));

	*key = malloc(MAXKEYLEN);
	*value = malloc(MAXVALLEN);

	alog(LOG_NORMAL, "Loading %s", db);

	fill_db_ptr(dbptr, 0, type, s_StatServ, db);
	

	if (!FileExists(dbptr->filename))
	{
		filedb_close(dbptr, key, value);
		return NULL;
	}

	
	if ((dbptr->fptr = FileOpen(dbptr->filename, FILE_READ)) == NULL)
	{
		filedb_close(dbptr, key, value);
		return NULL;
	}
	
	dbptr->db_version =
	    fgetc(dbptr->fptr) << 24 | fgetc(dbptr->fptr) << 16 | fgetc(dbptr->
		    fptr)
	    << 8 | fgetc(dbptr->fptr);

	if (ferror(dbptr->fptr))
	{
		alog(LOG_DEBUG, "debug: Error reading version number on %s",
		     dbptr->filename);
		filedb_close(dbptr, key, value);
		return NULL;
	}
	else if (feof(dbptr->fptr))
	{
		alog(LOG_DEBUG,
		     "debug: Error reading version number on %s: End of file detected",
		     dbptr->filename);
		filedb_close(dbptr, key, value);
		return NULL;
	}
	else if (dbptr->db_version < 1)
	{
		alog(LOG_DEBUG, "debug: Invalid version number (%d) on %s",
		     dbptr->db_version, dbptr->filename);
		filedb_close(dbptr, key, value);
		return NULL;
	}
	

	return dbptr;
}

/*************************************************************************/

DenoraDBFile *filedb_create(char * db, int type)
{
	DenoraDBFile *dbptr = calloc(1, sizeof(DenoraDBFile));

	fill_db_ptr(dbptr, 0, type, s_StatServ, db);
	

	/* time to backup the old db */
	rename(db, dbptr->temp_name);

	if (!(dbptr->fptr = FileOpen(dbptr->filename, FILE_WRITE)))
	{
		rename(dbptr->temp_name, db);
		free(dbptr);
		return NULL;
	}
	

	if (fputc(dbptr->core_db_version >> 24 & 0xFF, dbptr->fptr) < 0 ||
		fputc(dbptr->core_db_version >> 16 & 0xFF, dbptr->fptr) < 0 ||
		fputc(dbptr->core_db_version >> 8 & 0xFF, dbptr->fptr) < 0 ||
		fputc(dbptr->core_db_version & 0xFF, dbptr->fptr) < 0)
	{
		alog(LOG_DEBUG, "debug: Error writing version number on %s",
		     dbptr->filename);
		rename(dbptr->temp_name, db);
		free(dbptr);
		return NULL;
	}
	

	return dbptr;
}

/*************************************************************************/

void filedb_close(DenoraDBFile * dbptr, char **key, char **value)
{
	
	if (dbptr)
	{
		if (key && *key)
		{
			free(*key);
			*key = NULL;
		}
		

		if (value && *value)
		{
			free(*value);
			*value = NULL;
		}
		

		if (dbptr->fptr)
		{
			fclose(dbptr->fptr);
		}
		

		if (FileExists(dbptr->temp_name))
		{
			remove(dbptr->temp_name);
		}
		

		free(dbptr);
	}
}

/*************************************************************************/
