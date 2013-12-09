#pragma once

#include <vector>
#include <memory>
#include <cassert>

class buffer_pkt
{
    static const size_t m_default_len = 2000;
    static constexpr size_t m_headroom = 100;
    size_t m_buffer_len;

    std::vector<uint8_t> m_buffer;
    size_t m_len = 0;
    uint8_t *m_data;
    uint8_t *m_head;

  public:
    typedef std::shared_ptr<buffer_pkt> pointer;

    buffer_pkt(size_t buffer_len)
        : m_buffer_len(buffer_len),
          m_buffer(m_buffer_len + m_headroom),
          m_data(&m_buffer[0] + m_headroom),
          m_head(&m_buffer[0] + m_headroom)
    {
        assert(m_data);
        assert(m_head);
    }

    buffer_pkt()
        : buffer_pkt(m_default_len)
    {}

    uint8_t *data() const
    {
        assert(m_data);
        return m_data;
    }

    uint8_t *head() const
    {
        return m_head;
    }

    const char *c_str() const
    {
        return reinterpret_cast<char *>(m_data);
    }

    const uintptr_t data_val() const
    {
        return reinterpret_cast<uintptr_t>(m_data);
    }

    const uintptr_t head_val() const
    {
        return reinterpret_cast<uintptr_t>(m_head);
    }

    void reset()
    {
        m_data = &m_buffer[0] + m_headroom;
        m_head = &m_buffer[0] + m_headroom;
        m_len = 0;
    }

    void reset(size_t new_len)
    {
        m_buffer_len = new_len;
        m_buffer.resize(new_len + m_headroom);
        reset();
    }

    uint8_t *head_push(size_t size)
    {
        m_head -= size;
        m_len += size;

        return m_head;
    }

    uint8_t *head_pull(size_t size)
    {
        m_head += size;
        m_len -= size;

        return m_head;
    }

    uint8_t *head_reserve(size_t size)
    {
        m_data += size;

        return m_head;
    }

    uint8_t *head_reset()
    {
        m_head = m_data;

        return m_head;
    }

    uint8_t *head_reset(size_t size)
    {
        m_head = m_data - size;

        return m_head;
    }

    uint8_t *data_push(size_t size)
    {
        m_data += size;
        m_len -= size;

        return m_data;
    }

    uint8_t *data_pull(size_t size)
    {
        m_data -= size;
        m_len += size;

        return m_data;
    }

    uint8_t *data_put(size_t size)
    {
        m_len += size;

        return m_data;
    }

    void data_trim(size_t size)
    {
        m_len = size;
    }

    void push(size_t size)
    {
        m_len += size;
    }

    void pull(size_t size)
    {
        m_head += size;
        m_data += size;
        m_len -= size;
    }

    void trim(size_t size)
    {
        m_len = size;
    }

    void move(size_t size)
    {
        m_head += size;
        m_data += size;
    }

    size_t max_head_len() const
    {
        return m_head - &m_buffer[0];
    }

    size_t max_data_len() const
    {
        return m_buffer_len - (m_data - &m_buffer[0]);
    }

    size_t max_len() const
    {
        return m_buffer_len;
    }

    size_t len() const
    {
        return m_len;
    }

    size_t head_len() const
    {
        return m_data - m_head;
    }

    size_t data_len() const
    {
        return m_len - (m_data - m_head);
    }
};
