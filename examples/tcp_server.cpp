#include <functional>
#include <iostream>
#include <memory>
#include <getopt.h>

#include "signal.hpp"
#include "io.hpp"
#include "rlnc_codes.hpp"
#include "len_hdr.hpp"
#include "rlnc_data_dec.hpp"
#include "rlnc_hdr.hpp"
#include "rlnc_info.hpp"
#include "loss.hpp"
#include "tcp_hdr.hpp"
#include "tcp_sock.hpp"
#include "final_layer.hpp"
#include "buffer_pkt.hpp"
#include "buffer_pool.hpp"

struct args {
    /* address to listen on */
    char address[20]   = "localhost";

    /* port to listen on */
    char port[20]      = "8899";

    /* number of symbols in block */
    size_t symbols     = 100;

    /* size of each symbol */
    size_t symbol_size = 1300;

    /* synthetic error probability (errors[2]) */
    double errors[4]   = {0.99, 0.99, 0.1, 0.99};
};

struct option options[] = {
    {"address",     required_argument, NULL, 1},
    {"port",        required_argument, NULL, 2},
    {"symbols",     required_argument, NULL, 3},
    {"symbol_size", required_argument, NULL, 4},
    {"loss",        required_argument, NULL, 5},
    {0}
};

typedef len_hdr<
        rlnc_data_dec<kodo::sliding_window_decoder<fifi::binary8>,
        rlnc_hdr<
        loss<
        tcp_hdr<
        tcp_sock_peer<
        rlnc_info<
        buffer_pool<buffer_pkt,
        final_layer
        >>>>>>>> peer;

typedef tcp_sock_server<
        buffer_pool<buffer_pkt,
        final_layer
        >> server;

class tcp_server : public signal, public io
{
    typedef std::unique_ptr<peer> peer_ptr;
    std::vector<peer_ptr> m_peers;
    server m_srv;
    size_t m_symbols;
    size_t m_symbol_size;

    void add_peer(peer_ptr p)
    {
        size_t max = p->fd() + 1;

        if (m_peers.size() < max)
            m_peers.resize(max);

        m_peers[p->fd()] = std::move(p);
    }

    void read_pkt(int fd)
    {
        auto buf = m_peers[fd]->buffer();

        while (m_peers[fd]->read_pkt(buf)) {
            std::cout << std::string(buf->c_str());
            std::cout.flush();
            buf->reset();
        }
    }

    void accept_peer(int)
    {
        using std::placeholders::_1;

        auto rp = std::bind(&tcp_server::read_pkt, this, _1);
        int fd = m_srv.sock_accept();
        peer *p;

        p = new peer(
                     peer::file_descriptor=fd,
                     peer::symbols=m_symbols,
                     peer::symbol_size=m_symbol_size
                    );

        io::add_cb(fd, rp, NULL);
        add_peer(peer_ptr(p));
    }

  public:
    tcp_server(const struct args &args)
        : m_srv(
                m_srv.local_address=args.address,
                m_srv.port=args.port
               ),
          m_symbols(args.symbols),
          m_symbol_size(args.symbol_size)
    {
        using std::placeholders::_1;

        auto ap = std::bind(&tcp_server::accept_peer, this, _1);

        io::add_cb(m_srv.fd(), ap, nullptr);
    }

    void run()
    {
        while (signal::running())
            io::wait();
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
                args.symbols = atoi(optarg);
                break;

            case 4:
                args.symbol_size = atoi(optarg);
                break;

            case 5:
                args.errors[2] = strtod(optarg, NULL);
                break;

            case '?':
                return 1;
                break;
        }
    }

    tcp_server s(args);
    s.run();

    return EXIT_SUCCESS;
}
