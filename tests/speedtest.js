var fs     = require("fs"),
    sys    = require("sys"),
    sqlite = require("../sqlite");

var puts = sys.puts;
var inspect = sys.inspect;

var db = new sqlite.Database();

function readTest(db, callback) {
  var t0 = new Date;
  var rows = 0;
  db.query("SELECT * FROM t1", function(err, rows) {
      var d = ((new Date)-t0)/1000;
      puts("**** " + rows.length + " rows in " + d + "s (" + (rows/d) + "/s)");
      if (callback) callback(db);
  });
}

function writeTest(db, i, callback) {
  db.query("INSERT INTO t1 VALUES (1)", function (row) {
    if (!i--) {
      // end of results
      db.query("commit;", function () {
        var dt = ((new Date)-t0)/1000;
        puts("**** " + count + " insertions in " + dt + "s (" + (count/dt) + "/s)");
        if (callback) callback(db);
      });
    }
    else {
      writeTest(db, i--, callback);
    }
  });
}

var count = 100000;
var t0;

db.open("test.db", function () {
  db.query("begin transaction", function () {
      db.query("CREATE TABLE t1 (alpha INTEGER)", function () {
        t0 = new Date();
        writeTest(db, count, readTest);
    });
  });
  
});
