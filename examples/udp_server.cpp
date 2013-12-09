#include <iostream>

#include "arguments.hpp"
#include "io.hpp"
#include "signal.hpp"
#include "pause_hdr.hpp"
#include "plain_hdr.hpp"
#include "loss.hpp"
#include "udp_sock.hpp"
#include "buffer_pool.hpp"
#include "buffer_pkt.hpp"
#include "final_layer.hpp"

typedef pause_hdr<
        plain_hdr<
        loss<
        udp_sock_server<
        buffer_pool<buffer_pkt,
        final_layer
        >>>>> server;

class udp_server : public signal, public io, public server
{
    size_t m_timeout;

    void read_pkt(int)
    {
        auto buf = server::buffer();

        while (server::read_pkt(buf)) {
            std::cout << "received: " << std::string(buf->c_str()) << std::endl;
            buf->reset();
        }
    }

  public:
    udp_server(const struct args &args)
        : server(loss_prob=.10),
          m_timeout(args.timeout)
    {
        using std::placeholders::_1;

        auto rp = std::bind(&udp_server::read_pkt, this, _1);

        io::add_cb(server::fd(), rp, NULL);
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

            server::timer();
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

    udp_server u(args);
    u.run();

    return EXIT_SUCCESS;
}
