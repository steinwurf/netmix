#pragma once

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <linux/if_ether.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <system_error>
#include <cassert>

#include "kwargs.hpp"

static const char *tuntap_default_name = NULL;
static const char *tuntap_default_type = "tun";

struct tuntap_args
{
    static const Kwarg<const char *> interface;
    static const Kwarg<const char *> type;
};

enum tuntap_type {
    tuntap_tun = IFF_TUN,
    tuntap_tap = IFF_TAP,
    tuntap_arg,
};

decltype(tuntap_args::interface) tuntap_args::interface;
decltype(tuntap_args::type) tuntap_args::type;

template<tuntap_type template_type, class super>
class tuntap_base :
    public super,
    public tuntap_args
{
    typedef typename super::buffer_ptr buf_ptr;

    int m_fd;
    size_t m_mtu;
    const char *m_interface = NULL;
    const char *m_type_str = NULL;
    int m_type;
    char m_tun[IFNAMSIZ];

    int iface_type()
    {
        if (template_type != tuntap_arg)
            return template_type;

        assert(m_type_str);

        if (strcmp(m_type_str, "tun") == 0)
            return tuntap_tun;
        else if (strcmp(m_type_str, "tap") == 0)
            return tuntap_tap;
        else
            throw std::runtime_error("invalid tuntap type string");
    }

    void create()
    {
        struct ifreq ifr;

        if ((m_fd = open("/dev/net/tun", O_RDWR)) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to open /dev/net/tun");

        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_flags = m_type | IFF_NO_PI;

        if (strncmp(m_interface, "lo", IFNAMSIZ) != 0)
            strncpy(ifr.ifr_name, m_interface, IFNAMSIZ);

        if (ioctl(m_fd, TUNSETIFF, &ifr) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to create tun interface");

        strncpy(m_tun, ifr.ifr_name, IFNAMSIZ);
    }

    void read_mtu()
    {
        struct ifreq ifr;
        int sock = socket(AF_INET, SOCK_DGRAM, 0);

        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, m_tun, IFNAMSIZ);

        if (ioctl(sock, SIOCGIFMTU, &ifr) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to read interface mtu");

        close(sock);
        m_mtu = ifr.ifr_mtu;

        if (m_type == tuntap_tap)
            m_mtu += ETH_HLEN;
    }

  public:
    template<typename... Args> explicit
    tuntap_base(const Args&... args)
        : super(args...),
          m_interface(kwget(interface, tuntap_default_name, args...)),
          m_type_str(kwget(type, tuntap_default_type, args...)),
          m_type(iface_type())
    {
        create();
        read_mtu();
    }

    int fd()
    {
        return m_fd;
    }

    size_t data_size_max()
    {
        return m_mtu;
    }

    bool read_pkt(buf_ptr &buf)
    {
        int res;

        res = read(m_fd, buf->data(), buf->max_len());

        if (res > 0) {
            buf->trim(res);
            return true;
        }

        if (res == 0)
            return false;

        if (res < 0 && errno == EAGAIN)
            return false;

        throw std::system_error(errno, std::system_category(),
                                "unable to read pkt");
    }

    bool write_pkt(buf_ptr &buf)
    {
        int len = buf->len();
        int res = write(m_fd, buf->head(), buf->len());

        if (res == len)
            return true;

        if (res > 0) {
            std::cout << "incomplete write: " << res << std::endl;
            throw std::runtime_error("incomplete write");
        }

        if (res == 0)
            return false;

        if (res < 0 && errno == EAGAIN)
            return false;

        throw std::system_error(errno, std::system_category(),
                                "unable to write pkt");
    }
};

template<class super>
class tuntap : public tuntap_base<tuntap_arg, super>
{
    typedef tuntap_base<tuntap_arg, super> base;

  public:
    template<typename... Args> explicit
    tuntap(const Args&... args)
        : base(args...)
    {}
};
