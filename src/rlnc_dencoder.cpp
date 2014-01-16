#include <functional>
#include <cstring>
#include <getopt.h>

#include "signal.hpp"
#include "rlnc_codes.hpp"
#include "eth_filter.hpp"
#include "len_hdr.hpp"
#include "rlnc_data_enc.hpp"
#include "rlnc_data_dec.hpp"
#include "rlnc_hdr.hpp"
#include "budgets.hpp"
#include "loss.hpp"
#include "eth_hdr.hpp"
#include "eth_topology.hpp"
#include "eth_sock.hpp"
#include "tcp_hdr.hpp"
#include "tcp_sock.hpp"
#include "error_info.hpp"
#include "rlnc_info.hpp"
#include "buffer_pkt.hpp"
#include "buffer_pool.hpp"
#include "final_layer.hpp"
#include "io.hpp"

struct args
{
    /* address to connect to */
    char    address[20]         = "localhost";

    /* port to connect to */
    char    port[20]            = "15887";

    /* interface to send/receive encoded packets on */
    char    interface[IFNAMSIZ] = "lo";

    /* MAC address of node to send/receive encoded packets from/to */
    char    *neighbor           = NULL;

    /* MAC of node injecting packets from neighbor */
    char    *helper             = NULL;

    /* address of node sending packets to neighbor */
    char    *two_hop            = NULL;

    /* synthetic error probabilities */
    std::vector<double> errors  = {0.1, 0.1, 0.5, 0.75};

    /* number of symbols in one block */
    size_t  symbols             = 100;

    /* size of each symbol */
    size_t  symbol_size         = 1450;

    /* milliseconds to wait for ACK */
    ssize_t timeout             = 20;

    /* ratio to multiply source budget with */
    double overshoot            = 1.05;
};

static struct option options[] = {
    {"address",     required_argument, NULL, 1},
    {"port",        required_argument, NULL, 2},
    {"interface",   required_argument, NULL, 3},
    {"neighbor",    required_argument, NULL, 4},
    {"helper",      required_argument, NULL, 5},
    {"two_hop",     required_argument, NULL, 6},
    {"symbols",     required_argument, NULL, 7},
    {"symbol_size", required_argument, NULL, 8},
    {"e1",          required_argument, NULL, 9},
    {"e2",          required_argument, NULL, 10},
    {"e3",          required_argument, NULL, 11},
    {"e4",          required_argument, NULL, 12},
    {"timeout",     required_argument, NULL, 13},
    {"overshoot",   required_argument, NULL, 14},
    {0}
};

typedef tcp_hdr<
        tcp_sock_client<
        buffer_pool<buffer_pkt,
        final_layer
        >>> client_stack;

typedef eth_filter_enc<
        len_hdr<
        rlnc_data_enc<kodo::sliding_window_encoder<fifi::binary>,
        rlnc_hdr<
        source_budgets<
        eth_hdr<
        eth_topology<
        eth_sock<
        error_info<
        rlnc_info<
        buffer_pool<buffer_pkt,
        final_layer
        >>>>>>>>>>> enc_stack;

typedef eth_filter_dec<
        len_hdr<
        rlnc_data_dec<kodo::sliding_window_decoder<fifi::binary>,
        rlnc_hdr<
        eth_hdr<
        loss_dec<
        eth_topology<
        eth_sock<
        error_info<
        rlnc_info<
        buffer_pool<buffer_pkt,
        final_layer
        >>>>>>>>>>> dec_stack;

class rlnc_dencoder : public signal, public io
{
    int m_timeout;
    client_stack m_client;
    enc_stack m_enc;
    dec_stack m_dec;

    void read_client(int)
    {
        bool res;
        buffer_pkt::pointer buf = m_client.buffer();

        while (true) {
            res = m_client.read_pkt(buf);

            if (!res)
                break;

            m_enc.write_pkt(buf);
            buf->reset();

            if (m_enc.is_full()) {
                io::disable_read(m_client.fd());
                break;
            }
        }
    }

    void read_enc(int)
    {
        buffer_pkt::pointer buf = m_enc.buffer();
        bool res, was_full;

        while (true) {
            was_full = m_enc.is_full();
            res = m_enc.read_pkt(buf);

            if (was_full && !m_enc.is_full())
                io::enable_read(m_client.fd());

            if (!res)
                break;

            buf->reset();
        }
    }

    void read_dec(int)
    {
        buffer_pkt::pointer buf = m_dec.buffer();

        while (m_dec.read_pkt(buf)) {
            if (!m_client.write_pkt(buf)) {
                break;
            }
            buf->reset();
        }
    }

  public:
    rlnc_dencoder(const struct args &args)
        : m_timeout(args.timeout),
          m_client(
                   client_stack::remote_address=args.address,
                   client_stack::port=args.port
          ),
          m_enc(
                enc_stack::interface=args.interface,
                enc_stack::neighbor=args.neighbor,
                enc_stack::helper=args.helper,
                enc_stack::two_hop=args.two_hop,
                enc_stack::symbols=args.symbols,
                enc_stack::symbol_size=args.symbol_size,
                enc_stack::errors=args.errors,
                enc_stack::overshoot=args.overshoot
          ),
          m_dec(
                dec_stack::interface=args.interface,
                dec_stack::neighbor=args.neighbor,
                dec_stack::helper=args.helper,
                dec_stack::two_hop=args.two_hop,
                dec_stack::symbols=args.symbols,
                dec_stack::symbol_size=args.symbol_size,
                dec_stack::errors=args.errors
          )
    {
        using std::placeholders::_1;

        auto rc = std::bind(&rlnc_dencoder::read_client, this, _1);
        auto re = std::bind(&rlnc_dencoder::read_enc, this, _1);
        auto rd = std::bind(&rlnc_dencoder::read_dec, this, _1);

        io::add_cb(m_client.fd(), rc, NULL);
        io::add_cb(m_enc.fd(), re, NULL);
        io::add_cb(m_dec.fd(), rd, NULL);
    }

    void run()
    {
        int res;

        while (signal::running()) {
            res = io::wait(m_timeout);

            if (res < 0)
                break;

            if (res > 0)
                continue;

            m_dec.timer();
            m_enc.timer();
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
                args.helper = optarg;
                break;
            case 6:
                args.two_hop = optarg;
                break;
            case 7:
                args.symbols = atoi(optarg);
                break;
            case 8:
                args.symbol_size = atoi(optarg);
                break;
            case 9:
                args.errors[0] = strtod(optarg, NULL);
                break;
            case 10:
                args.errors[1] = strtod(optarg, NULL);
                break;
            case 11:
                args.errors[2] = strtod(optarg, NULL);
                break;
            case 12:
                args.errors[3] = strtod(optarg, NULL);
                break;
            case 13:
                args.timeout = atoi(optarg);
                break;
            case 14:
                args.overshoot = strtod(optarg, NULL);
                break;
            case '?':
                return EXIT_FAILURE;
        }
    }

    rlnc_dencoder de(args);

    try {
        de.run();
    } catch (const std::runtime_error &re) {
        std::cout << re.what() << std::endl;
    }

    return EXIT_SUCCESS;
}
