#include <iostream>
#include <thread>
#include <chrono>
#include <getopt.h>

#include "io.hpp"
#include "signal.hpp"
#include "pause_hdr.hpp"
#include "plain_hdr.hpp"
#include "udp_sock.hpp"
#include "buffer_pool.hpp"
#include "buffer_pkt.hpp"
#include "final_layer.hpp"

struct args {
    /* address to send datagrams to */
    char address[20] = "localhost";

    /* port to send datagrams to */
    char port[20]    = "8899";

    /* time to wait for ack */
    size_t timeout   = 100;
};

struct option options[] = {
    {"address", required_argument, NULL, 1},
    {"port",    required_argument, NULL, 2},
    {"timeout", required_argument, NULL, 3},
    {0}
};

typedef pause_hdr<
        plain_hdr<
        udp_sock_client<
        buffer_pool<buffer_pkt,
        final_layer
        >>>> client;

class udp_client : public signal, public io, public client
{
    void read_pkt(int)
    {
        while (true) {
            auto buf = client::buffer();
            bool was_full = client::is_full();
            bool res = client::read_pkt(buf);

            if (was_full && !client::is_full())
                io::enable_read(STDIN_FILENO);

            if (!res)
                break;
        }
    }

    void write_pkt(int)
    {
        size_t str_len;
        std::string str;
        auto buf = client::buffer();
        std::chrono::milliseconds delay(10);

        std::getline(std::cin, str);
        str_len = str.size() + 1; // include null termination
        std::cout << "sending: " << str << std::endl;
        memcpy(buf->data_put(str_len), str.c_str(), str_len);
        client::write_pkt(buf);

        if (client::is_full())
            io::disable_read(STDIN_FILENO);
    }

  public:
    udp_client(const struct args &args)
        : client(
                client::remote_address=args.address,
                client::port=args.port
                )
    {
        using std::placeholders::_1;

        auto rp = std::bind(&udp_client::read_pkt, this, _1);
        auto wp = std::bind(&udp_client::write_pkt, this, _1);

        io::add_cb(client::fd(), rp, NULL);
        io::add_cb(STDIN_FILENO, wp, NULL);
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

            client::timer();
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

            case '?':
                return 1;
                break;
        }
    }

    udp_client u(args);
    u.run(args.timeout);

    return EXIT_SUCCESS;
}
