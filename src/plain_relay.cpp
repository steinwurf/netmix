#include <functional>

#include "arguments.hpp"
#include "signal.hpp"
#include "budgets.hpp"
#include "eth_topology.hpp"
#include "eth_hdr.hpp"
#include "eth_sock.hpp"
#include "buffer_pkt.hpp"
#include "buffer_pool.hpp"
#include "io.hpp"
#include "final_layer.hpp"

typedef eth_topology<
        eth_hdr<
        eth_sock<
        buffer_pool<buffer_pkt,
        final_layer
        >>>> relay_stack;


class relay : public signal, public io
{
    relay_stack m_in;
    relay_stack m_out;

    void read_src(int)
    {

    }

  public:
    relay(const struct args &args)
        : m_in(),
          m_out()
    {
        using std::placeholders::_1;

        auto rs = std::bind(&relay::read_src, this, _1);

        io::add_cb(m_in.fd(), rs, NULL);
    }

    void run()
    {
        while (signal::running())
            if (io::wait() < 0)
                break;
    }
};

int main(int argc, char **argv)
{
    if (parse_args(argc, argv) < 0)
        return EXIT_FAILURE;

    if (args.help)
        return args_usage(argv[0]);

    relay r(args);
    r.run();

    return EXIT_SUCCESS;
}
