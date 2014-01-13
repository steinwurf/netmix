#pragma once

#include <cstring>

#include "rlnc_data_base.hpp"

template<class dec, class super>
class rlnc_data_dec : public super, public rlnc_data_base<dec>
{
    typedef rlnc_data_base<dec> base;
    typedef typename super::buffer_ptr buf_ptr;

    size_t m_decoded = 0;
    size_t m_linear = 0;

    bool validate_block(size_t block)
    {
        size_t diff = super::rlnc_hdr_block_diff(block);

        if (diff)
            std::cout << "dec diff " << diff << std::endl;

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
            case super::rlnc_rec:
            case super::rlnc_hlp:
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
        return base::m_coder->is_symbol_decoded(m_decoded);
    }

    void send_ack(size_t b, size_t r)
    {
        buf_ptr buf = super::buffer();

        super::rlnc_hdr_add_ack(buf, b);
        base::get_status(buf->data_put(base::hdr_len()), r);
        super::write_pkt(buf);
    }

    void get_pkt(buf_ptr &buf)
    {
        size_t size = base::m_coder->symbol_size();

        memcpy(buf->head(), base::m_coder->symbol(m_decoded++), size);
        buf->trim(size);
    }

    void put_pkt(buf_ptr &buf)
    {
        size_t rank;
        size_t block = super::rlnc_hdr_block(buf);

        if (!validate_block(block)) {
            send_ack(block, base::m_coder->symbols());
            return;
        }

        assert(buf->data_val() % 4u == 0);

        rank = base::m_coder->rank();
        super::rlnc_hdr_del(buf);
        base::m_coder->decode(buf->head());

        if (base::m_coder->rank() == rank)
            ++m_linear;
        else
            m_linear = 0;
    }

    void process_rank()
    {
        if (m_linear < 1)
            return;

        if (!is_partial_done())
            return;

        if (is_complete())
            return;

        send_ack(super::rlnc_hdr_block(), base::m_coder->rank());
    }

    void increment()
    {
        std::cerr << "dec ack block " << super::rlnc_hdr_block() << std::endl;
        send_ack(super::rlnc_hdr_block(), base::m_coder->rank());
        base::increment();
        super::increment();
        m_decoded = 0;
        m_linear = 0;
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

        while (true) {
            super::rlnc_hdr_reserve(buf_in);

            if (!super::read_pkt(buf_in))
                break;

            if (!validate_type(buf_in))
                continue;

            put_pkt(buf_in);
            process_rank();
            buf_in->reset();
        }

        if (is_done()) {
            increment();
            return false;
        }

        if (!is_partial_complete())
            return false;

        get_pkt(buf_out);

        return true;
    }
};
