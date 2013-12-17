#pragma once

#include "kwargs.hpp"

template<class coder>
class rlnc_data_base
{
    typedef typename coder::factory coder_factory;
    typedef typename coder::pointer coder_pointer;
    typedef uint16_t rank_type;

  protected:
    struct rank_hdr {
        rank_type rank;
        uint8_t status[];
    } __attribute__((packed));

    static constexpr size_t m_hdr_len = sizeof(rank_hdr);
    coder_factory m_factory;
    coder_pointer m_coder;

  protected:
    rlnc_data_base(size_t s, size_t size)
        : m_factory(s, size),
          m_coder(m_factory.build())
    {
    }

    void increment(size_t b = 0)
    {
        (void) b;
        m_coder->initialize(m_factory);
    }

    static struct rank_hdr *header(uint8_t *data)
    {
        return reinterpret_cast<struct rank_hdr *>(data);
    }

    void get_status(uint8_t *data, size_t rank)
    {
        auto hdr = header(data);

        hdr->rank = htobe16(rank);
        m_coder->write_feedback(hdr->status);
    }

    void put_status(uint8_t *data, size_t *decoder_rank)
    {
        auto hdr = header(data);

        *decoder_rank = be16toh(hdr->rank);
        m_coder->read_feedback(hdr->status);
    }

    static size_t read_rank(uint8_t *data)
    {
        return be16toh(header(data)->rank);
    }

    size_t status_len() const
    {
        return m_coder->feedback_size();
    }

    size_t hdr_len() const
    {
        return m_hdr_len + status_len();
    }
};
