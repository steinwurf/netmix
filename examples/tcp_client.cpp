#include <functional>
#include <iostream>
#include <getopt.h>

#include "signal.hpp"
#include "io.hpp"
#include "rlnc_codes.hpp"
#include "len_hdr.hpp"
#include "rlnc_data_enc.hpp"
#include "rlnc_hdr.hpp"
#include "budgets.hpp"
#include "tcp_hdr.hpp"
#include "tcp_sock.hpp"
#include "rlnc_info.hpp"
#include "error_info.hpp"
#include "final_layer.hpp"
#include "buffer_pkt.hpp"
#include "buffer_pool.hpp"

struct args {
    /* address to connect to */
    char address[20]   = "localhost";

    /* port to connect to */
    char port[20]      = "8899";

    /* time to wait for ACK before sending more symbols */
    size_t timeout     = 100;

    /* number of symbols in a block */
    size_t symbols     = 100;

    /* size of each symbol */
    size_t symbol_size = 1300;

    /* synthetic error probability (errors[2]) */
    double errors[4]   = {0.99, 0.99, 0.1, 0.99};
};

struct option options[] = {
    {"address",     required_argument, NULL, 1},
    {"port",        required_argument, NULL, 2},
    {"timeout",     required_argument, NULL, 3},
    {"symbols",     required_argument, NULL, 4},
    {"symbol_size", required_argument, NULL, 5},
    {"loss",        required_argument, NULL, 6},
    {0}
};

typedef fifi::binary8 field;

typedef len_hdr<
        rlnc_data_enc<kodo::sliding_window_encoder<fifi::binary8>,
        rlnc_hdr<
        source_budgets<
        tcp_hdr<
        tcp_sock_client<
        rlnc_info<
        error_info<
        buffer_pool<buffer_pkt,
        final_layer
        >>>>>>>>> client;

class tcp_client : public signal, public io
{
    client m_client;
    buffer_pkt::pointer m_buf;

    void read_in(int)
    {
        size_t len;

        m_buf = m_client.buffer();
        len = read(STDIN_FILENO, m_buf->data(), m_client.data_size_max());
        m_buf->data_trim(len);

        if (len == 0)
            return;

        io::disable_read(STDIN_FILENO);
        io::enable_write(m_client.fd());
    }

    void read_out(int)
    {
        bool res;

        do {
            auto buf = m_client.buffer();
            res = m_client.read_pkt(buf);

        } while (res);

        if (!m_client.is_full())
            io::enable_read(STDIN_FILENO);
    }

    void write_out(int)
    {
        io::disable_write(m_client.fd());

        if (m_buf) {
            m_client.write_pkt(m_buf);
            m_buf.reset();
        }

        if (!m_client.is_full())
            io::enable_read(STDIN_FILENO);
    }

  public:
    tcp_client(const struct args &args)
        : m_client(
                m_client.symbols=args.symbols,
                m_client.symbol_size=args.symbol_size,
                m_client.remote_address=args.address,
                m_client.errors=client::errors_type(args.errors, args.errors + sizeof(args.errors))
            )
    {
        using std::placeholders::_1;

        auto ro = std::bind(&tcp_client::read_out, this, _1);
        auto wo = std::bind(&tcp_client::write_out, this, _1);
        auto ri = std::bind(&tcp_client::read_in, this, _1);

        io::add_cb(m_client.fd(), ro, wo);
        io::add_cb(STDIN_FILENO, ri, NULL);
        io::disable_write(m_client.fd());
    }

    void run(size_t timeout)
    {
        int res;

        while (signal::running()) {
            res = io::wait(timeout);

            if (res == 0)
                m_client.timer();

            if (res < 0)
                break;
        }
    }
};

int main(int argc, char **argv)
{
    struct args args;
    signed char a;

    while ((a = getopt_long_only(argc, argv, "", options, NULL)) != -1) {
        switch (a) {
            case 1:
                strncpy(args.address, optarg, 20);
                break;

            case 2:
                strncpy(args.port, optarg, 20);
                break;

            case 3:
                args.timeout = atoi(optarg);
                break;

            case 4:
                args.symbols = atoi(optarg);
                break;

            case 5:
                args.symbol_size = atoi(optarg);
                break;

            case 6:
                args.errors[2] = strtod(optarg, NULL);
                break;

            case '?':
                return 1;
                break;
        }
    }

    tcp_client c(args);
    c.run(args.timeout);

    return EXIT_SUCCESS;
}
