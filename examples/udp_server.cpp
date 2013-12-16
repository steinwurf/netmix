#include <iostream>
#include <getopt.h>

#include "io.hpp"
#include "signal.hpp"
#include "pause_hdr.hpp"
#include "plain_hdr.hpp"
#include "loss.hpp"
#include "udp_sock.hpp"
#include "buffer_pool.hpp"
#include "buffer_pkt.hpp"
#include "final_layer.hpp"

struct args {
    /* address to listen on */
    char address[20] = "localhost";

    /* port to listen on */
    char port[20]    = "8899";

    /* milliseconds to wait for missing packets */
    size_t timeout   = 100;

    /* syntethic loss probability */
    double loss      = .1;
};

struct option options[] = {
    {"address", required_argument, NULL, 1},
    {"port",    required_argument, NULL, 2},
    {"timeout", required_argument, NULL, 3},
    {"loss",    required_argument, NULL, 4},
    {0}
};

typedef pause_hdr<
        plain_hdr<
        loss<
        udp_sock_server<
        buffer_pool<buffer_pkt,
        final_layer
        >>>>> server;

class udp_server : public signal, public io, public server
{
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
        : server(
                server::local_address=args.address,
                server::port=args.port,
                server::loss_prob=args.loss
                )
    {
        using std::placeholders::_1;

        auto rp = std::bind(&udp_server::read_pkt, this, _1);

        io::add_cb(server::fd(), rp, NULL);
    }

    void run(size_t timeout)
    {
        int res;

        while (signal::running()) {
            res = io::wait(timeout);

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
                args.loss = strtod(optarg, NULL);
                break;

            case '?':
                return 1;
                break;
        }
    }

    udp_server u(args);
    u.run(args.timeout);

    return EXIT_SUCCESS;
}
