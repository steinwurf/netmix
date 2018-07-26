#pragma once

#include "rlnc_data_base.hpp"
#include "stat_counter.hpp"

template<class recoder, class super>
class rlnc_data_hlp : public super, public rlnc_data_base<recoder>
{
    typedef rlnc_data_base<recoder> base;
    typedef typename super::buffer_ptr buf_ptr;

    stat_counter m_enc_count = {"hlp recv packets"};
    stat_counter m_hlp_count = {"hlp send packets"};
    stat_counter m_linear_count = {"hlp linear"};
    stat_counter m_ack_count = {"hlp ack"};
    stat_counter m_block_count = {"hlp blocks"};
    stat_counter m_late_count = {"hlp late"};
    stat_counter m_off_count = {"hlp off block"};

    size_t m_decoder_rank = 0;
    size_t m_hlp_packets = 0;

    bool validate_block(size_t block)
    {
        size_t diff = super::rlnc_hdr_block_diff(block);

        if (diff > 8) {
            ++m_late_count;
            return false;
        } else if (diff) {
            ++m_off_count;
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
        ++m_enc_count;

        if (base::m_coder->rank() > rank &&
            base::m_coder->rank() > super::threshold())
            super::increase_budget();

        if (base::m_coder->rank() == rank)
            ++m_linear_count;
    }

    void get_pkt(buf_ptr &buf)
    {
        size_t len, max_len = base::m_coder->payload_size();

        len = base::m_coder->recode(buf->data_put(max_len));
        buf->data_trim(len);
        super::rlnc_hdr_add_hlp(buf);
        ++m_hlp_count;
        ++m_hlp_packets;
    }

    void process_ack(buf_ptr &buf)
    {
        size_t block = super::rlnc_hdr_block(buf);

        if (!validate_block(block))
            return;

        base::put_status(buf->data(), &m_decoder_rank);
        ++m_ack_count;
    }

    void increment()
    {
        base::increment();
        super::increment();
        base::m_coder->set_systematic_off();
        m_decoder_rank = 0;
        m_hlp_packets = 0;
        ++m_block_count;
    }

    void increment(size_t block)
    {
        base::increment(block);
        super::increment(block);
        base::m_coder->set_systematic_off();
        m_decoder_rank = 0;
        m_hlp_packets = 0;
        ++m_block_count;
    }

  public:
    template<typename... Args> explicit
    rlnc_data_hlp(const Args&... args)
        : super(args...),
          base(super::rlnc_symbols(), super::rlnc_symbol_size())
    {
    }

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

        if (m_hlp_packets > super::budget_max())
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
