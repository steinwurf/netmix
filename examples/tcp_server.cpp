#include <functional>
#include <iostream>
#include <memory>

#include "arguments.hpp"
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
                     peer::symbols=args.symbols,
                     peer::symbol_size=args.symbol_size
                    );

        io::add_cb(fd, rp, NULL);
        add_peer(peer_ptr(p));
    }

  public:
    tcp_server(const struct args &args)
        : m_srv(
                m_srv.local_address=args.address,
                m_srv.port=args.port
               )
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
    if (parse_args(argc, argv) < 0)
        return EXIT_FAILURE;

    if (args.help) {
        args_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    tcp_server s(args);
    s.run();

    return EXIT_SUCCESS;
}
