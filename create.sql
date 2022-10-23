/* Create 3 basic tables. 
 * users-for user information
 * messages-information about messages(whom to whom, what)
 * files-information about files(name and owner)
 */
CREATE TABLE users(id INTEGER PRIMARY KEY AUTOINCREMENT, name VARCHAR UNIQUE NOT NULL);

CREATE TABLE messages(id integer primary key autoincrement, sender text references users(name), receiver text references users(name), msg text not null);

CREATE TABLE files( id integer primary key autoincrement, filename text unique not null, owner text references  users(name) );

/*
 * Create basic user
 */

INSERT INTO users(name) VALUES("johnny");