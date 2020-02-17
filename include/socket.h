#ifndef SL_NETWORK_SOCKET
#define SL_NETWORK_SOCKET

#include "ip_endpoint.h"

#include "socket_accept_operation.h"
#include "socket_connect_operation.h"
#include "socket_disconnect_operation.h"
#include "socket_recv_from_operation.h"
#include "socket_recv_operation.h"
#include "socket_send_operation.h"
#include "socket_send_to_operation.h"

namespace SL::Network {
class io_service;

class socket {
  public:
    /// Create a socket that can be used to communicate using TCP/IPv4 protocol.
    ///
    /// \param ioSvc
    /// The I/O service the socket will use for dispatching I/O completion events.
    ///
    /// \return
    /// The newly created socket.
    ///
    /// \throws std::system_error
    /// If the socket could not be created for some reason.
    static socket create_tcpv4(io_service &ioSvc);

    /// Create a socket that can be used to communicate using TCP/IPv6 protocol.
    ///
    /// \param ioSvc
    /// The I/O service the socket will use for dispatching I/O completion events.
    ///
    /// \return
    /// The newly created socket.
    ///
    /// \throws std::system_error
    /// If the socket could not be created for some reason.
    static socket create_tcpv6(io_service &ioSvc);

    /// Create a socket that can be used to communicate using UDP/IPv4 protocol.
    ///
    /// \param ioSvc
    /// The I/O service the socket will use for dispatching I/O completion events.
    ///
    /// \return
    /// The newly created socket.
    ///
    /// \throws std::system_error
    /// If the socket could not be created for some reason.
    static socket create_udpv4(io_service &ioSvc);

    /// Create a socket that can be used to communicate using UDP/IPv6 protocol.
    ///
    /// \param ioSvc
    /// The I/O service the socket will use for dispatching I/O completion events.
    ///
    /// \return
    /// The newly created socket.
    ///
    /// \throws std::system_error
    /// If the socket could not be created for some reason.
    static socket create_udpv6(io_service &ioSvc);

    socket(socket &&other) noexcept;

    /// Closes the socket, releasing any associated resources.
    ///
    /// If the socket still has an open connection then the connection will be
    /// reset. The destructor will not block waiting for queueud data to be sent.
    /// If you need to ensure that queued data is delivered then you must call
    /// disconnect() and wait until the disconnect operation completes.
    ~socket();

    socket &operator=(socket &&other) noexcept;

    /// Get the address and port of the local end-point.
    ///
    /// If the socket is not bound then this will be the unspecified end-point
    /// of the socket's associated address-family.
    ip_endpoint local_endpoint() const;

    /// Get the address and port of the remote end-point.
    ///
    /// If the socket is not in the connected state then this will be the unspecified
    /// end-point of the socket's associated address-family.
    ip_endpoint remote_endpoint() const;

    /// Bind the local end of this socket to the specified local end-point.
    ///
    /// \param localEndPoint
    /// The end-point to bind to.
    /// This can be either an unspecified address (in which case it binds to all available
    /// interfaces) and/or an unspecified port (in which case a random port is allocated).
    ///
    /// \throws std::system_error
    /// If the socket could not be bound for some reason.
    void bind(const ip_endpoint &localEndPoint);

    /// Put the socket into a passive listening state that will start acknowledging
    /// and queueing up new connections ready to be accepted by a call to 'accept()'.
    ///
    /// The backlog of connections ready to be accepted will be set to some default
    /// suitable large value, depending on the network provider. If you need more
    /// control over the size of the queue then use the overload of listen()
    /// that accepts a 'backlog' parameter.
    ///
    /// \throws std::system_error
    /// If the socket could not be placed into a listening mode.
    void listen();

    /// Put the socket into a passive listening state that will start acknowledging
    /// and queueing up new connections ready to be accepted by a call to 'accept()'.
    ///
    /// \param backlog
    /// The maximum number of pending connections to allow in the queue of ready-to-accept
    /// connections.
    ///
    /// \throws std::system_error
    /// If the socket could not be placed into a listening mode.
    void listen(std::uint32_t backlog);

    /// Connect the socket to the specified remote end-point.
    ///
    /// The socket must be in a bound but unconnected state prior to this call.
    ///
    /// \param remoteEndPoint
    /// The IP address and port-number to connect to.
    ///
    /// \return
    /// An awaitable object that must be co_await'ed to perform the async connect
    /// operation. The result of the co_await expression is type void.
    [[nodiscard]] socket_connect_operation connect(const ip_endpoint &remoteEndPoint) noexcept;
    [[nodiscard]] socket_accept_operation accept(socket &acceptingSocket) noexcept;
    [[nodiscard]] socket_disconnect_operation disconnect() noexcept;
    [[nodiscard]] socket_send_operation send(const void *buffer, std::size_t size) noexcept;
    [[nodiscard]] socket_recv_operation recv(void *buffer, std::size_t size) noexcept;
    [[nodiscard]] socket_recv_from_operation recv_from(void *buffer, std::size_t size) noexcept;
    [[nodiscard]] socket_send_to_operation send_to(const ip_endpoint &destination, const void *buffer, std::size_t size) noexcept;

    bool skip_completion_on_success() noexcept { return m_skipCompletionOnSuccess; }
    win32::socket_t native_handle() const { return m_handle; }

  private:
#if _WIN32
    explicit socket(win32::socket_t handle) noexcept;
    win32::socket_t m_handle;
    bool m_skipCompletionOnSuccess;
#endif
};
} // namespace SL::Network

#endif
