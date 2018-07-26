#include <functional>
#include <cstdlib>
#include <getopt.h>

#include "signal.hpp"
#include "loss.hpp"
#include "eth_topology.hpp"
#include "eth_hdr.hpp"
#include "eth_sock.hpp"
#include "buffer_pkt.hpp"
#include "buffer_pool.hpp"
#include "io.hpp"
#include "final_layer.hpp"

struct args {
    /* interfaces to relay raw packets from/to */
    char a_interface[IFNAMSIZ] = "lo";
    char b_interface[IFNAMSIZ] = "lo";

    /* mac addresses to relay raw packets from/to */
    char *a_neighbor           = NULL;
    char *b_neighbor           = NULL;

    /* synthetic loss probability for packets from neighbor */
    double loss              = .5;
};

struct option options[] = {
    {"a_interface", required_argument, NULL, 1},
    {"b_interface", required_argument, NULL, 2},
    {"a_neighbor",  required_argument, NULL, 3},
    {"b_neighbor",  required_argument, NULL, 4},
    {"loss",      required_argument, NULL, 5},
    {0}
};

typedef loss<
        eth_hdr<
        eth_topology<
        eth_sock<
        buffer_pool<buffer_pkt,
        final_layer
        >>>>> relay_stack;


class relay : public signal, public io
{
    relay_stack m_a;
    relay_stack m_b;

    void read_a(int)
    {
        buffer_pkt::pointer buf = m_a.buffer();

        while (m_a.read_pkt(buf)) {
            m_b.write_pkt(buf);
            buf->reset();
        }
    }

    void read_b(int)
    {
        buffer_pkt::pointer buf = m_b.buffer();

        while (m_b.read_pkt(buf)) {
            m_a.write_pkt(buf);
            buf->reset();
        }
    }

  public:
    relay(const struct args &args)
        : m_a(
                relay_stack::interface=args.a_interface,
                relay_stack::neighbor=args.a_neighbor,
                relay_stack::loss_prob=args.loss
             ),
          m_b(
                relay_stack::interface=args.b_interface,
                relay_stack::neighbor=args.b_neighbor,
                relay_stack::loss_prob=args.loss
             )
    {
        using std::placeholders::_1;

        auto ra = std::bind(&relay::read_a, this, _1);
        auto rb = std::bind(&relay::read_b, this, _1);

        io::add_cb(m_a.fd(), ra, NULL);
        io::add_cb(m_b.fd(), rb, NULL);
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
    signed char c;

    while ((c = getopt_long_only(argc, argv, "", options, NULL)) != -1) {
        switch (c) {
            case 1:
                strncpy(args.a_interface, optarg, IFNAMSIZ);
                break;
            case 2:
                strncpy(args.b_interface, optarg, IFNAMSIZ);
                break;
            case 3:
                args.a_neighbor = optarg;
                break;
            case 4:
                args.b_neighbor = optarg;
                break;
            case 5:
                args.loss = strtod(optarg, NULL);
                break;
            case '?':
                return EXIT_FAILURE;
        }
    }

    relay r(args);
    r.run();

    return EXIT_SUCCESS;
}
