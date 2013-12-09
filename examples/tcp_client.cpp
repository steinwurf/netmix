#include <functional>
#include <iostream>

#include "arguments.hpp"
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
    size_t m_timeout;
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
        : m_timeout(args.timeout),
          m_client(
                m_client.symbols=args.symbols,
                m_client.symbol_size=args.symbol_size,
                m_client.remote_address=args.dst.neighbor,
                m_client.errors=client::errors_type(args.dst.errors, args.dst.errors + e_max)
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

    void run()
    {
        int res;

        while (signal::running()) {
            res = io::wait(m_timeout);

            if (res == 0)
                m_client.timer();

            if (res < 0)
                break;
        }
    }
};

int main(int argc, char **argv)
{
    if (parse_args(argc, argv) < 0)
        return EXIT_FAILURE;

    if (args.help) {
        args_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    tcp_client c(args);
    c.run();

    return EXIT_SUCCESS;
}
