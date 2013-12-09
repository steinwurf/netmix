#pragma once

#include <sys/ioctl.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <string>
#include <cstring>

#include "kwargs.hpp"
#include "inet_sock.hpp"

struct tcp_sock_peer_args
{
    static const Kwarg<int> file_descriptor;
};

decltype(tcp_sock_peer_args::file_descriptor)
    tcp_sock_peer_args::file_descriptor;

template<class super>
class tcp_sock_peer : 
    public super,
    public inet_sock<typename super::buffer_ptr>,
    public tcp_sock_peer_args
{
    typedef inet_sock<typename super::buffer_ptr> base;

  public:
    template<typename... Args> explicit
    tcp_sock_peer(const Args&... args)
        : super(args...),
          base(kwget(file_descriptor, -1, args...))
    {}
};

template<class super>
class tcp_sock_client : public super, public inet_sock<typename super::buffer_ptr>
{
    typedef inet_sock<typename super::buffer_ptr> base;

  public:
    template<typename... Args> explicit
    tcp_sock_client(const Args&... args)
        : super(args...),
          base(args...)
    {
        base::sock_connect(SOCK_STREAM);
    }
};

template<class super>
class tcp_sock_server : public super, public inet_sock<typename super::buffer_ptr>
{
    typedef inet_sock<typename super::buffer_ptr> base;

    char m_client_address[INET6_ADDRSTRLEN];

    void read_address()
    {
        struct sockaddr *addr = base::sa_recv();
        auto in4 = reinterpret_cast<struct sockaddr_in *>(addr);
        auto in6 = reinterpret_cast<struct sockaddr_in6 *>(addr);

        const char *err;

        if (addr->sa_family == AF_INET6)
            err = inet_ntop(AF_INET6, &in6->sin6_addr, m_client_address,
                            INET6_ADDRSTRLEN);
        else
            err = inet_ntop(AF_INET, &in4->sin_addr, m_client_address,
                            INET_ADDRSTRLEN);

        if (err == NULL)
            throw std::system_error(errno, std::system_category(),
                                    "unable to parse client address");
    }

  public:
    template<typename... Args> explicit
    tcp_sock_server(const Args&... args)
        : super(args...),
          base(args...)
    {
        base::sock_bind(SOCK_STREAM);
    }

    char *client_address()
    {
        read_address();
        return m_client_address;
    }
};
