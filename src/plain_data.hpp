#pragma once

#include "kwargs.hpp"

template<class super>
class plain_data : public super
{
    typedef typename super::buffer_ptr buf_ptr;

  protected:
    template<typename... Args> explicit
    plain_data(const Args&... a)
        : super(a...)
    {}

    bool write_pkt(buf_ptr &buf)
    {
        super::plain_hdr_add(buf);

        return super::write_pkt(buf);
    };

    bool read_pkt(buf_ptr &buf)
    {
        super::plain_hdr_reserve(buf);

        if (!super::read_pkt(buf))
            return false;

        super::plain_hdr_del(buf);

        return true;
    }
};
