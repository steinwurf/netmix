#pragma once

#include <random>
#include <vector>
#include <ctime>

#include "kwargs.hpp"
#include "stat_counter.hpp"

struct loss_args
{
    static const Kwarg<double> loss_prob;
};

decltype(loss_args::loss_prob) loss_args::loss_prob;

template<class super>
class loss :
    public super,
    public loss_args
{
    typedef typename super::buffer_ptr buf_ptr;

    std::default_random_engine m_generator;
    std::bernoulli_distribution m_dist;

    bool loose_pkt()
    {
        return m_dist(m_generator);
    }

  public:
    template<typename... Args> explicit
    loss(const Args&... args)
        : super(args...),
          m_generator(time(0)),
          m_dist(kwget(loss_prob, .5, args...))
    {}

    bool read_pkt(buf_ptr &buf)
    {
        if (!super::read_pkt(buf))
            return false;

        if (loose_pkt())
            return false;

        return true;
    }
};

class loss_base
{
  private:
    typedef std::bernoulli_distribution distribution;

    std::default_random_engine m_generator;
    std::vector<distribution> m_dists;

  protected:
    loss_base(std::vector<double> &errors)
        : m_generator(time(0))
    {
        for (auto e : errors)
            m_dists.push_back(distribution(e));
    }

    bool is_lost(size_t e)
    {
        return m_dists[e](m_generator);
    }
};

template<class super>
class loss_dec : public super, public loss_base
{
    typedef typename super::buffer_ptr buf_ptr;

    enum {
        e1 = 0,
        e2,
        e3,
        e4
    };

    stat_counter m_e2_lost{"e2 lost"};
    stat_counter m_e3_lost{"e3 lost"};
    stat_counter m_e4_lost{"e4 lost"};
    stat_counter m_not_lost{"not lost"};

  protected:
    template<typename... Args> explicit
    loss_dec(const Args&... args)
        : super(args...),
          loss_base(super::errors_info())
    {}

    bool read_pkt(buf_ptr &buf)
    {
        if (!super::read_pkt(buf))
            return false;

        if (super::is_helper(buf) && is_lost(e2)) {
            ++m_e2_lost;
            return false;
        } else if (super::is_neighbor(buf) && is_lost(e3)) {
            ++m_e3_lost;
            return false;
        } else if (super::is_two_hop(buf) && is_lost(e4)) {
            ++m_e4_lost;
            return false;
        }

        ++m_not_lost;
        return true;
    }
};

template<class super>
class loss_hlp : public super, public loss_base
{
    typedef typename super::buffer_ptr buf_ptr;

    enum { e1 = 0 };

  protected:
    template<typename... Args> explicit
    loss_hlp(const Args&... args)
        : super(args...),
          loss_base(super::errors_info())
    {}

    bool read_pkt(buf_ptr &buf)
    {
        if (!super::read_pkt(buf))
            return false;

        if (super::is_source(buf) && is_lost(e1))
            return false;

        return true;
    }
};
