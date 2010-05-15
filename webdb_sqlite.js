(function(){
  var Database, SQLTransaction, openDatabase, sqlite, sys;
  // A HTML5 web database wrapper around the node-sqlite lib
  // Based on mrjjwright's fork of node-sqlite
  // Copyright (c) 2009, Eric Fredricksen <e@fredricksen.net>
  // Copyright (c) 2010, Orlando Vazquez <ovazquez@gmail.com>
  // Author: John J. Wright <mrjjwright@gmail.com>
  // See: http://github.com/mrjjwright/node-sqlite
  // & http://dev.w3.org/html5/webdatabase/
  // Permission to use, copy, modify, and/or distribute this software for any
  // purpose with or without fee is hereby granted, provided that the above
  // copyright notice and this permission notice appear in all copies.
  // THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  // WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  // MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  // ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  // WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  // ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  // OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
  if ((typeof process !== "undefined" && process !== null)) {
    sqlite = require("./sqlite");
    sys = require("sys");
  }
  Database = function(name, callback) {
    this.sqlite_db = new sqlite.Database();
    this.sqlite_db.open(name, function(err) {
      if ((typeof callback !== "undefined" && callback !== null)) {
        return callback();
      }
    });
    return this;
    return this;
  };
  // opens the database
  // Begin a transaction
  Database.prototype.transaction = function(start, failure, success) {
    return new SQLTransaction(this, start, failure, success);
  };

  SQLTransaction = function(db, start, failure, success) {
    var self;
    this.db = db;
    self = this;
    this.sql_queue = [];
    this.sqlite_db = db.sqlite_db;
    this.failure = failure;
    this.dequeued = 0;
    // Pointer used by the delayed shift mechanism
    try {
      // execute the steps as detailed here:
      // http://dev.w3.org/html5/webdatabase/#transaction-steps
      self.sqlite_db.execute("begin transaction;", function() {
        var execute_sql, finish_up;
        // caller hook to start queuing up sql
        start(self);
        // Try to execute the statements in the queue
        // until there are no more to process
        execute_sql = function() {
          var sql_wrapper;
          try {
            if (self.sql_queue.length === 0) {
              return finish_up();
            }
            sql_wrapper = self.sql_queue[self.dequeued];
            // delayed shift for efficient queue
            self.dequeued = self.dequeued + 1;
            if ((self.dequeued * 2) > self.sql_queue.length) {
              self.sql_queue = self.sql_queue.slice(self.dequeued);
              self.dequeued = 0;
            }
            self.sqlite_db.execute(sql_wrapper.sql, sql_wrapper.bindings, function(err, res) {
              var _a, sql_result_set;
              if (!self.handleTransactionError(err, sql_wrapper.errorCallback)) {
                return null;
              }
              // no error, let the caller know
              if ((typeof (_a = sql_wrapper.callback) !== "undefined" && _a !== null)) {
                //package up the results per the spec
                // this object is an array with a couple of other params
                sql_result_set = {};
                sql_result_set.insertId = self.sqlite_db.lastInsertRowid();
                sql_result_set.rowsAffected = self.sqlite_db.changes();
                sql_result_set.rows = (typeof res !== "undefined" && res !== null) ? res : [];
                sql_wrapper.callback(self, sql_result_set);
              }
              return execute_sql();
            });
          } catch (error) {
            sys.debug("Catch transaction: " + error);
            if (!self.handleTransactionError(error, sqlite_wrapper.errorCallback)) {
              return null;
            }
          }
        };
        execute_sql();
        finish_up = function() {
          return self.sqlite_db.execute("commit;", function() {
            if ((typeof success !== "undefined" && success !== null)) {
              return success(self);
            }
          });
        };
        return finish_up;
      });
    } catch (err) {
      sys.debug(err);
      self.transactionRollback(err);
    }
    return this;
  };
  // Prepares, and queues the statement as per the guidelines in the spec
  // Note that  mrjjwright's fork of node-sqlite also caches statements
  // for performance instead of preparing them each time.
  // sql can either be an escaped sql, or sql with ? placeholders.
  // optional bindings is an array of params in the right order
  // optional callback(transaction, resultSet)
  // optional errorCallback(transaction,error)
  SQLTransaction.prototype.executeSQL = function(sql, bindings, callback, errorCallback) {
    return this.sql_queue.push({
      sql: sql,
      bindings: bindings,
      callback: callback,
      errorCallback: errorCallback
    });
  };
  SQLTransaction.prototype.handleTransactionError = function(err, errorCallback) {
    var self;
    self = this;
    if ((typeof err !== "undefined" && err !== null)) {
      sys.debug("handleTransactionError: " + err);
      if ((typeof errorCallback !== "undefined" && errorCallback !== null)) {
        // according to the spec, the errorCallback should return true
        // if everything handled ok
        if (!errorCallback(self, err)) {
          return self.transactionRollback(err);
        }
      } else {
        return self.transactionRollback(err);
      }
    } else {
      return true;
    }
  };
  SQLTransaction.prototype.transactionRollback = function(err) {
    var self;
    sys.debug("transactionRollback: " + err);
    self = this;
    this.sqlite_db.execute("rollback;", function() {
      var _a;
      if ((typeof (_a = self.failure) !== "undefined" && _a !== null)) {
        return self.failure(self, err);
      }
    });
    return false;
  };

  // opens the database
  openDatabase = function(name, version, displayName, estimatedSize, callback) {
    (typeof version !== "undefined" && version !== null) && !(typeof displayName !== "undefined" && displayName !== null) ? (callback = version) : null;
    return new Database(name, callback);
  };
  (typeof process !== "undefined" && process !== null) ? (exports.openDatabase = openDatabase) : null;
})();
