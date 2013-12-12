#include <functional>
#include <getopt.h>

#include "signal.hpp"
#include "rlnc_codes.hpp"
#include "eth_filter.hpp"
#include "rlnc_data_hlp.hpp"
#include "rlnc_hdr.hpp"
#include "budgets.hpp"
#include "eth_hdr.hpp"
#include "loss.hpp"
#include "eth_topology.hpp"
#include "eth_sock.hpp"
#include "rlnc_info.hpp"
#include "error_info.hpp"
#include "buffer_pkt.hpp"
#include "buffer_pool.hpp"
#include "final_layer.hpp"
#include "io.hpp"

struct args {
    char a_interface[IFNAMSIZ] = "lo";
    char b_interface[IFNAMSIZ] = "lo";
    char *a_neighbor           = NULL;
    char *b_neighbor           = NULL;
    std::vector<double> errors = {0.1, 0.1, 0.5, 0.75};
    size_t symbols             = 100;
    size_t symbol_size         = 1450;
};

static struct option options[] = {
    {"a_interface", required_argument, NULL, 1},
    {"b_interface", required_argument, NULL, 2},
    {"a_neighbor",  required_argument, NULL, 3},
    {"b_neighbor",  required_argument, NULL, 4},
    {"e1",          required_argument, NULL, 5},
    {"e2",          required_argument, NULL, 6},
    {"e3",          required_argument, NULL, 7},
    {"e4",          required_argument, NULL, 8},
    {"symbols",     required_argument, NULL, 9},
    {"symbol_size", required_argument, NULL, 10},
    {0}
};

typedef eth_filter_hlp<
        rlnc_data_hlp<kodo::sliding_window_decoder<fifi::binary8>,
        rlnc_hdr<
        helper_budgets<
        eth_hdr<
        loss_hlp<
        eth_topology<
        eth_sock<
        error_info<
        rlnc_info<
        buffer_pool<buffer_pkt,
        final_layer
        >>>>>>>>>>> hlp_stack;

class rlnc_helper : public signal, public io
{
    hlp_stack m_a;
    hlp_stack m_b;

    void read_a(int)
    {
        buffer_pkt::pointer buf(m_a.buffer());

        while (m_a.read_pkt(buf)) {
            m_b.write_pkt(buf);
            buf->reset();
        }
    }

    void read_b(int)
    {
        buffer_pkt::pointer buf(m_b.buffer());

        while (m_b.read_pkt(buf)) {
            m_a.write_pkt(buf);
            buf->reset();
        }
    }

  public:
    rlnc_helper(const struct args &args)
        : m_a(
              hlp_stack::interface=args.a_interface,
              hlp_stack::neighbor=args.a_neighbor,
              hlp_stack::source=args.b_neighbor,
              hlp_stack::destination=args.a_neighbor,
              hlp_stack::symbols=args.symbols,
              hlp_stack::symbol_size=args.symbol_size,
              hlp_stack::errors=args.errors
             ),
          m_b(
              hlp_stack::interface=args.b_interface,
              hlp_stack::neighbor=args.b_neighbor,
              hlp_stack::source=args.a_neighbor,
              hlp_stack::destination=args.b_neighbor,
              hlp_stack::symbols=args.symbols,
              hlp_stack::symbol_size=args.symbol_size,
              hlp_stack::errors=args.errors
             )
    {
        using std::placeholders::_1;

        auto ra = std::bind(&rlnc_helper::read_a, this, _1);
        auto rb = std::bind(&rlnc_helper::read_b, this, _1);

        io::add_cb(m_a.fd(), ra, NULL);
        io::add_cb(m_b.fd(), rb, NULL);
    }

    void run()
    {
        int res;

        while (signal::running()) {
            res = io::wait();

            if (res < 0)
                break;

            if (res > 0)
                continue;
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
                args.errors[0] = strtod(optarg, NULL);
                break;
            case 6:
                args.errors[1] = strtod(optarg, NULL);
                break;
            case 7:
                args.errors[2] = strtod(optarg, NULL);
                break;
            case 8:
                args.errors[3] = strtod(optarg, NULL);
                break;
            case 9:
                args.symbols = atoi(optarg);
                break;
            case 10:
                args.symbol_size = atoi(optarg);
                break;
            case '?':
                return EXIT_FAILURE;
        }
    }

    rlnc_helper h(args);
    h.run();

    return EXIT_SUCCESS;
}
