#include <iostream>
#include <thread>
#include <chrono>

#include "arguments.hpp"
#include "io.hpp"
#include "signal.hpp"
#include "pause_hdr.hpp"
#include "plain_hdr.hpp"
#include "udp_sock.hpp"
#include "buffer_pool.hpp"
#include "buffer_pkt.hpp"
#include "final_layer.hpp"

typedef pause_hdr<
        plain_hdr<
        udp_sock_client<
        buffer_pool<buffer_pkt,
        final_layer
        >>>> client;

class udp_client : public signal, public io, public client
{
    size_t m_timeout;

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
        : client(),
          m_timeout(args.timeout)
    {
        using std::placeholders::_1;

        auto rp = std::bind(&udp_client::read_pkt, this, _1);
        auto wp = std::bind(&udp_client::write_pkt, this, _1);

        io::add_cb(client::fd(), rp, NULL);
        io::add_cb(STDIN_FILENO, wp, NULL);
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

            client::timer();
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

    udp_client u(args);
    u.run();

    return EXIT_SUCCESS;
}
