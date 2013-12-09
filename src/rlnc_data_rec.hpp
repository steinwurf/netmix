#pragma once

#include "rlnc_data_base.hpp"
#include "kwargs.hpp"

template<class recoder, class super>
class rlnc_data_rec : public super, public rlnc_data_base<recoder>
{
    typedef typename super::buffer_ptr buf_ptr;
    typedef rlnc_data_base<recoder> base;

    size_t m_encoder_rank = 0;
    size_t m_decoder_rank = 0;
    size_t m_linear = 0;
    bool m_stopped = false;

    bool validate_block(size_t block)
    {
        size_t diff = super::rlnc_hdr_block_diff(block);

        if (diff > 8)
            return false;
        else if (diff)
            throw std::runtime_error("off block");

        return true;
    }

    bool is_complete() const
    {
        return base::m_coder->is_complete();
    }

    void spend_budget()
    {
        buf_ptr buf = super::buffer();

        do {
            get_pkt(buf);
            std::cout << "rec len: " << buf->len() << std::endl;
            super::write_pkt(buf);
            buf->reset();
        } while (super::decrease_budget());
    }

    void put_pkt(buf_ptr &buf)
    {
        size_t rank = base::m_coder->rank();

        super::rlnc_hdr_del(buf);
        base::m_coder->decode(buf->data());
        m_encoder_rank = base::m_coder->remote_rank();

        if (base::m_coder->rank() > rank) {
            super::increase_budget();
            m_linear = 0;
        } else {
            m_linear++;
        }
    }

    void get_pkt(buf_ptr &buf)
    {
        size_t len, max_len = base::m_coder->payload_size();

        len = base::m_coder->recode(buf->data_put(max_len));
        buf->data_trim(len);
        super::rlnc_hdr_add_rec(buf);
    }

    void process_ack(buf_ptr &buf)
    {
        size_t block = super::rlnc_hdr_block(buf);

        if (!validate_block(block))
            return;

        base::put_status(buf->data(), &m_decoder_rank);

        if (m_decoder_rank < super::rlnc_symbols())
            return;

        std::cout << "rec ack block " << block << std::endl;
        increment();
    }

    void process_stop(buf_ptr &buf)
    {
        std::cout << "stop" << std::endl;
        m_stopped = true;
    }

    void increment()
    {
        base::increment();
        super::increment();
        m_decoder_rank = 0;
        m_encoder_rank = 0;
        m_stopped = false;
    }

    void increment(size_t block)
    {
        base::increment(block);
        super::increment(block);
        m_decoder_rank = 0;
        m_encoder_rank = 0;
        m_stopped = false;
    }


  public:
    template<typename... Args> explicit
    rlnc_data_rec(const Args&... args)
        : super(args...),
          base(super::rlnc_symbols(), super::rlnc_symbol_size())
    {}

    void stop()
    {
        buf_ptr buf = super::buffer();

        super::rlnc_hdr_add_stop(buf);
        super::write_pkt(buf);
    }

    bool is_full()
    {
        return base::m_coder->is_complete();
    }

    bool write_pkt(buf_ptr &buf)
    {
        size_t type = super::rlnc_hdr_type(buf);
        size_t block = super::rlnc_hdr_block(buf);

        switch (type) {
            case super::rlnc_enc:
            case super::rlnc_rec:
            case super::rlnc_hlp:
                break;

            case super::rlnc_ack:
                std::cout << "ack len: " << buf->len() << std::endl;
                return super::write_pkt(buf);

            default:
                std::cerr << "unexpted packet type: " << type << std::endl;
                return false;
        }

        if (!validate_block(block))
            return false;

        if (is_full())
            return true;

        put_pkt(buf);

        if (m_stopped)
            return true;

        spend_budget();

        return true;
    }

    bool read_pkt(buf_ptr &buf)
    {
        super::rlnc_hdr_reserve(buf);

        if (!super::read_pkt(buf))
            return false;

        switch (super::rlnc_hdr_type(buf)) {
            case super::rlnc_ack:
                process_ack(buf);
                return true;

            case super::rlnc_enc:
            case super::rlnc_rec:
            case super::rlnc_hlp:
                return true;

            case super::rlnc_stop:
                process_stop(buf);
                return false;
        }

        std::cerr << "unexpected packet type: "
                  << super::rlnc_hdr_type(buf) << std::endl;

        return false;
    }

    void timer()
    {
        if (m_decoder_rank == m_encoder_rank)
            return;

        super::increase_budget();
        spend_budget();
    }
};
