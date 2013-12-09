#include <functional>
#include <iostream>
#include <getopt.h>

#include "io.hpp"
#include "signal.hpp"
#include "loss.hpp"
#include "eth_filter.hpp"
#include "eth_topology.hpp"
#include "eth_hdr.hpp"
#include "eth_sock.hpp"
#include "tcp_hdr.hpp"
#include "tcp_sock.hpp"
#include "buffer_pkt.hpp"
#include "buffer_pool.hpp"
#include "final_layer.hpp"

struct args {
    char address[20]         = "localhost";
    char port[20]            = "15887";
    char interface[IFNAMSIZ] = "lo";
    char *neighbor           = NULL;
    double loss              = .5;
};

struct option options[] = {
    {"address",   required_argument, NULL, 1},
    {"port",      required_argument, NULL, 2},
    {"interface", required_argument, NULL, 3},
    {"neighbor",  required_argument, NULL, 4},
    {"loss",      required_argument, NULL, 5},
    {0}
};

typedef tcp_hdr<
        tcp_sock_client<
        buffer_pool<buffer_pkt,
        final_layer
        >>> client_stack;

typedef loss<
        eth_filter_edge<
        eth_hdr<
        eth_topology<
        eth_sock<
        buffer_pool<buffer_pkt,
        final_layer
        >>>>>> relay_stack;

class edge : public signal, public io
{
    client_stack m_client;
    relay_stack m_relay;

    void copy_buf(buffer_pkt::pointer &buf_in,
                  buffer_pkt::pointer &buf_out)
    {
        buf_out->head_reserve(buf_in->head_len());
        buf_out->data_put(buf_in->len());
        memcpy(buf_out->head(), buf_in->head(), buf_in->len());
    }

    void read_client(int)
    {
        buffer_pkt::pointer buf_in = m_relay.buffer();
        buffer_pkt::pointer buf_out = m_relay.buffer();

        if (!m_client.read_pkt(buf_in))
            return;

        copy_buf(buf_in, buf_out);
        m_relay.write_pkt(buf_out);
    }

    void read_relay(int)
    {
        buffer_pkt::pointer buf_in = m_relay.buffer();
        buffer_pkt::pointer buf_out = m_relay.buffer();

        if (!m_relay.read_pkt(buf_in))
            return;

        copy_buf(buf_in, buf_out);
        m_client.write_pkt(buf_out);
    }

  public:
    edge(const struct args &args)
        : m_client(
                   client_stack::remote_address=args.address,
                   client_stack::port=args.port
                  ),
          m_relay(
                  relay_stack::interface=args.interface,
                  relay_stack::neighbor=args.neighbor,
                  relay_stack::loss_prob=args.loss
                 )
    {
        using std::placeholders::_1;

        auto rc = std::bind(&edge::read_client, this, _1);
        auto rr = std::bind(&edge::read_relay, this, _1);

        io::add_cb(m_client.fd(), rc, NULL);
        io::add_cb(m_relay.fd(), rr, NULL);
    }

    void run()
    {
        while (signal::running())
            if (io::wait() < 0)
                break;
    }
};

int main(int argc, char **argv)
{
    struct args args;
    char c;

    while ((c = getopt_long_only(argc, argv, "", options, NULL)) != -1) {
        switch (c) {
            case 1:
                strncpy(args.address, optarg, 20);
                break;
            case 2:
                strncpy(args.port, optarg, 20);
                break;
            case 3:
                strncpy(args.interface, optarg, IFNAMSIZ);
                break;
            case 4:
                args.neighbor = optarg;
                break;
            case 5:
                args.loss = strtod(optarg, NULL);
                break;
            case '?':
                return EXIT_FAILURE;
        }
    }

    edge e(args);
    e.run();

    return EXIT_SUCCESS;
}

