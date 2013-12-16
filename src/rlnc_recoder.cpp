#include <functional>
#include <vector>
#include <getopt.h>
#include <iostream>

#include "io.hpp"
#include "signal.hpp"
#include "rlnc_codes.hpp"
#include "eth_filter.hpp"
#include "rlnc_data_rec.hpp"
#include "rlnc_hdr.hpp"
#include "budgets.hpp"
#include "eth_hdr.hpp"
#include "loss.hpp"
#include "eth_topology.hpp"
#include "eth_sock.hpp"
#include "error_info.hpp"
#include "rlnc_info.hpp"
#include "buffer_pkt.hpp"
#include "buffer_pool.hpp"
#include "final_layer.hpp"

struct side_args {
    /* interface to use */
    char interface[IFNAMSIZ] = "lo";

    /* MAC address of neighbor node on interface */
    char *neighbor = NULL;

    /* MAC address of helper node in interface */
    char *helper = NULL;

    /* MAC address of node sending packets to neighbor */
    char *two_hop = NULL;
};

struct args {
    /* arguments for first stack */
    struct side_args a;

    /* arguments for second stack */
    struct side_args b;

    /* number of symbols in one block */
    size_t symbols = 100;

    /* size of each symbol */
    size_t symbol_size = 1450;

    /* time to wait for ack */
    size_t timeout = 20;

    /* synthetic error probabilities */
    std::vector<double> errors = {0.1, 0.1, 0.5, 0.75};
};

struct option options[] = {
    {"a_interface", required_argument, NULL,  1},
    {"b_interface", required_argument, NULL,  2},
    {"a_neighbor",  required_argument, NULL,  3},
    {"b_neighbor",  required_argument, NULL,  4},
    {"a_helper",    required_argument, NULL,  5},
    {"b_helper",    required_argument, NULL,  6},
    {"a_two_hop",   required_argument, NULL,  7},
    {"b_two_hop",   required_argument, NULL,  8},
    {"symbols",     required_argument, NULL,  9},
    {"symbol_size", required_argument, NULL, 10},
    {"e1",          required_argument, NULL, 11},
    {"e2",          required_argument, NULL, 12},
    {"e3",          required_argument, NULL, 13},
    {"e4",          required_argument, NULL, 14},
    {"timeout",     required_argument, NULL, 15},
    {0}
};

typedef eth_filter_rec<
        rlnc_data_rec<kodo::sliding_window_decoder<fifi::binary8>,
        rlnc_hdr<
        relay_budgets<
        eth_hdr<
        loss_dec<
        eth_topology<
        eth_sock<
        error_info<
        rlnc_info<
        buffer_pool<buffer_pkt,
        final_layer
        >>>>>>>>>>> rec_stack;

class rlnc_recoder : public signal, public io
{
    size_t m_timeout;
    rec_stack m_a;
    rec_stack m_b;

    void read_a(int)
    {
        buffer_pkt::pointer buf = m_a.buffer();

        while (m_a.read_pkt(buf)) {
            m_b.write_pkt(buf);
            buf->reset();

            if (m_b.is_full())
                m_a.stop();
        }
    }

    void read_b(int)
    {
        buffer_pkt::pointer buf(m_b.buffer());

        while (m_b.read_pkt(buf)) {
            m_a.write_pkt(buf);
            buf->reset();

            if (m_a.is_full()) {
                m_b.stop();
                break;
            }
        }
    }

  public:
    rlnc_recoder(const struct args &args)
        : m_timeout(args.timeout),
          m_a(
              rec_stack::interface=args.a.interface,
              rec_stack::neighbor=args.a.neighbor,
              rec_stack::helper=args.a.helper,
              rec_stack::two_hop=args.a.two_hop,
              rec_stack::symbols=args.symbols,
              rec_stack::symbol_size=args.symbol_size,
              rec_stack::errors=args.errors
             ),
          m_b(
              rec_stack::interface=args.b.interface,
              rec_stack::neighbor=args.b.neighbor,
              rec_stack::helper=args.b.helper,
              rec_stack::two_hop=args.b.two_hop,
              rec_stack::symbols=args.symbols,
              rec_stack::symbol_size=args.symbol_size,
              rec_stack::errors=args.errors
             )
    {
        using std::placeholders::_1;

        auto ra = std::bind(&rlnc_recoder::read_a, this, _1);
        auto rb = std::bind(&rlnc_recoder::read_b, this, _1);

        io::add_cb(m_a.fd(), ra, NULL);
        io::add_cb(m_b.fd(), rb, NULL);
    }

    void run()
    {
        int res;

        while (signal::running()) {
            res = io::wait(m_timeout);

            if (res < 0)
                return;

            if (res > 0)
                continue;

            m_a.timer();
            m_b.timer();
        }
    }
};

int main(int argc, char **argv)
{
    struct args args;
    signed char c;

    while ((c = getopt_long_only(argc, argv, "", options, NULL)) != -1) {
        switch (c) {
            case 1:
                strncpy(args.a.interface, optarg, IFNAMSIZ);
                break;
            case 2:
                strncpy(args.b.interface, optarg, IFNAMSIZ);
                break;
            case 3:
                args.a.neighbor = optarg;
                break;
            case 4:
                args.b.neighbor = optarg;
                break;
            case 5:
                args.a.helper = optarg;
                break;
            case 6:
                args.b.helper = optarg;
                break;
            case 7:
                args.a.two_hop = optarg;
                break;
            case 8:
                args.b.two_hop = optarg;
                break;
            case 9:
                args.symbols = atoi(optarg);
                break;
            case 10:
                args.symbol_size = atoi(optarg);
                break;
            case 11:
                args.errors[0] = strtod(optarg, NULL);
                break;
            case 12:
                args.errors[1] = strtod(optarg, NULL);
                break;
            case 13:
                args.errors[2] = strtod(optarg, NULL);
                break;
            case 14:
                args.errors[3] = strtod(optarg, NULL);
                break;
            case 15:
                args.timeout = atoi(optarg);
                break;
            default:
                return EXIT_FAILURE;
        }
    }

    rlnc_recoder r(args);
    r.run();

    return EXIT_SUCCESS;
}
