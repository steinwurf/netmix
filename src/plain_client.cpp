#include <cstdlib>
#include <functional>
#include <iostream>
#include <cstdio>
#include <memory>
#include <vector>
#include <getopt.h>

#include "signal.hpp"
#include "tun.hpp"
#include "tcp_hdr.hpp"
#include "tcp_sock.hpp"
#include "buffer_pkt.hpp"
#include "buffer_pool.hpp"
#include "final_layer.hpp"
#include "io.hpp"

struct args
{
    char *interface = NULL;
    char address[20] = "localhost";
    char port[20]    = "15887";
};

static struct option options[] = {
    {"interface",   required_argument, NULL, 1},
    {"address",     required_argument, NULL, 2},
    {"port",        required_argument, NULL, 3},
    {0}
};

typedef tun<
        buffer_pool<buffer_pkt,
        final_layer
        >> tun_stack;

typedef tcp_sock_server<
        buffer_pool<buffer_pkt,
        final_layer
        >> server_stack;

typedef tcp_hdr<
        tcp_sock_peer<
        buffer_pool<buffer_pkt,
        final_layer
        >>> peer_stack;

class client : public signal, public io
{
    typedef std::unique_ptr<peer_stack> peer_ptr;
    std::vector<peer_ptr> m_peers;
    buffer_pkt::pointer m_tun_buf;
    tun_stack m_tun;
    server_stack m_srv;

    void recv_tun(int)
    {
        m_tun_buf = m_tun.buffer(m_tun.data_size_max());

        if (!m_tun.read_pkt(m_tun_buf))
            return;

        io::enable_write(m_peers.back()->fd());
        io::disable_read(m_tun.fd());
    }

    void recv_peer(int fd)
    {
        buffer_pkt::pointer buf = m_tun.buffer();

        while (m_peers[fd]->read_pkt(buf)) {
            m_tun.write_pkt(buf);
            buf->reset();
        }
    }

    void send_peer(int fd)
    {
        if (m_tun_buf)
            m_peers[fd]->write_pkt(m_tun_buf);

        m_tun_buf.reset();
        io::disable_write(fd);
        io::enable_read(m_tun.fd());
    }

    void accept_peer(int)
    {
        using std::placeholders::_1;

        peer_stack *p;
        int fd = m_srv.sock_accept();
        auto rp = std::bind(&client::recv_peer, this, _1);
        auto sp = std::bind(&client::send_peer, this, _1);

        p = new peer_stack(peer_stack::file_descriptor=fd);

        io::add_cb(fd, rp, sp);
        add_peer(peer_ptr(p));
    }

    void add_peer(peer_ptr p)
    {
        size_t max = p->fd() + 1;

        if (m_peers.size() < max)
            m_peers.resize(max);

        m_peers[p->fd()] = std::move(p);
        io::enable_read(m_tun.fd());
    }

  public:
    client(const struct args &args)
        : m_tun(m_tun.interface=args.interface),
          m_srv(server_stack::local_address=args.address,
                server_stack::port=args.port)
    {
        using std::placeholders::_1;

        auto rt = std::bind(&client::recv_tun, this, _1);
        auto ap = std::bind(&client::accept_peer, this, _1);

        io::add_cb(m_tun.fd(), rt, NULL);
        io::add_cb(m_srv.fd(), ap, NULL);
        io::disable_read(m_tun.fd());
    }

    void run()
    {
        while (signal::running())
        {
            if (io::wait() < 0)
                break;
        }
    }
};

int main(int argc, char **argv)
{
    struct args args;
    char c;

    while ((c = getopt_long_only(argc, argv, "", options, NULL)) != -1) {
        switch (c) {
            case 1:
                args.interface = optarg;
                break;
            case 2:
                strncpy(args.address, optarg, 20);
                break;
            case 3:
                strncpy(args.port, optarg, 20);
                break;
            case '?':
                return EXIT_FAILURE;
        }
    }

    client cl(args);
    cl.run();

    return EXIT_SUCCESS;
}
