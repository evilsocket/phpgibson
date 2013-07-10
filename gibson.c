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
} gbContext;

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

	gb_disconnect(ctx->socket TSRMLS_CC);

	efree(ctx->socket);
	efree(ctx);
}
/* }}} */

static void gibson_psocket_destructor(zend_rsrc_list_entry * rsrc TSRMLS_DC) /* {{{ */
{
	gbContext *ctx = (gbContext *)rsrc->ptr;

	gb_debug("psocket destructor %d", ctx->id);

	gb_disconnect(ctx->socket TSRMLS_CC);

	pefree(ctx->socket,1);
	pefree(ctx,1);
}
/* }}} */

/* retrieve the socket property by object id */
#define GET_SOCKET_RESOURCE(s) {																				\
	zval **_tmp_zval;																							\
	gbContext *ctx;																								\
	if (zend_hash_find(Z_OBJPROP_P(getThis()), "socket", sizeof("socket"), (void **)&_tmp_zval) == FAILURE) {	\
		gb_debug("unable to find socket resource");																\
		RETURN_FALSE;																							\
	}																											\
	\
	ZEND_FETCH_RESOURCE2(ctx, gbContext *, _tmp_zval, -1, "socket", le_gibson_sock, le_gibson_psock);			\
	\
	(s) = ctx->socket;																							\
}

#define GB_IS_UNIX_SOCKET(host ) ( (host)[0] == '/' || (host)[0] == '.')



#define GB_SOCK_CONNECT(ret, sock, host, port, timeout)	\
	gb_disconnect((sock) TSRMLS_CC);						\
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
	zval *object;
	int host_len, id, ret, pkey_len = 0;
	char *host = NULL, *pkey = NULL;
	long port = -1;
	zend_rsrc_list_entry *le, new_le;
	long timeout = 100;
	gbClient *sock = NULL;
	gbContext *ctx = NULL;
	int type;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os|ll", &object, gibson_ce, &host, &host_len, &port, &timeout) == FAILURE) {
		return FAILURE;
	}
	else if (host_len == 0) {
		return FAILURE;
	}
	else if (timeout < 0L || timeout > INT_MAX) {
		return FAILURE;
	}
	/* not unix socket, set to default value */
	else if (port == -1 && host_len && GB_IS_UNIX_SOCKET(host) == 0) {
		port = 10128;
	}

	if (persistent)
	{
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
					gb_debug("reconnecting persistent socket ( ping failed with %d )", ctx->socket->error);
					GB_SOCK_CONNECT(ret, ctx->socket, host, port, timeout);
					if (ret != 0) {
						GB_SOCK_ERROR(ctx, host, port, "reconnect to");
						gb_debug("reconnection failed: %d", ctx->socket->error);
						efree(pkey);
						return FAILURE;
					}
				}

				add_property_resource(object, "socket", ctx->id);
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
			add_property_resource(object, "socket", ctx->id);
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
		add_property_resource(object, "socket", ctx->id);

		gb_debug("succesfully created connection ( id %d )", ctx->id);

		return SUCCESS;
	}
}
/* }}} */


static PHP_METHOD(Gibson, __construct)  /* {{{ */
{
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		RETURN_FALSE;
	}
}
/* }}} */

static PHP_METHOD(Gibson, getLastError)  /* {{{ */
{
	zval *object;
	gbClient *sock;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &object, gibson_ce ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	switch(sock->error)
	{
		case REPL_ERR 	        : RETURN_STRING("Generic error", 1);
		case REPL_ERR_NOT_FOUND : RETURN_STRING("Invalid key, item not found", 1);
		case REPL_ERR_NAN		: RETURN_STRING("Invalid value, not a number", 1);
		case REPL_ERR_MEM       : RETURN_STRING("Gibson server is out of memory", 1);
		case REPL_ERR_LOCKED    : RETURN_STRING("The item is locked", 1);

		default:
								  RETURN_STRING(strerror(sock->error), 1);
	}
}
/* }}} */

static PHP_METHOD(Gibson,__destruct)  /* {{{ */
{
	gbClient *sock;

	if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);
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

#define PHP_GIBSON_RETURN_REPLY()									\
	if (sock->reply.encoding == GB_ENC_PLAIN) {						\
		RETURN_STRINGL( sock->reply.buffer, sock->reply.size, 1);	\
	} else if (sock->reply.encoding == GB_ENC_NUMBER) {			\
		RETURN_LONG(gb_reply_number(sock));						\
	} else {														\
		RETURN_FALSE;												\
	}

static PHP_METHOD(Gibson, set) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL, *val = NULL;
	int key_len, val_len;
	long expire = -1;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oss|l", &object, gibson_ce, &key, &key_len, &val, &val_len, &expire) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_set(sock, key, key_len, val, val_len, expire) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, mset) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL, *val = NULL;
	int key_len, val_len;
	long expire = -1;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Oss", &object, gibson_ce, &key, &key_len, &val, &val_len) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_mset(sock, key, key_len, val, val_len) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, ttl) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;
	long expire = -1;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Osl", &object, gibson_ce, &key, &key_len, &expire) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_ttl(sock, key, key_len, expire) != 0)
		RETURN_FALSE;

	RETURN_TRUE;
}
/* }}} */

static PHP_METHOD(Gibson, mttl) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;
	long expire = -1;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Osl", &object, gibson_ce, &key, &key_len, &expire) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_mttl(sock, key, key_len, expire) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, get) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_get(sock, key, key_len) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, mget) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len, i;
	gbMultiBuffer mb;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_mget(sock, key, key_len) != 0)
		RETURN_FALSE;

	gb_reply_multi(sock, &mb);

	array_init(return_value);

	for (i = 0; i < mb.count; i++) {
		if (mb.values[i].encoding == GB_ENC_PLAIN) {
			add_assoc_stringl(return_value, mb.keys[i], mb.values[i].buffer, mb.values[i].size, 1);
		}
		else if (sock->reply.encoding == GB_ENC_NUMBER) {
			add_assoc_long(return_value, mb.keys[i], *(long *)mb.values[i].buffer);
		}
	}

	gb_reply_multi_free(&mb);
}
/* }}} */

static PHP_METHOD(Gibson, del) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_del(sock, key, key_len) != 0)
		RETURN_FALSE;

	RETURN_TRUE;
}
/* }}} */

static PHP_METHOD(Gibson, mdel) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_mdel(sock, key, key_len) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, inc) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_inc(sock, key, key_len) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, minc) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_minc(sock, key, key_len) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, dec) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_dec(sock, key, key_len) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, mdec) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_mdec(sock, key, key_len) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, lock) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;
	long time = -1;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Osl", &object, gibson_ce, &key, &key_len, &time) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_lock(sock, key, key_len, time) != 0)
		RETURN_FALSE;

	RETURN_TRUE;
}
/* }}} */

static PHP_METHOD(Gibson, mlock) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;
	long time = -1;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Osl", &object, gibson_ce, &key, &key_len, &time) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_mlock(sock, key, key_len, time) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, unlock) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_unlock(sock, key, key_len) != 0)
		RETURN_FALSE;

	RETURN_TRUE;
}
/* }}} */

static PHP_METHOD(Gibson, munlock) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_munlock(sock, key, key_len) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, count) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_count(sock, key, key_len) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, sizeof) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_sizeof(sock, key, key_len) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, msizeof) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_msizeof(sock, key, key_len) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, encof) /* {{{ */
{
	zval *object;
	gbClient *sock;
	char *key = NULL;
	int key_len;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "Os", &object, gibson_ce, &key, &key_len ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_encof(sock, key, key_len) != 0)
		RETURN_FALSE;

	PHP_GIBSON_RETURN_REPLY();
}
/* }}} */

static PHP_METHOD(Gibson, stats) /* {{{ */
{
	zval *object;
	gbClient *sock;
	int i;
	gbMultiBuffer mb;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &object, gibson_ce ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_stats(sock) != 0) {
		RETURN_FALSE;
	}

	gb_reply_multi(sock, &mb);

	array_init(return_value);

	for (i = 0; i < mb.count; i++) {
		if (mb.values[i].encoding == GB_ENC_PLAIN) {
			add_assoc_stringl(return_value, mb.keys[i], mb.values[i].buffer, mb.values[i].size, 1);
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
	zval *object;
	gbClient *sock;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &object, gibson_ce ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_ping(sock) != 0)
		RETURN_FALSE;

	RETURN_TRUE;
}
/* }}} */

static PHP_METHOD(Gibson, quit) /* {{{ */
{
	zval *object;
	gbClient *sock;

	if (zend_parse_method_parameters(ZEND_NUM_ARGS() TSRMLS_CC, getThis(), "O", &object, gibson_ce ) == FAILURE) {
		RETURN_FALSE;
	}

	GET_SOCKET_RESOURCE(sock);

	if (gb_quit(sock) != 0)
		RETURN_FALSE;

	RETURN_TRUE;
}
/* }}} */

static zend_function_entry gibson_functions[] = { /* {{{ */
	PHP_ME(Gibson, __construct, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, __destruct, NULL, ZEND_ACC_PUBLIC)
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
	PHP_ME(Gibson, sizeof, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, msizeof, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, encof, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, stats, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, ping, NULL, ZEND_ACC_PUBLIC)
	PHP_ME(Gibson, quit, NULL, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

static PHP_MINIT_FUNCTION(gibson) /* {{{ */
{
	zend_class_entry gibson_class_entry;

	/* Gibson class */
	INIT_CLASS_ENTRY(gibson_class_entry, "Gibson", gibson_functions);
	gibson_ce = zend_register_internal_class(&gibson_class_entry TSRMLS_CC);

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
