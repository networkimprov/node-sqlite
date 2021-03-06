/*
Copyright (c) 2009, Eric Fredricksen <e@fredricksen.net>
Copyright (c) 2010, Orlando Vazquez <ovazquez@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <v8.h>
#include <node.h>
#include <node_events.h>
#include <node_buffer.h>

#include <sqlite3.h>

using namespace v8;
using namespace node;

static Persistent<String> callback_sym;

#define CHECK(rc) { if ((rc) != SQLITE_OK)                              \
      return ThrowException(Exception::Error(String::New(               \
                                             sqlite3_errmsg(*db)))); }

#define SCHECK(rc) { if ((rc) != SQLITE_OK)                             \
      return ThrowException(Exception::Error(String::New(               \
                        sqlite3_errmsg(sqlite3_db_handle(sto->stmt_))))); }

#define REQ_ARGS(N)                                                     \
  if (args.Length() < (N))                                              \
    return ThrowException(Exception::TypeError(                         \
                             String::New("Expected " #N "arguments")));

#define REQ_STR_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsString())                     \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a string")));    \
  String::Utf8Value VAR(args[I]->ToString());

#define REQ_FUN_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsFunction())                   \
    return ThrowException(Exception::TypeError(                         \
                  String::New("Argument " #I " must be a function")));  \
  Local<Function> VAR = Local<Function>::Cast(args[I]);

#define REQ_EXT_ARG(I, VAR)                                             \
  if (args.Length() <= (I) || !args[I]->IsExternal())                   \
    return ThrowException(Exception::TypeError(                         \
                              String::New("Argument " #I " invalid"))); \
  Local<External> VAR = Local<External>::Cast(args[I]);

#define OPT_INT_ARG(I, VAR, DEFAULT)                                    \
  int VAR;                                                              \
  if (args.Length() <= (I)) {                                           \
    VAR = (DEFAULT);                                                    \
  } else if (args[I]->IsInt32()) {                                      \
    VAR = args[I]->Int32Value();                                        \
  } else {                                                              \
    return ThrowException(Exception::TypeError(                         \
              String::New("Argument " #I " must be an integer")));      \
  }

class Sqlite3Db : public EventEmitter {
public:
  static Persistent<FunctionTemplate> constructor_template;
  static void Init(v8::Handle<Object> target) {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    constructor_template = Persistent<FunctionTemplate>::New(t);
    constructor_template->Inherit(EventEmitter::constructor_template);
    constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
    constructor_template->SetClassName(String::NewSymbol("Database"));

    NODE_SET_PROTOTYPE_METHOD(constructor_template, "open", Open);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", Close);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "prepare", Prepare);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "changes", Changes);
    NODE_SET_PROTOTYPE_METHOD(constructor_template, "lastInsertRowid", LastInsertRowid);

    target->Set(v8::String::NewSymbol("Database"),
            constructor_template->GetFunction());

    Statement::Init(target);
  }

protected:
  Sqlite3Db() : EventEmitter(), db_(NULL) { }

  ~Sqlite3Db() {
    assert(db_ == NULL);
  }

  sqlite3* db_;

  // Return a pointer to the Sqlite handle pointer so that EIO_Open can
  // pass it to sqlite3_open which wants a pointer to an sqlite3 pointer. This
  // is because it wants to initialize our original (sqlite3*) pointer to
  // point to an valid object.
  sqlite3** GetDBPtr(void) { return &db_; }

protected:

  static Handle<Value> New(const Arguments& args) {
    HandleScope scope;
    Sqlite3Db* dbo = new Sqlite3Db();
    dbo->Wrap(args.This());
    return args.This();
  }

  // To pass arguments to the functions that will run in the thread-pool we
  // have to pack them into a struct and pass eio_custom a pointer to it.

  struct open_request {
    Persistent<Function> cb;
    Sqlite3Db *dbo;
    char filename[1];
  };

  static int EIO_AfterOpen(eio_req *req) {
    ev_unref(EV_DEFAULT_UC);
    HandleScope scope;
    struct open_request *open_req = (struct open_request *)(req->data);

    Local<Value> argv[1];
    bool err = false;
    if (req->result) {
      err = true;
      argv[0] = Exception::Error(String::New("Error opening database"));
    }

    TryCatch try_catch;

    open_req->dbo->Unref();
    open_req->cb->Call(Context::GetCurrent()->Global(), err ? 1 : 0, argv);

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }

    open_req->dbo->Emit(String::New("ready"), 0, NULL);
    open_req->cb.Dispose();

    free(open_req);

    return 0;
  }

  static int EIO_Open(eio_req *req) {
    struct open_request *open_req = (struct open_request *)(req->data);

    sqlite3 **dbptr = open_req->dbo->GetDBPtr();
    int rc = sqlite3_open_v2( open_req->filename
                            , dbptr
                            , SQLITE_OPEN_READWRITE
                              | SQLITE_OPEN_CREATE
                              | SQLITE_OPEN_FULLMUTEX
                            , NULL);

    req->result = rc;

//     sqlite3 *db = *dbptr;
//     sqlite3_commit_hook(db, CommitHook, open_req->dbo);
//     sqlite3_rollback_hook(db, RollbackHook, open_req->dbo);
//     sqlite3_update_hook(db, UpdateHook, open_req->dbo);

    return 0;
  }

  static Handle<Value> Open(const Arguments& args) {
    HandleScope scope;

    REQ_STR_ARG(0, filename);
    REQ_FUN_ARG(1, cb);

    Sqlite3Db* dbo = ObjectWrap::Unwrap<Sqlite3Db>(args.This());

    struct open_request *open_req = (struct open_request *)
        calloc(1, sizeof(struct open_request) + filename.length());

    if (!open_req) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
        String::New("Could not allocate enough memory")));
    }

    strcpy(open_req->filename, *filename);
    open_req->cb = Persistent<Function>::New(cb);
    open_req->dbo = dbo;

    eio_custom(EIO_Open, EIO_PRI_DEFAULT, EIO_AfterOpen, open_req);

    ev_ref(EV_DEFAULT_UC);
    dbo->Ref();

    return Undefined();
  }

  //
  // JS DatabaseSync bindings
  //

  // TODO: libeio'fy
  static Handle<Value> Changes(const Arguments& args) {
    HandleScope scope;
    Sqlite3Db* dbo = ObjectWrap::Unwrap<Sqlite3Db>(args.This());
    Local<Number> result = Integer::New(sqlite3_changes(dbo->db_));
    return scope.Close(result);
  }

  static Handle<Value> Close(const Arguments& args) {
    HandleScope scope;

    Sqlite3Db* dbo = ObjectWrap::Unwrap<Sqlite3Db>(args.This());
    sqlite3_close(dbo->db_);
    dbo->db_ = NULL;
    return Undefined();
  }

  // TODO: libeio'fy
  static Handle<Value> LastInsertRowid(const Arguments& args) {
    HandleScope scope;
    Sqlite3Db* dbo = ObjectWrap::Unwrap<Sqlite3Db>(args.This());
    Local<Number> result = Integer::New(sqlite3_last_insert_rowid(dbo->db_));
    return scope.Close(result);
  }

  // Hooks

//   static int CommitHook(void* v_this) {
//     HandleScope scope;
//     Sqlite3Db* db = static_cast<Sqlite3Db*>(v_this);
//     db->Emit(String::New("commit"), 0, NULL);
//     // TODO: allow change in return value to convert to rollback...somehow
//     return 0;
//   }
//
//   static void RollbackHook(void* v_this) {
//     HandleScope scope;
//     Sqlite3Db* db = static_cast<Sqlite3Db*>(v_this);
//     db->Emit(String::New("rollback"), 0, NULL);
//   }
//
//   static void UpdateHook(void* v_this, int operation, const char* database,
//                          const char* table, sqlite_int64 rowid) {
//     HandleScope scope;
//     Sqlite3Db* db = static_cast<Sqlite3Db*>(v_this);
//     Local<Value> args[] = { Int32::New(operation), String::New(database),
//                             String::New(table), Number::New(rowid) };
//     db->Emit(String::New("update"), 4, args);
//   }

  struct prepare_request {
    Persistent<Function> cb;
    Sqlite3Db *dbo;
    sqlite3_stmt* stmt;
    const char* tail;
    char sql[1];
  };

  static int EIO_AfterPrepare(eio_req *req) {
    ev_unref(EV_DEFAULT_UC);
    struct prepare_request *prep_req = (struct prepare_request *)(req->data);
    HandleScope scope;

    Local<Value> argv[2];
    int argc = 0;

    // if the prepare failed
    if (req->result != SQLITE_OK) {
      argv[0] = Exception::Error(
                  String::New(sqlite3_errmsg(prep_req->dbo->db_)));
      argc = 1;
    }
    else {
      if (req->int1 == SQLITE_DONE) {
        argc = 0;
      } else {
        argv[0] = External::New(prep_req->stmt);
        argv[1] = Integer::New(req->int1);
        Persistent<Object> statement(
          Statement::constructor_template->GetFunction()->NewInstance(2, argv));

        if (prep_req->tail) {
          statement->Set(String::New("tail"), String::New(prep_req->tail));
        }

        argv[0] = Local<Value>::New(Undefined());
        argv[1] = Local<Value>::New(statement);
        argc = 2;
      }
    }

    TryCatch try_catch;

    prep_req->dbo->Unref();
    prep_req->cb->Call(Context::GetCurrent()->Global(), argc, argv);

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }

    prep_req->cb.Dispose();
    free(prep_req);

    return 0;
  }

  static int EIO_Prepare(eio_req *req) {
    struct prepare_request *prep_req = (struct prepare_request *)(req->data);

    prep_req->stmt = NULL;
    prep_req->tail = NULL;
    sqlite3* db = prep_req->dbo->db_;

    int rc = sqlite3_prepare_v2(db, prep_req->sql, -1,
                &(prep_req->stmt), &(prep_req->tail));

    req->result = rc;
    req->int1 = -1;

    return 0;
  }

  static Handle<Value> Prepare(const Arguments& args) {
    HandleScope scope;
    REQ_STR_ARG(0, sql);
    REQ_FUN_ARG(1, cb);

    Sqlite3Db* dbo = ObjectWrap::Unwrap<Sqlite3Db>(args.This());

    struct prepare_request *prep_req = (struct prepare_request *)
        calloc(1, sizeof(struct prepare_request) + sql.length());

    if (!prep_req) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
        String::New("Could not allocate enough memory")));
    }

    strcpy(prep_req->sql, *sql);
    prep_req->cb = Persistent<Function>::New(cb);
    prep_req->dbo = dbo;

    eio_custom(EIO_Prepare, EIO_PRI_DEFAULT, EIO_AfterPrepare, prep_req);

    ev_ref(EV_DEFAULT_UC);
    dbo->Ref();

    return Undefined();
  }

  class Statement : public EventEmitter {

  public:
    static Persistent<FunctionTemplate> constructor_template;

    static void Init(v8::Handle<Object> target) {
      HandleScope scope;

      Local<FunctionTemplate> t = FunctionTemplate::New(New);

      constructor_template = Persistent<FunctionTemplate>::New(t);
      constructor_template->Inherit(EventEmitter::constructor_template);
      constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
      constructor_template->SetClassName(String::NewSymbol("Statement"));

      NODE_SET_PROTOTYPE_METHOD(t, "bind", Bind);
//       NODE_SET_PROTOTYPE_METHOD(t, "clearBindings", ClearBindings);
      NODE_SET_PROTOTYPE_METHOD(t, "finalize", Finalize);
//       NODE_SET_PROTOTYPE_METHOD(t, "bindParameterCount", BindParameterCount);
      NODE_SET_PROTOTYPE_METHOD(t, "reset", Reset);
      NODE_SET_PROTOTYPE_METHOD(t, "step", Step);
      NODE_SET_PROTOTYPE_METHOD(t, "sql", Sql);

      callback_sym = Persistent<String>::New(String::New("callback"));

      target->Set(v8::String::NewSymbol("Statement"), constructor_template->GetFunction());
    }

    static Handle<Value> New(const Arguments& args) {
      HandleScope scope;
      REQ_EXT_ARG(0, stmt);
      int first_rc = args[1]->IntegerValue();

      Statement *sto = new Statement((sqlite3_stmt*)stmt->Value(), first_rc);
      sto->Wrap(args.This());
      sto->Ref();

      return args.This();
    }

  protected:
    Statement(sqlite3_stmt* stmt, int first_rc = -1)
    : EventEmitter(), first_rc_(first_rc), stmt_(stmt) {
      column_count_ = -1;
      column_types_ = NULL;
      column_names_ = NULL;
      column_data_ = NULL;
    }

    ~Statement() {
      if (stmt_) sqlite3_finalize(stmt_);
      if (column_types_) free(column_types_);
      if (column_names_) free(column_names_);
      if (column_data_) free(column_data_);
    }

    //
    // JS prepared statement bindings
    //

    static Handle<Value> Bind(const Arguments& args) {
      HandleScope scope;
      Statement* sto = ObjectWrap::Unwrap<Statement>(args.This());

      REQ_ARGS(2);
      if (!args[0]->IsString() && !args[0]->IsInt32())
        return ThrowException(Exception::TypeError(                     
               String::New("First argument must be a string or integer")));
      int index = args[0]->IsString() ?
        sqlite3_bind_parameter_index(sto->stmt_, *String::Utf8Value(args[0])) :
        args[0]->Int32Value();

      if (args[1]->IsInt32()) {
        sqlite3_bind_int(sto->stmt_, index, args[1]->Int32Value());
      } else if (args[1]->IsNumber()) {
        sqlite3_bind_double(sto->stmt_, index, args[1]->NumberValue());
      } else if (args[1]->IsString()) {
        String::Utf8Value text(args[1]);
        sqlite3_bind_text(sto->stmt_, index, *text, text.length(),SQLITE_TRANSIENT);
      } else if (Buffer::HasInstance(args[1])) {
        Buffer* buffer = Buffer::Unwrap<Buffer>(args[1]->ToObject());
        sqlite3_bind_blob(sto->stmt_, index, buffer->data(), buffer->length(), SQLITE_TRANSIENT);
      } else if (args[1]->IsNull() || args[1]->IsUndefined()) {
        sqlite3_bind_null(sto->stmt_, index);
      } else {
        return ThrowException(Exception::TypeError(                     
               String::New("Unable to bind value of this type")));
      }
      return args.This();
    }

    static Handle<Value> BindParameterCount(const Arguments& args) {
      HandleScope scope;
      Statement* sto = ObjectWrap::Unwrap<Statement>(args.This());
      Local<Number> result = Integer::New(sqlite3_bind_parameter_count(sto->stmt_));
      return scope.Close(result);
    }

    static Handle<Value> ClearBindings(const Arguments& args) {
      HandleScope scope;
      Statement* sto = ObjectWrap::Unwrap<Statement>(args.This());
      SCHECK(sqlite3_clear_bindings(sto->stmt_));
      return Undefined();
    }

    static Handle<Value> Finalize(const Arguments& args) {
      HandleScope scope;
      Statement* sto = ObjectWrap::Unwrap<Statement>(args.This());
      if (sto->stmt_) {
        sqlite3_finalize(sto->stmt_);
        sto->stmt_ = NULL;
      }
      return Undefined();
    }

    static Handle<Value> Reset(const Arguments& args) {
       HandleScope scope;
       Statement* sto = ObjectWrap::Unwrap<Statement>(args.This());
       sqlite3_reset(sto->stmt_);
       return Undefined();
     }

    static Handle<Value> Sql(const Arguments& args) {
       HandleScope scope;
       Statement* sto = ObjectWrap::Unwrap<Statement>(args.This());
       Local<String> result = String::New(sto->stmt_ ? sqlite3_sql(sto->stmt_) : "[none]");
       return scope.Close(result);
     }

    union column_datum {
      char* string;
      struct { int size; char* data; } blob;
      int integer;
      double floatt;
    };

    static int EIO_AfterStep(eio_req *req) {
      ev_unref(EV_DEFAULT_UC);

      HandleScope scope;

      Statement *sto = (class Statement *)(req->data);

      Local<Value> argv[2];

      if (sto->error_) {
        int errcode = sqlite3_errcode(sqlite3_db_handle(sto->stmt_));
        argv[0] = Exception::Error(
            String::New(errcode == sto->error_ ? sqlite3_errmsg(sqlite3_db_handle(sto->stmt_)) : "lookup result"));
        Local<Object> obj = argv[0]->ToObject();
        obj->Set(String::NewSymbol("result"), Integer::New(sto->error_));
      }
      else {
        argv[0] = Local<Value>::New(Undefined());
      }

      if (req->result == SQLITE_DONE) {
        argv[1] = Local<Value>::New(Null());
      }
      else {
        Local<Object> row = Object::New();

        for (int i = 0; i < sto->column_count_; i++) {
          switch (sto->column_types_[i]) {
            // XXX why does using String::New make v8 croak here?
            case SQLITE_INTEGER:
              row->Set(String::NewSymbol((char*) sto->column_names_[i]),
                       Int32::New(sto->column_data_[i].integer));
              break;

            case SQLITE_FLOAT:
              row->Set(String::New(sto->column_names_[i]),
                       Number::New(sto->column_data_[i].floatt));
              break;

            case SQLITE_TEXT:
              //if (!strlen((char*)sto->column_data_[i]))
              //  row->Set(String::New(sto->column_names_[i]), Undefined());
              row->Set(String::New(sto->column_names_[i]),
                       String::New(sto->column_data_[i].string));
              // don't free this pointer, it's owned by sqlite3
              break;

            case SQLITE_BLOB: {
              Buffer* buffer = Buffer::New(sto->column_data_[i].blob.size);
              memcpy(buffer->data(), sto->column_data_[i].blob.data, sto->column_data_[i].blob.size);
              row->Set(String::New(sto->column_names_[i]), buffer->handle_);
              } break;

            case SQLITE_NULL:
              row->Set(String::New(sto->column_names_[i]), Null());
            // no default
          }
        }

        argv[1] = row;
      }

      TryCatch try_catch;

      Local<Function> cb = sto->GetCallback();

      cb->Call(sto->handle_, 2, argv);

      if (try_catch.HasCaught()) {
        FatalException(try_catch);
      }

      //if (req->result == SQLITE_DONE && sto->column_count_) {
      //  sto->FreeColumnData();
      //}

      return 0;
    }

/*    void FreeColumnData(void) {
      if (!column_count_) return;
      for (int i = 0; i < column_count_; i++) {
        switch (column_types_[i]) {
          case SQLITE_INTEGER:
            free(column_data_[i]);
            break;
          case SQLITE_FLOAT:
            free(column_data_[i]);
            break;
        }
        column_data_[i] = NULL;
      }

      free(column_data_);
      column_data_ = NULL;
    }*/

    static int EIO_Step(eio_req *req) {
      Statement *sto = (class Statement *)(req->data);
      sqlite3_stmt *stmt = sto->stmt_;
      //assert(stmt);
      int rc;

      // check if we have already taken a step immediately after prepare
      if (sto->first_rc_ != -1) {
        // This is the first one! Let's just use the rc from when we called
        // it in EIO_Prepare
        rc = req->result = sto->first_rc_;
        // Now we set first_rc_ to -1 so that on the next step, it won't
        // think this is the first.
        sto->first_rc_ = -1;
      }
      else if (stmt) {
        rc = req->result = sqlite3_step(stmt);
      }
      else {
        rc = req->result = SQLITE_DONE;
      }

      sto->error_ = 0;

      if (rc == SQLITE_ROW) {
        // If these pointers are NULL, look up and store the number of columns
        // their names and types.
        // Otherwise that means we have already looked up the column types and
        // names so we can simply re-use that info.
        if (!sto->column_types_ && !sto->column_names_) {
          sto->column_count_ = sqlite3_column_count(stmt);
          assert(sto->column_count_);
          sto->column_types_ = (int *) calloc(sto->column_count_, sizeof(int));
          sto->column_names_ = (char **) calloc(sto->column_count_, sizeof(char *));
          sto->column_data_ = (column_datum *) calloc(sto->column_count_, sizeof(column_datum));

          for (int i = 0; i < sto->column_count_; i++) {
            sto->column_names_[i] = (char *) sqlite3_column_name(stmt, i);
            assert(sto->column_names_[i]);
          }
        }

        assert(sto->column_types_ && sto->column_data_ && sto->column_names_);

        for (int i = 0; i < sto->column_count_; i++) {
          sto->column_types_[i] = sqlite3_column_type(stmt, i);
          assert(sto->column_types_[i]);

          switch(sto->column_types_[i]) {
            case SQLITE_INTEGER: 
              sto->column_data_[i].integer = sqlite3_column_int(stmt, i);
              break;

            case SQLITE_FLOAT:
              sto->column_data_[i].floatt = sqlite3_column_double(stmt, i);
              break;

            case SQLITE_TEXT: {
                // It shouldn't be necessary to copy or free() this value,
                // according to http://www.sqlite.org/c3ref/column_blob.html
                // it will be reclaimed on the next step, reset, or finalize.
                // I'm going to assume it's okay to keep this pointer around
                // until it is used in `EIO_AfterStep`
                sto->column_data_[i].string = (char *) sqlite3_column_text(stmt, i);
                if (!sto->column_data_[i].string)
                  sto->column_data_[i].string = (char *)"";
                break;
              }

            case SQLITE_BLOB:
              sto->column_data_[i].blob.data = (char*) sqlite3_column_blob(stmt, i);
              sto->column_data_[i].blob.size = sqlite3_column_bytes(stmt, i);
              break;
              
            case SQLITE_NULL:
              break;
              
            default:
              assert(true);
              break;
          }
        }
      }
      else if (rc == SQLITE_DONE || rc == SQLITE_OK) {
        // nothing to do in this case
      }
      else {
        sto->error_ = rc;
      }

      return 0;
    }

    static Handle<Value> Step(const Arguments& args) {
      HandleScope scope;

      Statement* sto = ObjectWrap::Unwrap<Statement>(args.This());

      if (sto->HasCallback()) {
        return ThrowException(Exception::Error(String::New("Already stepping")));
      }

      REQ_FUN_ARG(0, cb);

      sto->SetCallback(cb);

      eio_custom(EIO_Step, EIO_PRI_DEFAULT, EIO_AfterStep, sto);

      ev_ref(EV_DEFAULT_UC);

      return Undefined();
    }

    // The following three methods must be called inside a HandleScope

    bool HasCallback() {
      return ! handle_->GetHiddenValue(callback_sym).IsEmpty();
    }

    void SetCallback(Local<Function> cb) {
      handle_->SetHiddenValue(callback_sym, cb);
    }

    Local<Function> GetCallback() {
      Local<Value> cb_v = handle_->GetHiddenValue(callback_sym);
      assert(cb_v->IsFunction());
      Local<Function> cb = Local<Function>::Cast(cb_v);
      handle_->DeleteHiddenValue(callback_sym);
      return cb;
    }

    int  column_count_;
    int  *column_types_;
    char **column_names_;
    column_datum *column_data_;
    int error_;

    int first_rc_;
    sqlite3_stmt* stmt_;
  };
};

Persistent<FunctionTemplate> Sqlite3Db::Statement::constructor_template;
Persistent<FunctionTemplate> Sqlite3Db::constructor_template;

extern "C" void init (v8::Handle<Object> target) {
  Sqlite3Db::Init(target);
}

