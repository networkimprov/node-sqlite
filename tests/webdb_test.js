(function(){
  var assert, db, fs, sys, webdb_sqlite;
  webdb_sqlite = require("../webdb_sqlite");
  assert = require("assert");
  sys = require("sys");
  fs = require("fs");
  fs.unlinkSync("web_potatoes");
  db = webdb_sqlite.openDatabase("web_potatoes", function() {
    sys.puts("Starting with a simple flow");
    return db.transaction(function(transaction) {
      sys.puts("Trying to create table potatoes");
      return transaction.executeSQL("create table potatoes(type, takes_ketchup, is_ready)", null, function(transaction, sql_result_set) {
        return assert.equal(sql_result_set.rowsAffected, 0, "should successfuly create table");
      }, function(transaction, err) {
        throw err;
      });
    }, function(transaction, err) {
      throw err;
    }, function(transaction) {
      return sys.puts("Table created.");
    });
  });
})();
