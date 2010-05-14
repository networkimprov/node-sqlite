webdb_sqlite: require "../webdb_sqlite"
assert: require "assert"
sys: require "sys"
fs: require "fs"

fs.unlinkSync("web_potatoes")
db: webdb_sqlite.openDatabase "web_potatoes", ->
	sys.puts("Starting with a simple flow")
	db.transaction(
		(transaction) ->
			sys.puts "Trying to create table potatoes"
			transaction.executeSQL(
				"create table potatoes(type, takes_ketchup, is_ready)", 
				null, 
				(transaction, sql_result_set) ->
					assert.equal sql_result_set.rowsAffected, 0, "should successfuly create table"
				, (transaction, err) ->
					throw err		
			)
		, (transaction, err) ->
			throw err
		, (transaction) ->
			sys.puts "Table created."
	)