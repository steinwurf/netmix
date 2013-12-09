#pragma once

template<class super>
class pause_hdr : public super
{
    enum pause_type : typename super::hdr_first
    {
        type_stop = super::hdr_base(),
        type_start,
        type_ignore,
        type_max
    };

    struct hdr {
        typename super::hdr_first type;
        typename super::hdr_pad padding;
    } __attribute__((packed));

    struct hdr *header(uint8_t *data)
    {
        return reinterpret_cast<struct hdr *>(data);
    }

    typedef typename super::buffer_ptr buf_ptr;

    constexpr static size_t m_hdr_len = sizeof(struct hdr);
    bool m_stopped = false;

    bool process(buf_ptr &buf)
    {
        auto hdr = header(buf->head());

        buf->head_pull(m_hdr_len);

        switch (hdr->type) {
            case type_stop:
                m_stopped = true;
                return false;

            case type_start:
                m_stopped = false;
                return false;

            case type_ignore:
                return true;

            default:
                return false;
        }
    }

    void add_hdr(buf_ptr &buf, pause_type t)
    {
        auto hdr = header(buf->head_push(m_hdr_len));

        hdr->type = t;
        hdr->padding = 0;
    }

    void send_pause(pause_type t)
    {
        buf_ptr buf = super::buffer();

        add_hdr(buf, t);
        super::write_pkt(buf);
    }

  protected:
    constexpr static typename super::hdr_first hdr_base()
    {
        return type_max;
    }

  public:
    template<typename... Args> explicit
    pause_hdr(const Args&... args)
        : super(args...)
    {}

    size_t data_size_max()
    {
        return super::data_size_max() - m_hdr_len;
    }

    void start()
    {
        send_pause(type_start);
    }

    void stop()
    {
        send_pause(type_stop);
    }

    bool stopped()
    {
        return m_stopped;
    }

    bool write_pkt(buf_ptr &buf)
    {
        add_hdr(buf, type_ignore);

        return super::write_pkt(buf);
    }

    bool read_pkt(buf_ptr &buf)
    {
        buf->head_reserve(m_hdr_len);

        if (!super::read_pkt(buf))
            return false;

        return process(buf);
    }
};
