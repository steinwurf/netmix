#pragma once

class final_layer
{
  protected:
    typedef uint8_t hdr_first;
    typedef uint8_t hdr_pad;

    constexpr static hdr_first hdr_base()
    {
        return 0;
    }

  public:
    template<typename... Args> explicit
    final_layer(const Args&...)
    {}

    void timer()
    {}

    void increment(size_t b = 0)
    { (void) b; }

    size_t hdr_len()
    {
        return 0;
    }
};
