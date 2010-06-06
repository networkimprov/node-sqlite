webdb_sqlite: require "../webdb_sqlite"
assert: require "assert"
sys: require "sys"
fs: require "fs"

try
	fs.unlinkSync("web_potatoes.db")
catch err
	sys.debug(err)
	
db: webdb_sqlite.openDatabase "web_potatoes.db", ->
	sys.debug("Starting with a simple flow")
	db.transaction(
		(transaction) ->
			transaction.executeSql(
				"create table potatoes(type, takes_ketchup, is_ready)", 
				null, 
				(transaction, sql_result_set) ->
					assert.equal sql_result_set.rowsAffected, 0, "should successfuly create table"
					sys.debug "Created table potoatoes"
				(transaction, err) ->
					throw err		
			)
			transaction.executeSql(
				"insert into potatoes(type, takes_ketchup, is_ready) values(?, ?, ?)", 
				["red", 0, 0], 
				(transaction, sql_result_set) ->
					assert.equal sql_result_set.rowsAffected, 1, "should successfuly insert 1 row"
					assert.notEqual sql_result_set.insertId, undefined, "should have a insertId"
					sys.debug "Insert a red potato"
				(transaction, err) ->
					throw err		
			)
			transaction.executeSql(
				"insert into potatoes(type, takes_ketchup, is_ready) values(?, ?, ?)", 
				["yukon", 0, 0], 
				(transaction, sql_result_set) ->
					assert.equal sql_result_set.rowsAffected, 1, "should successfuly insert 1 row"
					assert.notEqual sql_result_set.insertId, undefined, "should have a insertId"
					sys.debug "Insert a yukon potato"
				(transaction, err) ->
					throw err		
			)
			transaction.executeSql(
				"select * from potatoes", 
				null, 
				(transaction, sql_result_set) ->
					assert.equal sql_result_set.rows.length, 2, "show return 2 rows"
				(transaction, err) ->
					throw err		
			)
		(transaction, err) ->
			throw err
		(transaction) ->
			russian_potatoes_test()
	)
	
	russian_potatoes_test: ->
		sys.debug("Ok, let make enough potatoes to feed a Russian army (200000)")
		db.transaction(
			(transaction) ->
				for i in [1..200000]
					transaction.executeSql(
						"insert into potatoes values(3, 3, 3)", 
						null,
						null,
						(transaction, err) ->
							throw err		
					)
				transaction.executeSql(
					"select count(*) from potatoes", 
					null, 
					(transaction, sql_result_set) ->
						assert.equal sql_result_set.rows[0]["count(*)"], 200002, "show return 200002 rows"
				)
			(transaction, err) ->
				throw err
			(transaction) ->
				sys.debug "Fed the army."
		)
	return true
	