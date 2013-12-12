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
#include "counters.hpp"
#include "tcp_hdr.hpp"
#include "tcp_sock.hpp"
#include "tun.hpp"
#include "buffer_pool.hpp"
#include "buffer_pkt.hpp"
#include "final_layer.hpp"

struct args {
    char interface[IFNAMSIZ] = "tun0";
    char address[20]         = "localhost";
    char port[20]            = "8899";
    char *a_src              = NULL;
    char *b_src              = NULL;
    bool server              = false;
    size_t symbols           = 100;
    size_t symbol_size       = 1400;
    size_t send_buf          = 16384;
    size_t timeout           = 100;
    size_t status_interval   = 50;
    double overshoot         = 1.5;
};

struct option options[] = {
    {"interface",       required_argument, NULL, 1},
    {"address",         required_argument, NULL, 2},
    {"port",            required_argument, NULL, 3},
    {"a_src",           required_argument, NULL, 4},
    {"b_src",           required_argument, NULL, 5},
    {"symbols",         required_argument, NULL, 6},
    {"symbol_size",     required_argument, NULL, 7},
    {"server",          no_argument,       NULL, 8},
    {"send_buf",        required_argument, NULL, 9},
    {"overshoot",       required_argument, NULL, 10},
    {"timeout",         required_argument, NULL, 11},
    {"status_interval", required_argument, NULL, 12},
    {0}
};

typedef tun<
        buffer_pool<buffer_pkt,
        final_layer
        >> tun_stack;

typedef counters<
        tcp_hdr<
        tcp_sock_client<
        buffer_pool<buffer_pkt,
        final_layer
        >>>> client_stack;

typedef counters<
        tcp_hdr<
        tcp_sock_peer<
        buffer_pool<buffer_pkt,
        final_layer
        >>>> peer_stack;

typedef tcp_sock_server<
        buffer_pool<buffer_pkt,
        final_layer
        >> srv_stack;

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
    enum rlnc_type : uint8_t {
        rlnc_enc    = 0,
        rlnc_status = 1,
    };

    struct rlnc_hdr {
        uint8_t type;
        uint8_t block;
    } __attribute__((packed));

    struct status_hdr {
        struct rlnc_hdr rlnc;
        uint16_t interval;
        uint16_t status[0];
    } __attribute__((packed));

    struct len_hdr {
        uint32_t length;
    };

    constexpr static size_t m_rlnc_hdr_size = sizeof(struct rlnc_hdr);
    constexpr static size_t m_len_hdr_size = sizeof(struct len_hdr);
    size_t m_status_hdr_size = sizeof(struct status_hdr);

    std::vector<peer_ptr> m_peers;
    encoder::factory m_enc_factory;
    decoder::factory m_dec_factory;
    encoder::pointer m_enc;
    decoder::pointer m_dec;
    tun_stack m_tun;
    buf_ptr m_tmp_enc;
    class signal m_sig;

    size_t m_peers_count = 0;
    size_t m_decoded = 0;
    size_t m_encoded_sent = 0;
    size_t m_encoded_received = 0;
    size_t m_linear = 0;
    size_t m_enc_block = 0;
    size_t m_dec_block = 0;
    size_t m_max;
    size_t m_send_buf;
    size_t m_status_interval;

    static struct rlnc_hdr *rlnc_hdr(uint8_t *ptr)
    {
        return reinterpret_cast<struct rlnc_hdr *>(ptr);
    }

    static struct status_hdr *status_hdr(uint8_t *ptr)
    {
        return reinterpret_cast<struct status_hdr *>(ptr);
    }

    static struct len_hdr *len_hdr(uint8_t *ptr)
    {
        return reinterpret_cast<struct len_hdr *>(ptr);
    }

    static size_t rlnc_hdr_type(uint8_t *ptr)
    {
        return rlnc_hdr(ptr)->type;
    }

    static size_t rlnc_hdr_block(uint8_t *ptr)
    {
        return rlnc_hdr(ptr)->block;
    }

    static size_t status_hdr_interval(buf_ptr &buf)
    {
        return be16toh(status_hdr(buf->head())->interval);
    }

    static uint16_t *status_hdr_status(buf_ptr &buf)
    {
        return status_hdr(buf->head())->status;
    }

    static size_t len_hdr_length(buf_ptr &buf)
    {
        auto hdr = len_hdr(buf->head());
        return be32toh(hdr->length);
    }

    void enc_hdr_add(buf_ptr &buf)
    {
        auto hdr = rlnc_hdr(buf->head_push(m_rlnc_hdr_size));

        hdr->type = rlnc_enc;
        hdr->block = m_enc_block;
    }

    void status_hdr_add(buf_ptr &buf, uint16_t *status)
    {
        auto hdr = status_hdr(buf->head_push(m_status_hdr_size));

        hdr->rlnc.type = rlnc_status;
        hdr->rlnc.block = m_dec_block;
        hdr->interval = htobe16(m_status_interval);
        memcpy(hdr->status, status, m_peers_count*2);
    }

    void len_hdr_add(buf_ptr &buf)
    {
        size_t len = buf->len();
        auto hdr = len_hdr(buf->head_push(m_len_hdr_size));
        hdr->length = htobe32(len);
    }

    void put_plain_data(buf_ptr &buf)
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

    void peers_enable_read()
    {
        for (auto &p : m_peers)
            if (p)
                m_io.enable_read(p->fd());
    }

    void peers_enable_write()
    {
        int fd = -1;
        size_t ratio = m_max;

        for (auto &p : m_peers) {
            if (!p)
                continue;

            if (p->ratio_spend())
                continue;

            if (p->ratio_packets > ratio)
                continue;

            ratio = p->ratio_packets;
            fd = p->fd();
        }

        assert(fd >= 0);
        m_io.enable_write(fd);
    }

    void peers_enable_write_next(int current_fd, bool consider_ratio)
    {
        int fd = -1;
        size_t ratio = m_max;

        for (auto &p : m_peers) {
            if (!p)
                continue;

            if (p->fd() == current_fd)
                continue;

            if (p->ratio_packets > ratio)
                continue;

            if (consider_ratio && p->ratio_spend())
                continue;

            ratio = p->ratio_packets;
            fd = p->fd();
        }

        assert(fd >= 0);

        if (!consider_ratio)
            m_peers[fd]->set_max_ratio();

        m_io.enable_write(fd);
    }

    void peers_disable_write()
    {
        for (auto &p : m_peers)
            if (p)
                m_io.disable_write(p->fd());
    }

    void peers_initialize()
    {
        for (auto &p : m_peers) {
            if (!p)
                continue;

            m_io.disable_write(p->fd());
            p->sent_packets = 0;
        }
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

    void rlnc_status_process(buf_ptr &buf)
    {
        size_t i = 0;
        size_t count;
        uint16_t *counts = status_hdr_status(buf);
        size_t interval = status_hdr_interval(buf);
        size_t sum = 0;

        m_max = 0;

        for (auto &p : m_peers) {
            if (!p)
                continue;

            count = be16toh(counts[i++]);
            sum += count;
            m_max += p->set_ratio(count, interval);
        }

        peers_enable_write();

        assert(sum == interval);
        assert(sum == m_status_interval);
    }

    void rlnc_status_send()
    {
        buf_ptr buf;
        std::vector<uint16_t> counts;
        size_t max = 0;
        int best_fd = 0;

        if (m_encoded_received < m_status_interval)
            return;

        for (auto &p : m_peers) {
            if (!p)
                continue;

            if (p->received_packets > max) {
                best_fd = p->fd();
                max = p->received_packets;
            }

            counts.push_back(htobe16(p->received_packets));
            p->received_packets = 0;
        }

        buf = m_peers[best_fd]->buffer();
        status_hdr_add(buf, &counts[0]);
        m_peers[best_fd]->write_pkt(buf);
        m_encoded_received = 0;
    }

    bool rlnc_enc_process(buf_ptr &buf_in, int fd)
    {
        size_t max_len = m_dec->symbol_size();
        buf_ptr buf_out = m_tun.buffer(max_len);
        size_t len;
        size_t rank;

        if (validate_block(buf_in, m_dec_block) < 0)
            return true;

        if (validate_block(buf_in, m_dec_block) > 0) {
            m_tmp_enc.swap(buf_in);
            buf_in = m_peers[fd]->buffer();
            m_io.disable_read(fd);
            return false;
        }

        if (m_dec->is_complete())
            return true;

        if (buf_in->data_len() < m_dec->symbol_size())
            std::cout << "invalid length, data: " << buf_in->data_len()
                      << ", head: " << buf_in->head_len() << std::endl;
        assert(buf_in->data_len() > m_dec->symbol_size());

        rank = m_dec->rank();
        buf_in->head_pull(m_rlnc_hdr_size);
        m_dec->decode(buf_in->head());

        if (m_dec->rank() == rank) {
            m_linear++;
        } else {
            m_peers[fd]->received_packets++;
            m_encoded_received++;
        }

        while (m_decoded < m_dec->symbols() &&
               m_dec->is_symbol_decoded(m_decoded)) {
            buf_out->head_push(m_len_hdr_size);
            memcpy(buf_out->head(), m_dec->symbol(m_decoded++), max_len);
            len = len_hdr_length(buf_out);
            buf_out->head_pull(m_len_hdr_size);
            buf_out->data_put(len);
            m_tun.write_pkt(buf_out);
            buf_out->reset();
        }

        rlnc_status_send();

        if (!m_dec->is_complete())
            return true;

        std::cout << "decoded block " << m_dec_block << " (linear: "
                  << m_linear << ")" << std::endl;
        m_dec->initialize(m_dec_factory);
        m_dec_block++;
        m_decoded = 0;
        m_linear = 0;
        peers_enable_read();

        if (!m_tmp_enc)
            return false;

        rlnc_enc_process(m_tmp_enc, fd);
        m_tmp_enc.reset();

        return false;
    }

    void recv_tun(int fd)
    {
        buf_ptr buf = m_tun.buffer(m_tun.data_size_max());

        while (m_tun.read_pkt(buf)) {
            put_plain_data(buf);
            buf->reset(m_tun.data_size_max());
            peers_enable_write();

            if (m_enc->rank() < m_enc->symbols())
                continue;

            m_io.disable_read(fd);
            break;
        }
    }

    void recv_peer(int fd)
    {
        buf_ptr buf = m_peers[fd]->buffer();
        buf->head_reserve(m_rlnc_hdr_size);

        while (m_peers[fd]->read_pkt(buf)) {
            switch (rlnc_hdr_type(buf->head())) {
                case rlnc_enc:
                    if (!rlnc_enc_process(buf, fd))
                        return;
                    break;

                case rlnc_status:
                    rlnc_status_process(buf);
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

        if (symbols_added < symbols && m_encoded_sent >= symbols_added) {
            m_io.disable_write(fd);
            return false;
        }

        if (m_encoded_sent >= m_max)
            return false;

        if (m_peers[fd]->ratio_spend()) {
            m_io.disable_write(fd);
            peers_enable_write_next(fd, true);
            return false;
        }

        buf = m_peers[fd]->buffer();
        get_enc_data(buf);

        if (!m_peers[fd]->write_pkt(buf)) {
            peers_enable_write_next(fd, false);
            return false;
        }

        m_encoded_sent++;
        m_peers[fd]->sent_packets++;

        return true;
    }

    void send_peer(int fd)
    {
        while (send_enc_packet(fd))
            ;

        if (m_encoded_sent < m_enc->symbols())
            return;

        if (m_peers[fd]->is_alone())
            return;

        std::cout << "sent block " << m_enc_block << ", ";
        for (auto &p : m_peers)
            if (p)
                std::cout << p->sent_packets << " ";
        std::cout << std::endl;

        m_enc->initialize(m_enc_factory);
        m_enc_block++;
        m_io.enable_read(m_tun.fd());
        peers_initialize();
        m_encoded_sent = 0;
    }

  public:
    coder(const struct args &args)
        : m_enc_factory(args.symbols, args.symbol_size),
          m_dec_factory(args.symbols, args.symbol_size),
          m_enc(m_enc_factory.build()),
          m_dec(m_dec_factory.build()),
          m_tun(tun_stack::interface=args.interface),
          m_max(args.symbols * args.overshoot),
          m_send_buf(args.send_buf),
          m_status_interval(args.status_interval)
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

        size_t max = p->fd() + 1;
        auto rp = std::bind(&coder::recv_peer, this, _1);
        auto sp = std::bind(&coder::send_peer, this, _1);

        if (m_peers.size() < max)
            m_peers.resize(max);

        p->sock_send_buf(m_send_buf);
        m_io.add_cb(p->fd(), rp, sp);
        m_io.disable_write(p->fd());
        m_peers[p->fd()] = std::move(p);
        m_peers_count++;
        m_status_hdr_size += 2;
    }

    void run(size_t timeout)
    {
        int res;

        while (m_sig.running()) {
            res = m_io.wait(timeout);

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
    client(const struct args &args)
        : coder(args)
    {
        client_stack *c;

        c = new client_stack(
                client_stack::redundancy=(args.overshoot - 1),
                client_stack::symbols=args.symbols,
                client_stack::local_address=args.a_src,
                client_stack::remote_address=args.address,
                client_stack::port=args.port
                );
        coder::add_peer(coder::peer_ptr(c));

        c = new client_stack(
                client_stack::redundancy=(args.overshoot - 1),
                client_stack::symbols=args.symbols,
                client_stack::local_address=args.b_src,
                client_stack::remote_address=args.address,
                client_stack::port=args.port
                );
        coder::add_peer(coder::peer_ptr(c));
    }
};

class server : public coder<peer_stack>
{
    srv_stack m_srv;
    double m_overshoot;
    size_t m_symbols;

    void accept_peer(int)
    {
        int fd = m_srv.sock_accept();
        peer_stack *p;

        p = new peer_stack(
                peer_stack::redundancy=m_overshoot,
                peer_stack::symbols=m_symbols,
                peer_stack::file_descriptor=fd
                );
        coder::add_peer(coder::peer_ptr(p));
    }

  public:
    server(const struct args &args)
        : coder(args),
          m_srv(srv_stack::local_address=args.address,
                srv_stack::port=args.port),
          m_overshoot(args.overshoot - 1),
          m_symbols(args.symbols)
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
                args.a_src = optarg;
                break;
            case 5:
                args.b_src = optarg;
                break;
            case 6:
                args.symbols = atoi(optarg);
                break;
            case 7:
                args.symbol_size = atoi(optarg);
                break;
            case 8:
                args.server = true;
                break;
            case 9:
                args.send_buf = atoi(optarg);
                break;
            case 10:
                args.overshoot = strtod(optarg, NULL);
                break;
            case 11:
                args.timeout = atoi(optarg);
                break;
            case 12:
                args.status_interval = atoi(optarg);
                break;
            case '?':
                return 1;
                break;
        }
    }

    if (args.server) {
        server s(args);
        s.run(args.timeout);
    } else {
        client c(args);
        c.run(args.timeout);
    }

    return 0;
}
