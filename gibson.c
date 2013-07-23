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
#include "ext/standard/info.h"
#include "php_gibson.h"
#include <gibson.h>

typedef struct {
	int id;
	gbClient *socket;
	zend_bool persistent;
} gbContext;

static zend_object_handlers php_gibson_client_handlers;

typedef struct _php_gibson_client {
	zend_object std;
	gbContext *ctx;
} php_gibson_client;

int le_gibson_sock;
int le_gibson_psock;

zend_class_entry *gibson_ce;

#ifdef COMPILE_DL_GIBSON
ZEND_GET_MODULE(gibson)
#endif

#define GB_DEBUG_ENABLED 0

static void gb_debug(const char *format, ...) /* {{{ */
{
#if GB_DEBUG_ENABLED == 1
	TSRMLS_FETCH();

	char buffer[1024] = {0};
	va_list args;

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer)-1, format, args);
	va_end(args);

	FILE *fp = fopen("/tmp/phpgibson_debug.log", "a+t");
	if (fp) {
		fprintf(fp, "%s\n", buffer);
		fflush(fp);
		fclose(fp);
	}
#else

#endif
}
/* }}} */

static void add_constant_long(zend_class_entry *ce, char *name, int value) /* {{{ */
{
	zval *constval;

	constval = pemalloc(sizeof(zval), 1);
	INIT_PZVAL(constval);
	ZVAL_LONG(constval, value);
	zend_hash_add(&ce->constants_table, name, 1 + strlen(name), (void*)&constval, sizeof(zval*), NULL);
}
/* }}} */

static void gibson_socket_destructor(zend_rsrc_list_entry * rsrc TSRMLS_DC) /* {{{ */
{
	gbContext *ctx = (gbContext *)rsrc->ptr;

	gb_debug("socket destructor %d", ctx->id);

	gb_disconnect(ctx->socket);

	efree(ctx->socket);
	efree(ctx);
}
/* }}} */

static void gibson_psocket_destructor(zend_rsrc_list_entry * rsrc TSRMLS_DC) /* {{{ */
{
	gbContext *ctx = (gbContext *)rsrc->ptr;

	gb_debug("psocket destructor %d", ctx->id);

	gb_disconnect(ctx->socket);

	pefree(ctx->socket,1);
	pefree(ctx,1);
}
/* }}} */

static void php_gibson_client_obj_dtor(void *object TSRMLS_DC) /* {{{ */
{
	php_gibson_client *c = (php_gibson_client *)object;

	if (c) {
		if (c->ctx) {
			if (c->ctx->id >= 0) {
				zend_list_delete(c->ctx->id);
			}
			efree(c->ctx);
		}
		zend_object_std_dtor(&c->std TSRMLS_CC);
		efree(c);
	}
}
/* }}} */

static zend_object_value php_gibson_client_new(zend_class_entry *ce TSRMLS_DC) /* {{{ */
{
	php_gibson_client *c;
	zend_object_value retval;
#if PHP_VERSION_ID < 50399
	zval *tmp;
#endif

	c = ecalloc(1, sizeof(*c));
	zend_object_std_init(&c->std, ce TSRMLS_CC);

	if (!c->std.properties) {
		ALLOC_HASHTABLE(c->std.properties);
		zend_hash_init(c->std.properties, 0, NULL, ZVAL_PTR_DTOR, 0);
	}
#if PHP_VERSION_ID < 50399
	zend_hash_copy(c->std.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
#else
	object_properties_init(&c->std, ce);
#endif
	retval.handle = zend_objects_store_put(c, (zend_objects_store_dtor_t)zend_objects_destroy_object, php_gibson_client_obj_dtor, NULL TSRMLS_CC);
	retval.handlers = &php_gibson_client_handlers;
	return retval;
}
/* }}} */

#define GB_IS_UNIX_SOCKET(host ) ( (host)[0] == '/' || (host)[0] == '.')

#define GB_SOCK_CONNECT(ret, sock, host, port, timeout)	\
	gb_disconnect((sock));						\
	if (GB_IS_UNIX_SOCKET(host)) {							\
		(ret) = gb_unix_connect((sock), (host), (timeout)); \
	} else {												\
		(ret) = gb_tcp_connect((sock), (host), (port), (timeout));\
	}

#define GB_SOCK_ERROR(ctx, host, port, action)																						\
	if (GB_IS_UNIX_SOCKET((host))) {																								\
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to %s socket '%s': %s (%d)", (action), (host), strerror((ctx)->socket->error), (ctx)->socket->error);	\
	} else {																														\
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "failed to %s '%s:%ld': %s (%d)", (action), (host), (port), strerror((ctx)->socket->error), (ctx)->socket->error);	\
	}

#define PHP_GIBSON_INITIALIZED(c)																	\
	if (!(c) || !(c)->ctx) {																		\
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "using uninitialized Gibson client object");	\
		RETURN_FALSE;																				\
	}

#define PHP_GIBSON_CONNECTED(c)																		\
	PHP_GIBSON_INITIALIZED(c);																		\
	if ((c)->ctx->socket == NULL) {																	\
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "using unconnected Gibson client object");		\
		RETURN_FALSE;																				\
	}

/* get a persistent socket identifier */
static void gibson_socket_pid(char *address, int port, int timeout, char **pkey, int *pkey_len) /* {{{ */
{
	/* unix domain socket */
	if (GB_IS_UNIX_SOCKET(address)) {
		*pkey_len = spprintf(&( *pkey ), 0, "phpgibson_%s_%d", address, timeout);
	}
	/* tcp socket */
	else {
		*pkey_len = spprintf(&( *pkey ), 0, "phpgibson_%s_%d_%d", address, port, timeout);
	}
}
/* }}} */

PHPAPI int gibson_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent)  /* {{{ */
{
	php_gibson_client *c;
	int host_len, ret, pkey_len = 0;
	char *host = NULL, *pkey = NULL;
	long port = -1;
	zend_rsrc_list_entry *le, new_le;
	long timeout = 100;
	gbContext *ctx = NULL;
	int type;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|ll", &host, &host_len, &port, &timeout) == FAILURE) {
		return FAILURE;
	}

	if (host_len == 0) {
		return FAILURE;
	}

	if (timeout < 0L || timeout > INT_MAX) {
		return FAILURE;
	}

	/* not unix socket, set to default value */
	if (port == -1 && host_len && GB_IS_UNIX_SOCKET(host) == 0) {
		port = 10128;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);

	if (!c || !c->ctx) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "using uninitialized Gibson client object");
		return FAILURE;
	}

	if (c->ctx && c->ctx->id >= 0) {
		/* close existing connection */
		zend_list_delete(c->ctx->id);
		c->ctx->socket = NULL;
	}

	if (persistent) {
		gibson_socket_pid(host, port, timeout, &pkey, &pkey_len);

		/* search for persistent connections */
		if (zend_hash_find( &EG(persistent_list), pkey, pkey_len + 1, (void **)&le) == SUCCESS) {
			gb_debug("reusing persistent socket %s", pkey);

			if (le->type != le_gibson_psock) {
				/* this is something strange, kill it */
				zend_hash_del(&EG(persistent_list), pkey, pkey_len + 1);
				efree(pkey);
				return FAILURE;
			} else {

				ctx = le->ptr;

				/* sanity check to ensure that the resource is still a regular resource number */
				if (zend_list_find(ctx->id, &type) == ctx) {
					/* add a reference to the persistent socket  */
					zend_list_addref(ctx->id);
				} else {
					/* this is totally ok, persistent connection is not present in the regular list at the beginning of a request */
					ctx->id = ZEND_REGISTER_RESOURCE(NULL, ctx, le_gibson_psock);
				}

				/* Make sure the socket is connected (with a PING command) */
				if (gb_ping(ctx->socket) != 0) {
					char gb_last_error[1024] = {0};
					gb_getlasterror( gb_last_error, 1024 );

					gb_debug("reconnecting persistent socket ( ping failed with '%s' )", gb_last_error );
					GB_SOCK_CONNECT(ret, ctx->socket, host, port, timeout);
					if (ret != 0) {
						GB_SOCK_ERROR(ctx, host, port, "reconnect to");
						gb_debug("reconnection failed: %d", ctx->socket->error);
						efree(pkey);
						return FAILURE;
					}
				}

				c->ctx->id = ctx->id;
				c->ctx->socket = ctx->socket;

				efree(pkey);
				return SUCCESS;
			}
		}
		/* no connection created with this persistence key yet */
		else {
			gb_debug("creating persistent socket %s", pkey);

			ctx = pecalloc(1, sizeof(gbContext), 1);
			ctx->socket = pecalloc(1, sizeof(gbClient), 1);

			GB_SOCK_CONNECT(ret, ctx->socket, host, port, timeout);
			if (ret != 0) {
				GB_SOCK_ERROR(ctx, host, port, "connect to");
				gb_debug("connection failed: %d", ctx->socket->error);
				efree(pkey);
				pefree(ctx->socket,1);
				pefree(ctx,1);
				return FAILURE;
			}

			/* store the reference */
			new_le.ptr  = ctx;
			new_le.type = le_gibson_psock;

			zend_hash_update(&EG(persistent_list), pkey, pkey_len + 1, &new_le, sizeof(zend_rsrc_list_entry), NULL);

			ctx->id = ZEND_REGISTER_RESOURCE(NULL, ctx, le_gibson_psock);

			c->ctx->id = ctx->id;
			c->ctx->socket = ctx->socket;
		}

		gb_debug("succesfully created persistent connection ( id %d )", ctx->id);

		efree(pkey);
		return SUCCESS;
	}
	/* non persistent connection */
	else {
		gb_debug("creating volatile connection");

		ctx = ecalloc(1, sizeof(gbContext));
		ctx->socket = ecalloc(1, sizeof(gbClient) );

		GB_SOCK_CONNECT(ret, ctx->socket, host, port, timeout);
		if (ret != 0) {
			GB_SOCK_ERROR(ctx, host, port, "connect to");
			gb_debug("connection failed: %d", ctx->socket->error);
			efree(ctx->socket);
			efree(ctx);
			return FAILURE;
		}

		ctx->id = ZEND_REGISTER_RESOURCE(NULL, ctx, le_gibson_sock);

		c->ctx->id = ctx->id;
		c->ctx->socket = ctx->socket;
		gb_debug("succesfully created connection ( id %d )", ctx->id);

		return SUCCESS;
	}
}
/* }}} */


static PHP_METHOD(Gibson, __construct)  /* {{{ */
{
	php_gibson_client *c;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);

	/* called the __construct() twice */
	if (c->ctx) {
		return;
	}

	c->ctx = ecalloc(1, sizeof(gbContext));
	c->ctx->id = -1;
}
/* }}} */

static PHP_METHOD(Gibson, getLastError)  /* {{{ */
{
	php_gibson_client *c;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	switch(c->ctx->socket->error)
	{
		case REPL_ERR 	        : RETURN_STRING("Generic error", 1);
		case REPL_ERR_NOT_FOUND : RETURN_STRING("Invalid key, item not found", 1);
		case REPL_ERR_NAN		: RETURN_STRING("Invalid value, not a number", 1);
		case REPL_ERR_MEM       : RETURN_STRING("Gibson server is out of memory", 1);
		case REPL_ERR_LOCKED    : RETURN_STRING("The item is locked", 1);

		default:
								  RETURN_STRING(strerror(c->ctx->socket->error), 1);
	}
}
/* }}} */

static PHP_METHOD(Gibson, connect) /* {{{ */
{
	if (gibson_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0) == FAILURE) {
		RETURN_FALSE;
	} else {
		RETURN_TRUE;
	}
}
/* }}} */

static PHP_METHOD(Gibson, pconnect) /* {{{ */
{
	if (gibson_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1) == FAILURE) {
		RETURN_FALSE;
	} else {
		RETURN_TRUE;
	}
}
/* }}} */

#define PHP_GIBSON_RETURN_REPLY(c)												\
	if ((c)->ctx->socket->reply.encoding == GB_ENC_PLAIN) {						\
		RETURN_STRINGL((char *)(c)->ctx->socket->reply.buffer, (c)->ctx->socket->reply.size, 1);		\
	} else if ((c)->ctx->socket->reply.encoding == GB_ENC_NUMBER) {							\
		RETURN_LONG(gb_reply_number(&(c)->ctx->socket->reply));										\
	} else {																	\
		RETURN_FALSE;															\
	}

static PHP_METHOD(Gibson, set) /* {{{ */
{
	php_gibson_client *c;
	char *key, *val;
	int key_len, val_len;
	long expire = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|l", &key, &key_len, &val, &val_len, &expire) == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	if (gb_set(c->ctx->socket, key, key_len, val, val_len, expire) != 0) {
		RETURN_FALSE;
	}

	PHP_GIBSON_RETURN_REPLY(c);
}
/* }}} */

static PHP_METHOD(Gibson, mset) /* {{{ */
{
	php_gibson_client *c;
	char *key, *val;
	int key_len, val_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &key_len, &val, &val_len) == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	if (gb_mset(c->ctx->socket, key, key_len, val, val_len) != 0) {
		RETURN_FALSE;
	}

	PHP_GIBSON_RETURN_REPLY(c);
}
/* }}} */

static PHP_METHOD(Gibson, ttl) /* {{{ */
{
	php_gibson_client *c;
	char *key;
	int key_len;
	long expire = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &key, &key_len, &expire) == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	if (gb_ttl(c->ctx->socket, key, key_len, expire) != 0) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

static PHP_METHOD(Gibson, mttl) /* {{{ */
{
	php_gibson_client *c;
	char *key;
	int key_len;
	long expire = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &key, &key_len, &expire) == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	if (gb_mttl(c->ctx->socket, key, key_len, expire) != 0) {
		RETURN_FALSE;
	}

	PHP_GIBSON_RETURN_REPLY(c);
}
/* }}} */

static PHP_METHOD(Gibson, mget) /* {{{ */
{
	php_gibson_client *c;
	char *key;
	int key_len, i;
	gbMultiBuffer mb;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	if (gb_mget(c->ctx->socket, key, key_len) != 0) {
		RETURN_FALSE;
	}

	gb_reply_multi(c->ctx->socket, &mb);

	array_init(return_value);

	for (i = 0; i < mb.count; i++) {
		if (mb.values[i].encoding == GB_ENC_PLAIN) {
			add_assoc_stringl(return_value, mb.keys[i], (char *)mb.values[i].buffer, mb.values[i].size, 1);
		}
		else if (c->ctx->socket->reply.encoding == GB_ENC_NUMBER) {
			add_assoc_long(return_value, mb.keys[i], *(long *)mb.values[i].buffer);
		}
	}

	gb_reply_multi_free(&mb);
}
/* }}} */

#define GIBSON_KEY_ONLY_METHOD_BOOL(name)		\
static PHP_METHOD(Gibson, name)				\
{											\
	php_gibson_client *c;					\
	char *key;								\
	int key_len;							\
											\
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) { \
		RETURN_FALSE;						\
	}										\
											\
	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);	\
	PHP_GIBSON_CONNECTED(c);				\
											\
	if (gb_ ##name(c->ctx->socket, key, key_len) != 0) { \
		RETURN_FALSE;						\
	}										\
	RETURN_TRUE;							\
}

#define GIBSON_KEY_ONLY_METHOD_VALUE(name)	\
static PHP_METHOD(Gibson, name)				\
{											\
	php_gibson_client *c;					\
	char *key;								\
	int key_len;							\
											\
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) { \
		RETURN_FALSE;						\
	}										\
											\
	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);	\
	PHP_GIBSON_CONNECTED(c);				\
											\
	if (gb_ ##name(c->ctx->socket, key, key_len) != 0) { \
		RETURN_FALSE;						\
	}										\
	PHP_GIBSON_RETURN_REPLY(c);				\
}

GIBSON_KEY_ONLY_METHOD_VALUE(get);

GIBSON_KEY_ONLY_METHOD_BOOL(del);

GIBSON_KEY_ONLY_METHOD_VALUE(mdel);

GIBSON_KEY_ONLY_METHOD_VALUE(inc);

GIBSON_KEY_ONLY_METHOD_VALUE(minc);

GIBSON_KEY_ONLY_METHOD_VALUE(dec);

GIBSON_KEY_ONLY_METHOD_VALUE(mdec);

GIBSON_KEY_ONLY_METHOD_BOOL(unlock);

GIBSON_KEY_ONLY_METHOD_VALUE(munlock);

GIBSON_KEY_ONLY_METHOD_VALUE(count);

static PHP_METHOD(Gibson, lock) /* {{{ */
{
	php_gibson_client *c;
	char *key;
	int key_len;
	long time = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &key, &key_len, &time) == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	if (gb_lock(c->ctx->socket, key, key_len, time) != 0) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

static PHP_METHOD(Gibson, mlock) /* {{{ */
{
	php_gibson_client *c;
	char *key;
	int key_len;
	long time = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &key, &key_len, &time) == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	if (gb_mlock(c->ctx->socket, key, key_len, time) != 0) {
		RETURN_FALSE;
	}

	PHP_GIBSON_RETURN_REPLY(c);
}
/* }}} */

static PHP_METHOD(Gibson, meta) /* {{{ */
{
	php_gibson_client *c;
	char *key, *meta;
	int key_len, meta_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &key_len, &meta, &meta_len) == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	if (gb_meta(c->ctx->socket, key, key_len, meta, meta_len) != 0) {
		RETURN_FALSE;
	}

	PHP_GIBSON_RETURN_REPLY(c);
}
/* }}} */

static PHP_METHOD(Gibson, keys) /* {{{ */
{
	php_gibson_client *c;
	char *key;
	int key_len, i;
	gbMultiBuffer mb;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	if (gb_keys(c->ctx->socket, key, key_len) != 0) {
		RETURN_FALSE;
	}

	gb_reply_multi(c->ctx->socket, &mb);

	array_init(return_value);

	for (i = 0; i < mb.count; i++) {
		if (mb.values[i].encoding == GB_ENC_PLAIN) {
			add_assoc_stringl(return_value, mb.keys[i], (char *)mb.values[i].buffer, mb.values[i].size, 1);
		}
		else if (mb.values[i].encoding == GB_ENC_NUMBER) {
			add_assoc_long(return_value, mb.keys[i], *(long *)mb.values[i].buffer);
		}
	}

	gb_reply_multi_free(&mb);
}
/* }}} */

static PHP_METHOD(Gibson, stats) /* {{{ */
{
	php_gibson_client *c;
	int i;
	gbMultiBuffer mb;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	if (gb_stats(c->ctx->socket) != 0) {
		RETURN_FALSE;
	}

	gb_reply_multi(c->ctx->socket, &mb);

	array_init(return_value);

	for (i = 0; i < mb.count; i++) {
		if (mb.values[i].encoding == GB_ENC_PLAIN) {
			add_assoc_stringl(return_value, mb.keys[i], (char *)mb.values[i].buffer, mb.values[i].size, 1);
		}
		else if (mb.values[i].encoding == GB_ENC_NUMBER) {
			add_assoc_long(return_value, mb.keys[i], *(long *)mb.values[i].buffer);
		}
	}

	gb_reply_multi_free(&mb);
}
/* }}} */

static PHP_METHOD(Gibson, ping) /* {{{ */
{
	php_gibson_client *c;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	if (gb_ping(c->ctx->socket) != 0) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

static PHP_METHOD(Gibson, quit) /* {{{ */
{
	php_gibson_client *c;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		RETURN_FALSE;
	}

	c = (php_gibson_client *)zend_object_store_get_object(getThis() TSRMLS_CC);
	PHP_GIBSON_CONNECTED(c);

	if (gb_quit(c->ctx->socket) != 0) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

static zend_function_entry gibson_functions[] = { /* {{{ */
	PHP_ME(Gibson, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, getLastError, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, connect, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, pconnect, NULL, ZEND_ACC_PUBLIC)
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
	PHP_ME(Gibson, meta, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, keys, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, stats, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, ping, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, quit, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

static PHP_MINIT_FUNCTION(gibson) /* {{{ */
{
	zend_class_entry gibson_class_entry;

	memcpy(&php_gibson_client_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_gibson_client_handlers.clone_obj = NULL;

	/* Gibson class */
	INIT_CLASS_ENTRY(gibson_class_entry, "Gibson", gibson_functions);
	gibson_ce = zend_register_internal_class(&gibson_class_entry TSRMLS_CC);
	gibson_ce->create_object = php_gibson_client_new;

	le_gibson_sock = zend_register_list_destructors_ex(gibson_socket_destructor, NULL, PHP_GIBSON_SOCK_NAME, module_number);

	le_gibson_psock = zend_register_list_destructors_ex(NULL, gibson_psocket_destructor, PHP_GIBSON_SOCK_NAME, module_number);

	add_constant_long(gibson_ce, "REPL_ERR", REPL_ERR);
	add_constant_long(gibson_ce, "REPL_ERR_NOT_FOUND", REPL_ERR_NOT_FOUND);
	add_constant_long(gibson_ce, "REPL_ERR_NAN", REPL_ERR_NAN);
	add_constant_long(gibson_ce, "REPL_ERR_MEM", REPL_ERR_MEM);
	add_constant_long(gibson_ce, "REPL_ERR_LOCKED", REPL_ERR_LOCKED);
	add_constant_long(gibson_ce, "REPL_OK", REPL_OK);
	add_constant_long(gibson_ce, "REPL_VAL", REPL_VAL);
	add_constant_long(gibson_ce, "REPL_KVAL", REPL_KVAL);

	add_constant_long(gibson_ce, "ENC_PLAIN", GB_ENC_PLAIN);
	add_constant_long(gibson_ce, "ENC_NUMBER", GB_ENC_NUMBER);
	add_constant_long(gibson_ce, "ENC_LZF", GB_ENC_LZF);

	return SUCCESS;
}
/* }}} */

static PHP_MSHUTDOWN_FUNCTION(gibson) /* {{{ */
{
	gb_debug("MSHUTDOWN");
	return SUCCESS;
}
/* }}} */

static PHP_RINIT_FUNCTION(gibson) /* {{{ */
{
	gb_debug("RINIT");
	return SUCCESS;
}
/* }}} */

static PHP_RSHUTDOWN_FUNCTION(gibson) /* {{{ */
{
	gb_debug("RSHUTDOWN");
	return SUCCESS;
}
/* }}} */

static PHP_MINFO_FUNCTION(gibson) /* {{{ */
{
	php_info_print_table_start();
	php_info_print_table_header(2, "Gibson Support", "enabled");
	php_info_print_table_row(2, "Gibson Version", PHP_GIBSON_VERSION);
	php_info_print_table_end();
}
/* }}} */


zend_module_entry gibson_module_entry = { /* {{{ */
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
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
