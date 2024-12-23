CREATE TABLE posters (
   poster_id serial,
   name varchar(255) not null,
   blurb text
);

CREATE TABLE threads (
   thread_id serial,
   poster_id int not null,
   title text,
   url text,
   created timestamp      
);

CREATE TABLE posts (
   thread_id int not null,
   poster_id int not null,
   created timestamp,
   content text
);

CREATE TABLE accounts (
   account_id serial,
   email varchar(255),
   first_name varchar(255),
   last_name varchar(255),
   last_update timestamp
);

CREATE INDEX idx$email ON accounts (email);

CREATE TABLE accounts_auth (
   provider_id int,
   provider_type int,
   account_key varchar(255),
   auth_data jsonb
);

CREATE INDEX idx$provider ON accounts_auth (provider_id, account_key);

CREATE TABLE sessions (
   session_hash uuid,
   session_token text,
   provider_id int,
   account_key varchar(255),
   last_updated timestamp,
   expires timestamp,
   idle_timeout int,
   claims jsonb
);