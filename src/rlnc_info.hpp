#pragma once

#include "kwargs.hpp"

struct rlnc_info_args
{
    static const Kwarg<size_t> symbols;
    static const Kwarg<size_t> symbol_size;
};

decltype(rlnc_info_args::symbols)     rlnc_info_args::symbols;
decltype(rlnc_info_args::symbol_size) rlnc_info_args::symbol_size;

template<class super>
class rlnc_info :
    public super,
    public rlnc_info_args
{
    size_t m_symbols;
    size_t m_symbol_size;

  protected:
    size_t rlnc_symbols()
    {
        return m_symbols;
    }

    size_t rlnc_symbol_size()
    {
        return m_symbol_size;
    }

  public:
    template<typename... Args> explicit
    rlnc_info(const Args&... args)
        : super(args...),
          m_symbols(kwget(symbols, 100, args...)),
          m_symbol_size(kwget(symbol_size, 1450, args...))
    {}
};
