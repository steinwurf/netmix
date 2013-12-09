#pragma once

#include <functional>
#include <vector>

template<class buf, class super>
class buffer_pool : public super
{
  public:
    typedef typename buf::pointer buffer_ptr;
    typedef buf buffer_type;

  private:
    struct pool {
        std::vector<buf *> m_pool;

        buf *get()
        {
            buf *b;

            if (m_pool.size() == 0)
                return new buf;

            b = m_pool.back();
            m_pool.pop_back();
            assert(b);
            b->reset();

            return b;
        }

        buf *get(size_t max)
        {
            buf *b;

            if (m_pool.size() == 0)
                return new buf(max);

            b = m_pool.back();
            m_pool.pop_back();
            b->reset(max);

            return b;
        }

        void put(buf *b)
        {
            m_pool.push_back(b);
        }
    };

    pool m_pool;
    std::function<void(buf *)> m_deleter;

  public:
    template<typename... Args> explicit
    buffer_pool(const Args&... a)
        : super(a...),
          m_deleter(std::bind(&pool::put, &m_pool, std::placeholders::_1))
    {}

    buffer_ptr buffer(size_t max)
    {
        return buffer_ptr(m_pool.get(max), m_deleter);
    }

    buffer_ptr buffer()
    {
        return buffer_ptr(m_pool.get(), m_deleter);
    }
};
