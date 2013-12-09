#pragma once

#include "rlnc_data_base.hpp"

template<class recoder, class super>
class rlnc_data_hlp : public super, public rlnc_data_base<recoder>
{
    typedef rlnc_data_base<recoder> base;
    typedef typename super::buffer_ptr buf_ptr;

    size_t m_decoder_rank = 0;

    bool validate_block(size_t block)
    {
        size_t diff = super::rlnc_hdr_block_diff(block);

        if (diff > 8) {
            return false;
        } else if (diff == 1) {
            increment(block);
            return false;
        } else if (diff) {
            increment(block);
            return false;
        }

        return true;
    }

    void put_pkt(buf_ptr &buf)
    {
        size_t rank = base::m_coder->rank();

        super::rlnc_hdr_del(buf);
        base::m_coder->decode(buf->data());

        if (base::m_coder->rank() > rank)
            super::increase_budget();
    }

    void get_pkt(buf_ptr &buf)
    {
        size_t len, max_len = base::m_coder->payload_size();

        len = base::m_coder->recode(buf->data_put(max_len));
        buf->data_trim(len);
        super::rlnc_hdr_add_hlp(buf);
    }

    void process_ack(buf_ptr &buf)
    {
        size_t block = super::rlnc_hdr_block(buf);

        if (!validate_block(block))
            return;

        m_decoder_rank = base::read_rank(buf->data());

        if (m_decoder_rank == super::rlnc_symbols()) {
            std::cout << "hlp ack block " << block << std::endl;
            increment();
        }
    }

    void increment()
    {
        base::increment();
        super::increment();
        m_decoder_rank = 0;
    }

    void increment(size_t block)
    {
        base::increment(block);
        super::increment(block);
        m_decoder_rank = 0;
    }

  public:
    template<typename... Args> explicit
    rlnc_data_hlp(const Args&... args)
        : super(args...),
          base(super::rlnc_symbols(), super::rlnc_symbol_size())
    {}

    bool write_pkt(buf_ptr &buf_in)
    {
        size_t type = super::rlnc_hdr_type(buf_in);
        size_t block = super::rlnc_hdr_block(buf_in);
        buf_ptr buf_out;

        if (type != super::rlnc_enc && type != super::rlnc_rec) {
            std::cout << "unexpted packet type: " << type << std::endl;
            return super::write_pkt(buf_in);
        }

        if (!validate_block(block))
            return false;

        put_pkt(buf_in);

        if (base::m_coder->rank() < super::threshold())
            return true;

        buf_out = super::buffer();

        do {
            get_pkt(buf_out);
            super::write_pkt(buf_out);
            buf_out->reset();
        } while (super::decrease_budget());

        return true;
    }

    bool read_pkt(buf_ptr &buf)
    {
        super::rlnc_hdr_reserve(buf);
        size_t type;

        if (!super::read_pkt(buf))
            return false;

        type = super::rlnc_hdr_type(buf);

        switch (type) {
            case super::rlnc_ack:
                process_ack(buf);
                return false;

            case super::rlnc_stop:
                return false;

            case super::rlnc_enc:
            case super::rlnc_rec:
                return true;
        }

        std::cerr << "unexpected packet type: " << type << std::endl;

        return true;
    }
};
