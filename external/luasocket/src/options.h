#ifndef OPTIONS_H
#define OPTIONS_H
/*=========================================================================*\
* Common option interface 
* LuaSocket toolkit
*
* This module provides a common interface to socket options, used mainly by
* modules UDP and TCP. 
\*=========================================================================*/

#include "lua.h"
#include "socket.h"

/* option registry */
typedef struct t_opt {
  const char *name;
  int (*func)(lua_State *L, p_socket ps);
} t_opt;
typedef t_opt *p_opt;

/* supported options for setoption */
int opt_set_dontroute(lua_State *L, p_socket ps);
int opt_set_broadcast(lua_State *L, p_socket ps);
int opt_set_tcp_nodelay(lua_State *L, p_socket ps);
int opt_set_keepalive(lua_State *L, p_socket ps);
int opt_set_linger(lua_State *L, p_socket ps);
int opt_set_reuseaddr(lua_State *L, p_socket ps);
int opt_set_reuseport(lua_State *L, p_socket ps);
int opt_set_ip_multicast_if(lua_State *L, p_socket ps);
int opt_set_ip_multicast_ttl(lua_State *L, p_socket ps);
int opt_set_ip_multicast_loop(lua_State *L, p_socket ps);
int opt_set_ip_add_membership(lua_State *L, p_socket ps);
int opt_set_ip_drop_membersip(lua_State *L, p_socket ps);
int opt_set_ip6_unicast_hops(lua_State *L, p_socket ps);
int opt_set_ip6_multicast_hops(lua_State *L, p_socket ps);
int opt_set_ip6_multicast_loop(lua_State *L, p_socket ps);
int opt_set_ip6_add_membership(lua_State *L, p_socket ps);
int opt_set_ip6_drop_membersip(lua_State *L, p_socket ps);
int opt_set_ip6_v6only(lua_State *L, p_socket ps);

/* supported options for getoption */
int opt_get_dontroute(lua_State *L, p_socket ps);
int opt_get_broadcast(lua_State *L, p_socket ps);
int opt_get_reuseaddr(lua_State *L, p_socket ps);
int opt_get_reuseport(lua_State *L, p_socket ps);
int opt_get_tcp_nodelay(lua_State *L, p_socket ps);
int opt_get_keepalive(lua_State *L, p_socket ps);
int opt_get_linger(lua_State *L, p_socket ps);
int opt_get_ip_multicast_loop(lua_State *L, p_socket ps);
int opt_get_ip_multicast_if(lua_State *L, p_socket ps);
int opt_get_error(lua_State *L, p_socket ps);
int opt_get_ip6_multicast_loop(lua_State *L, p_socket ps);
int opt_get_ip6_multicast_hops(lua_State *L, p_socket ps);
int opt_get_ip6_unicast_hops(lua_State *L, p_socket ps);
int opt_get_ip6_v6only(lua_State *L, p_socket ps);
int opt_get_reuseport(lua_State *L, p_socket ps);

/* invokes the appropriate option handler */
int opt_meth_setoption(lua_State *L, p_opt opt, p_socket ps);
int opt_meth_getoption(lua_State *L, p_opt opt, p_socket ps);

#endif

#if 0 // old code

#ifndef OPTIONS_H
#define OPTIONS_H
/*=========================================================================*\
* Common option interface 
* LuaSocket toolkit
*
* This module provides a common interface to socket options, used mainly by
* modules UDP and TCP. 
*
* RCS ID: $Id: options.h,v 1.4 2005/10/07 04:40:59 diego Exp $
\*=========================================================================*/

#include "lua.h"
#include "socket.h"

/* option registry */
typedef struct t_opt {
  const char *name;
  int (*func)(lua_State *L, p_socket ps);
} t_opt;
typedef t_opt *p_opt;

/* supported options */
int opt_dontroute(lua_State *L, p_socket ps);
int opt_broadcast(lua_State *L, p_socket ps);
int opt_reuseaddr(lua_State *L, p_socket ps);
int opt_tcp_nodelay(lua_State *L, p_socket ps);
int opt_keepalive(lua_State *L, p_socket ps);
int opt_linger(lua_State *L, p_socket ps);
int opt_reuseaddr(lua_State *L, p_socket ps);
int opt_ip_multicast_ttl(lua_State *L, p_socket ps);
int opt_ip_multicast_loop(lua_State *L, p_socket ps);
int opt_ip_add_membership(lua_State *L, p_socket ps);
int opt_ip_drop_membersip(lua_State *L, p_socket ps);

/* invokes the appropriate option handler */
int opt_meth_setoption(lua_State *L, p_opt opt, p_socket ps);

/*--------------------------------------------------------------------------
 * LuaSec 0.4.1
 * Copyright (C) 2006-2011 Bruno Silvestre
 *
 *--------------------------------------------------------------------------*/

struct ssl_option_s {
  const char *name;
  unsigned long code;
};
typedef struct ssl_option_s ssl_option_t;

/*
-- Supported SSL options and script in Lua 5.1 to generate the file.
-- Ugly, but easier to maintain.

local options = [[
SSL_OP_ALL
SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
SSL_OP_CIPHER_SERVER_PREFERENCE
SSL_OP_CISCO_ANYCONNECT
SSL_OP_COOKIE_EXCHANGE
SSL_OP_CRYPTOPRO_TLSEXT_BUG
SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
SSL_OP_EPHEMERAL_RSA
SSL_OP_LEGACY_SERVER_CONNECT
SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER
SSL_OP_MICROSOFT_SESS_ID_BUG
SSL_OP_MSIE_SSLV2_RSA_PADDING
SSL_OP_NETSCAPE_CA_DN_BUG
SSL_OP_NETSCAPE_CHALLENGE_BUG
SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG
SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG
SSL_OP_NO_COMPRESSION
SSL_OP_NO_QUERY_MTU
SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
SSL_OP_NO_SSLv2
SSL_OP_NO_SSLv3
SSL_OP_NO_TICKET
SSL_OP_NO_TLSv1
SSL_OP_PKCS1_CHECK_1
SSL_OP_PKCS1_CHECK_2
SSL_OP_SINGLE_DH_USE
SSL_OP_SINGLE_ECDH_USE
SSL_OP_SSLEAY_080_CLIENT_DH_BUG
SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG
SSL_OP_TLS_BLOCK_PADDING_BUG
SSL_OP_TLS_D5_BUG
SSL_OP_TLS_ROLLBACK_BUG
]]

print([[static ssl_option_t ssl_options[] = {]])

for option in string.gmatch(options, "(%S+)") do
  local name = string.lower(string.sub(option, 8))
  print(string.format([[#if defined(%s)]], option))
  print(string.format([[  {"%s", %s},]], name, option))
  print([[#endif]])
end

print([[  {NULL, 0L}]])
print([[};]])
 */

static ssl_option_t ssl_options[] = {
#if defined(SSL_OP_ALL)
  {"all", SSL_OP_ALL},
#endif
#if defined(SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION)
  {"allow_unsafe_legacy_renegotiation", SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION},
#endif
#if defined(SSL_OP_CIPHER_SERVER_PREFERENCE)
  {"cipher_server_preference", SSL_OP_CIPHER_SERVER_PREFERENCE},
#endif
#if defined(SSL_OP_CISCO_ANYCONNECT)
  {"cisco_anyconnect", SSL_OP_CISCO_ANYCONNECT},
#endif
#if defined(SSL_OP_COOKIE_EXCHANGE)
  {"cookie_exchange", SSL_OP_COOKIE_EXCHANGE},
#endif
#if defined(SSL_OP_CRYPTOPRO_TLSEXT_BUG)
  {"cryptopro_tlsext_bug", SSL_OP_CRYPTOPRO_TLSEXT_BUG},
#endif
#if defined(SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS)
  {"dont_insert_empty_fragments", SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS},
#endif
#if defined(SSL_OP_EPHEMERAL_RSA)
  {"ephemeral_rsa", SSL_OP_EPHEMERAL_RSA},
#endif
#if defined(SSL_OP_LEGACY_SERVER_CONNECT)
  {"legacy_server_connect", SSL_OP_LEGACY_SERVER_CONNECT},
#endif
#if defined(SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER)
  {"microsoft_big_sslv3_buffer", SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER},
#endif
#if defined(SSL_OP_MICROSOFT_SESS_ID_BUG)
  {"microsoft_sess_id_bug", SSL_OP_MICROSOFT_SESS_ID_BUG},
#endif
#if defined(SSL_OP_MSIE_SSLV2_RSA_PADDING)
  {"msie_sslv2_rsa_padding", SSL_OP_MSIE_SSLV2_RSA_PADDING},
#endif
#if defined(SSL_OP_NETSCAPE_CA_DN_BUG)
  {"netscape_ca_dn_bug", SSL_OP_NETSCAPE_CA_DN_BUG},
#endif
#if defined(SSL_OP_NETSCAPE_CHALLENGE_BUG)
  {"netscape_challenge_bug", SSL_OP_NETSCAPE_CHALLENGE_BUG},
#endif
#if defined(SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG)
  {"netscape_demo_cipher_change_bug", SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG},
#endif
#if defined(SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG)
  {"netscape_reuse_cipher_change_bug", SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG},
#endif
#if defined(SSL_OP_NO_COMPRESSION)
  {"no_compression", SSL_OP_NO_COMPRESSION},
#endif
#if defined(SSL_OP_NO_QUERY_MTU)
  {"no_query_mtu", SSL_OP_NO_QUERY_MTU},
#endif
#if defined(SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION)
  {"no_session_resumption_on_renegotiation", SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION},
#endif
#if defined(SSL_OP_NO_SSLv2)
  {"no_sslv2", SSL_OP_NO_SSLv2},
#endif
#if defined(SSL_OP_NO_SSLv3)
  {"no_sslv3", SSL_OP_NO_SSLv3},
#endif
#if defined(SSL_OP_NO_TICKET)
  {"no_ticket", SSL_OP_NO_TICKET},
#endif
#if defined(SSL_OP_NO_TLSv1)
  {"no_tlsv1", SSL_OP_NO_TLSv1},
#endif
#if defined(SSL_OP_PKCS1_CHECK_1)
  {"pkcs1_check_1", SSL_OP_PKCS1_CHECK_1},
#endif
#if defined(SSL_OP_PKCS1_CHECK_2)
  {"pkcs1_check_2", SSL_OP_PKCS1_CHECK_2},
#endif
#if defined(SSL_OP_SINGLE_DH_USE)
  {"single_dh_use", SSL_OP_SINGLE_DH_USE},
#endif
#if defined(SSL_OP_SINGLE_ECDH_USE)
  {"single_ecdh_use", SSL_OP_SINGLE_ECDH_USE},
#endif
#if defined(SSL_OP_SSLEAY_080_CLIENT_DH_BUG)
  {"ssleay_080_client_dh_bug", SSL_OP_SSLEAY_080_CLIENT_DH_BUG},
#endif
#if defined(SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG)
  {"sslref2_reuse_cert_type_bug", SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG},
#endif
#if defined(SSL_OP_TLS_BLOCK_PADDING_BUG)
  {"tls_block_padding_bug", SSL_OP_TLS_BLOCK_PADDING_BUG},
#endif
#if defined(SSL_OP_TLS_D5_BUG)
  {"tls_d5_bug", SSL_OP_TLS_D5_BUG},
#endif
#if defined(SSL_OP_TLS_ROLLBACK_BUG)
  {"tls_rollback_bug", SSL_OP_TLS_ROLLBACK_BUG},
#endif
  {NULL, 0L}
};

#endif
#endif // 0
