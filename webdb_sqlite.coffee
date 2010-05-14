# A HTML5 web database wrapper around the node-sqlite lib
#
# Based on mrjjwright's fork of node-sqlite
# Copyright (c) 2009, Eric Fredricksen <e@fredricksen.net>
# Copyright (c) 2010, Orlando Vazquez <ovazquez@gmail.com>
#
# Author: John J. Wright <mrjjwright@gmail.com>
#
# See: http://github.com/mrjjwright/node-sqlite
# & http://dev.w3.org/html5/webdatabase/
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.

# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

if process?
	sqlite: require "./sqlite"
	sys: require "sys"

class Database
	
	# opens the database	
	constructor: (name, callback) ->
		@sqlite_db: new sqlite.Database()
		@sqlite_db.open name, (err) ->
			callback() if callback?
		return this
	
		
	# Set options for this wrapper
	# (not part of the HTML5 Web DB spec)
	#
	#
	setOptions: (options) ->
		@options: if @options then _.extend(@options, options) else options
	
	# Begin a transaction		
	transaction: (start, failure, success) ->
		new SQLTransaction(this, start, failure, success)
		
class SQLTransaction

	constructor: (db, start, failure, success) ->
		@db: db
		self: this
		@sql_queue: []
		@sqlite_db: db.sqlite_db
		@failure: failure
		
		try
			# execute the steps as detailed here:
			# http://dev.w3.org/html5/webdatabase/#transaction-steps
			self.sqlite_db.execute "begin transaction;", ->
				# caller hook to start queuing up sql
				start(self)
				# Try to execute the statements in the queue
				# until there are no more to process
				execute_sql: ->
					try
						return finish_up() if self.sql_queue.length is 0
						sql_wrapper: self.sql_queue.shift()
						self.sqlite_db.execute sql_wrapper.sql, sql_wrapper.bindings, (err, res) ->
							return if not self.handleTransactionError(err, sql_wrapper.errorCallback)
							# no error, let the caller know
							if sql_wrapper.callback?
								#package up the results per the spec
								sql_result_set: {insertId: self.sqlite_db.lastInsertRowid(), rowsAffected: self.sqlite_db.changes()}		
								sql_wrapper.callback(self, sql_result_set)
								execute_sql()
					catch error
						sys.debug("Catch transaction: " + error)
						return if not self.handleTransactionError(error, sqlite_wrapper.errorCallback)	
				execute_sql()
				finish_up: ->
					self.sqlite_db.execute "commit;", ->
						success(self) if success?
		catch err
			sys.debug(err)
			self.transactionRollback(err)
		
	# Prepares, and queues the statement as per the guidelines in the spec
	#
	# Note that mrjjwright's fork of node-sqlite also caches statements
	# for performance instead of preparing them each time. 
	#
	# sql can either be an escaped sql, or sql with ? placeholders.
	# optional bindings is an array of params in the right order
	# optional callback(transaction, resultSet) 
	# optional errorCallback(transaction,error) 
	executeSQL: (sql, bindings, callback, errorCallback) ->
			@sql_queue.push(
				{sql: sql, bindings: bindings, callback: callback, errorCallback: errorCallback}
			)
	
	handleTransactionError: (err, errorCallback) ->
		self: this
		if err?
			sys.debug("handleTransactionError: " + err)
			if errorCallback?
				# according to the spec, the errorCallback should return true 
				# if everything handled ok
				if not errorCallback(self, err) then return self.transactionRollback(err)
			else return self.transactionRollback(err)
		else return true
			
	transactionRollback: (err) ->
		sys.debug("transactionRollback: " + err)
		self: this
		@sqlite_db.execute "rollback;", ->
			self.failure(self, err) if self.failure?
		return false
			
# opens the database	
openDatabase: (name, version, displayName, estimatedSize, callback) ->
	if version? and not displayName?
		callback: version
	return new Database(name, callback)
								
# Stolen from underscore.coffee
# Extend a given object with all of the properties in a source object.
extend: (obj) ->
    for source in _.rest(arguments)
      (obj[key]: val) for key, val of source
    obj

rest: (array, index, guard) ->
    slice.call(array, if _.isUndefined(index) or guard then 1 else index)

if process?
	exports.openDatabase: openDatabase
