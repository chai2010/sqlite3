
// +build ingore

#include <string.h>
#include <stdio.h>
#include <sqlite3ext.h>

// http://cm.bell-labs.com/cm/cs/tpop/grep.c

static int match(const char*, const char*);
static int matchhere(const char*, const char*);
static int matchstar(int, const char*, const char*);

/* matchhere: search for regexp at beginning of text */
static int matchhere(const char *regexp, const char *text)
{
	if (regexp[0] == '\0')
		return 1;
	if (regexp[1] == '*')
		return matchstar(regexp[0], regexp+2, text);
	if (regexp[0] == '$' && regexp[1] == '\0')
		return *text == '\0';
	if (*text!='\0' && (regexp[0]=='.' || regexp[0]==*text))
		return matchhere(regexp+1, text+1);
	return 0;
}

/* match: search for regexp anywhere in text */
static int match(const char *regexp, const char *text)
{
	if (regexp[0] == '^')
		return matchhere(regexp+1, text);
	do {	/* must look even if string is empty */
		if (matchhere(regexp, text))
			return 1;
	} while (*text++ != '\0');
	return 0;
}

/* matchstar: search for c*regexp at beginning of text */
static int matchstar(int c, const char *regexp, const char *text)
{
	do {	/* a * matches zero or more instances */
		if (matchhere(regexp, text))
			return 1;
	} while (*text != '\0' && (*text++ == c || c == '.'));
	return 0;
}

SQLITE_EXTENSION_INIT1
static void regexp_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
  if (argc >= 2) {
    const char *target  = (const char *)sqlite3_value_text(argv[1]);
    const char *pattern = (const char *)sqlite3_value_text(argv[0]);
    const char* errstr = NULL;
    int rc;
    rc = match(pattern, target); 
    if (rc <= 0) {
      sqlite3_result_error(context, errstr, 0);
      return;
    }
    sqlite3_result_int(context, 1);
  }
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_extension_init(sqlite3 *db, char **errmsg, const sqlite3_api_routines *api) {
  SQLITE_EXTENSION_INIT2(api);
  return sqlite3_create_function(db, "regexp", 2, SQLITE_UTF8, (void*)db, regexp_func, NULL, NULL);
}
