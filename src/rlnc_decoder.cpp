#include <functional>

#include "arguments.hpp"
#include "signal.hpp"
#include "rlnc_codes.hpp"
#include "eth_filter.hpp"
#include "len_hdr.hpp"
#include "pause_hdr.hpp"
#include "plain_hdr.hpp"
#include "rlnc_data_dec.hpp"
#include "rlnc_hdr.hpp"
#include "eth_hdr.hpp"
#include "loss.hpp"
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

typedef eth_filter_dec<
        len_hdr<
        rlnc_data_dec<kodo::sliding_window_decoder<fifi::binary8>,
        rlnc_hdr<
        eth_hdr<
        loss_dec<
        eth_topology<
        eth_sock<
        error_info<
        rlnc_info<
        buffer_pool<buffer_pkt,
        final_layer
        >>>>>>>>>>> in;

typedef tcp_hdr<
        tcp_sock_client<
        buffer_pool<buffer_pkt,
        final_layer
        >>> out;

class rlnc_decoder : public signal, public io
{
    in m_in;
    out m_out;

    void read_in(int)
    {
        buffer_pkt::pointer buf = m_in.buffer();

        while (m_in.read_pkt(buf)) {
            m_out.write_pkt(buf);
            buf->reset();
        }
    }

  public:
    rlnc_decoder(const struct args &args)
        : m_in(
                m_in.interface=args.src.interface,
                m_in.neighbor=args.src.neighbor,
                m_in.helper=args.src.helper,
                m_in.two_hop=args.src.two_hop,
                m_in.symbols=args.symbols,
                m_in.symbol_size=args.symbol_size,
                m_in.errors=std::vector<double>(args.src.errors, args.src.errors + e_max)
              ),
         m_out(m_out.local_address=args.address, m_out.port=args.port)
    {
        using std::placeholders::_1;

        auto ri = std::bind(&rlnc_decoder::read_in, this, _1);

        io::add_cb(m_in.fd(), ri, NULL);
    }

    void run()
    {
        while (signal::running()) {
            if (io::wait() < 0)
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

    rlnc_decoder d(args);
    d.run();

    return EXIT_SUCCESS;
}
