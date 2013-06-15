/*
* Copyright (c) 2013, Simone Margaritelli <evilsocket at gmail dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*   * Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
*   * Neither the name of Gibson nor the names of its contributors may be used
*     to endorse or promote products derived from this software without
*     specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"
#include "php_gibson.h"
#include <gibson.h>

int le_gibson_sock;

zend_class_entry *gibson_ce;

static void gibson_socket_destructor(zend_rsrc_list_entry * rsrc TSRMLS_DC);

ZEND_DECLARE_MODULE_GLOBALS(gibson)

static zend_function_entry gibson_functions[] = {
	PHP_ME(Gibson, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, __destruct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, getLastError, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, connect, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, set, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, mset, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, ttl, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, mttl, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, get, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, mget, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, del, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, mdel, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, inc, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, minc, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, mdec, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, dec, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, lock, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, mlock, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, unlock, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, munlock, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, count, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, stats, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, quit, NULL, ZEND_ACC_PUBLIC)

	{NULL, NULL, NULL}
};

zend_module_entry gibson_module_entry = {
	#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
	#endif
	PHP_GIBSON_EXTNAME,
	NULL,
	PHP_MINIT(gibson),
	PHP_MSHUTDOWN(gibson),
	PHP_RINIT(gibson),
	PHP_RSHUTDOWN(gibson),
	PHP_MINFO(gibson),
	#if ZEND_MODULE_API_NO >= 20010901
	PHP_GIBSON_VERSION,
	#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_GIBSON
ZEND_GET_MODULE(gibson)
#endif

void add_constant_long(zend_class_entry *ce, char *name, int value) {
	zval *constval;

	constval = pemalloc(sizeof(zval), 1);
	INIT_PZVAL(constval);
	ZVAL_LONG(constval, value);
	zend_hash_add(&ce->constants_table, name, 1 + strlen(name),
	(void*)&constval, sizeof(zval*), NULL);
}

/**
* PHP_MINIT_FUNCTION
*/
PHP_MINIT_FUNCTION(gibson)
{
	zend_class_entry gibson_class_entry;

	/* Gibson class */
	INIT_CLASS_ENTRY(gibson_class_entry, "Gibson", gibson_functions);
	gibson_ce = zend_register_internal_class(&gibson_class_entry TSRMLS_CC);

	le_gibson_sock = zend_register_list_destructors_ex(
		gibson_socket_destructor,
		NULL,
		PHP_GIBSON_SOCK_NAME,
		module_number
	);

	add_constant_long(gibson_ce, "REPL_ERR", REPL_ERR);
	add_constant_long(gibson_ce, "REPL_ERR_NOT_FOUND", REPL_ERR_NOT_FOUND);
	add_constant_long(gibson_ce, "REPL_ERR_NAN", REPL_ERR_NAN);
	add_constant_long(gibson_ce, "REPL_ERR_MEM", REPL_ERR_MEM);
	add_constant_long(gibson_ce, "REPL_ERR_LOCKED", REPL_ERR_LOCKED);
	add_constant_long(gibson_ce, "REPL_OK", REPL_OK);
	add_constant_long(gibson_ce, "REPL_VAL", REPL_VAL);
	add_constant_long(gibson_ce, "REPL_KVAL", REPL_KVAL);

	return SUCCESS;
}

int gibson_sock_get(zval *id, gbClient **sock TSRMLS_DC, int no_throw)
{
	zval **socket;
	int resource_type;

	if (Z_TYPE_P(id) != IS_OBJECT || zend_hash_find(Z_OBJPROP_P(id), "socket", sizeof("socket"), (void **) &socket) == FAILURE) {
		// Throw an exception unless we've been requested not to
		if(!no_throw) {
			PHP_GIBSON_EXCEPTION("Gibson server gone away");
		}
		return -1;
	}

	*sock = (gbClient *) zend_list_find(Z_LVAL_PP(socket), &resource_type);

	if (!*sock || resource_type != le_gibson_sock) {
		// Throw an exception unless we've been requested not to
		if(!no_throw) {
			PHP_GIBSON_EXCEPTION("Gibson server gone away");
		}
		return -1;
	}

	return Z_LVAL_PP(socket);
}

static void gibson_socket_destructor(zend_rsrc_list_entry * rsrc TSRMLS_DC)
{
	gbClient *sock = (gbClient *)rsrc->ptr;

	gb_disconnect(sock TSRMLS_CC);

	efree(sock);
}

PHPAPI int gibson_connect(INTERNAL_FUNCTION_PARAMETERS) {
	zval *object;
	zval **socket;
	int host_len, id, ret;
	char *host = NULL;
	long port = -1;

	double timeout = 100.0;
	gbClient *sock  = NULL;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os|ld",
			&object, gibson_ce, &host, &host_len, &port,
			&timeout ) == FAILURE) {
		return FAILURE;
	}

	if (timeout < 0L || timeout > INT_MAX) {
		PHP_GIBSON_EXCEPTION("Invalid timeout");
		return FAILURE;
	}

	if(port == -1 && host_len && host[0] != '/') { /* not unix socket, set to default value */
		port = 10128;
	}

	/* if there is a gibson sock already we have to remove it from the list */
	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) > 0) {
		if (zend_hash_find(Z_OBJPROP_P(object), "socket",sizeof("socket"), (void **) &socket) == FAILURE) {
			/* maybe there is a socket but the id isn't known.. what to do? */
		} else {
			zend_list_delete(Z_LVAL_PP(socket)); /* the refcount should be decreased and the detructor called */
		}
	} else {
		zend_clear_exception(TSRMLS_C); /* clear exception triggered by non-existent socket during connect(). */
	}

	sock = ecalloc(1, sizeof(gbClient));

	if( host[0] == '/' ){
		ret = gb_unix_connect( sock, host, timeout );
	}
	else {
		ret = gb_tcp_connect( sock, host, port, timeout );
	}

	if( ret != 0 ) {
		efree(sock);
		return FAILURE;
	}

	#if PHP_VERSION_ID >= 50400
	id = zend_list_insert(sock, le_gibson_sock TSRMLS_CC);
	#else
	id = zend_list_insert(sock, le_gibson_sock);
	#endif
	add_property_resource(object, "socket", id);

	return SUCCESS;
}

/**
* PHP_MSHUTDOWN_FUNCTION
*/
PHP_MSHUTDOWN_FUNCTION(gibson)
{
	return SUCCESS;
}

/**
* PHP_RINIT_FUNCTION
*/
PHP_RINIT_FUNCTION(gibson)
{
	return SUCCESS;
}

/**
* PHP_RSHUTDOWN_FUNCTION
*/
PHP_RSHUTDOWN_FUNCTION(gibson)
{
	return SUCCESS;
}

/**
* PHP_MINFO_FUNCTION
*/
PHP_MINFO_FUNCTION(gibson)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Gibson Support", "enabled");
	php_info_print_table_row(2, "Gibson Version", PHP_GIBSON_VERSION);
	php_info_print_table_end();
}

PHP_METHOD(Gibson, __construct) {
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		RETURN_FALSE;
	}
}

PHP_METHOD(Gibson, getLastError) {
	zval *object;
	gbClient *sock;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &object, gibson_ce ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	switch( sock->error )
	{
		case REPL_ERR 	        : RETURN_STRING( "Generic error.", 1 );
		case REPL_ERR_NOT_FOUND : RETURN_STRING( "Invalid key, item not found", 1 );
		case REPL_ERR_NAN		: RETURN_STRING( "Invalid value, not a number", 1 );
		case REPL_ERR_MEM       : RETURN_STRING( "Gibson server is out of memory", 1 );
		case REPL_ERR_LOCKED    : RETURN_STRING( "The item is locked", 1 );

		default:
			RETURN_STRING( strerror(sock->error), 1 );
	}
}

PHP_METHOD(Gibson,__destruct) {
	gbClient *sock;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(getThis(), &sock TSRMLS_CC, 1) < 0) {
		RETURN_FALSE;
	}
}

PHP_METHOD(Gibson, connect)
{
	if (gibson_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU) == FAILURE) {
		RETURN_FALSE;
	} else {
		RETURN_TRUE;
	}
}

#define PHP_GIBSON_RETURN_REPLY() if( sock->reply.encoding == GB_ENC_PLAIN ){ \
									RETURN_STRINGL( sock->reply.buffer, sock->reply.size, 0); \
								  } \
								  else if( sock->reply.encoding == GB_ENC_NUMBER ){ \
									  RETURN_LONG( gb_reply_number(sock) ); \
								  } \
								  else { \
									  RETURN_FALSE; \
								  }

PHP_METHOD(Gibson, set)
{
	zval *object;
	gbClient *sock;
	char *key = NULL, *val = NULL;
	int key_len, val_len;
	long expire = -1;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oss|l",
			&object, gibson_ce, &key, &key_len,
			&val, &val_len, &expire) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_set( sock, key, key_len, val, val_len, expire ) != 0 )
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}

PHP_METHOD(Gibson, mset)
{
	zval *object;
	gbClient *sock;
	char *key = NULL, *val = NULL;
	int key_len, val_len;
	long expire = -1;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oss",
			&object, gibson_ce, &key, &key_len,
			&val, &val_len) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_mset( sock, key, key_len, val, val_len ) != 0 )
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}

PHP_METHOD(Gibson, ttl)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;
	long expire = -1;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Osl",
			&object, gibson_ce, &key, &key_len,
		&expire) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_ttl( sock, key, key_len, expire ) != 0 )
		RETURN_FALSE;

	RETURN_TRUE;
}

PHP_METHOD(Gibson, mttl)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;
	long expire = -1;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Osl",
			&object, gibson_ce, &key, &key_len,
			&expire) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_ttl( sock, key, key_len, expire ) != 0 )
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}

PHP_METHOD(Gibson, get)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
			&object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_get( sock, key, key_len ) != 0 )
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}

PHP_METHOD(Gibson, mget)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len, i;
	gbMultiBuffer mb;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
			&object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_mget( sock, key, key_len ) != 0 )
		RETURN_FALSE;

	gb_reply_multi( sock, &mb );

	array_init(return_value);

	for( i = 0; i < mb.count; i++ ){
		if( mb.values[i].encoding == GB_ENC_PLAIN ){
			add_assoc_stringl( return_value, mb.keys[i], mb.values[i].buffer, mb.values[i].size, 1 );
		}
		else if( sock->reply.encoding == GB_ENC_NUMBER ){
			add_assoc_long( return_value, mb.keys[i], *(long *)mb.values[i].buffer );
		}
	}

	gb_reply_multi_free(&mb);
}

PHP_METHOD(Gibson, del)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
			&object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_del( sock, key, key_len ) != 0 )
		RETURN_FALSE;

	RETURN_TRUE;
}

PHP_METHOD(Gibson, mdel)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
			&object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_mdel( sock, key, key_len ) != 0 )
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}

PHP_METHOD(Gibson, inc)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
			&object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_inc( sock, key, key_len ) != 0 )
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}

PHP_METHOD(Gibson, minc)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
			&object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_minc( sock, key, key_len ) != 0 )
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}

PHP_METHOD(Gibson, dec)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
			&object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_dec( sock, key, key_len ) != 0 )
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}

PHP_METHOD(Gibson, mdec)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
			&object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_mdec( sock, key, key_len ) != 0 )
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}

PHP_METHOD(Gibson, lock)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;
	long time = -1;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Osl",
			&object, gibson_ce, &key, &key_len,
			&time) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_lock( sock, key, key_len, time ) != 0 )
		RETURN_FALSE;

	RETURN_TRUE;
}

PHP_METHOD(Gibson, mlock)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;
	long time = -1;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Osl",
			&object, gibson_ce, &key, &key_len,
			&time) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_mlock( sock, key, key_len, time ) != 0 )
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}

PHP_METHOD(Gibson, unlock)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
			&object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_unlock( sock, key, key_len ) != 0 )
		RETURN_FALSE;

	RETURN_TRUE;
}

PHP_METHOD(Gibson, munlock)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
			&object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_munlock( sock, key, key_len ) != 0 )
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}

PHP_METHOD(Gibson, count)
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os",
			&object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_count( sock, key, key_len ) != 0 )
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}

PHP_METHOD(Gibson, stats)
{
	zval *object;
	gbClient *sock;
	int i;
	gbMultiBuffer mb;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &object, gibson_ce ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_stats( sock ) != 0 ) {
		RETURN_FALSE;
	}

	gb_reply_multi( sock, &mb );

	array_init(return_value);

	for( i = 0; i < mb.count; i++ ){
		if( mb.values[i].encoding == GB_ENC_PLAIN ){
			add_assoc_stringl( return_value, mb.keys[i], mb.values[i].buffer, mb.values[i].size, 1 );
		}
		else if( mb.values[i].encoding == GB_ENC_NUMBER ){
			add_assoc_long( return_value, mb.keys[i], *(long *)mb.values[i].buffer );
		}
	}

	gb_reply_multi_free(&mb);
}

PHP_METHOD(Gibson, quit)
{
	zval *object;
	gbClient *sock;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &object, gibson_ce ) == FAILURE) {
		RETURN_FALSE;
	}

	if (gibson_sock_get(object, &sock TSRMLS_CC, 0) < 0)
		RETURN_FALSE;

	if( gb_quit( sock ) != 0 )
		RETURN_FALSE;

	RETURN_TRUE;
}

