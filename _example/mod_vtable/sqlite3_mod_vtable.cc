#include <string>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <sqlite3ext.h>

#include "picojson.h"

#ifdef _WIN32
# define EXPORT __declspec(dllexport)
#else
# define EXPORT
#endif

SQLITE_EXTENSION_INIT1;

typedef struct {
  char* data;   // response data from server
  size_t size;  // response size of data
} MEMFILE;

MEMFILE*
memfopen() {
  MEMFILE* mf = (MEMFILE*) malloc(sizeof(MEMFILE));
  if (mf) {
    mf->data = NULL;
    mf->size = 0;
  }
  return mf;
}

void
memfclose(MEMFILE* mf) {
  if (mf->data) free(mf->data);
  free(mf);
}

size_t
memfwrite(char* ptr, size_t size, size_t nmemb, void* stream) {
  MEMFILE* mf = (MEMFILE*) stream;
  int block = size * nmemb;
  if (!mf) return block; // through
  if (!mf->data)
    mf->data = (char*) malloc(block);
  else
    mf->data = (char*) realloc(mf->data, mf->size + block);
  if (mf->data) {
    memcpy(mf->data + mf->size, ptr, block);
    mf->size += block;
  }
  return block;
}

char*
memfstrdup(MEMFILE* mf) {
  char* buf;
  if (mf->size == 0) return NULL;
  buf = (char*) malloc(mf->size + 1);
  memcpy(buf, mf->data, mf->size);
  buf[mf->size] = 0;
  return buf;
}

static int
my_connect(sqlite3 *db, void *pAux, int argc, const char * const *argv, sqlite3_vtab **ppVTab, char **c) {
  std::stringstream ss;
  ss << "CREATE TABLE " << argv[0]
    << "(id int, full_name text, description text, html_url text)";
  int rc = sqlite3_declare_vtab(db, ss.str().c_str());
  *ppVTab = (sqlite3_vtab *) sqlite3_malloc(sizeof(sqlite3_vtab));
  memset(*ppVTab, 0, sizeof(sqlite3_vtab));
  return rc;
}

static int
my_create(sqlite3 *db, void *pAux, int argc, const char * const * argv, sqlite3_vtab **ppVTab, char **c) {
  return my_connect(db, pAux, argc, argv, ppVTab, c);
}

static int my_disconnect(sqlite3_vtab *pVTab) {
  sqlite3_free(pVTab);
  return SQLITE_OK;
}

static int
my_destroy(sqlite3_vtab *pVTab) {
  sqlite3_free(pVTab);
  return SQLITE_OK;
}

typedef struct {
  sqlite3_vtab_cursor base;
  int index;
  picojson::value* rows;
} cursor;

static int
my_open(sqlite3_vtab *pVTab, sqlite3_vtab_cursor **ppCursor) {
  FILE* fin;
  MEMFILE* mf;
  char* json;

  mf = memfopen();

  // https://api.github.com/repositories
  fin = fopen("github-repositories.json", "rt");
  if (fin == NULL) {
    std::cerr << "open github-repositories.json failed!" << std::endl;
    return SQLITE_FAIL;
  }

  // file size
  fseek(fin, 0, SEEK_END);
  mf->size = ftell(fin);
  fseek(fin, 0, SEEK_SET);

  // copy to mf
  mf->data = (char*)malloc(mf->size);
  fread(mf->data, 1, mf->size, fin);
  fclose(fin);

  picojson::value* v = new picojson::value;
  std::string err;
  picojson::parse(*v, mf->data, mf->data + mf->size, &err);
  memfclose(mf);

  if (!err.empty()) {
    delete v;
    std::cerr << err << std::endl;
    return SQLITE_FAIL;
  }

  cursor *c = (cursor *)sqlite3_malloc(sizeof(cursor));
  c->rows = v;
  c->index = 0;
  *ppCursor = &c->base;
  return SQLITE_OK;
}

static int
my_close(cursor *c) {
  delete c->rows;
  sqlite3_free(c);
  return SQLITE_OK;
}

static int
my_filter(cursor *c, int idxNum, const char *idxStr, int argc, sqlite3_value **argv) {
  c->index = 0;
  return SQLITE_OK;
}

static int
my_next(cursor *c) {
  c->index++;
  return SQLITE_OK;
}

static int
my_eof(cursor *c) {
  return c->index >= c->rows->get<picojson::array>().size() ? 1 : 0;
}

static int
my_column(cursor *c, sqlite3_context *ctxt, int i) {
  picojson::value v = c->rows->get<picojson::array>()[c->index];
  picojson::object row = v.get<picojson::object>();
  const char* p = NULL;
  switch (i) {
  case 0:
    p = row["id"].to_str().c_str();
    break;
  case 1:
    p = row["full_name"].to_str().c_str();
    break;
  case 2:
    p = row["description"].to_str().c_str();
    break;
  case 3:
    p = row["html_url"].to_str().c_str();
    break;
  }
  sqlite3_result_text(ctxt, strdup(p), strlen(p), free);
  return SQLITE_OK;
}

static int
my_rowid(cursor *c, sqlite3_int64 *pRowid) {
  *pRowid = c->index;
  return SQLITE_OK;
}

static int
my_bestindex(sqlite3_vtab *tab, sqlite3_index_info *pIdxInfo) {
  return SQLITE_OK;
}

static const sqlite3_module module = {
  0,
  my_create,
  my_connect,
  my_bestindex,
  my_disconnect,
  my_destroy,
  my_open,
  (int (*)(sqlite3_vtab_cursor *)) my_close,
  (int (*)(sqlite3_vtab_cursor *, int, char const *, int, sqlite3_value **)) my_filter,
  (int (*)(sqlite3_vtab_cursor *)) my_next,
  (int (*)(sqlite3_vtab_cursor *)) my_eof,
  (int (*)(sqlite3_vtab_cursor *, sqlite3_context *, int)) my_column,
  (int (*)(sqlite3_vtab_cursor *, sqlite3_int64 *)) my_rowid,
  NULL, // my_update
  NULL, // my_begin
  NULL, // my_sync
  NULL, // my_commit
  NULL, // my_rollback
  NULL, // my_findfunction
  NULL, // my_rename
};

static void
destructor(void *arg) {
  return;
}


extern "C" {

EXPORT int
sqlite3_extension_init(sqlite3 *db, char **errmsg, const sqlite3_api_routines *api) {
  SQLITE_EXTENSION_INIT2(api);
  sqlite3_create_module_v2(db, "github", &module, NULL, destructor);
  return 0;
}

}
