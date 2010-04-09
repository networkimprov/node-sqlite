var fs     = require("fs"),
    sys    = require("sys"),
    sqlite = require("../sqlite");

var puts = sys.puts;
var inspect = sys.inspect;

var db = new sqlite.Database();


var count = 100000;
var t0;

function readTest(db, callback) {
  var t0 = new Date;
  var rows = 0;
  db.execute("SELECT * FROM t1", function(err, rows) {
      var d = ((new Date)-t0)/1000;
      puts("**** " + rows.length + " rows in " + d + "s (" + (rows/d) + "/s)");
      if (callback) callback(db);
  });
}

function writeTest(db, statement, i, callback) {
    db.execute(statement, function (results) {
      if (!i--) {
        // end of results
        statement.finalize( function () {
          db.execute("commit", function () {
            var dt = ((new Date)-t0)/1000;
            puts("**** " + count + " insertions in " + dt + "s (" + (count/dt) + "/s)");
            if (callback) callback(db);
          });          
        });
      }
      else {
        writeTest(db, statement, i--, callback);
      }
    });    
}

db.open("test.db", function () {
  db.execute("begin transaction", function () {
      db.execute("CREATE TABLE t1 (alpha INTEGER)", function () {
        db.prepare("INSERT INTO t1 VALUES (1)", function (err, statement) {
          t0 = new Date();
          writeTest(db, statement, count, readTest);
        });
    });
  });
  
});
