#pragma once
#include "defs.h"

namespace SL {
namespace NET {
    namespace INTERNAL {

        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_DEBUG(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_ACCEPTCONN(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_BROADCAST(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_REUSEADDR(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_KEEPALIVE(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<LingerOption>> SOCKET_LITE_EXTERN getsockopt_O_LINGER(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_OOBINLINE(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<int>> SOCKET_LITE_EXTERN getsockopt_O_SNDBUF(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<int>> SOCKET_LITE_EXTERN getsockopt_O_RCVBUF(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<std::chrono::seconds>> SOCKET_LITE_EXTERN getsockopt_O_SNDTIMEO(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<std::chrono::seconds>> SOCKET_LITE_EXTERN getsockopt_O_RCVTIMEO(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<int>> SOCKET_LITE_EXTERN getsockopt_O_ERROR(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<SockOptStatus>> SOCKET_LITE_EXTERN getsockopt_O_NODELAY(PlatformSocket handle);
        std::tuple<StatusCode, std::optional<Blocking_Options>> SOCKET_LITE_EXTERN getsockopt_O_BLOCKING(PlatformSocket handle);

        template <SocketOptions SO> struct getsockopt_factory_impl;
        template <> struct getsockopt_factory_impl<SocketOptions::O_DEBUG> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_DEBUG(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_ACCEPTCONN> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_ACCEPTCONN(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_BROADCAST> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_BROADCAST(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_REUSEADDR> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_REUSEADDR(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_KEEPALIVE> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_KEEPALIVE(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_LINGER> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_LINGER(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_OOBINLINE> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_OOBINLINE(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_SNDBUF> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_SNDBUF(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_RCVBUF> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_RCVBUF(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_RCVTIMEO> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_RCVTIMEO(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_SNDTIMEO> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_SNDTIMEO(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_ERROR> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_ERROR(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_NODELAY> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_NODELAY(handle); }
        };
        template <> struct getsockopt_factory_impl<SocketOptions::O_BLOCKING> {
            static auto getsockopt_(PlatformSocket handle) { return getsockopt_O_BLOCKING(handle); }
        };

        bool SOCKET_LITE_EXTERN setsockopt_O_DEBUG(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_BROADCAST(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_REUSEADDR(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_KEEPALIVE(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_LINGER(PlatformSocket handle, LingerOption o);
        bool SOCKET_LITE_EXTERN setsockopt_O_OOBINLINE(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_SNDBUF(PlatformSocket handle, int b);
        bool SOCKET_LITE_EXTERN setsockopt_O_RCVBUF(PlatformSocket handle, int b);
        bool SOCKET_LITE_EXTERN setsockopt_O_SNDTIMEO(PlatformSocket handle, std::chrono::seconds sec);
        bool SOCKET_LITE_EXTERN setsockopt_O_RCVTIMEO(PlatformSocket handle, std::chrono::seconds sec);
        bool SOCKET_LITE_EXTERN setsockopt_O_NODELAY(PlatformSocket handle, SockOptStatus b);
        bool SOCKET_LITE_EXTERN setsockopt_O_BLOCKING(PlatformSocket handle, Blocking_Options b);

        template <SocketOptions SO> struct setsockopt_factory_impl;
        template <> struct setsockopt_factory_impl<SocketOptions::O_DEBUG> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_DEBUG(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_BROADCAST> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_BROADCAST(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_REUSEADDR> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_REUSEADDR(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_KEEPALIVE> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_KEEPALIVE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_LINGER> {
            static auto setsockopt_(PlatformSocket handle, LingerOption b) { return setsockopt_O_LINGER(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_OOBINLINE> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_OOBINLINE(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_SNDBUF> {
            static auto setsockopt_(PlatformSocket handle, int b) { return setsockopt_O_SNDBUF(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_RCVBUF> {
            static auto setsockopt_(PlatformSocket handle, int b) { return setsockopt_O_RCVBUF(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_RCVTIMEO> {
            static auto setsockopt_(PlatformSocket handle, std::chrono::seconds b) { return setsockopt_O_RCVTIMEO(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_SNDTIMEO> {
            static auto setsockopt_(PlatformSocket handle, std::chrono::seconds b) { return setsockopt_O_SNDTIMEO(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_NODELAY> {
            static auto setsockopt_(PlatformSocket handle, SockOptStatus b) { return setsockopt_O_NODELAY(handle, b); }
        };
        template <> struct setsockopt_factory_impl<SocketOptions::O_BLOCKING> {
            static auto setsockopt_(PlatformSocket handle, Blocking_Options b) { return setsockopt_O_BLOCKING(handle, b); }
        };
        SOCKET_LITE_EXTERN StatusCode bind(PlatformSocket &handle, sockaddr addr);
        SOCKET_LITE_EXTERN PlatformSocket Socket(AddressFamily family);
        template <SocketOptions SO> auto getsockopt(PlatformSocket handle) { return INTERNAL::getsockopt_factory_impl<SO>::getsockopt_(handle); }
        template <SocketOptions SO, typename... Args> auto setsockopt(PlatformSocket handle, Args &&... args)
        {
            return INTERNAL::setsockopt_factory_impl<SO>::setsockopt_(handle, std::forward<Args>(args)...);
        }

    } // namespace INTERNAL
    std::tuple<StatusCode, std::vector<sockaddr>> SOCKET_LITE_EXTERN getaddrinfo(const char *nodename, PortNumber pServiceName, AddressFamily family);
} // namespace NET
} // namespace SL