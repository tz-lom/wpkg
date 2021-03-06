// TCP Client & Server -- classes to ease handling sockets
// Copyright (C) 2012-2015  Made to Order Software Corp.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

/** \file
 * \brief A simple TCP/IP client/server implementation.
 *
 * This file has functions to handle connections to a TCP/IP server. This
 * class is very basic and has no knowledge of a higher level protocol.
 *
 * At this point we use these classes to connect to websites in order to
 * retrieve packages to install on a target system.
 */

#include "libdebpackages/tcp_client_server.h"
#include "libdebpackages/compatibility.h"
#include <errno.h>
#if defined(MO_WINDOWS)
#include <ws2tcpip.h>
// all the close() calls we do here are to close sockets
// (and we already defined the macro in compatibility.h)
#undef close
#define close closesocket
#else
#include <unistd.h>
#include <netdb.h>
#endif
#if defined(MO_FREEBSD)
#include <sys/socket.h>
#endif


namespace tcp_client_server
{

/** \class tcp_client_server_logic_error
 * \brief A logic error occurred.
 *
 * This is a base error to derive from to define errors that represent
 * logic errors in the way the TCP Client/Server classes are used.
 */


/** \class tcp_client_server_runtime_error
 * \brief A runtime error occurred.
 *
 * This is an exception used whenever a runtime error occurs. Runtime
 * errors are those that are in link with connecting, reading, writing,
 * to and from the network.
 */


/** \class tcp_client_server_parameter_error
 * \brief A parameter was not valid.
 *
 * This exception is raised whenever a parameter passed to a function is
 * not valid for that function.
 */


/** \class tcp_client
 * \brief A TCP/IP Client class used to connect to servers.
 *
 * This class is used to create a client which can be used to connect to
 * TCP/IP servers.
 *
 * The constructor connects to the server. Once connected (i.e. it did not
 * throw) you can read and write data over the TCP connection.
 */


/** \class tcp_server
 * \brief A TCP/IP Server class used to listen for connections.
 *
 * This class is used to create a TCP/IP server which listen for connections
 * and acts on them.
 *
 * The server is initialized by the constructor to listen to the
 * specified IP address. To accept connections, call the accept()
 * function. Note that the socket is open to be blocking and it
 * should remain that way if you want to make sure you can server
 * many connections without losing any data.
 */


/** \brief Address info class to auto-free the structures.
 *
 * This class is used so we can auto-free the addrinfo structure(s)
 * because otherwise we find ourselves with many freeaddrinfo()
 * calls (and that's not safe in case you have exceptions.)
 */
class addrinfo_t
{
public:
    /** \brief Initialize the structure pointer to NULL.
     *
     * The constructor ensures that the pointer is NULL.
     */
    addrinfo_t()
        : f_addrinfo(NULL)
    {
    }

    /** \brief Ensures that the structures get cleaned up.
     *
     * This function ensures that the addrinfo pointers get freed with a
     * call to the freeaddrinfo().
     *
     * If no structure was allocated, nothing happens.
     */
    ~addrinfo_t()
    {
        if(f_addrinfo != NULL)
        {
            freeaddrinfo(f_addrinfo);
        }
    }

    /** \brief The address info.
     *
     * This is the address, it is public because we just use that internally
     * in the client and server constructors (see below.)
     */
    struct addrinfo *   f_addrinfo;
};


void initialize_winsock()
{
#ifdef _MSC_VER
    static bool initialized(false);

    if(!initialized)
    {
        WSADATA startup_data;
        WORD winsock_version = MAKEWORD(2, 0);
        if(WSAStartup(winsock_version, &startup_data) != 0)
        {
            throw tcp_client_server_runtime_error("WSAStartup() failed");
        }
        initialized = true;
    }
#endif
}


// ========================= CLIENT =========================


/** \class tcp_client
 * \brief Create a client socket and connect to a server.
 *
 * This class is a client socket implementation used to connect to a server.
 * The server is expected to be running at the time the client is created
 * otherwise it fails connecting.
 *
 * This class is not appropriate to connect to a server that may come and go
 * over time.
 */

/** \brief Construct a tcp_client object.
 *
 * The tcp_client constructor initializes a TCP client object by connecting
 * to the specified server. The server is defined with the address and port
 * specified as parameters.
 *
 * \exception tcp_client_server_parameter_error
 * This exception is raised if the port parameter is out of range or the
 * IP address is an empty string or otherwise an invalid address.
 *
 * \exception tcp_client_server_runtime_error
 * This exception is raised if the client connect create the socket or it
 * cannot connect to the server.
 *
 * \param[in] addr  The address of the server to connect to. It must be valid.
 * \param[in] port  The port the server is listening on.
 */
tcp_client::tcp_client(const std::string& addr, int port)
    : f_socket(INVALID_SOCKET)
    , f_port(port)
    , f_addr(addr)
{
    if(f_port < 0 || f_port >= 65536)
    {
        throw tcp_client_server_parameter_error("invalid port for a client socket");
    }
    if(f_addr.empty())
    {
        throw tcp_client_server_parameter_error("an empty address is not valid for a client socket");
    }

    initialize_winsock();

    char decimal_port[16];
    snprintf(decimal_port, sizeof(decimal_port), "%d", f_port);
    decimal_port[sizeof(decimal_port) / sizeof(decimal_port[0]) - 1] = '\0';
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo_t addr_info;
    int r(getaddrinfo(addr.c_str(), decimal_port, &hints, &addr_info.f_addrinfo));
    if(r != 0 || addr_info.f_addrinfo == NULL)
    {
        throw tcp_client_server_runtime_error("invalid address or port for client connection: \"" + addr + ":" + decimal_port + "\"");
    }

    f_socket = socket(addr_info.f_addrinfo->ai_family, SOCK_STREAM, IPPROTO_TCP);
    if(f_socket < 0)
    {
        f_socket = INVALID_SOCKET;
        throw tcp_client_server_runtime_error("could not create socket for client");
    }

    // under MS-Windows you get a warning on the ai_addrlen (size_t to int)
    if(connect(f_socket, addr_info.f_addrinfo->ai_addr, static_cast<int>(addr_info.f_addrinfo->ai_addrlen)) < 0)
    {
        close(f_socket);
        f_socket = INVALID_SOCKET;
        throw tcp_client_server_runtime_error("could not connect client socket to \"" + f_addr + "\"");
    }
}

/** \brief Clean up the TCP client object.
 *
 * This function cleans up the TCP client object by closing the attached socket.
 */
tcp_client::~tcp_client()
{
    // the socket should always be valid here since we throw in the
    // constructor if not
    if(f_socket != INVALID_SOCKET)
    {
        close(f_socket);
    }
}

/** \brief Get the socket descriptor.
 *
 * This function returns the TCP client socket descriptor. This can be
 * used to change the descriptor behavior (i.e. make it non-blocking for
 * example.)
 *
 * \return The socket descriptor.
 */
socket_t tcp_client::get_socket() const
{
    return f_socket;
}

/** \brief Get the TCP client port.
 *
 * This function returns the port used when creating the TCP client.
 * Note that this is the port the server is listening to and not the port
 * the TCP client is currently connected to.
 *
 * \return The TCP client port.
 */
int tcp_client::get_port() const
{
    return f_port;
}

/** \brief Get the TCP client address.
 *
 * This function returns the address used when creating the TCP address.
 * Note that this is the address of the server where the client is connected
 * and not the address where the client is running (although it may be the
 * same.)
 *
 * \return The TCP client address.
 */
std::string tcp_client::get_addr() const
{
    return f_addr;
}

/** \brief Read data from the socket.
 *
 * A TCP socket is a stream type of socket and one can read data from it
 * as if it were a regular file. This function reads \p size bytes and
 * returns. The function returns early if the server closes the connection.
 *
 * If your socket is blocking, \p size should be exactly what you are
 * expected or this function will block forever.
 *
 * The function returns -1 if an error occurs. The error is available in
 * errno as expected in the POSIX interface.
 *
 * \warning
 * Under MS-Windows, the \p size parameter is cast to int to avoid
 * warnings. We do not expect such large buffer reads that it would
 * cause a problem.
 *
 * \param[in,out] buf  The buffer where the data is read.
 * \param[in] size  The size of the buffer.
 *
 * \return The number of bytes read from the socket, or -1 on errors.
 */
int tcp_client::read(char *buf, size_t size)
{
#ifdef _MSC_VER
    return ::recv(f_socket, buf, static_cast<int>(size), 0);
#else
    return ::read(f_socket, buf, size);
#endif
}

/** \brief Write data to the socket.
 *
 * A TCP socket is a stream type of socket and one can write data to it
 * as if it were a regular file. This function writes \p size bytes to
 * the socket and then returns. This function returns early if the server
 * closes the connection.
 *
 * If your socket is not blocking, less than \p size bytes may be written
 * to the socket. In that case you are responsible for calling the function
 * again to write the remainder of the buffer until the function returns
 * a number of bytes written equal to \p size.
 *
 * The function returns -1 if an error occurs. The error is available in
 * errno as expected in the POSIX interface.
 *
 * \warning
 * Under MS-Windows, the \p size parameter is cast to int to avoid
 * warnings. We do not expect such large buffer reads that it would
 * cause a problem.
 *
 * \param[in] buf  The buffer with the data to send over the socket.
 * \param[in] size  The number of bytes in buffer to send over the socket.
 *
 * \return The number of bytes that were actually accepted by the socket
 * or -1 if an error occurs.
 */
int tcp_client::write(const char *buf, size_t size)
{
#ifdef _MSC_VER
    return ::send(f_socket, buf, static_cast<int>(size), 0);
#else
    return ::write(f_socket, buf, size);
#endif
}


// ========================= SERVER =========================

/** \brief Initialize the server and start listening for connections.
 *
 * The server constructor creates a socket, binds it, and then listen to it.
 *
 * By default the server accepts a maximum of \p max_connections (set to
 * -1 to get the default tcp_server::MAX_CONNECTIONS) in its waiting queue.
 * If you use the server and expect a low connection rate, you may want to
 * reduce the count to 5. Although some very busy servers use larger numbers.
 *
 * The address is made non-reusable (which is the default for TCP sockets.)
 * It is possible to mark the server address as immediately reusable by
 * setting the \p reuse_addr to true.
 *
 * By default the server is marked as "keepalive". You can turn it off
 * using the keepalive() function with false.
 *
 * \exception tcp_client_server_parameter_error
 * This exception is raised if the address parameter is an empty string or
 * otherwise an invalid IP address, or if the port is out of range.
 *
 * \exception tcp_client_server_runtime_error
 * This exception is raised if the socket cannot be created, bound to
 * the specified IP address and port, or listen() fails on the socket.
 *
 * \param[in] addr  The address to listen on. It may be set to "0.0.0.0".
 * \param[in] port  The port to listen on.
 * \param[in] max_connections  The number of connections to keep in the listen queue.
 * \param[in] reuse_addr  Whether to mark the socket with the SO_REUSEADDR flag.
 * \param[in] auto_close  Automatically close the the client socket in accept and the destructor.
 */
tcp_server::tcp_server(const std::string& addr, int port, int max_connections, bool reuse_addr, bool auto_close)
    : f_max_connections(max_connections < 1 ? MAX_CONNECTIONS : max_connections)
    , f_socket(INVALID_SOCKET)
    , f_port(port)
    , f_addr(addr)
    , f_accepted_socket(INVALID_SOCKET)
    , f_keepalive(true)
    , f_auto_close(auto_close)
{
    if(f_addr.empty())
    {
        throw tcp_client_server_parameter_error("the address cannot be an empty string");
    }
    if(f_port < 0 || f_port >= 65536)
    {
        throw tcp_client_server_parameter_error("invalid port for a client socket");
    }

    initialize_winsock();

    char decimal_port[16];
    snprintf(decimal_port, sizeof(decimal_port), "%d", f_port);
    decimal_port[sizeof(decimal_port) / sizeof(decimal_port[0]) - 1] = '\0';
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo_t addr_info;
    int r(getaddrinfo(addr.c_str(), decimal_port, &hints, &addr_info.f_addrinfo));
    if(r != 0 || addr_info.f_addrinfo == NULL)
    {
        throw tcp_client_server_runtime_error("invalid address or port for server: \"" + addr + ":" + decimal_port + "\"");
    }

    f_socket = socket(addr_info.f_addrinfo->ai_family, SOCK_STREAM, IPPROTO_TCP);
    if(f_socket < 0)
    {
        f_socket = INVALID_SOCKET;
        throw tcp_client_server_runtime_error("could not create socket for client");
    }

    // this should be optional as reusing an address for TCP/IP is not 100% safe
    if(reuse_addr)
    {
        // try to mark the socket address as immediately reusable
        // if this fails, we ignore the error (TODO log an INFO message)
#if defined(MO_WINDOWS)
        // see definitions here:
        // http://msdn.microsoft.com/en-us/library/windows/desktop/ms740476(v=vs.85).aspx
        BOOL optval(1);
        socklen_t optlen(sizeof(optval));
        setsockopt(f_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&optval), optlen);
#else
        int optval(1);
        socklen_t optlen(sizeof(optval));
        setsockopt(f_socket, SOL_SOCKET, SO_REUSEADDR, &optval, optlen);
#endif
    }

    // the size of the address is a size_t and windows uses int instead
    if(bind(f_socket, addr_info.f_addrinfo->ai_addr, static_cast<int>(addr_info.f_addrinfo->ai_addrlen)) < 0)
    {
        f_socket = INVALID_SOCKET;
        throw tcp_client_server_runtime_error("could not bind the socket to \"" + f_addr + "\"");
    }

    // start listening, we expect the caller to then call accept() to
    // acquire connections
    if(listen(f_socket, f_max_connections) < 0)
    {
        f_socket = INVALID_SOCKET;
        throw tcp_client_server_runtime_error("could not listen to the socket bound to \"" + f_addr + "\"");
    }
}

/** \brief Clean up the server sockets.
 *
 * This function ensures that the server sockets get cleaned up.
 *
 * If the \pauto_close parameter was set to true in the constructor, then
 * the last accepter socket gets closed by this function.
 */
tcp_server::~tcp_server()
{
    if(f_socket != INVALID_SOCKET)
    {
        close(f_socket);
    }
    if(f_auto_close && f_accepted_socket != INVALID_SOCKET)
    {
        close(f_accepted_socket);
    }
}

/** \brief Retrieve the socket descriptor.
 *
 * This function returns the socket descriptor. It can be used to
 * tweak things on the socket such as making it non-blocking or
 * directly accessing the data.
 *
 * \return The socket descriptor.
 */
socket_t tcp_server::get_socket() const
{
    return f_socket;
}

/** \brief Retrieve the maximum number of connections.
 *
 * This function returns the maximum number of connections that can
 * be accepted by the socket. This was set by the constructor and
 * it cannot be changed later.
 *
 * \return The maximum number of incoming connections.
 */
int tcp_server::get_max_connections() const
{
    return f_max_connections;
}

/** \brief Return the server port.
 *
 * This function returns the port the server was created with. This port
 * is exactly what the server currently uses. It cannot be changed.
 *
 * \return The server port.
 */
int tcp_server::get_port() const
{
    return f_port;
}

/** \brief Retrieve the server IP address.
 *
 * This function returns the IP address used to bind the socket. This
 * is the address clients have to use to connect to the server unless
 * the address was set to all zeroes (0.0.0.0) in which case any user
 * can connect.
 *
 * The IP address cannot be changed.
 *
 * \return The server IP address.
 */
std::string tcp_server::get_addr() const
{
    return f_addr;
}

/** \brief Return the current status of the keepalive flag.
 *
 * This function returns the current status of the keepalive flag. This
 * flag is set to true by default (in the constructor.) It can be
 * changed with the keepalive() function.
 *
 * The flag is used to mark new connections with the SO_KEEPALIVE flag.
 * This is used whenever a service may take a little to long to answer
 * and avoid losing the TCP connection before the answer is sent to
 * the client.
 *
 * \return The current status of the keepalive flag.
 */
bool tcp_server::get_keepalive() const
{
    return f_keepalive;
}

/** \brief Set the keepalive flag.
 *
 * This function sets the keepalive flag to either true (i.e. mark connection
 * sockets with the SO_KEEPALIVE flag) or false. The default is true (as set
 * in the constructor,) because in most cases this is a feature people want.
 *
 * \param[in] yes  Whether to keep new connections alive even when no traffic
 * goes through.
 */
void tcp_server::keepalive(bool yes)
{
    f_keepalive = yes;
}

/** \brief Accept a connection.
 *
 * A TCP server accepts incoming connections. This call is a blocking call.
 * If no connections are available on the line, then the call blocks until
 * a connection becomes available.
 *
 * To prevent being blocked by this call you can either check the status of
 * the file descriptor (use the get_socket() function to retrieve the
 * descriptor and use an appropriate wait with 0 as a timeout,) or transform
 * the socket in a non-blocking socket (not tested, though.)
 *
 * This TCP socket implementation is expected to be used in one of two ways:
 *
 * (1) the main server accepts connections and then fork()'s to handle the
 * transaction with the client, in that case we want to set the \p auto_close
 * parameter of the constructor to true so the accept() function automatically
 * closes the last accepted socket.
 *
 * (2) the main server keeps a set of connections and handles them alongside
 * the main server connection. Although there are limits to what you can do
 * in this way, it is very efficient, but this also means the accept() call
 * cannot close the last accepted socket since the rest of the software may
 * still be working on it.
 *
 * The function returns a client/server socket. This is the socket one can
 * use to communicate with the client that just connected to the server. This
 * descriptor can be written to or read from.
 *
 * This function is the one that applies the keepalive flag to the
 * newly accepted socket.
 *
 * \note
 * If you prevent SIGCHLD from stopping your code, you may want to allow it
 * when calling this function (that is, if you're interested in getting that
 * information immediately, otherwise it is cleaner to always block those
 * signals.)
 *
 * \return A client socket descriptor or -1 if an error occurred.
 */
socket_t tcp_server::accept()
{
    // auto-close?
    if(f_auto_close && f_accepted_socket != INVALID_SOCKET)
    {
        // if the close is interrupted, make sure we try again otherwise
        // we could lose that stream until next restart (this could happen
        // if you have SIGCHLD)
        if(close(f_accepted_socket) == -1)
        {
            if(errno == EINTR)
            {
                close(f_accepted_socket);
            }
        }
    }
    f_accepted_socket = INVALID_SOCKET;

    // accept the next connection
    socklen_t addr_len(sizeof(f_accepted_addr));
    memset(&f_accepted_addr, 0, sizeof(f_accepted_addr));
    f_accepted_socket = ::accept(f_socket, reinterpret_cast<struct sockaddr *>(&f_accepted_addr), &addr_len);

    // mark the new connection with the SO_KEEPALIVE flag
    if(f_accepted_socket != -1 && f_keepalive)
    {
        // if this fails, we ignore the error (TODO log an INFO message)
#if defined(MO_WINDOWS)
        BOOL optval(1);
        socklen_t optlen(sizeof(optval));
        setsockopt(f_accepted_socket, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char *>(&optval), optlen);
#else
        int optval(1);
        socklen_t optlen(sizeof(optval));
        setsockopt(f_accepted_socket, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen);
#endif
    }

    return f_accepted_socket;
}

/** \brief Retrieve the last accepted socket descriptor.
 *
 * This function returns the last accepted socket descriptor as retrieved by
 * accept(). If accept() was never called or failed, then this returns -1.
 *
 * \return The last accepted socket descriptor.
 */
socket_t tcp_server::get_last_accepted_socket() const
{
    return f_accepted_socket;
}




} // namespace tcp_client_server
// vim: ts=4 sw=4 et
