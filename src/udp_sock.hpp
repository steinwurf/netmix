#pragma once

#include <sys/types.h>
#include <sys/socket.h>

#include "inet_sock.hpp"

template<class super>
class udp_sock_client : public super, public inet_sock<typename super::buffer_ptr>
{
    typedef inet_sock<typename super::buffer_ptr> base;

  public:
    template<typename... Args> explicit
    udp_sock_client(const Args&... args)
        : super(args...),
          base(args...)
    {
        base::sock_connect(SOCK_DGRAM);
    }
};

template<class super>
class udp_sock_server : public super, public inet_sock<typename super::buffer_ptr>
{
    typedef inet_sock<typename super::buffer_ptr> base;

  public:
    template<typename... Args> explicit
    udp_sock_server(const Args&... args)
        : super(args...),
          base(args...)
    {
        base::sock_bind(SOCK_DGRAM);
    }

    struct sockaddr *sa_recv()
    {
        return base::sa_recv();
    }

    struct sockaddr *sa_send()
    {
        return base::sa_recv();
    }

    uint32_t *sa_recv_len()
    {
        return base::sa_recv_len();
    }

    uint32_t sa_send_len()
    {
        return *base::sa_recv_len();
    }
};
