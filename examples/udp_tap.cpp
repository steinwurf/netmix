#include <iostream>
#include <getopt.h>
#include <cstring>
#include <vector>
#include <memory>
#include <functional>
#include <endian.h>

#include "io.hpp"
#include "signal.hpp"
#include "udp_sock.hpp"
#include "tun.hpp"
#include "buffer_pool.hpp"
#include "buffer_pkt.hpp"
#include "final_layer.hpp"

struct args {
    /* name of virtual interface */
    char interface[IFNAMSIZ] = "tun0";

    /* address to listen on (server)
     * address to connect to (client) */
    char address[20] = "localhost";

    /* port number to use for tcp connections */
    char port[20] = "8899";

    /* type of virtual interface (tun/tap) */
    char type[sizeof("tun")] = "tun";

    /* source address to use for the connection (client only) */
    char *src = NULL;

    /* number of symbols in one block */
    size_t symbols = 100;

    /* size of each symbol in block */
    size_t symbol_size = 1400;

    /* start as server if argument is given, client otherwise */
    bool server = false;

    /* size of udp socket buffer for sending */
    size_t send_buf = 16384;

    /* number of packets received between sending status reports */
    size_t status_interval = 50;
};

struct option options[] = {
    {"interface",       required_argument, NULL, 1},
    {"address",         required_argument, NULL, 2},
    {"port",            required_argument, NULL, 3},
    {"src",             required_argument, NULL, 4},
    {"symbols",         required_argument, NULL, 5},
    {"symbol_size",     required_argument, NULL, 6},
    {"server",          no_argument,       NULL, 7},
    {"status_interval", required_argument, NULL, 8},
    {"type",            required_argument, NULL, 9},
    {0}
};

typedef tuntap<
        buffer_pool<buffer_pkt,
        final_layer
        >> tun_stack;

typedef udp_sock_client<
        buffer_pool<buffer_pkt,
        final_layer
        >> client_stack;

typedef udp_sock_server<
        buffer_pool<buffer_pkt,
        final_layer
        >> server_stack;

template<class stack>
class handler
{
  public:
    typedef buffer_pkt::pointer buf_ptr;
    typedef std::unique_ptr<stack> peer_ptr;

    io m_io;

  private:
    class signal m_sig;
    tun_stack m_tun;
    peer_ptr m_peer;


    void recv_tun(int /*fd*/)
    {
        buf_ptr buf = m_tun.buffer(m_tun.data_size_max());

        while (m_tun.read_pkt(buf))
        {
            m_peer->write_pkt(buf);
            buf->reset();
        }

    }

    void recv_peer(int /*fd*/)
    {
        buf_ptr buf = m_tun.buffer();

        while (m_peer->read_pkt(buf))
        {
            m_tun.write_pkt(buf);
            buf->reset();
        }
    }

  public:
    handler(const struct args &args)
        : m_tun(tun_stack::interface=args.interface,
                tun_stack::type=args.type)
    {
        using std::placeholders::_1;

        auto rt = std::bind(&handler::recv_tun, this, _1);

        m_io.add_cb(m_tun.fd(), rt, NULL);
    }

    void add_peer(peer_ptr p)
    {
        using std::placeholders::_1;

        auto rp = std::bind(&handler::recv_peer, this, _1);

        m_io.add_cb(p->fd(), rp, NULL);

        m_peer = std::move(p);
    }

    void run()
    {
        int res;

        while (m_sig.running()) {
            res = m_io.wait();

            if (res < 0)
                break;

            if (res > 0)
                continue;
        }
    }
};

class client : public handler<client_stack>
{
  public:
    client(const struct args &args)
        : handler(args)
    {
        client_stack *c = new client_stack(
            client_stack::local_address=args.src,
            client_stack::remote_address=args.address,
            client_stack::port=args.port
        );
        add_peer(peer_ptr(c));
    }

};

class server : public handler<server_stack>
{
  public:
    server(const struct args &args)
        : handler(args)
    {
        server_stack *s = new server_stack(
            server_stack::local_address=args.address,
            server_stack::port=args.port
        );

        add_peer(peer_ptr(s));
    }
};

int main(int argc, char **argv)
{
    struct args args;
    signed char a;

    while ((a = getopt_long_only(argc, argv, "", options, NULL)) != -1) {
        switch (a) {
            case 1:
                strncpy(args.interface, optarg, IFNAMSIZ);
                break;
            case 2:
                strncpy(args.address, optarg, 20);
                break;
            case 3:
                strncpy(args.port, optarg, 20);
                break;
            case 4:
                args.src = optarg;
                break;
            case 5:
                args.symbols = atoi(optarg);
                break;
            case 6:
                args.symbol_size = atoi(optarg);
                break;
            case 7:
                args.server = true;
                break;
            case 8:
                args.status_interval = atoi(optarg);
                break;
            case 9:
                strncpy(args.type, optarg, sizeof(args.type));
                break;
            case '?':
                return 1;
                break;
        }
    }

    if (args.server) {
        server s(args);
        s.run();
    } else {
        client c(args);
        c.run();
    }

    return 0;
}
