#pragma once

#include <linux/types.h>
#include <endian.h>

template<class super>
class len_hdr : public super
{
    typedef typename super::buffer_ptr buf_ptr;

    struct hdr {
        __be32 length;
    } __attribute__((packed));

    static constexpr size_t m_hdr_len = sizeof(struct hdr);

    struct hdr *header(uint8_t *data)
    {
        return reinterpret_cast<struct hdr *>(data);
    }

  public:
    template<typename... Args> explicit
    len_hdr(const Args&... args)
        : super(args...)
    {}

    size_t data_size_max()
    {
        return super::data_size_max() - m_hdr_len;
    }

    bool write_pkt(buf_ptr &buf)
    {
        size_t length = buf->len();
        struct hdr *hdr = header(buf->head_push(m_hdr_len));

        hdr->length = htobe32(length);

        return super::write_pkt(buf);
    }

    bool read_pkt(buf_ptr &buf)
    {
        buf->head_reserve(m_hdr_len);

        if (!super::read_pkt(buf))
            return false;

        struct hdr *hdr = header(buf->head());

        buf->head_pull(m_hdr_len);
        buf->data_trim(be32toh(hdr->length));

        return true;
    }
};
