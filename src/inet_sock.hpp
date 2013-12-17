#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <system_error>
#include <netdb.h>

#include "sock.hpp"
#include "kwargs.hpp"

static const char default_remote[] = "localhost";
static const char *default_local = nullptr;
static const char default_port[] = "15887";

struct inet_sock_args
{
    typedef const Kwarg<const char *> arg_type;

    static arg_type port;
    static arg_type local_address;
    static arg_type remote_address;
};

decltype(inet_sock_args::port)     inet_sock_args::port;
decltype(inet_sock_args::local_address)  inet_sock_args::local_address;
decltype(inet_sock_args::remote_address)  inet_sock_args::remote_address;

template<class buffer_ptr>
class inet_sock :
    public sock<buffer_ptr>,
    public inet_sock_args
{
    struct sockaddr *m_sa_send = NULL;
    struct sockaddr *m_sa_recv = NULL;
    uint32_t m_sa_send_len = 0;
    uint32_t m_sa_recv_len = 0;
    const char *m_port;
    const char *m_local_address;
    const char *m_remote_address;
    int m_fd;

    uint32_t init_sockaddr(struct addrinfo *ai, struct sockaddr **sa)
    {
        uint8_t *sa_buf = new uint8_t[ai->ai_addrlen];
        *sa = reinterpret_cast<struct sockaddr *>(sa_buf);
        memcpy(*sa, ai->ai_addr, ai->ai_addrlen);

        return ai->ai_addrlen;
    }

  public:
    template<typename... Args> explicit
    inet_sock(const Args&... args)
          : m_port(kwget(port, default_port, args...)),
            m_local_address(kwget(local_address, default_local, args...)),
            m_remote_address(kwget(remote_address, default_remote, args...))
    {}

    inet_sock(int fd)
        : m_fd(fd)
    {
        std::cout << "inet_sock: " << m_fd << std::endl;
    }

    ~inet_sock()
    {
        if (m_sa_send)
            delete[] m_sa_send;

        if (m_sa_recv)
            delete[] m_sa_recv;

        close(m_fd);
    }

    int fd()
    {
        return m_fd;
    }

    int sock_bind_client(int family)
    {
        sockaddr_in sa4;
        memset(&sa4, 0, sizeof(sa4));
        sockaddr_in6 sa6;
        memset(&sa6, 0, sizeof(sa6));
        struct sockaddr *sa;
        socklen_t len;
        int res = -1;

        sa4.sin_family = AF_INET;
        sa4.sin_port = 0;

        sa6.sin6_family = AF_INET6;
        sa6.sin6_port = 0;

        if (m_local_address == nullptr)
            return 0;

        switch (family) {
            case AF_INET:
                res = inet_pton(AF_INET, m_local_address, &sa4.sin_addr);
                sa = reinterpret_cast<struct sockaddr *>(&sa4);
                len = sizeof(sa4);
                break;

            case AF_INET6:
                res = inet_pton(AF_INET6, m_local_address, &sa6.sin6_addr);
                sa = reinterpret_cast<struct sockaddr *>(&sa6);
                len = sizeof(sa6);
                break;
        }

        if (res != 1)
            return -1;

        if (bind(m_fd, sa, len) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to bind");

        return 0;
    }

    void sock_connect(int socktype = 0)
    {
        struct addrinfo *res, *rp;
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        int err;

        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = socktype;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;

        err = getaddrinfo(m_remote_address, m_port, &hints, &res);
        if (err)
            throw std::runtime_error(std::string("unable to resolv host: ") +
                                     gai_strerror(err));

        for (rp = res; rp != NULL; rp = rp->ai_next) {
            m_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

            if (m_fd < 0)
                continue;

            if (sock_bind_client(rp->ai_family) < 0) {
                close(m_fd);
                continue;
            }

            if (connect(m_fd, rp->ai_addr, rp->ai_addrlen) == 0)
                break;

            close(m_fd);
        }

        if (rp == NULL)
            throw std::system_error(errno, std::system_category(),
                                    "unable to connect");

        m_sa_send_len = init_sockaddr(rp, &m_sa_send);
        freeaddrinfo(res);
    }

    void sock_bind(int socktype = 0)
    {
        struct addrinfo *res, *rp;
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        int err, reuse = 1;

        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = socktype;
        hints.ai_flags = AI_PASSIVE;

        std::cout << m_local_address << std::endl;
        err = getaddrinfo(m_local_address, m_port, &hints, &res);
        if (err)
            throw std::runtime_error(std::string("unable to lookup local address: ") +
                                     gai_strerror(err));

        for (rp = res; rp != NULL; rp = rp->ai_next) {
            m_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

            if (m_fd < 0)
                continue;

            if (setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)))
                throw std::system_error(errno, std::system_category(),
                                        "unable to set reuse on socket");

            if (bind(m_fd, rp->ai_addr, rp->ai_addrlen) == 0)
                break;

            close(m_fd);
        }

        if (rp == NULL)
            throw std::system_error(errno, std::system_category(),
                                    "unable to bind");

        if (socktype == SOCK_STREAM && listen(m_fd, 1) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to listen on socket");


        m_sa_recv_len = init_sockaddr(rp, &m_sa_recv);
        freeaddrinfo(res);
    }

    int sock_accept()
    {
        int fd;

        fd = accept(m_fd, m_sa_recv, &m_sa_recv_len);

        if (fd < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to accept connection");

        return fd;
    }

    struct sockaddr *sa_send()
    {
        return m_sa_send;
    }

    struct sockaddr *sa_recv()
    {
        return m_sa_recv;
    }

    uint32_t sa_send_len()
    {
        return m_sa_send_len;
    }

    uint32_t *sa_recv_len()
    {
        return &m_sa_recv_len;
    }
};
