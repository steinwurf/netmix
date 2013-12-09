#pragma once

#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <string>
#include <system_error>
#include <memory>

#include "kwargs.hpp"
#include "sock.hpp"

static constexpr char default_interface[] = "lo";

struct eth_sock_args
{
    static const Kwarg<const char *> interface;
};

decltype(eth_sock_args::interface) eth_sock_args::interface;

template<class super>
class eth_sock :
    public super,
    public sock<typename super::buffer_ptr>,
    public eth_sock_args
{
    static const int m_domain = PF_PACKET;
    static const int m_family = SOCK_RAW;
    static const uint16_t m_proto = 0x4307;

    int m_fd, m_index, m_mtu;
    const char *m_interface;
    uint8_t m_address[ETH_ALEN];
    struct sockaddr_ll m_sock_addr = {0};

    void sock_open()
    {
        m_fd = socket(m_domain, m_family, m_proto);

        if (m_fd < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to open socket");
    }

    void sock_bind()
    {
        if (bind(m_fd, sockaddr(), sockaddr_len()) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to bind to interface");
    }

    void read_address()
    {
        struct ifreq ifr;

        strncpy(ifr.ifr_name, m_interface, IFNAMSIZ);

        if (ioctl(m_fd, SIOCGIFHWADDR, &ifr) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to read interface address");

        memcpy(m_address, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
    }

    void read_index()
    {
        struct ifreq ifr;

        strncpy(ifr.ifr_name, m_interface, IFNAMSIZ);

        if (ioctl(m_fd, SIOCGIFINDEX, &ifr) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to read interface index");

        m_index = ifr.ifr_ifindex;
    }

    void read_mtu()
    {
        struct ifreq ifr;

        strncpy(ifr.ifr_name, m_interface, IFNAMSIZ);

        if (ioctl(m_fd, SIOCGIFMTU, &ifr) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to read interface mtu");

        m_mtu = ifr.ifr_mtu;
    }

    void promisc_on()
    {
        struct ifreq ifr;

        strncpy(ifr.ifr_name, m_interface, IFNAMSIZ);

        if (ioctl(m_fd, SIOCGIFFLAGS, &ifr) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to read interface flags");

        ifr.ifr_flags |= IFF_PROMISC;

        if (ioctl(m_fd, SIOCSIFFLAGS, &ifr) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to set interface promisc");
    }

    void promisc_off()
    {
        struct ifreq ifr;

        strncpy(ifr.ifr_name, m_interface, IFNAMSIZ);

        if (ioctl(m_fd, SIOCGIFFLAGS, &ifr) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to read interface flags");

        ifr.ifr_flags &= ~IFF_PROMISC;

        if (ioctl(m_fd, SIOCSIFFLAGS, &ifr) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unabel to unset interface promisc");
    }

  public:
    template<typename ... Args> explicit
    eth_sock(const Args&... args)
        : super(args...)
    {
        m_interface = kwget(interface, default_interface, args...);
        sock_open();
        promisc_on();
        read_address();
        read_index();
        read_mtu();

        m_sock_addr.sll_family   = m_domain;
        m_sock_addr.sll_protocol = htons(m_proto);
        m_sock_addr.sll_ifindex  = m_index;
        m_sock_addr.sll_halen    = ETH_ALEN;

        sock_bind();
    }

    ~eth_sock()
    {
        promisc_off();
    }

    int fd()
    {
        return m_fd;
    }

    uint16_t proto()
    {
        return m_proto;
    }

    size_t data_size_max()
    {
        return m_mtu + ETH_HLEN;
    }

    const struct sockaddr *sockaddr() const
    {
        return reinterpret_cast<const struct sockaddr *>(&m_sock_addr);
    }

    static size_t sockaddr_len()
    {
        return sizeof(m_sock_addr);
    }

    const uint8_t *interface_address() const
    {
        return m_address;
    }
};
