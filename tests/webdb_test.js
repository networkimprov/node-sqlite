(function(){
  var assert, db, fs, sys, webdb_sqlite;
  webdb_sqlite = require("../webdb_sqlite");
  assert = require("assert");
  sys = require("sys");
  fs = require("fs");
  try {
    fs.unlinkSync("web_potatoes.db");
  } catch (err) {
    // skip
  }
  db = webdb_sqlite.openDatabase("web_potatoes.db", function() {
    var russian_potatoes_test;
    sys.debug("Starting with a simple flow");
    db.transaction(function(transaction) {
      transaction.executeSql("create table potatoes(type, takes_ketchup, is_ready)", null, function(transaction, sql_result_set) {
        assert.equal(sql_result_set.rowsAffected, 0, "should successfuly create table");
        return sys.debug("Created table potoatoes");
      }, function(transaction, err) {
        throw err;
      });
      transaction.executeSql("insert into potatoes(type, takes_ketchup, is_ready) values(?, ?, ?)", ["red", 0, 0], function(transaction, sql_result_set) {
        assert.equal(sql_result_set.rowsAffected, 1, "should successfuly insert 1 row");
        assert.notEqual(sql_result_set.insertId, undefined, "should have a insertId");
        return sys.debug("Insert a red potato");
      }, function(transaction, err) {
        throw err;
      });
      transaction.executeSql("insert into potatoes(type, takes_ketchup, is_ready) values(?, ?, ?)", ["yukon", 0, 0], function(transaction, sql_result_set) {
        assert.equal(sql_result_set.rowsAffected, 1, "should successfuly insert 1 row");
        assert.notEqual(sql_result_set.insertId, undefined, "should have a insertId");
        return sys.debug("Insert a yukon potato");
      }, function(transaction, err) {
        throw err;
      });
      return transaction.executeSql("select * from potatoes", null, function(transaction, sql_result_set) {
        assert.equal(sql_result_set.rows.length, 2, "show return 2 rows");
        sys.debug("Select 2 rows");
        return russian_potatoes_test();
      }, function(transaction, err) {
        throw err;
      });
    });
    russian_potatoes_test = function() {
      sys.debug("Ok, let make enough potatoes to feed a Russian army (200000)");
      return db.transaction(function(transaction) {
        var _a, _b, i;
        _a = 1; _b = 200000;
        for (i = _a; (_a <= _b ? i <= _b : i >= _b); (_a <= _b ? i += 1 : i -= 1)) {
          (function() {
            return transaction.executeSql("insert into potatoes(type, takes_ketchup, is_ready) values(?, ?, ?)", ["red", 0, 0], null, function(transaction, err) {
              throw err;
            });
          })();
        }
        return transaction.executeSql("select count(*) from potatoes", null, function(transaction, sql_result_set) {
          return assert.equal(sql_result_set.rows[0]["count(*)"], 200002, "show return 200002 rows");
        }, function(transaction, err) {
          throw err;
        });
      }, function(transaction, err) {
        throw err;
      }, function(transaction) {
        return sys.debug("Fed the army.");
      });
    };
    return true;
  });
})();
