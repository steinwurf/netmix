#pragma once

#include "rlnc_hdr_base.hpp"

template<class super>
class rlnc_hdr : public super, public rlnc_hdr_base<typename super::buffer_type>
{
    typedef rlnc_hdr_base<typename super::buffer_type> base;

  protected:
    template<typename... Args> explicit
    rlnc_hdr(const Args&... args)
        : super(args...)
    {}

    size_t data_size_max()
    {
        return super::data_size_max() - base::hdr_len();
    }

    void increment()
    {
        base::increment();
        super::increment();
    }

    void increment(size_t b)
    {
        super::increment(b);
        base::increment(b);
    }
};
