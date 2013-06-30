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
#ifndef PHP_GIBSON_H
#define PHP_GIBSON_H 1
#define PHP_GIBSON_VERSION "1.0.0"
#define PHP_GIBSON_EXTNAME "gibson"
#define PHP_GIBSON_SOCK_NAME "Gibson Socket Buffer"

#define PHP_GIBSON_EXCEPTION(msg) zend_throw_exception(zend_exception_get_default(TSRMLS_C), msg, 0 TSRMLS_CC)

PHP_METHOD(Gibson, __construct);
PHP_METHOD(Gibson, __destruct);
PHP_METHOD(Gibson, getLastError);
PHP_METHOD(Gibson, connect);
PHP_METHOD(Gibson, set);
PHP_METHOD(Gibson, mset);
PHP_METHOD(Gibson, ttl);
PHP_METHOD(Gibson, mttl);
PHP_METHOD(Gibson, get);
PHP_METHOD(Gibson, mget);
PHP_METHOD(Gibson, del);
PHP_METHOD(Gibson, mdel);
PHP_METHOD(Gibson, inc);
PHP_METHOD(Gibson, minc);
PHP_METHOD(Gibson, mdec);
PHP_METHOD(Gibson, dec);
PHP_METHOD(Gibson, lock);
PHP_METHOD(Gibson, mlock);
PHP_METHOD(Gibson, unlock);
PHP_METHOD(Gibson, munlock);
PHP_METHOD(Gibson, count);
PHP_METHOD(Gibson, stats);
PHP_METHOD(Gibson, ping);
PHP_METHOD(Gibson, quit);

PHP_MINIT_FUNCTION(gibson);
PHP_MSHUTDOWN_FUNCTION(gibson);
PHP_RINIT_FUNCTION(gibson);
PHP_RSHUTDOWN_FUNCTION(gibson);
PHP_MINFO_FUNCTION(gibson);

#ifndef _MSC_VER
ZEND_BEGIN_MODULE_GLOBALS(gibson)
ZEND_END_MODULE_GLOBALS(gibson)
#endif

extern zend_module_entry gibson_module_entry;
#define phpext_gibson_ptr &gibson_module_entry

#endif
