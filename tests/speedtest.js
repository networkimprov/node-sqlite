var fs     = require("fs"),
    sys    = require("sys"),
    sqlite = require("../sqlite");

var puts = sys.puts;
var inspect = sys.inspect;

var db = new sqlite.Database();


var count = 200000;
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

function writeTest(db, sql, i, callback) {
    db.execute(sql, function (results) {
      if (!i--) {
        // end of results
        db.execute("commit", function () {
          var dt = ((new Date)-t0)/1000;
          puts("**** " + count + " insertions in " + dt + "s (" + (count/dt) + "/s)");
          if (callback) callback(db);
        });          
      }
      else {
        writeTest(db, sql, i--, callback);
      }
    });    
}

db.open("test.db", function () {
  db.execute("begin transaction", function () {
    db.execute("CREATE TABLE t1 (alpha INTEGER)", function () {
      sql = "INSERT INTO t1 VALUES (1)";
      db.execute(sql, function (err, res) {
        t0 = new Date();
        writeTest(db, sql, count, readTest);
      });
    });
  }); 
});
