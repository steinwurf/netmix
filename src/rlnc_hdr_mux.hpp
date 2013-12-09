#pragma once

#include <vector>
#include <iostream>

template<class stream, class super>
class rlnc_hdr_mux : public super, public stream::types
{
    typedef typename stream::pointer stream_ptr;
    typedef typename super::buffer_ptr buf_ptr;

    enum stream_idx {
        below   = 0,
        between = 1,
        above   = 2,
        idx_max = 3,
    };

    std::vector<stream_ptr> m_streams;
    size_t m_min, m_max;

    stream_idx index(size_t len)
    {
        if (len <= m_min)
            return below;

        if (len >= m_max)
            return above;

        return between;
    }

  public:
    const Kwarg<size_t> min_size;
    const Kwarg<size_t> max_size;

  protected:
    template<typename... Args> explicit
    rlnc_hdr_mux(const Args&... a)
        : super(a...),
          m_min(kwget(min_size, 0, a...)),
          m_max(kwget(max_size, 0, a...)),
          m_streams(idx_max)
    {
        m_streams[below]   = stream_ptr(new stream(below));
        m_streams[between] = stream_ptr(new stream(between));
        m_streams[above]   = stream_ptr(new stream(above));
    }

    size_t data_size_max()
    {
        return super::data_size_max() - stream::hdr_len();
    }

    void rlnc_hdr_add_plain(buf_ptr &buf)
    {
        size_t idx = index(buf->len());

        m_streams[idx]->rlnc_hdr_add_plain(buf);
    }

    void rlnc_hdr_del(buf_ptr &buf)
    {
        size_t idx = index(buf->len());

        m_streams[idx]->rlnc_hdr_del(buf);
    }

    void rlnc_hdr_reserve(buf_ptr &buf)
    {
        size_t idx = index(buf->len());

        m_streams[idx]->rlnc_hdr_reserve(buf);
    }
};
