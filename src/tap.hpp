#pragma once

#include <tuntap.hpp>

template<class super>
class tun : public tuntap<tuntap_tap, super>
{
    typedef tuntap<tuntap_tap, super> base;

  public:
    template<typename... Args> explicit
    tun(const Args&... args)
        : base(args...)
    {}
};
