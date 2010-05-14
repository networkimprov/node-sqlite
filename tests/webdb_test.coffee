webdb_sqlite: require "../webdb_sqlite"
assert: require "assert"
sys: require "sys"
fs: require "fs"

try
	fs.unlinkSync("web_potatoes.db")
catch err
	# skip
db: webdb_sqlite.openDatabase "web_potatoes.db", ->
	sys.puts("Starting with a simple flow")
	db.transaction(
		(transaction) ->
			transaction.executeSQL(
				"create table potatoes(type, takes_ketchup, is_ready)", 
				null, 
				(transaction, sql_result_set) ->
					assert.equal sql_result_set.rowsAffected, 0, "should successfuly create table"
					sys.puts "Created table potoatoes"
				, (transaction, err) ->
					throw err		
			)
			transaction.executeSQL(
				"insert into potatoes(type, takes_ketchup, is_ready) values(?, ?, ?)", 
				["red", 0, 0], 
				(transaction, sql_result_set) ->
					assert.equal sql_result_set.rowsAffected, 1, "should successfuly insert 1 row"
					assert.notEqual sql_result_set.insertId, undefined, "should have a insertId"
					sys.puts "Insert a red potato"
				, (transaction, err) ->
					throw err		
			)
			transaction.executeSQL(
				"insert into potatoes(type, takes_ketchup, is_ready) values(?, ?, ?)", 
				["yukon", 0, 0], 
				(transaction, sql_result_set) ->
					assert.equal sql_result_set.rowsAffected, 1, "should successfuly insert 1 row"
					assert.notEqual sql_result_set.insertId, undefined, "should have a insertId"
					sys.puts "Insert a yukon potato"
				, (transaction, err) ->
					throw err		
			)
			transaction.executeSQL(
				"select * from potatoes", 
				null, 
				(transaction, sql_result_set) ->
					assert.equal sql_result_set.rows.length, 2, "show return 2 rows"
					sys.puts "Select 2 rows"
				, (transaction, err) ->
					throw err		
			)
		, (transaction, err) ->
			throw err
		, (transaction) ->
			sys.puts "Simple flow complete."
	)