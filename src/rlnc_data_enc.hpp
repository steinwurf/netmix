#pragma once

#include "rlnc_data_base.hpp"
#include "stat_counter.hpp"

template<class enc, class super>
class rlnc_data_enc : public super, public rlnc_data_base<enc>
{
    typedef rlnc_data_base<enc> base;
    typedef typename super::buffer_ptr buf_ptr;

    stat_counter m_pkt_count = {"enc packets"};
    stat_counter m_pkt_fail = {"enc failed"};
    stat_counter m_block_count = {"enc blocks"};
    stat_counter m_ack_count = {"enc ack"};
    stat_counter m_late_count = {"enc late"};
    stat_counter m_timeout_count = {"enc timeouts"};
    stat_counter m_interrupted = {"enc interrupted"};

    size_t m_decoder_rank = 0;
    bool m_stopped = false;

    bool validate_block(size_t block)
    {
        size_t diff = super::rlnc_hdr_block_diff(block);

        if (diff)
            ++m_late_count;

        if (diff > 8)
            return false;
        else if (diff == 0)
            return true;

        throw std::runtime_error("off block");
    }

    void get_pkt(buf_ptr &buf)
    {
        size_t len, max_len = base::m_coder->payload_size();

        len = base::m_coder->encode(buf->data_put(max_len));
        buf->data_trim(len);
        super::rlnc_hdr_add_enc(buf);
        ++m_pkt_count;
    }

    void put_pkt(buf_ptr &buf)
    {
        assert(buf->head_val() % 4u == 0);

        sak::const_storage symbol(buf->head(), buf->len());

        base::m_coder->set_symbol(base::m_coder->symbols_initialized(), symbol);
        super::increase_budget();
    }

    void increment()
    {
        m_stopped = false;
        m_decoder_rank = 0;
        base::increment();
        super::increment();
        ++m_block_count;
    }

    void process_ack(buf_ptr &buf)
    {
        size_t block = super::rlnc_hdr_block(buf);

        if (!validate_block(block)) {
            return;
        }

        ++m_ack_count;
        base::put_status(buf->data(), &m_decoder_rank);

        if (m_decoder_rank < super::rlnc_symbols())
            return;

        increment();
    }

  public:
    template<typename... Args> explicit
    rlnc_data_enc(const Args&... args)
        : super(args...),
          base(super::rlnc_symbols(), super::rlnc_symbol_size())
    {}

    size_t data_size_max()
    {
        return base::m_coder->symbol_size();
    }

    bool is_full()
    {
        return m_stopped || base::m_coder->rank() == base::m_coder->symbols();
    }

    bool is_empty()
    {
        return base::m_coder->rank() == 0;
    }

    bool write_pkt(buf_ptr &buf_in)
    {
        buf_ptr buf_out = super::buffer();
        put_pkt(buf_in);

        if (base::m_coder->rank() < super::rlnc_symbols()) {
            get_pkt(buf_out);
            if (!super::write_pkt(buf_out)) {
                ++m_pkt_fail;
                return false;
            }

            super::decrease_budget();
            return true;
        }

        do {
            buf_in->reset();
            buf_out->reset();

            if (read_pkt(buf_in)) {
                ++m_interrupted;
                break;
            }

            get_pkt(buf_out);

            if (!super::write_pkt(buf_out)) {
                ++m_pkt_fail;
                return false;
            }

        } while (super::decrease_budget());

        return true;
    }

    bool read_pkt(buf_ptr &buf)
    {
        buf_ptr buf_in = super::buffer();
        super::rlnc_hdr_reserve(buf_in);

        if (!super::read_pkt(buf_in))
            return false;

        switch (super::rlnc_hdr_type(buf_in)) {
            case super::rlnc_ack:
                process_ack(buf_in);
                return false;

            case super::rlnc_stop:
                std::cout << "enc stopped" << std::endl;
                m_stopped = true;
                return false;

            default:
                std::cout << "unexpected packet: "
                          << super::rlnc_hdr_type(buf_in) << std::endl;
                break;
        }

        return true;
    }

    void timer()
    {
        buf_ptr buf;
        super::timer();

        if (base::m_coder->symbols_initialized() == 0)
            return;

        if (m_decoder_rank == base::m_coder->rank())
            return;

        if (m_stopped)
            return;

        for (size_t i = 0; i < 5; ++i) {
            buf = super::buffer();
            get_pkt(buf);
            super::write_pkt(buf);
        }

        ++m_timeout_count;
    }
};
