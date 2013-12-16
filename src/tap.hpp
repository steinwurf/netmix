#pragma once

#include <tuntap.hpp>

template<class super>
class tun : public tuntap_base<tuntap_tap, super>
{
    typedef tuntap_base<tuntap_tap, super> base;

  public:
    template<typename... Args> explicit
    tun(const Args&... args)
        : base(args...)
    {}
};
