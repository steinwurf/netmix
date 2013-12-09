#pragma once

#include <cstring>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <iostream>

template<class super>
class eth_hdr : public super
{
    typedef typename super::buffer_ptr buf_ptr;

    struct ethhdr *header(uint8_t *data)
    {
        return reinterpret_cast<struct ethhdr *>(data);
    }

  public:
    template<typename... Args> explicit
    eth_hdr(const Args&... a)
        : super(a...)
    {}

    size_t data_size_max()
    {
        return super::data_size_max() - ETH_HLEN;
    }

    bool write_pkt(buf_ptr &buf)
    {
        struct ethhdr *hdr = header(buf->head_push(ETH_HLEN));

        memcpy(hdr->h_dest, super::neighbor_addr(), ETH_ALEN);
        memcpy(hdr->h_source, super::interface_address(), ETH_ALEN);
        hdr->h_proto = htons(super::proto());

        return super::write_pkt(buf);
    }

    bool read_pkt(buf_ptr &buf)
    {
        size_t offset = sizeof(struct ethhdr) % 4;
        buf->move(-offset);
        buf->head_reserve(ETH_HLEN);

        if (!super::read_pkt(buf))
            return false;

        buf->head_pull(ETH_HLEN);

        return true;
    }
};
