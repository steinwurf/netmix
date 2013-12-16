#include <iostream>
#include <getopt.h>
#include <cstring>
#include <vector>
#include <memory>
#include <functional>
#include <endian.h>

#include "io.hpp"
#include "signal.hpp"
#include "rlnc_codes.hpp"
#include "tcp_hdr.hpp"
#include "tcp_sock.hpp"
#include "tun.hpp"
#include "buffer_pool.hpp"
#include "buffer_pkt.hpp"
#include "final_layer.hpp"

struct args {
    /* name of virtual interface */
    char interface[IFNAMSIZ] = "tun0";

    /* address to listen on (server)
     * address to connect to (client) */
    char address[20] = "localhost";

    /* port number to use for tcp connections */
    char port[20] = "8899";

    /* type of virtual interface (tun/tap) */
    char type[sizeof("tun")] = "tun";

    /* source address to use for the connection (client only) */
    char *src = NULL;

    /* number of symbols in one block */
    size_t symbols = 100;

    /* size of each symbol in block */
    size_t symbol_size = 1400;

    /* start as server if argument is given, client otherwise */
    bool server = false;

    /* size of tcp socket buffer for sending */
    size_t send_buf = 16384;

    /* number of packets received between sending status reports */
    size_t status_interval = 50;
};

struct option options[] = {
    {"interface",       required_argument, NULL, 1},
    {"address",         required_argument, NULL, 2},
    {"port",            required_argument, NULL, 3},
    {"src",             required_argument, NULL, 4},
    {"symbols",         required_argument, NULL, 5},
    {"symbol_size",     required_argument, NULL, 6},
    {"server",          no_argument,       NULL, 7},
    {"status_interval", required_argument, NULL, 8},
    {"type",            required_argument, NULL, 9},
    {0}
};

typedef tuntap<
        buffer_pool<buffer_pkt,
        final_layer
        >> tun_stack;

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
        >> srv_stack;

typedef fifi::binary8 field;
typedef kodo::on_the_fly_encoder<field> kodo_encoder;
typedef kodo::on_the_fly_decoder<field> kodo_decoder;

class len_hdr
{
    struct hdr {
        uint32_t length;
    };

    typedef buffer_pkt::pointer buf_ptr;
    constexpr static size_t m_hdr_size = sizeof(struct hdr);

    static struct hdr *hdr(uint8_t *ptr)
    {
        return reinterpret_cast<struct hdr *>(ptr);
    }

  public:
    static size_t get(buf_ptr &buf)
    {
        auto h = hdr(buf->head());
        return be32toh(h->length);
    }

    static void add(buf_ptr &buf)
    {
        size_t len = buf->len();
        auto h = hdr(buf->head_push(m_hdr_size));
        h->length = htobe32(len);
    }

    static size_t size()
    {
        return m_hdr_size;
    }
};

class encoder
{
    typedef buffer_pkt::pointer buf_ptr;

    kodo_encoder::factory m_factory;
    kodo_encoder::pointer m_enc;

    size_t m_encoded_symbols = 0;

  public:
    encoder(size_t symbols, size_t symbol_size)
        : m_factory(symbols, symbol_size),
          m_enc(m_factory.build())
    {}

    bool is_full()
    {
        return m_enc->symbols_initialized() == m_enc->symbols();
    }

    bool is_sent()
    {
        return m_encoded_symbols == m_enc->symbols_initialized();
    }

    bool is_complete()
    {
        return m_encoded_symbols == m_enc->symbols();
    }

    void reset()
    {
        m_enc->initialize(m_factory);
        m_encoded_symbols = 0;
    }

    void add_pkt(buf_ptr &buf)
    {
        len_hdr::add(buf);
        sak::const_storage symbol(buf->head(), buf->len());
        m_enc->set_symbol(m_enc->symbols_initialized(), symbol);
    }

    void get_pkt(buf_ptr &buf)
    {
        size_t len;

        len = m_enc->encode(buf->data());
        buf->data_put(len);
        m_encoded_symbols++;
    }
};

class decoder
{
    typedef buffer_pkt::pointer buf_ptr;

    kodo_decoder::factory m_factory;
    kodo_decoder::pointer m_dec;

    size_t m_decoded_symbols = 0;

  public:
    decoder(size_t symbols, size_t symbol_size)
        : m_factory(symbols, symbol_size),
          m_dec(m_factory.build())
    {}

    bool is_complete()
    {
        return m_dec->is_complete();
    }

    bool is_partial_complete()
    {
        return m_decoded_symbols < m_dec->symbols() &&
               m_dec->is_symbol_decoded(m_decoded_symbols);
    }

    void reset()
    {
        m_dec->initialize(m_factory);
        m_decoded_symbols = 0;
    }

    void add_pkt(buf_ptr &buf)
    {
        m_dec->decode(buf->head());
    }

    void get_pkt(buf_ptr &buf)
    {
        size_t len;

        buf->head_push(len_hdr::size());
        memcpy(buf->head(), m_dec->symbol(m_decoded_symbols++),
               m_dec->symbol_size());
        len = len_hdr::get(buf);
        buf->head_pull(len_hdr::size());
        buf->data_put(len);
    }
};

template<class stack>
class handler
{
  public:
    typedef buffer_pkt::pointer buf_ptr;
    typedef std::unique_ptr<stack> peer_ptr;

    io m_io;

  private:
    class signal m_sig;
    tun_stack m_tun;
    peer_ptr m_peer;
    encoder m_enc;
    decoder m_dec;

    void send_tun()
    {
        buf_ptr buf;

        while (m_dec.is_partial_complete()) {
            buf = m_tun.buffer();

            m_dec.get_pkt(buf);
            m_tun.write_pkt(buf);
        }
    }

    void recv_tun(int fd)
    {
        buf_ptr buf = m_tun.buffer(m_tun.data_size_max());

        while (m_tun.read_pkt(buf)) {
            m_enc.add_pkt(buf);
            buf->reset(m_tun.data_size_max());
            m_io.enable_write(m_peer->fd());

            if (!m_enc.is_full())
                continue;

            m_io.disable_read(fd);
            break;
        }
    }

    void recv_peer(int fd)
    {
        buf_ptr buf = m_peer->buffer();

        while (m_peer->read_pkt(buf)) {
            m_dec.add_pkt(buf);

            if (m_dec.is_complete())
                break;

            buf->reset();
        }

        send_tun();

        if (!m_dec.is_complete())
            return;

        m_dec.reset();
    }

    void send_peer(int fd)
    {
        buf_ptr buf;

        do {
            if (m_enc.is_sent()) {
                m_io.disable_write(fd);
                break;
            }

            buf = m_peer->buffer();
            m_enc.get_pkt(buf);
        } while (m_peer->write_pkt(buf));

        if (!m_enc.is_complete())
            return;

        m_enc.reset();
        m_io.enable_read(m_tun.fd());
    }

  public:
    handler(const struct args &args)
        : m_tun(tun_stack::interface=args.interface,
                tun_stack::type=args.type),
          m_enc(args.symbols, args.symbol_size),
          m_dec(args.symbols, args.symbol_size)
    {
        using std::placeholders::_1;

        auto rt = std::bind(&handler::recv_tun, this, _1);

        m_io.add_cb(m_tun.fd(), rt, NULL);
    }

    void add_peer(peer_ptr p)
    {
        using std::placeholders::_1;

        auto rp = std::bind(&handler::recv_peer, this, _1);
        auto sp = std::bind(&handler::send_peer, this, _1);

        m_io.add_cb(p->fd(), rp, sp);
        m_io.disable_write(p->fd());

        m_peer = std::move(p);
    }

    void run()
    {
        int res;

        while (m_sig.running()) {
            res = m_io.wait();

            if (res < 0)
                break;

            if (res > 0)
                continue;
        }
    }
};

class client : public handler<client_stack>
{
  public:
    client(const struct args &args)
        : handler(args)
    {
        client_stack *c;

        c = new client_stack(
                client_stack::local_address=args.src,
                client_stack::remote_address=args.address,
                client_stack::port=args.port
                );
        add_peer(peer_ptr(c));
    }

};

class server : public handler<peer_stack>
{
    srv_stack m_srv;

    void accept_peer(int)
    {
        int fd = m_srv.sock_accept();
        peer_stack *p;

        p = new peer_stack(peer_stack::file_descriptor=fd);
        add_peer(peer_ptr(p));
    }

  public:
    server(const struct args &args)
        : handler(args),
          m_srv(srv_stack::local_address=args.address,
                srv_stack::port=args.port)
    {
        using std::placeholders::_1;

        auto ap = std::bind(&server::accept_peer, this, _1);

        m_io.add_cb(m_srv.fd(), ap, NULL);
    }
};

int main(int argc, char **argv)
{
    struct args args;
    signed char a;

    while ((a = getopt_long_only(argc, argv, "", options, NULL)) != -1) {
        switch (a) {
            case 1:
                strncpy(args.interface, optarg, IFNAMSIZ);
                break;
            case 2:
                strncpy(args.address, optarg, 20);
                break;
            case 3:
                strncpy(args.port, optarg, 20);
                break;
            case 4:
                args.src = optarg;
                break;
            case 5:
                args.symbols = atoi(optarg);
                break;
            case 6:
                args.symbol_size = atoi(optarg);
                break;
            case 7:
                args.server = true;
                break;
            case 8:
                args.status_interval = atoi(optarg);
                break;
            case 9:
                strncpy(args.type, optarg, sizeof(args.type));
                break;
            case '?':
                return 1;
                break;
        }
    }

    if (args.server) {
        server s(args);
        s.run();
    } else {
        client c(args);
        c.run();
    }

    return 0;
}
