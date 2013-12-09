#pragma once

#include <algorithm>
#include <kwargs.hpp>

struct counters_args
{
    static const Kwarg<double> redundancy;
    static const Kwarg<size_t> symbols;
};

decltype(counters_args::redundancy) counters_args::redundancy;
decltype(counters_args::symbols)    counters_args::symbols;

template<class super>
class counters : public super, public counters_args
{
    size_t m_symbols;
    size_t m_redundant;
    size_t m_wm_low;
    size_t m_wm_high;
    size_t m_report = 0;

  public:
    size_t received_packets = 0;
    size_t sent_packets = 0;
    size_t ratio_packets;

    template<typename... Args>
    counters(const Args&... args)
        : super(args...)
    {
        double r = kwget(redundancy, .10, args...);
        size_t g = kwget(symbols, 100, args...);

        m_symbols = g;
        m_redundant = g*(r/2);
        m_wm_high = g + g*.05;
        m_wm_low = 2;
        ratio_packets = g*.8 + m_redundant;
        std::cout << "ratio: " << ratio_packets << std::endl;
        std::cout << "redundancy: " << r << std::endl;
        std::cout << "symbols: " << g << std::endl;
    }

    size_t set_ratio(double packets, size_t interval)
    {
        double ratio = packets/interval;
        ratio_packets = ratio*m_symbols + m_redundant;
        ratio_packets = std::max<size_t>(ratio_packets, m_wm_low);
        ratio_packets = std::min<size_t>(ratio_packets, m_wm_high);

        std::cout << "packets: " << packets << ", ratio: " << ratio_packets << std::endl;

        return ratio_packets;
    }

    void set_max_ratio()
    {
        ratio_packets = m_wm_high;
    }

    bool ratio_spend()
    {
        return sent_packets > ratio_packets;
    }

    bool is_alone()
    {
        return sent_packets >= m_wm_high;
    }
};
