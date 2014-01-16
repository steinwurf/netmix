#pragma once

#include<string>
#include<iostream>

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

class stat_counter
{
    typedef uint32_t counter_type;
    typedef std::pair<std::string, counter_type> item_type;
    typedef std::vector<item_type> vector_type;

    static vector_type m_counters;
    static counter_type m_max_count;
    static size_t m_max_desc;
    size_t m_index;

    static size_t count_width()
    {
        if (!m_max_count)
            return 1;

        return log10(m_max_count) + 1;
    }

    static size_t desc_width()
    {
        return m_max_desc;
    }

  public:
    stat_counter(const char *desc)
        : m_index(m_counters.size())
    {
        m_counters.push_back(item_type(desc, 0));
        m_max_desc = std::max(m_max_desc, m_counters.back().first.length());
    }

    size_t operator++()
    {
        m_max_count = std::max(m_max_count, m_counters[m_index].second + 1);

        return ++m_counters[m_index].second;
    }

    size_t operator+=(size_t i)
    {
        counter_type new_count(m_counters[m_index].second += i);
        m_max_count = std::max(m_max_count, new_count);

        return new_count;
    }

    size_t count() const
    {
        return m_counters[m_index].second;
    }

    std::string &description() const
    {
        return m_counters[m_index].first;
    }

    static std::ostream &all(std::ostream &stream)
    {
        for (auto &item : m_counters) {
            stream.width(desc_width() + 2);
            stream << std::left << item.first + ": ";
            stream.width(count_width());
            stream << std::right << item.second << std::endl;
        }

        return stream;
    }

    friend std::ostream& operator<<(std::ostream &stream, const stat_counter &c)
    {
        stream.width(desc_width() + 2);
        stream << std::left << c.description() + ": ";
        stream.width(count_width());
        stream << c.count();

        return stream;
    }
};

stat_counter::vector_type stat_counter::m_counters{};
stat_counter::counter_type stat_counter::m_max_count{0};
size_t stat_counter::m_max_desc{0};
