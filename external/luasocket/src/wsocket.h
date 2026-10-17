#ifndef WSOCKET_H
#define WSOCKET_H
/*=========================================================================*\
* Socket compatibilization module for Win32
* LuaSocket toolkit
\*=========================================================================*/

/*
 * If project does not target a specific Windows desktop OS version, then target Windows XP.
 * When targeting XP, inet_pton() function won't be defined in "WS2tcpip.h" file,
 * forcing the LuaSocket library to use its internal implementation.
 */
#if (defined(_WIN32) && !defined(WINAPI_FAMILY)) || (defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP))
#	if !defined(WINVER) && !defined(_WIN32_WINNT)
#		define WINVER 0x0501
#		define _WIN32_WINNT WINVER
#	endif
#endif

/*=========================================================================*\
* WinSock include files
\*=========================================================================*/
#include <winsock2.h>
#include <ws2tcpip.h>
#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY != WINAPI_FAMILY_DESKTOP_APP)
#      include <ws2def.h>
#      include <ws2ipdef.h>
#endif

typedef int socklen_t;
typedef SOCKADDR_STORAGE t_sockaddr_storage;
typedef SOCKET t_socket;
typedef t_socket *p_socket;

#define WAITFD_R        1
#define WAITFD_W        2
#define WAITFD_E        4
#define WAITFD_C        (WAITFD_E|WAITFD_W)

#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY 27
#endif

#define SOCKET_INVALID (INVALID_SOCKET)

#ifndef SO_REUSEPORT
#define SO_REUSEPORT SO_REUSEADDR
#endif

#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV (0)
#endif

char* wsocket_gai_strerror_utf8(int errorCode);

/* If compiling for ANSI, then redefine gai_strerror() macro to return a UTF-8 string instead. */
/* Note: For WinRT apps, ANSI version of gai_strerror() is a private API and will cause an app store rejection. */
#if defined(gai_strerror) && !defined(UNICODE)
#	undef gai_strerror
#	define gai_strerror wsocket_gai_strerror_utf8
#endif

#endif /* WSOCKET_H */
