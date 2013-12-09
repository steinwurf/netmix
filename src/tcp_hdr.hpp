#pragma once

#include <linux/types.h>
#include <endian.h>
#include <cassert>

template<class super>
class tcp_hdr : public super
{
    typedef typename super::buffer_ptr buf_ptr;

    struct hdr_type {
        __be32 length;
    } __attribute__((packed));

    static constexpr size_t m_hdr_len = sizeof(struct hdr_type);
    struct hdr_type m_in_hdr = {0};
    buf_ptr m_in_buf;
    buf_ptr m_out_buf;

    struct hdr_type *header(uint8_t *data)
    {
        return reinterpret_cast<struct hdr_type *>(data);
    }

    void reset()
    {
        m_in_buf = super::buffer();
        m_in_hdr = {0};
    }

    size_t remain()
    {
        return m_in_hdr.length - m_in_buf->len();
    }

    bool read_pkt_new(buf_ptr &buf)
    {
        assert(m_in_buf->len() < m_hdr_len);

        if (!m_in_buf->head_len())
            m_in_buf->head_reserve(m_hdr_len);

        super::read_pkt(m_in_buf, m_hdr_len - m_in_buf->len(), m_in_buf->len());

        if (m_in_buf->len() != m_hdr_len)
            return false;

        auto hdr = header(m_in_buf->head());
        m_in_hdr.length = be32toh(hdr->length);

        m_in_buf->head_pull(m_hdr_len);

        assert(m_in_buf->head_len() == 0);

        m_in_buf->head_reserve(buf->head_len());

        return read_pkt_cont(buf);
    }

    bool read_pkt_cont(buf_ptr &buf)
    {
        do {
            if (!super::read_pkt(m_in_buf, remain(), m_in_buf->len()))
                return false;

        } while (remain());

        assert(buf->len() == 0);
        assert(m_in_buf->len() == m_in_hdr.length);
        buf.swap(m_in_buf);
        reset();

        return true;
    }

    bool send_pkt_new(buf_ptr &buf)
    {
        uint32_t len = buf->len();
        auto hdr = header(buf->head_push(m_hdr_len));

        hdr->length = htobe32(len);
        assert(buf->len());

        if (!super::write_pkt(buf) || buf->len()) {
            m_out_buf.swap(buf);
            return false;
        }

        return true;
    }

    bool send_pkt_cont(buf_ptr &buf)
    {
        assert(m_out_buf->len());

        if (!super::write_pkt(m_out_buf)) {
            std::cout << "unable to send cached pkt: " << m_out_buf->len()
                      << std::endl;
            return false;
        }

        m_out_buf.reset();
        return send_pkt_new(buf);
    }

  public:
    template<typename... Args> explicit
    tcp_hdr(const Args&... a)
        : super(a...),
          m_in_buf(super::buffer())
    {}

    size_t hdr_len()
    {
        return super::hdr_len() + m_hdr_len;
    }

    bool write_pkt(buf_ptr &buf)
    {
        if (m_out_buf)
            return send_pkt_cont(buf);
        else
            return send_pkt_new(buf);
    }

    bool read_pkt(buf_ptr &buf)
    {
        if (m_in_hdr.length)
            return read_pkt_cont(buf);
        else
            return read_pkt_new(buf);

    }
};
