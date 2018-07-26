#pragma once

#include <cstring>

#include "rlnc_data_base.hpp"
#include "stat_counter.hpp"

template<class dec, class super>
class rlnc_data_dec : public super, public rlnc_data_base<dec>
{
    typedef rlnc_data_base<dec> base;
    typedef typename super::buffer_ptr buf_ptr;

    stat_counter m_block_count = {"dec blocks"};
    stat_counter m_decoded_count = {"dec packets"};
    stat_counter m_linear_count = {"dec linear"};
    stat_counter m_diff_count = {"dec late"};
    stat_counter m_ack_count = {"dec ack"};
    stat_counter m_partial_count = {"dec partial"};
    stat_counter m_hlp_count = {"dec recv hlp"};
    stat_counter m_rec_count = {"dec recv rec"};
    stat_counter m_enc_count = {"dec recv enc"};
    stat_counter m_lin_5 = {"dec 5 linear"};
    stat_counter m_lin_10 = {"dec 10 linear"};

    size_t m_decoded = 0;
    size_t m_linear = 0;
    size_t m_linear_block = 0;
    size_t m_late_pkts = 0;

    bool validate_block(size_t block)
    {
        size_t diff = super::rlnc_hdr_block_diff(block);

        if (diff)
            ++m_diff_count;

        if (diff > 8)
            return false;
        else if (diff)
            throw std::runtime_error("off block");

        return true;
    }

    bool validate_type(buf_ptr &buf)
    {
        size_t type = super::rlnc_hdr_type(buf);

        switch (type) {
            case super::rlnc_enc:
                ++m_enc_count;
                return true;

            case super::rlnc_rec:
                ++m_rec_count;
                return true;

            case super::rlnc_hlp:
                ++m_hlp_count;
                return true;
        }

        std::cerr << "dec unexpected type: " << type << std::endl;

        return false;
    }

    bool is_partial_done() const
    {
        size_t rank = base::m_coder->rank();
        size_t symbols = base::m_coder->symbols_decoded();
        size_t remote_rank = base::m_coder->remote_rank();

        return rank == remote_rank && rank == symbols;
    }

    bool is_done() const
    {
        return m_decoded == base::m_coder->symbols();
    }

    bool is_complete() const
    {
        return base::m_coder->is_complete();
    }

    bool is_partial_complete() const
    {
        return base::m_coder->is_complete() ||
               base::m_coder->is_symbol_decoded(m_decoded);
    }

    void send_ack(size_t b, size_t r)
    {
        buf_ptr buf = super::buffer();

        super::rlnc_hdr_add_ack(buf, b);
        base::get_status(buf->data_put(base::hdr_len()), r);
        super::write_pkt(buf);
        ++m_ack_count;
    }

    void get_pkt(buf_ptr &buf)
    {
        size_t size = base::m_coder->symbol_size();

        memcpy(buf->head(), base::m_coder->symbol(m_decoded++), size);
        buf->trim(size);
        ++m_decoded_count;
    }

    void put_pkt(buf_ptr &buf)
    {
        size_t rank;
        size_t block = super::rlnc_hdr_block(buf);

        if (!validate_block(block)) {
            if (m_late_pkts++ % 5 == 1)
                send_ack(block, base::m_coder->symbols());
            return;
        }

        assert(buf->data_val() % 4u == 0);
        assert(buf->data_len() >= super::rlnc_symbol_size());

        rank = base::m_coder->rank();
        super::rlnc_hdr_del(buf);
        base::m_coder->decode(buf->head());

        assert(base::m_coder->rank() <= base::m_coder->remote_rank());

        if (base::m_coder->rank() == rank) {
            ++m_linear;
            ++m_linear_block;
            ++m_linear_count;
        } else {
            m_linear = 0;
        }

        if (m_linear < 50)
            return;

        std::cout << "emergency ack " << base::m_coder->rank() << std::endl;
        send_ack(super::rlnc_hdr_block(), base::m_coder->rank());
    }

    void process_rank()
    {
        if (is_partial_done() && !is_complete() && m_linear % 4 == 1)
            send_ack(super::rlnc_hdr_block(), base::m_coder->rank());
    }

    void increment()
    {
        base::increment();
        super::increment();

        if (m_linear_block >= 10)
            ++m_lin_10;
        if (m_linear_block >= 5)
            ++m_lin_5;

        m_decoded = 0;
        m_linear = 0;
        m_linear_block = 0;
        m_late_pkts = 0;
        ++m_block_count;
    }

  public:
    template<typename... Args> explicit
    rlnc_data_dec(const Args&... args)
        : super(args...),
          base(super::rlnc_symbols(), super::rlnc_symbol_size())
    {}

    bool read_pkt(buf_ptr &buf_out)
    {
        buf_ptr buf_in = super::buffer();

        if (!is_done() && is_partial_complete()) {
            get_pkt(buf_out);
            return true;
        } else if (is_done()) {
            increment();
            return false;
        }

        while (true) {
            super::rlnc_hdr_reserve(buf_in);

            if (!super::read_pkt(buf_in))
                break;

            if (!validate_type(buf_in))
                continue;

            put_pkt(buf_in);
            process_rank();
            buf_in->reset();

            if (is_complete()) {
                send_ack(super::rlnc_hdr_block(), base::m_coder->rank());
                break;
            }
        }

        if (!is_partial_complete())
            return false;

        get_pkt(buf_out);

        return true;
    }
};
