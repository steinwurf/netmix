#include <functional>

#include "arguments.hpp"
#include "signal.hpp"
#include "rlnc_codes.hpp"
#include "eth_filter.hpp"
#include "len_hdr.hpp"
#include "rlnc_data_enc.hpp"
#include "rlnc_hdr.hpp"
#include "budgets.hpp"
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

typedef tcp_hdr<
        tcp_sock_server<
        buffer_pool<buffer_pkt,
        final_layer
        >>> in;

typedef eth_filter_enc<
        len_hdr<
        rlnc_data_enc<kodo::sliding_window_encoder<fifi::binary8>,
        rlnc_hdr<
        source_budgets<
        eth_hdr<
        eth_topology<
        eth_sock<
        error_info<
        rlnc_info<
        buffer_pool<buffer_pkt,
        final_layer
        >>>>>>>>>>> out;

class rlnc_encoder : public signal, public io
{
    int m_timeout;
    in m_in;
    out m_out;

    void read_in(int)
    {
        bool res;
        buffer_pkt::pointer buf = m_in.buffer();

        while (true) {
            res = m_in.read_pkt(buf);

            if (!res)
                break;

            m_out.write_pkt(buf);
            buf->reset();

            if (m_out.is_full()) {
                io::disable_read(m_in.fd());
                break;
            }
        }
    }

    void read_out(int)
    {
        buffer_pkt::pointer buf = m_out.buffer();
        bool res, was_full;

        while (true) {
            was_full = m_out.is_full();
            res = m_out.read_pkt(buf);

            if (was_full && !m_out.is_full())
                io::enable_read(m_in.fd());

            if (!res)
                break;

            buf->reset();
        }
    }

  public:
    rlnc_encoder(const struct args &args)
        : m_timeout(args.timeout),
          m_in(m_in.local_address=args.address, m_in.port=args.port),
          m_out(
                m_out.interface=args.dst.interface,
                m_out.neighbor=args.dst.neighbor,
                m_out.helper=args.dst.helper,
                m_out.two_hop=args.dst.two_hop,
                m_out.symbols=args.symbols,
                m_out.symbol_size=args.symbol_size,
                m_out.errors=std::vector<double>(args.dst.errors, args.dst.errors + e_max)
               )
    {
        using std::placeholders::_1;

        auto ri = std::bind(&rlnc_encoder::read_in, this, _1);
        auto ro = std::bind(&rlnc_encoder::read_out, this, _1);

        io::add_cb(m_in.fd(), ri, NULL);
        io::add_cb(m_out.fd(), ro, NULL);
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

            m_out.timer();
            m_in.timer();
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

    rlnc_encoder e(args);
    e.run();

    return EXIT_SUCCESS;
}
