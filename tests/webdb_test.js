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
    sys.puts("Starting with a simple flow");
    return db.transaction(function(transaction) {
      transaction.executeSQL("create table potatoes(type, takes_ketchup, is_ready)", null, function(transaction, sql_result_set) {
        assert.equal(sql_result_set.rowsAffected, 0, "should successfuly create table");
        return sys.puts("Created table potoatoes");
      }, function(transaction, err) {
        throw err;
      });
      transaction.executeSQL("insert into potatoes(type, takes_ketchup, is_ready) values(?, ?, ?)", ["red", 0, 0], function(transaction, sql_result_set) {
        assert.equal(sql_result_set.rowsAffected, 1, "should successfuly insert 1 row");
        assert.notEqual(sql_result_set.insertId, undefined, "should have a insertId");
        return sys.puts("Insert a red potato");
      }, function(transaction, err) {
        throw err;
      });
      transaction.executeSQL("insert into potatoes(type, takes_ketchup, is_ready) values(?, ?, ?)", ["yukon", 0, 0], function(transaction, sql_result_set) {
        assert.equal(sql_result_set.rowsAffected, 1, "should successfuly insert 1 row");
        assert.notEqual(sql_result_set.insertId, undefined, "should have a insertId");
        return sys.puts("Insert a yukon potato");
      }, function(transaction, err) {
        throw err;
      });
      return transaction.executeSQL("select * from potatoes", null, function(transaction, sql_result_set) {
        assert.equal(sql_result_set.rows.length, 2, "show return 2 rows");
        return sys.puts("Select 2 rows");
      }, function(transaction, err) {
        throw err;
      });
    }, function(transaction, err) {
      throw err;
    }, function(transaction) {
      return sys.puts("Simple flow complete.");
    });
  });
})();
