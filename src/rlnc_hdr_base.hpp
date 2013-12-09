#pragma once

#include <memory>

class rlnc_types {
  protected:
    enum rlnc_t : uint8_t {
        rlnc_none  = 0,
        rlnc_enc   = 1,
        rlnc_rec   = 2,
        rlnc_hlp   = 3,
        rlnc_ack   = 4,
        rlnc_stop  = 5,
    };

    typedef uint16_t sequence_t;
    typedef uint8_t id_t;
};

template<class buffer>
class rlnc_hdr_base : public rlnc_types
{
    struct hdr {
        rlnc_t type;
        id_t id;
        sequence_t seq;
    } __attribute__((packed));

    typedef typename buffer::pointer buf_ptr;

    size_t m_group = 0;
    size_t m_block = 0;
    sequence_t m_sequence = 0;
    static constexpr size_t m_hdr_len = sizeof(struct hdr);

    struct hdr *header(uint8_t *data)
    {
        return reinterpret_cast<struct hdr *>(data);
    }

    id_t id(size_t g, size_t b)
    {
        return (g << 4) | (b & 0x0F);
    }

    size_t group(uint8_t *data)
    {
        struct hdr *hdr = header(data);

        return hdr->id >> 4;
    }

    size_t block(uint8_t *data)
    {
        struct hdr *hdr = header(data);

        return hdr->id & 0x0F;
    }

  public:
    typedef std::shared_ptr<rlnc_hdr_base> pointer;
    typedef class rlnc_types types;

    rlnc_hdr_base(uint8_t g = 0)
        : m_group(g)
    {}

    static size_t hdr_len()
    {
        return m_hdr_len;
    }

    size_t rlnc_hdr_type(buf_ptr &buf)
    {
        return header(buf->head())->type;
    }

    size_t rlnc_hdr_seq(buf_ptr &buf)
    {
        return header(buf->data())->seq;
    }

    size_t rlnc_hdr_block(buf_ptr &buf)
    {
        return block(buf->head());
    }

    bool rlnc_hdr_is_ack(buf_ptr &buf)
    {
        return header(buf->head())->type == rlnc_ack;
    }

    size_t rlnc_hdr_seq()
    {
        return m_sequence;
    }

    size_t rlnc_hdr_block()
    {
        return m_block & 0x0F;
    }

    size_t rlnc_hdr_block_diff(size_t remote)
    {
        return (remote - m_block) & 0x0f;
    }

    void rlnc_hdr_del(buf_ptr &buf)
    {
        buf->head_pull(m_hdr_len);
    }

    void rlnc_hdr_reserve(buf_ptr &buf)
    {
        buf->head_reserve(m_hdr_len);
    }

    void rlnc_hdr_add(buf_ptr &buf, enum rlnc_t type)
    {
        struct hdr *hdr = header(buf->head_push(m_hdr_len));

        hdr->type = type;
        hdr->id   = id(m_group, m_block);
        hdr->seq  = m_sequence++;
    }

    void rlnc_hdr_add_enc(buf_ptr &buf)
    {
        rlnc_hdr_add(buf, rlnc_enc);
    }

    void rlnc_hdr_add_ack(buf_ptr &buf, size_t b)
    {
        rlnc_hdr_add(buf, rlnc_ack);
        header(buf->head())->id = id(0, b);
    }

    void rlnc_hdr_add_rec(buf_ptr &buf)
    {
        rlnc_hdr_add(buf, rlnc_rec);
    }

    void rlnc_hdr_add_hlp(buf_ptr &buf)
    {
        rlnc_hdr_add(buf, rlnc_hlp);
    }

    void rlnc_hdr_add_stop(buf_ptr &buf)
    {
        rlnc_hdr_add(buf, rlnc_stop);
    }

    void increment()
    {
        m_block++;
    }

    void increment(size_t b)
    {
        m_block = b;
    }
};
