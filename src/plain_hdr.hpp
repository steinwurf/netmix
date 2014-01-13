#pragma once

#include <endian.h>
#include <boost/circular_buffer.hpp>
#include <bitset>

template<class super>
class plain_hdr : public super
{
    enum plain_type : typename super::hdr_first {
        type_data = super::hdr_base(),
        type_req,
        type_max,
    };

    struct hdr {
        typename super::hdr_first type;
        typename super::hdr_pad group;
        uint16_t seq;
    } __attribute__((packed));

    typedef typename super::buffer_ptr buf_ptr;

    static constexpr size_t m_hdr_len = sizeof(struct hdr);
    static constexpr size_t m_window_size = 64;
    std::bitset<m_window_size> m_pkts_recv = 0;
    std::bitset<m_window_size> m_pkts_send = 0;
    boost::circular_buffer<buf_ptr> m_buf_sent, m_buf_recv;
    uint16_t m_seq_recv = 0, m_seq_sent = 0;
    uint16_t m_offset_recv = 0, m_offset_send;
    bool m_pending_req = false;

    struct req {
        decltype(m_pkts_recv.to_ullong()) received;
    };

    static constexpr size_t m_req_len = sizeof(struct req);

    struct hdr *header(uint8_t *data)
    {
        return reinterpret_cast<struct hdr *>(data);
    }

    struct req *req_info(uint8_t *data)
    {
        return reinterpret_cast<struct req *>(data);
    }

    bool is_missing_pkts()
    {
        if (m_pkts_recv.count() == 0 && m_pending_req)
            return true;

        if (!m_pkts_recv[0] && m_pkts_recv.count())
            return true;

        return false;
    }

    void req_hdr_send()
    {
        buf_ptr buf = super::buffer();
        auto hdr = header(buf->head_push(m_hdr_len));
        auto req = req_info(buf->data_put(m_req_len));

        hdr->type = type_req;
        hdr->group = 0;
        hdr->seq = htobe16(m_seq_recv);
        req->received = htobe64(m_pkts_recv.to_ullong());

        super::write_pkt(buf);
        m_pending_req = m_pkts_recv.count();
    }

    void resend_packets()
    {
        for (size_t i = 0; i < m_buf_sent.size(); ++i) {
            if (m_pkts_send[i])
                continue;

            buf_ptr out = m_buf_sent[i];
            super::write_pkt(out);
        }
    }

    void req_hdr_process(buf_ptr &buf)
    {
        auto hdr = header(buf->head());
        auto req = req_info(buf->data());
        m_pkts_send = be64toh(req->received);

        /* fast forward to last seen */
        while (m_buf_sent.size() >
               static_cast<size_t>(m_seq_sent - be16toh(hdr->seq)))
            m_buf_sent.pop_front();

        resend_packets();
    }

    void data_hdr_add(buf_ptr &buf)
    {
        auto hdr = header(buf->head_push(m_hdr_len));

        hdr->type = type_data;
        hdr->group = 0;
        hdr->seq = htobe16(m_seq_sent++);
    }

    void data_hdr_process(buf_ptr &buf)
    {
        auto hdr = header(buf->head());
        m_offset_recv = be16toh(hdr->seq) - m_seq_recv;

        if (be16toh(hdr->seq) < m_seq_recv) {
            req_hdr_send();
            return;
        }

        if (m_offset_recv > m_window_size) {
            std::cout << m_pkts_recv << std::endl;
            throw std::runtime_error("window size exceeded");
        }

        buf->head_pull(m_hdr_len);
        m_pkts_recv[m_offset_recv] = true;
        m_buf_recv[m_offset_recv] = buf;
    }

    void hdr_process(buf_ptr &buf)
    {
        switch (header(buf->head())->type) {
            case type_data:
                data_hdr_process(buf);
                break;

            case type_req:
                req_hdr_process(buf);
                break;
        }
    }

    bool get_pkt(buf_ptr &buf)
    {
        if (!m_pkts_recv[0])
            return false;

        buf = m_buf_recv.front();
        m_buf_recv.push_back(buf_ptr());
        m_pkts_recv >>= 1;
        m_seq_recv++;

        return true;
    }

    void put_pkt(buf_ptr &buf)
    {
        buf_ptr new_buf = super::buffer();

        buf->head_reset(m_hdr_len);
        m_buf_sent.push_back(buf);
        buf.swap(new_buf);
    }

  protected:
    static constexpr typename super::hdr_first hdr_base()
    {
        return type_max;
    }

    static constexpr typename super::hdr_first hdr_data()
    {
        return type_data;
    }

    static constexpr typename super::hdr_first hdr_req()
    {
        return type_req;
    }

  public:
    template<typename... Args> explicit
    plain_hdr(const Args&... args)
        : super(args...),
          m_buf_sent(m_window_size),
          m_buf_recv(m_window_size, buf_ptr())
    {}

    size_t data_size_max()
    {
        return super::data_size_max() - m_hdr_len;
    }

    bool is_full()
    {
        return m_buf_sent.full();
    }

    bool data_empty()
    {
        return m_buf_sent.size() == 0;
    }

    bool write_pkt(buf_ptr &buf)
    {
        data_hdr_add(buf);

        if (!super::write_pkt(buf))
            return false;

        put_pkt(buf);

        return true;
    }

    bool read_pkt(buf_ptr &buf_out)
    {
        buf_ptr buf_in;

        if (get_pkt(buf_out))
            return true;

        buf_in = super::buffer(buf_out->max_len());
        buf_in->head_reserve(m_hdr_len);
        buf_in->head_reserve(buf_out->head_len());

        while (super::read_pkt(buf_in)) {
            hdr_process(buf_in);

            if (get_pkt(buf_out))
                return true;
        }

        if (is_missing_pkts())
            req_hdr_send();

        return false;
    }

    void timer()
    {
        if (is_missing_pkts())
            req_hdr_send();

        if (m_pkts_send.count() && m_buf_sent.size())
            resend_packets();
    }
};
