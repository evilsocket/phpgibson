# PhpGibson

The phpgibson extension provides an API for communicating with the [Gibson](http://gibson-db.in/) cache server. It is released under the BSD License.
This code has been developed and maintained by Simone Margaritelli.

You can send comments, patches, questions [here on github](https://github.com/evilsocket/phpgibson/issues) or to evilsocket@gmail.com ([@evilsocket](http://twitter.com/evilsocket)).


# Table of contents
-----
1. [Installing/Configuring](#installingconfiguring)
   * [Installation](#installation)
   * [Installation on OSX](#installation-on-osx)
   * [Building on Windows](#building-on-windows)
1. [Classes and methods](#classes-and-methods)
   * [Usage](#usage)
   * [Connection](#connection)
   * [Methods](#methods)

-----

# Installing/Configuring
-----

Everything you should need to install PhpGibson on your system.

## Installation

~~~
phpize
./configure
make && make install
~~~

This extension exports a single class, [Gibson](#class-gibson).


## Installation on OSX

If the install fails on OSX, type the following commands in your shell before trying again:
~~~
MACOSX_DEPLOYMENT_TARGET=10.6
CFLAGS="-arch i386 -arch x86_64 -g -Os -pipe -no-cpp-precomp"
CCFLAGS="-arch i386 -arch x86_64 -g -Os -pipe"
CXXFLAGS="-arch i386 -arch x86_64 -g -Os -pipe"
LDFLAGS="-arch i386 -arch x86_64 -bind_at_load"
export CFLAGS CXXFLAGS LDFLAGS CCFLAGS MACOSX_DEPLOYMENT_TARGET
~~~

If that still fails and you are running Zend Server CE, try this right before "make": `./configure CFLAGS="-arch i386"`.

## Building on Windows

1. Install visual studio 2008 (express or professional). If using visual studio 2008 express, also install the latest windows SDK.
2. Download PHP source code
3. Extract to C:\php\php-5.4.9
4. Download http://windows.php.net/downloads/php-sdk/php-sdk-binary-tools-20110915.zip and extract to C:\php
5. In cmd.exe
  * cd C:\php\php-5.4.9\ext
  * git clone https://github.com/evilsocket/phpgibson.git
  * cd ..
  * buildconf.js
  * "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv" /x86 /xp /release
  * path "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin";%PATH%
  * bin\phpsdk_setvars.bat
  * "C:\Program Files\Microsoft Visual Studio 9.0\VC\vcvarsall.bat"
  * configure.js --disable-all --enable-cli --enable-gibson 
  * nmake php_gibson.dll
  * fix any compilation errors

# Classes and methods
-----

## Usage

1. [Class Gibson](#class-gibson)

### Class Gibson
-----
_**Description**_: Creates a Gibson client

##### *Example*

~~~
$gb = new Gibson();
~~~

## Connection

1. [connect](#connect) - Connect to a server
1. [pconnect](#pconnect) - Create a persistent connection or reuse a previous one if present.
1. [quit](#quit) - Close the connection

### connect
-----
_**Description**_: Connects to a Gibson instance.

##### *Parameters*

*host*: string. can be a host, or the path to a unix domain socket  
*port*: int, optional  
*timeout*: float, value in milli seconds (optional, default is 0 meaning unlimited)  

##### *Return value*

*BOOL*: `TRUE` on success, `FALSE` on error.

##### *Example*

~~~
$gibson->connect('127.0.0.1', 10128);
$gibson->connect('127.0.0.1'); // port 10128 by default
$gibson->connect('127.0.0.1', 10128, 200); // 200 ms timeout.
$gibson->connect('/tmp/gibson.sock'); // unix domain socket.
~~~

### pconnect
-----
_**Description**_: Create a persistent connection or reuse a previous one if present. If a previous connection exists but
is not valid ( timed out, disconnected, etc ) the connection will be enstablished again.

##### *Parameters*

*host*: string. can be a host, or the path to a unix domain socket  
*port*: int, optional  
*timeout*: float, value in milli seconds (optional, default is 0 meaning unlimited)  

##### *Return value*

*BOOL*: `TRUE` on success, `FALSE` on error.

##### *Example*

~~~
$gibson->pconnect('127.0.0.1', 10128);
$gibson->pconnect('127.0.0.1'); // port 10128 by default
$gibson->pconnect('127.0.0.1', 10128, 200); // 200 ms timeout.
$gibson->pconnect('/tmp/gibson.sock'); // unix domain socket.
~~~

### quit
-----
_**Description**_: Disconnects from the Gibson instance.

## Methods

1. [getLastError](#getLastError) - If an error occurred, return its human readable description.
1. [set](#set) - Set the value for the given key, with an optional TTL.
1. [mset](#mset) - Set the value for keys verifying the given expression.
1. [ttl](#ttl) - Set the TTL of a key.
1. [mttl](#mttl) - Set the TTL for keys verifying the given expression.
1. [get](#get) - Get the value for a given key.
1. [mget](#mget) - Get the values for keys verifying the given expression.
1. [del](#del) - Delete the given key.
1. [mdel](#mdel) - Delete keys verifying the given expression.
1. [inc](#inc) - Increment by one the given key.
1. [minc](#minc) - Increment by one keys verifying the given expression.
1. [mdec](#mdec) - Decrement by one the given keys.
1. [dec](#dec) - Decrement by one keys verifying the given expression.
1. [lock](#lock) - Prevent the given key from being modified for a given amount of seconds.
1. [mlock](#mlock) - Prevent keys verifying the given expression from being modified for a given amount of seconds.
1. [unlock](#unlock) - Remove the lock on a given key.
1. [munlock](#munlock) - Remove the lock on keys verifying the given expression.
1. [count](#count) - Count items for a given expression.
1. [sizeof](#sizeof) - Return the size of the value with the given key, if the value is LZF encoded its compressed size is returned.
1. [msizeof](#msizeof) - Return the total size of values for the given expression, if one or more values are LZF encoded their compressed size is returned.
1. [encof](#encof) - Return the encoding of the value with the given key.
1. [stats](#stats) - Get system stats about the Gibson instance.
1. [ping](#ping) - Ping the server instance to refresh client last seen timestamp.

### getLastError
-----
_**Description**_: If an error occurred, return its human readable description.

##### *Parameters*
None.

##### *Return value*
*string*: The error description.

##### *Example*
~~~
echo $gibson->getLastError();
~~~

### set
-----
_**Description**_: Set the value for the given key, with an optional TTL.

##### *Parameters*
*key* (string) The key to set.
*value* (string) The value.
*ttl* (int) The optional ttl in seconds.

##### *Return value*
*Mixed* The string value itself in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->set( 'foo', 'bar' );
$gibson->set( 'foo2', 'bar2', 2 ); // 2 seconds TTL
~~~

### mset
-----
_**Description**_: Set the value for keys verifying the given expression.

##### *Parameters*
*key* (string) The key prefix to use as expression.
*value* (string) The value.

##### *Return value*
*Mixed* The integer number of updated items in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->set( 'foo', 'bar' );
$gibson->set( 'fuu', 'rar' );

$gibson->mset( 'f', 'yeah' );
~~~

### ttl
-----
_**Description**_: Set the TTL of a key.

##### *Parameters*
*key* (string) The key.
*ttl* (integer) The TTL in seconds.

##### *Return value*
*BOOL* `TRUE` in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->ttl( 'foo', 3600 ); // 1 hour TTL
~~~

### mttl
-----
_**Description**_: Set the TTL for keys verifying the given expression.

##### *Parameters*
*key* (string) The key prefix to use as expression.
*ttl* (integer) The TTL in seconds.

##### *Return value*
*Mixed* The integer number of updated items in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->mttl( 'f', 3600 ); // 1 hour TTL for every f* key.
~~~

### get
-----
_**Description**_: Get the value for a given key.

##### *Parameters*
*key* (string) The key to get.

##### *Return value*
*Mixed* An integer or string value (depends on item encoding) in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->set( 'foo', 'bar' );
$gibson->get( 'foo' ); 
~~~

### mget
-----
_**Description**_: Get the value for a given key.

##### *Parameters*
*key* (string) The key prefix to use as expression.

##### *Return value*
*Mixed* An array of key => value items in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->mget( 'f' ); 
~~~

### del
-----
_**Description**_: Delete the given key.

##### *Parameters*
*key* (string) The key to delete.

##### *Return value*
*BOOL* `TRUE` in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->set( 'foo', 'bar' );
$gibson->del( 'foo' ); 
~~~

### mdel
-----
_**Description**_: Delete keys verifying the given expression.

##### *Parameters*
*key* (string) The key prefix to use as expression.

##### *Return value*
*Mixed* The integer number of deleted items in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->mdel( 'f' ); // Delete every f* key.
~~~

### inc
-----
_**Description**_: Increment by one the given key.

##### *Parameters*
*key* (string) The key to increment.

##### *Return value*
*Mixed* The incremented integer value in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->set( 'foo', '1' );
$gibson->inc( 'foo' ); 
~~~

### minc
-----
_**Description**_: Increment by one keys verifying the given expression.

##### *Parameters*
*key* (string) The key prefix to use as expression.

##### *Return value*
*Mixed* The integer number of incremented items in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->minc( 'f' ); // Increment by one every f* key.
~~~

### dec
-----
_**Description**_: Decrement by one the given key.

##### *Parameters*
*key* (string) The key to decrement.

##### *Return value*
*Mixed* The decremented integer value in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->set( 'foo', '1' );
$gibson->dec( 'foo' ); 
~~~

### mdec
-----
_**Description**_: Decrement by one keys verifying the given expression.

##### *Parameters*
*key* (string) The key prefix to use as expression.

##### *Return value*
*Mixed* The integer number of decremented items in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->mdec( 'f' ); // Decrement by one every f* key.
~~~

### lock
-----
_**Description**_: Prevent the given key from being modified for a given amount of seconds.

##### *Parameters*
*key* (string) The key.
*time* (integer) The time in seconds to lock the item.

##### *Return value*
*BOOL* `TRUE` in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->lock( 'foo', 3600 ); // 1 hour lock
~~~

### mlock
-----
_**Description**_: Prevent keys verifying the given expression from being modified for a given amount of seconds.

##### *Parameters*
*key* (string) The key prefix to use as expression.
*ttl* (integer) The lock period in seconds.

##### *Return value*
*Mixed* The integer number of locked items in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->mlock( 'f', 3600 ); // 1 hour lock for every f* key.
~~~

### unlock
-----
_**Description**_: Remove the lock from the given key.

##### *Parameters*
*key* (string) The key.

##### *Return value*
*BOOL* `TRUE` in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->unlock( 'foo' ); // foo now is unlocked
~~~

### munlock
-----
_**Description**_: Remove the lock on keys verifying the given expression.

##### *Parameters*
*key* (string) The key prefix to use as expression.

##### *Return value*
*Mixed* The integer number of unlocked items in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->munlock( 'f' ); // Every f* key is now unlocked.
~~~

### count
-----
_**Description**_: Count items for a given expression.

##### *Parameters*
*key* (string) The key prefix to use as expression.

##### *Return value*
*Mixed* The integer count of items in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->count( 'f' ); // Count every f* key
~~~

### sizeof
-----
_**Description**_: Return the size of the value with the given key, if the value is LZF encoded its compressed size is returned.

##### *Parameters*
*key* (string) The key of the value to search.

##### *Return value*
*Mixed* An integer with the size in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->set( 'foo', 'bar' );

echo $gibson->sizeof('foo')."\n"; // will print 3
~~~

### msizeof
-----
_**Description**_: Return the total size of values for the given expression, if one or more values are LZF encoded their compressed size is returned.

##### *Parameters*
*key* (string) The key prefix to use as expression.

##### *Return value*
*Mixed* An integer with the total size in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->set( 'foo', 'bar' );
$gibson->set( 'fu', 'kung' );

echo $gibson->msizeof('f')."\n"; // will print 7
~~~

### encof
-----
_**Description**_: Return the encoding of the value with the given key ( can be `Gibson::ENC_PLAIN` for strings, `Gibson::ENC_NUMBER` for numbers and `Gibson::ENC_LZF` for LZF compressed items ).

##### *Parameters*
*key* (string) The key of the value to search.

##### *Return value*
*Mixed* An integer with the encoding in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->set( 'foo', 'bar' );

echo $gibson->encof('foo')."\n"; // will print 0 which is `Gibson::ENC_PLAIN`
~~~

### stats
-----
_**Description**_: Get system stats about the Gibson instance.

##### *Parameters*
None

##### *Return value*
*Mixed* An array of key => value items in case of success, `FALSE` in case of failure.

##### *Example*
~~~
print_r( $gibson->stats() );
~~~

Output:
~~~
Array
(
    [server_started] => 1369857026
    [server_time] => 1369857149
    [first_item_seen] => 1369857132
    [last_item_seen] => 1369857132
    [total_items] => 1
    [total_compressed_items] => 0
    [total_clients] => 1
    [total_cron_done] => 1228
    [memory_used] => 1772
    [memory_peak] => 1772
    [item_size_avg] => 1772
)
~~~

### ping
-----
_**Description**_: Ping the server instance to refresh client last seen timestamp.

##### *Parameters*
None

##### *Return value*
*BOOL* `TRUE` in case of success, `FALSE` in case of failure.

##### *Example*
~~~
$gibson->ping(); 
~~~

