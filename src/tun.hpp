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
#include <system_error>

#include "kwargs.hpp"

static const char *default_tun = NULL;

struct tun_args
{
    static const Kwarg<const char *> interface;
};

decltype(tun_args::interface) tun_args::interface;

template<class super>
class tun :
    public super,
    public tun_args
{
    typedef typename super::buffer_ptr buf_ptr;

    int m_fd;
    size_t m_mtu;
    const char *m_interface = NULL;
    char m_tun[IFNAMSIZ];

    void create()
    {
        struct ifreq ifr;

        if ((m_fd = open("/dev/net/tun", O_RDWR)) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to open /dev/net/tun");

        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

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
    }

  public:
    template<typename... Args> explicit
    tun(const Args&... a)
        : super(a...),
          m_interface(kwget(interface, default_tun, a...))
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
        int res = write(m_fd, buf->head(), buf->len());

        if (res == buf->len())
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
