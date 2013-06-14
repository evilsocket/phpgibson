PHP Gibson Client
===

Gibson is a high efficiency, tree based memory cache server. It is not meant to replace a database, since it was written to be a key-value store to be used as a cache server, but it's not the usual cache server.
Normal key-value stores ( memcache, redis, etc ) uses a hash table as their main data structure, so every key is hashed with a specific algorithm and the resulting hash is used to identify the given value in memory. This approach, although very fast, doesn't allow the user to execute globbing expressions/selections with on a given keyset, thus resulting on a pure one-by-one access paradigm.
Gibson is different, it uses a tree based structure allowing the user to perform operations on multiple key sets using a prefix expression achieving the same performance grades

<http://gibson-db.in/>  

Compilation / Installation
---
In order to compile gibson, you will need cmake and autotools installed, then:

    $ cd /path/to/phpgibson-source/
    $ ./configure
    $ make
    # make install

License
---

Released under the BSD license.  
Copyright &copy; 2013, Simone Margaritelli <evilsocket@gmail.com>  
All rights reserved.
