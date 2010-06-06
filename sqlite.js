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

var sys = require("sys");
var sqlite = require("./sqlite3_bindings");

var Database = exports.Database = function () {
  var self = this;
  var db = new sqlite.Database();
  db.__proto__ = Database.prototype;
  return db;
};

Database.prototype = {
  __proto__: sqlite.Database.prototype,
  constructor: Database,
};

// Iterate over the list of bindings. Since we can't use something as
// simple as a for or while loop, we'll just chain them via the event loop
function _setBindingsByIndex(statement, bindings) {
  
  if (!bindings.length) {
    return;
  }

  for (var i=0; i < bindings.length; i++) {
    statement.bind(i+1, bindings[i]);
  }
}

// Wrapper around sqlite step method
// Calls rowCallback(err, row) for each row
// When there are no more rows emits executeComplete on db
function _doStep(db, statement, rowCallback) {
  statement.step(function (error, row) {
    if (error) return rowCallback(error);
    if (!row) {
      if (db.statement === statement) {
        return db.statement.finalize(function() {
          db.emit("executeComplete");
          rowCallback();
        });
      } else  {
        db.emit("executeComplete");
        return rowCallback();
      }
    }
    rowCallback(null, row);
    _doStep(db, statement, rowCallback);
  });
}

function _onPrepare(db, statement, bindings, rowCallback) {
  if (Array.isArray(bindings)) {
    if (bindings.length) {
      _setBindingsByIndex(statement, bindings);
    }
    _doStep(db, statement, rowCallback);
  }
}

function _onExecute(db, statement, bindings, callback) {
  if (statement) {
    var results = []
    if (!bindings) bindings = [];
    // this will be called after the update or once for every row returned
    _onPrepare(db, statement, bindings, function(error, row){
		//	sys.debug(new Date().getTime() - x);
      if (error && callback) return callback(error);
      if (!row && callback) return callback(null, results);
      results.push(row);
    });
  } else if (callback) callback();  
}


// Executes a select, insert, or update statement.
// Params: 
// sql - a sql string with optional ? for place holders 
//       prepared statements are cached by sql name
//
// bindings - optional, an array of objects to bind to ?s
// callback - will be called as callback(err, results_array)
//            both can be empty.  Results, even a single row, are returned
//            as an  array
Database.prototype.execute = function(sql, bindings, callback) {
  //sys.debug(sql);
  var self = this;
  // bindings are optional, see if they were passed
  if (typeof(bindings) == "function") {
    callback = bindings;
    bindings = [];
  }

  // check the statement cache
  if (!self.statement_cache) self.statement_cache = {}
  statement = self.statement_cache[sql];
  
  if (!statement) {
    // statements have step functions, it's a sql string
    this.prepare(sql, function(error, the_statement) {
      if (error) return callback(error);
      self.statement_cache[sql] = the_statement;
      _onExecute(self, the_statement, bindings, callback);
    }); 
  } else {
    //the user is passing in their own statement
    statement.reset()
    _onExecute(self, statement, bindings, callback);
  } 
    
}

Database.prototype.finalizeAndClose = function () {
  counter = 0;
  self = this;
  if (typeof this.statement_cache !== "undefined" && this.statement_cache !== null) {
    keys = Object.keys(this.statement_cache);
    keys.forEach(function(key) {
      statement = self.statement_cache[key];
      if (typeof statement !== "undefined" && statement !== null) {
        self.statement_cache[key].finalize();
        if (counter++ ===  keys.length - 1) self.close();
      }
    });
  } else {
    this.close();
  }
}
