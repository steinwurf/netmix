#include <functional>
#include <iostream>
#include <iomanip>
#include <memory>
#include <chrono>
#include <getopt.h>
#include <endian.h>
#include <deque>
#include <cmath>
#include <cstdlib>

#include "io.hpp"
#include "signal.hpp"
#include "tcp_hdr.hpp"
#include "tcp_sock.hpp"
#include "buffer_pkt.hpp"
#include "buffer_pool.hpp"
#include "final_layer.hpp"

struct args {
    /* address to listen on (server)
     * address to connect to (client) */
    char address[20] = "localhost";

    /* port to use for connection */
    char port[20]    = "15887";

    /* interval between sending data */
    size_t interval  = 1000;

    /* number of data writes */
    size_t count     = 0;

    /* start as server instead of client */
    bool server      = false;

    /* don't print results on the fly */
    bool quiet       = false;
};

struct option options[] = {
    {"address",  required_argument, NULL, 1},
    {"port",     required_argument, NULL, 2},
    {"interval", required_argument, NULL, 3},
    {"count",    required_argument, NULL, 4},
    {"server",   no_argument,       NULL, 5},
    {"quiet",    no_argument,       NULL, 6},
    {0}
};

typedef tcp_hdr<
        tcp_sock_client<
        buffer_pool<buffer_pkt,
        final_layer
        >>> client_stack;

typedef tcp_hdr<
        tcp_sock_peer<
        buffer_pool<buffer_pkt,
        final_layer
        >>> peer_stack;

typedef tcp_sock_server<
        buffer_pool<buffer_pkt,
        final_layer
        >> server_stack;

class client : public io, public signal
{
    typedef buffer_pkt::pointer buf_ptr;
    typedef std::chrono::steady_clock clock;
    typedef clock::time_point time_point;
    typedef std::chrono::milliseconds resolution;

    static constexpr size_t m_payload_len = 10;
    time_point m_send_time;
    time_point m_recv_time;
    std::deque<time_point> m_times;
    client_stack m_client;
    bool   m_quiet;
    size_t m_interval;
    size_t m_limit;
    size_t m_min = -1;
    size_t m_max = 0;
    size_t m_sum = 0;
    size_t m_sq_sum = 0;
    size_t m_count = 0;
    double m_std_dev = 0;

    void send_req()
    {
        buffer_pkt::pointer buf = m_client.buffer();
        buf->data_put(10);
        m_times.push_back(clock::now());
        m_client.write_pkt(buf);
    }

    void read_res(int)
    {
        buffer_pkt::pointer buf = m_client.buffer();

        if (!m_client.read_pkt(buf))
            return;

        m_recv_time = clock::now();
        m_send_time = m_times.front();
        m_times.pop_front();
        update_counters();
    }

    void update_counters()
    {
        using std::chrono::duration_cast;

        size_t delay = duration_cast<resolution>(m_recv_time - m_send_time).count();

        m_count++;
        m_sum += delay;
        m_sq_sum += delay*delay;
        m_min = std::min(m_min, delay);
        m_max = std::max(m_max, delay);

        if (m_quiet)
            return;

        std::cout << "seq: "   << std::setw(3) << m_count
                  << ", rtt: " << std::setw(4) << delay << " ms"
                  << std::endl;
    }

  public:
    client(const struct args &args)
        : m_client(
                   client_stack::remote_address=args.address,
                   client_stack::port=args.port
                  ),
          m_quiet(args.quiet),
          m_interval(args.interval),
          m_limit(args.count)
    {
        using std::placeholders::_1;

        auto rr = std::bind(&client::read_res, this, _1);

        io::add_cb(m_client.fd(), rr, NULL);
    }

    void print()
    {
        if (m_count < 2)
            return;

        double mean = 1.0*m_sum / m_count;
        double variance = m_sq_sum / m_count - mean * mean;
        m_std_dev = std::sqrt(variance);

        std::cout << std::endl;
        std::cout << "sum:   " << m_sum << std::endl;
        std::cout << "count: " << m_count << std::endl;
        std::cout << "min:   " << m_min << std::endl;
        std::cout << "avg:   " << std::setprecision(4) << mean << std::endl;
        std::cout << "max:   " << m_max << std::endl;
        std::cout << "dev:   " << std::setprecision(4) << m_std_dev << std::endl;
    }

    void run()
    {
        int res;

        while (signal::running()) {
            res = io::wait(m_interval);

            if (res > 0)
                continue;

            if (res < 0)
                break;

            if (m_limit && m_count >= m_limit)
                break;

            send_req();
        }
    }
};

class server : public io, public signal
{
    typedef std::unique_ptr<peer_stack> peer_ptr;
    peer_ptr m_peer;
    server_stack m_srv;

    void read_peer(int)
    {
        buffer_pkt::pointer buf = m_peer->buffer();

        if (!m_peer->read_pkt(buf))
            return;

        m_peer->write_pkt(buf);
    }

    void accept_peer(int)
    {
        using std::placeholders::_1;

        auto rp = std::bind(&server::read_peer, this, _1);
        int fd = m_srv.sock_accept();

        m_peer = peer_ptr(new peer_stack(peer_stack::file_descriptor=fd));
        io::add_cb(fd, rp, NULL);
    }

  public:
    server(const struct args &args)
        : m_srv(
                server_stack::local_address=args.address,
                server_stack::port=args.port
               )
    {
        using std::placeholders::_1;

        auto ap = std::bind(&server::accept_peer, this, _1);

        io::add_cb(m_srv.fd(), ap, NULL);
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
    struct args args;
    signed char c;

    while ((c = getopt_long_only(argc, argv, "", options, NULL)) != -1) {
        switch (c) {
            case 1:
                strncpy(args.address, optarg, 20);
                break;
            case 2:
                strncpy(args.port, optarg, 20);
                break;
            case 3:
                args.interval = atoi(optarg);
                break;
            case 4:
                args.count = atoi(optarg);
                break;
            case 5:
                args.server = true;
                break;
            case 6:
                args.quiet = true;
                break;
            case '?':
                return EXIT_FAILURE;
        }
    }

    if (args.server) {
        server s(args);
        s.run();
    } else {
        client cl(args);
        cl.run();
        cl.print();
    }

    return EXIT_SUCCESS;
}
