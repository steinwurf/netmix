#include <iostream>
#include <getopt.h>
#include <cstring>
#include <ctime>
#include <vector>
#include <memory>
#include <functional>
#include <endian.h>

#include "io.hpp"
#include "signal.hpp"
#include "counters.hpp"
#include "rlnc_codes.hpp"
#include "udp_sock.hpp"
#include "tun.hpp"
#include "buffer_pool.hpp"
#include "buffer_pkt.hpp"
#include "final_layer.hpp"

struct args
{
    /* name of virtual interface */
    char interface[IFNAMSIZ] = "tap0";

    /* address to listen on (server)
     * address to connect to (client) */
    char address[20] = "localhost";

    /* port number to use for UDP connections */
    char port[20] = "8899";

    /* type of virtual interface (tun/tap) */
    char type[sizeof("tun")] = "tap";

    /* source address to use for the connection (client only) */
    char *src = NULL;

    /* number of symbols in one block */
    size_t symbols = 10;

    /* size of each symbol in block */
    size_t symbol_size = 1414;

    /* start as server if argument is given, client otherwise */
    bool server = false;

    /* do not print debug messages per default */
    bool verbose = false;

    /* size of udp socket buffer for sending */
    size_t send_buf = 16384;

    /* number of packets received between sending status reports */
    size_t status_interval = 8;

    /* ratio of extra packets distributed on a connection */
    double overshoot = 1.5;
};

struct option options[] =
{
    {"interface",       required_argument, NULL, 1},
    {"address",         required_argument, NULL, 2},
    {"port",            required_argument, NULL, 3},
    {"src",             required_argument, NULL, 4},
    {"symbols",         required_argument, NULL, 5},
    {"symbol_size",     required_argument, NULL, 6},
    {"server",          no_argument,       NULL, 7},
    {"status_interval", required_argument, NULL, 8},
    {"overshoot",       required_argument, NULL, 9},
    {"type",            required_argument, NULL, 10},
    {"verbose",         no_argument,       NULL, 11},
    {0}
};

typedef tuntap<
        buffer_pool<buffer_pkt,
        final_layer
        >> tun_stack;

typedef udp_sock_client<
        buffer_pool<buffer_pkt,
        final_layer
        >> client_stack;

typedef udp_sock_server<
        buffer_pool<buffer_pkt,
        final_layer
        >> server_stack;

// typedef fifi::binary field;
// typedef kodo::on_the_fly_encoder<field> kodo_encoder;
// typedef kodo::on_the_fly_decoder<field> kodo_decoder;
//
// class len_hdr
// {
//     struct hdr
//     {
//         uint32_t length;
//     };
//
//     typedef buffer_pkt::pointer buf_ptr;
//     constexpr static size_t m_hdr_size = sizeof(struct hdr);
//
//     static struct hdr *hdr(uint8_t *ptr)
//     {
//         return reinterpret_cast<struct hdr *>(ptr);
//     }
//
// public:
//
//     static size_t get(buf_ptr &buf)
//     {
//         auto h = hdr(buf->head());
//         return be32toh(h->length);
//     }
//
//     static void add(buf_ptr &buf)
//     {
//         size_t len = buf->len();
//         auto h = hdr(buf->head_push(m_hdr_size));
//         h->length = htobe32(len);
//     }
//
//     static size_t size()
//     {
//         return m_hdr_size;
//     }
// };
//
// class encoder
// {
//     typedef buffer_pkt::pointer buf_ptr;
//
//     kodo_encoder::factory m_factory;
//     kodo_encoder::pointer m_enc;
//
//     size_t m_encoded_symbols = 0;
//
//   public:
//     encoder(size_t symbols, size_t symbol_size)
//         : m_factory(symbols, symbol_size),
//           m_enc(m_factory.build())
//     {}
//
//     bool is_full()
//     {
//         return m_enc->symbols_initialized() == m_enc->symbols();
//     }
//
//     bool is_sent()
//     {
//         return m_encoded_symbols == m_enc->symbols_initialized();
//     }
//
//     bool is_complete()
//     {
//         return m_encoded_symbols == m_enc->symbols();
//     }
//
//     void reset()
//     {
//         m_enc->initialize(m_factory);
//         m_encoded_symbols = 0;
//     }
//
//     void add_pkt(buf_ptr &buf)
//     {
//         len_hdr::add(buf);
//         sak::const_storage symbol(buf->head(), buf->len());
//         m_enc->set_symbol(m_enc->symbols_initialized(), symbol);
//     }
//
//     void get_pkt(buf_ptr &buf)
//     {
//         size_t len;
//
//         len = m_enc->encode(buf->data());
//         buf->data_put(len);
//         m_encoded_symbols++;
//     }
// };
//
// class decoder
// {
//     typedef buffer_pkt::pointer buf_ptr;
//
//     kodo_decoder::factory m_factory;
//     kodo_decoder::pointer m_dec;
//
//     size_t m_decoded_symbols = 0;
//
//   public:
//     decoder(size_t symbols, size_t symbol_size)
//         : m_factory(symbols, symbol_size),
//           m_dec(m_factory.build())
//     {}
//
//     bool is_complete()
//     {
//         return m_dec->is_complete();
//     }
//
//     bool is_partial_complete()
//     {
//         return m_decoded_symbols < m_dec->symbols() &&
//                m_dec->is_symbol_decoded(m_decoded_symbols);
//     }
//
//     void reset()
//     {
//         m_dec->initialize(m_factory);
//         m_decoded_symbols = 0;
//     }
//
//     void add_pkt(buf_ptr &buf)
//     {
//         m_dec->decode(buf->head());
//     }
//
//     void get_pkt(buf_ptr &buf)
//     {
//         size_t len;
//
//         buf->head_push(len_hdr::size());
//         memcpy(buf->head(), m_dec->symbol(m_decoded_symbols++),
//                m_dec->symbol_size());
//         len = len_hdr::get(buf);
//         buf->head_pull(len_hdr::size());
//         buf->data_put(len);
//     }
// };
//
// template<class stack>
// class handler
// {
//   public:
//     typedef buffer_pkt::pointer buf_ptr;
//     typedef std::unique_ptr<stack> peer_ptr;
//
//     io m_io;
//
//   private:
//     class signal m_sig;
//     tun_stack m_tun;
//     peer_ptr m_peer;
//
//     encoder m_enc;
//     decoder m_dec;
//
//     void send_tun()
//     {
//         buf_ptr buf;
//
//         while (m_dec.is_partial_complete())
//         {
//             buf = m_tun.buffer();
//
//             m_dec.get_pkt(buf);
//             m_tun.write_pkt(buf);
//         }
//     }
//
//     void recv_tun(int fd)
//     {
//         buf_ptr buf = m_tun.buffer(m_tun.data_size_max());
//
//         while (m_tun.read_pkt(buf))
//         {
//             m_enc.add_pkt(buf);
//             buf->reset(m_tun.data_size_max());
//             m_io.enable_write(m_peer->fd());
//
//             if (m_enc.is_full() == false)
//                 continue;
//
//             m_io.disable_read(fd);
//             break;
//         }
//     }
//
//     void recv_peer(int /*fd*/)
//     {
//         buf_ptr buf = m_peer->buffer();
//
//         while (m_peer->read_pkt(buf))
//         {
//             m_dec.add_pkt(buf);
//
//             if (m_dec.is_complete())
//                 break;
//
//             buf->reset();
//         }
//
//         send_tun();
//
//         if (!m_dec.is_complete())
//             return;
//
//         m_dec.reset();
//     }
//
//     void send_peer(int fd)
//     {
//         buf_ptr buf;
//
//         do
//         {
//             if (m_enc.is_sent())
//             {
//                 m_io.disable_write(fd);
//                 break;
//             }
//
//             buf = m_peer->buffer();
//             m_enc.get_pkt(buf);
//         }
//         while (m_peer->write_pkt(buf));
//
//         if (!m_enc.is_complete())
//             return;
//
//         m_enc.reset();
//         m_io.enable_read(m_tun.fd());
//    }
//
//   public:
//     handler(const struct args &args)
//         : m_tun(tun_stack::interface=args.interface,
//                 tun_stack::type=args.type),
//            m_enc(args.symbols, args.symbol_size),
//            m_dec(args.symbols, args.symbol_size)
//     {
//         using std::placeholders::_1;
//
//         auto rt = std::bind(&handler::recv_tun, this, _1);
//
//         m_io.add_cb(m_tun.fd(), rt, NULL);
//     }
//
//     void add_peer(peer_ptr p)
//     {
//         using std::placeholders::_1;
//
//         auto rp = std::bind(&handler::recv_peer, this, _1);
//         auto sp = std::bind(&handler::send_peer, this, _1);
//
//         m_io.add_cb(p->fd(), rp, sp);
//         m_io.disable_write(p->fd());
//
//         m_peer = std::move(p);
//     }
//
//     void run()
//     {
//         int res;
//
//         while (m_sig.running())
//         {
//             res = m_io.wait();
//
//             if (res < 0)
//                 break;
//
//             if (res > 0)
//                 continue;
//         }
//     }
// };


template<class stack>
class coder
{
    typedef fifi::binary field;
    typedef kodo::on_the_fly_encoder<field> encoder;
    typedef kodo::on_the_fly_decoder<field> decoder;
    typedef buffer_pkt::pointer buf_ptr;

public:
    typedef std::unique_ptr<stack> peer_ptr;
    io m_io;

private:
    enum rlnc_type : uint8_t
    {
        rlnc_enc    = 0,
        rlnc_status = 1,
    };

    struct rlnc_hdr
    {
        uint8_t type;
        uint32_t block;
    } __attribute__((packed));

//     struct status_hdr
//     {
//         struct rlnc_hdr rlnc;
//         uint16_t interval;
//         uint16_t status[0];
//     } __attribute__((packed));

    struct len_hdr
    {
        uint32_t length;
    };

    constexpr static size_t m_rlnc_hdr_size = sizeof(struct rlnc_hdr);
    constexpr static size_t m_len_hdr_size = sizeof(struct len_hdr);
    //size_t m_status_hdr_size = sizeof(struct status_hdr);

    //std::vector<peer_ptr> m_peers;
    peer_ptr m_peer;
    encoder::factory m_enc_factory;
    decoder::factory m_dec_factory;
    encoder::pointer m_enc;
    decoder::pointer m_dec;
    tun_stack m_tun;
    buf_ptr m_tmp_buf;
    class signal m_sig;

    size_t m_peers_count = 0;
    size_t m_decoded = 0;
    size_t m_encoded_sent = 0;
    size_t m_encoded_received = 0;
    size_t m_linear = 0;
    uint32_t m_enc_block = 0;
    uint32_t m_dec_block = 0;
    size_t m_max;
    double m_overshoot;
    size_t m_send_buf;
    size_t m_status_interval;

    bool   m_verbose;

    static rlnc_hdr* rlnc_hdr(uint8_t* ptr)
    {
        return reinterpret_cast<struct rlnc_hdr*>(ptr);
    }

//     static struct status_hdr *status_hdr(uint8_t *ptr)
//     {
//         return reinterpret_cast<struct status_hdr *>(ptr);
//     }

    static len_hdr* len_hdr(uint8_t* ptr)
    {
        return reinterpret_cast<struct len_hdr*>(ptr);
    }

    static uint8_t rlnc_hdr_type(uint8_t* ptr)
    {
        return rlnc_hdr(ptr)->type;
    }

    static uint32_t rlnc_hdr_block(uint8_t* ptr)
    {
        return rlnc_hdr(ptr)->block;
    }

//     static size_t status_hdr_interval(buf_ptr &buf)
//     {
//         return be16toh(status_hdr(buf->head())->interval);
//     }
//
//     static uint16_t *status_hdr_status(buf_ptr &buf)
//     {
//         return status_hdr(buf->head())->status;
//     }

    static size_t len_hdr_length(buf_ptr &buf)
    {
        auto hdr = len_hdr(buf->head());
        return be32toh(hdr->length);
    }

    void enc_hdr_add(buf_ptr &buf)
    {
        auto hdr = rlnc_hdr(buf->head_push(m_rlnc_hdr_size));

        hdr->type = rlnc_type::rlnc_enc;
        hdr->block = m_enc_block;
    }

//     void status_hdr_add(buf_ptr &buf, uint16_t *status)
//     {
//         auto hdr = status_hdr(buf->head_push(m_status_hdr_size));
//
//         hdr->rlnc.type = rlnc_type::rlnc_status;
//         hdr->rlnc.block = m_dec_block;
//         hdr->interval = htobe16(m_status_interval);
//         memcpy(hdr->status, status, m_peers_count*2);
//     }

    void len_hdr_add(buf_ptr &buf)
    {
        size_t len = buf->len();
        auto hdr = len_hdr(buf->head_push(m_len_hdr_size));
        hdr->length = htobe32(len);
    }

    void add_raw_packet(buf_ptr &buf)
    {
        len_hdr_add(buf);
        sak::const_storage symbol(buf->head(), buf->len());
        m_enc->set_symbol(m_enc->symbols_initialized(), symbol);
    }

    void get_enc_data(buf_ptr &buf)
    {
        size_t len = m_enc->encode(buf->data());
        buf->data_put(len);
        enc_hdr_add(buf);
        assert(len > m_enc->symbol_size());
        assert(buf->len() == len + m_rlnc_hdr_size);
    }

    int validate_block(buf_ptr &buf, size_t block)
    {
        size_t diff = (rlnc_hdr_block(buf->head()) - block) & 0xff;

        if (diff == 0)
            return 0;
        else if (diff > 128)
            return -1;
        else if (diff <= 128)
            return 1;

        throw std::runtime_error("off block");
    }

//     void rlnc_status_process(buf_ptr &buf)
//     {
//         size_t i = 0;
//         size_t count;
//         uint16_t* counts = status_hdr_status(buf);
//         size_t interval = status_hdr_interval(buf);
//         size_t sum = 0;
//
//         m_max = 0;
//
//         for (auto &p : m_peers)
//         {
//             if (!p)
//                 continue;
//
//             count = be16toh(counts[i++]);
//             sum += count;
//             m_max += p->set_ratio(count, interval);
//         }
//
//         peers_enable_write();
//
//         assert(sum == interval);
//         assert(sum == m_status_interval);
//     }
//
//     void rlnc_status_send()
//     {
//         buf_ptr buf;
//         std::vector<uint16_t> counts;
//         size_t max = 0;
//         int best_fd = 0;
//
//         if (m_encoded_received < m_status_interval)
//             return;
//
//         for (auto &p : m_peers)
//         {
//             if (!p)
//                 continue;
//
//             if (p->received_packets > max)
//             {
//                 best_fd = p->fd();
//                 max = p->received_packets;
//             }
//
//             counts.push_back(htobe16(p->received_packets));
//             p->received_packets = 0;
//         }
//
//         buf = m_peers[best_fd]->buffer();
//         status_hdr_add(buf, &counts[0]);
//         m_peers[best_fd]->write_pkt(buf);
//         m_encoded_received = 0;
//     }

    void increment_dec_block()
    {
        m_dec->initialize(m_dec_factory);
        m_dec_block++;
        m_decoded = 0;
        m_linear = 0;
    }

    bool rlnc_enc_process(buf_ptr &buf_in)
    {
        size_t max_len = m_dec->symbol_size();
        buf_ptr buf_out = m_tun.buffer(max_len);
        size_t len;
        size_t rank;

        // Ignore old blocks
        if (rlnc_hdr_block(buf_in->head()) < m_dec_block)
        {
            return true;
        }

        // We failed to decode a block
        if (rlnc_hdr_block(buf_in->head()) > m_dec_block)
        {
            if (m_verbose)
                std::cout << "FAILED DECODING: " << m_dec_block
                          << " (rank: " << m_dec->rank() << ")" << std::endl;
            increment_dec_block();
        }

        if (m_dec->is_complete())
            return true;

        if (buf_in->data_len() < m_dec->symbol_size())
        {
            std::cerr << "invalid length, data: " << buf_in->data_len()
                      << ", head: " << buf_in->head_len() << std::endl;
        }
        assert(buf_in->data_len() > m_dec->symbol_size());

        rank = m_dec->rank();
        buf_in->head_pull(m_rlnc_hdr_size);
        m_dec->decode(buf_in->head());

        if (m_dec->rank() == rank)
        {
            m_linear++;
        }
        else
        {
            //m_peer->received_packets++;
            m_encoded_received++;
        }

        while (m_decoded < m_dec->symbols() &&
               m_dec->is_symbol_decoded(m_decoded))
        {
            buf_out->head_push(m_len_hdr_size);
            memcpy(buf_out->head(), m_dec->symbol(m_decoded++), max_len);
            len = len_hdr_length(buf_out);
            buf_out->head_pull(m_len_hdr_size);
            buf_out->data_put(len);
            m_tun.write_pkt(buf_out);
            buf_out->reset();
        }

        //rlnc_status_send();

        if (!m_dec->is_complete())
            return true;

        if (m_verbose)
            std::cout << "decoded block " << m_dec_block
                      << " (linear: " << m_linear << ")" << std::endl;
        increment_dec_block();

        return false;
    }

    void recv_tun(int fd)
    {
        buf_ptr buf = m_tun.buffer(m_tun.data_size_max());

        while (m_tun.read_pkt(buf))
        {
            // Store the packet in the encoder (with a length header)
            add_raw_packet(buf);
            buf->reset(m_tun.data_size_max());
            m_io.enable_write(m_peer->fd());

            // Fill up the encoder as much as possible
            if (m_enc->rank() < m_enc->symbols())
                continue;

            // Disable further reads if the encoder is full
            m_io.disable_read(fd);
            break;
        }
    }

    void recv_peer(int /*fd*/)
    {
        buf_ptr buf = m_peer->buffer();
        buf->head_reserve(m_rlnc_hdr_size);

        while (m_peer->read_pkt(buf))
        {
            switch (rlnc_hdr_type(buf->head()))
            {
            case rlnc_enc:
                rlnc_enc_process(buf);
                break;

            case rlnc_status:
                //rlnc_status_process(buf);
                break;

            default:
                std::cerr << "unknown packet type: "
                          << rlnc_hdr_type(buf->head()) << std::endl;
            }

            buf->reset();
            buf->head_reserve(m_rlnc_hdr_size);
        }
    }

    bool send_enc_packet(int fd)
    {
        buf_ptr buf;
        size_t symbols = m_enc->symbols();
        size_t symbols_added = m_enc->symbols_initialized();

        // Send repair packets in the middle of a generation
        double current_limit = symbols_added * m_overshoot;
        if (symbols_added < symbols && m_encoded_sent > current_limit)
        {
            m_io.disable_write(fd);
            return false;
        }

        if (m_encoded_sent >= m_max)
            return false;

        buf = m_peer->buffer();
        get_enc_data(buf);

        if (!m_peer->write_pkt(buf))
            return false;

        m_encoded_sent++;
        //m_peer->sent_packets++;

        return true;
    }

    void send_peer(int fd)
    {
        // Can send multiple packets (if overshooting is enabled)
        while (send_enc_packet(fd))
        { }

        if (m_encoded_sent >= m_max)
        {
            if (m_verbose)
                std::cout << "sent block " << m_enc_block << ", "
                          << m_encoded_sent << std::endl;

            m_enc->initialize(m_enc_factory);
            m_enc_block++;
            m_io.enable_read(m_tun.fd());

            m_io.disable_write(m_peer->fd());
            //m_peer->sent_packets = 0;
            m_encoded_sent = 0;
        }
    }

public:
    coder(const struct args &args) :
        m_enc_factory(args.symbols, args.symbol_size),
        m_dec_factory(args.symbols, args.symbol_size),
        m_enc(m_enc_factory.build()),
        m_dec(m_dec_factory.build()),
        m_tun(tun_stack::interface=args.interface,
              tun_stack::type=args.type),
        m_max(args.symbols * (args.overshoot * 1.4)),
        m_overshoot(args.overshoot),
        m_send_buf(args.send_buf),
        m_status_interval(args.status_interval),
        m_verbose(args.verbose)
    {
        using std::placeholders::_1;

        auto rt = std::bind(&coder::recv_tun, this, _1);

        m_io.add_cb(m_tun.fd(), rt, NULL);
        std::cout << "payload size: " << m_enc->payload_size() << std::endl;
        std::cout << "max: " << m_max << std::endl;
    }

    void add_peer(peer_ptr p)
    {
        using std::placeholders::_1;

        auto rp = std::bind(&coder::recv_peer, this, _1);
        auto sp = std::bind(&coder::send_peer, this, _1);

        p->sock_send_buf(m_send_buf);
        m_io.add_cb(p->fd(), rp, sp);
        m_io.disable_write(p->fd());
        m_peer = std::move(p);
        m_peers_count++;
        //m_status_hdr_size += 2;
    }

    void run()
    {
        int res;

        while (m_sig.running())
        {
            res = m_io.wait();

            if (res < 0)
                break;

            if (res > 0)
                continue;
        }
    }
};

class client : public coder<client_stack>
{
  public:
    client(const struct args &args) :
        coder(args)
    {
        client_stack *c = new client_stack(
            client_stack::local_address=args.src,
            client_stack::remote_address=args.address,
            client_stack::port=args.port
        );
        add_peer(peer_ptr(c));
    }

};

class server : public coder<server_stack>
{
  public:
    server(const struct args &args) :
        coder(args)
    {
        server_stack *s = new server_stack(
            server_stack::local_address=args.address,
            server_stack::port=args.port
        );

        add_peer(peer_ptr(s));
    }
};

int main(int argc, char **argv)
{
    struct args args;
    signed char a;

    while ((a = getopt_long_only(argc, argv, "", options, NULL)) != -1)
    {
        switch (a)
        {
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
                args.overshoot = strtod(optarg, NULL);
                break;
            case 10:
                strncpy(args.type, optarg, sizeof(args.type));
                break;
            case 11:
                args.verbose = true;
                break;
            case '?':
                return 1;
                break;
        }
    }

    srand(static_cast<uint32_t>(time(0)));
    if (args.server)
    {
        server s(args);
        s.run();
    }
    else
    {
        client c(args);
        c.run();
    }

    return 0;
}
