#pragma once

#include <linux/if_ether.h>
#include <netinet/ether.h>
#include <net/ethernet.h>
#include <stdexcept>
#include <cstring>
#include <cstdlib>

#include "kwargs.hpp"

struct eth_topology_args
{
    typedef const Kwarg<const char *> arg_type;

    static arg_type source;
    static arg_type destination;
    static arg_type neighbor;
    static arg_type helper;
    static arg_type two_hop;
};

decltype(eth_topology_args::source)      eth_topology_args::source;
decltype(eth_topology_args::destination) eth_topology_args::destination;
decltype(eth_topology_args::neighbor)    eth_topology_args::neighbor;
decltype(eth_topology_args::helper)      eth_topology_args::helper;
decltype(eth_topology_args::two_hop)     eth_topology_args::two_hop;

template<class super>
class eth_topology :
    public super,
    public eth_topology_args
{
    typedef typename super::buffer_ptr buf_ptr;

    static constexpr size_t m_input_len = 6*3 - 1;
    static const int m_hex_base = 16;

    uint8_t m_source_buf[ETH_ALEN];
    uint8_t m_destination_buf[ETH_ALEN];
    uint8_t m_neighbor_buf[ETH_ALEN];
    uint8_t m_helper_buf[ETH_ALEN];
    uint8_t m_two_hop_buf[ETH_ALEN];

    uint8_t *m_source;
    uint8_t *m_destination;
    uint8_t *m_neighbor;
    uint8_t *m_helper;
    uint8_t *m_two_hop;

    static bool parse_address(const char *in, uint8_t *out)
    {
        size_t count = 0, j;
        char *i = const_cast<char *>(in);
        auto addr = reinterpret_cast<struct ether_addr *>(out);

        if (!in)
            return false;

        if (ether_hostton(in, addr) == 0)
            return true;

        if (strnlen(in, m_input_len + 1) != m_input_len)
            throw std::runtime_error("invalid address length");

        for (j = 1; j < ETH_ALEN; ++j)
            if (in[j*3 - 1] != ':')
                throw std::runtime_error("invalid address format");

        while (i && *i && count < ETH_ALEN)
            if (*i != ':')
                out[count++] = strtol(i, &i, m_hex_base);
            else
                i++;

        return true;
    }

    struct ethhdr *header(uint8_t *data)
    {
        return reinterpret_cast<struct ethhdr *>(data);
    }

    uint8_t *hdr_source(uint8_t *data)
    {
        return header(data)->h_source;
    }

    bool compare_source(buf_ptr &buf, uint8_t *addr)
    {
        return addr && memcmp(hdr_source(buf->head()), addr, ETH_ALEN) == 0;
    }

  public:
    template<typename... Args> explicit
    eth_topology(const Args&... args)
        : super(args...),
          m_source(m_source_buf),
          m_destination(m_destination_buf),
          m_neighbor(m_neighbor_buf),
          m_helper(m_helper_buf),
          m_two_hop(m_two_hop_buf)
    {
        constexpr const char *def = NULL;
        const char *source_tmp = kwget(source, def, args...);
        const char *destination_tmp = kwget(destination, def, args...);
        const char *neighbor_tmp = kwget(neighbor, def, args...);
        const char *helper_tmp = kwget(helper, def, args...);
        const char *two_hop_tmp = kwget(two_hop, def, args...);

        if (!parse_address(source_tmp, m_source))
            m_source = NULL;

        if (!parse_address(destination_tmp, m_destination))
            m_destination = NULL;

        if (!parse_address(neighbor_tmp, m_neighbor))
            m_neighbor = NULL;

        if (!parse_address(helper_tmp, m_helper))
            m_helper = NULL;

        if (!parse_address(two_hop_tmp, m_two_hop))
            m_two_hop = NULL;
    }

    const uint8_t *source_addr() const
    {
        return m_source;
    }

    const uint8_t *destination_addr() const
    {
        return m_destination;
    }

    const uint8_t *neighbor_addr() const
    {
        return m_neighbor;
    }

    const uint8_t *helper_addr() const
    {
        return m_helper;
    }

    const uint8_t *two_hop_addr() const
    {
        return m_two_hop;
    }

    const bool is_source(buf_ptr &buf)
    {
        return compare_source(buf, m_source);
    }

    const bool is_destination(buf_ptr &buf)
    {
        return compare_source(buf, m_destination);
    }

    const bool is_neighbor(buf_ptr &buf)
    {
        return compare_source(buf, m_neighbor);
    }

    const bool is_helper(buf_ptr &buf)
    {
        return compare_source(buf, m_helper);
    }

    const bool is_two_hop(buf_ptr &buf)
    {
        return compare_source(buf, m_two_hop);
    }
};
