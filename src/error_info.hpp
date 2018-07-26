#pragma once

#include "kwargs.hpp"

struct error_info_args
{
    typedef double error_type;
    typedef std::vector<double> errors_type;

    static const Kwarg<errors_type> errors;
    static const Kwarg<double> overshoot;
};

decltype(error_info_args::errors) error_info_args::errors;
decltype(error_info_args::overshoot) error_info_args::overshoot;

template<class super>
class error_info : public super, public error_info_args
{
  private:
    errors_type m_errors;
    double m_overshoot;

  protected:
    enum error_num {
        e1 = 0,
        e2,
        e3,
        e4,
        e_max
    };

    error_type error(error_num e)
    {
        return m_errors[e];
    }

    errors_type &errors_info()
    {
        return m_errors;
    }

    double overshoot_ratio() const
    {
        return m_overshoot;
    }

  public:
    template<typename... Args>
    error_info(const Args&... args)
        : super(args...),
          m_errors(kwget(errors, errors_type(e_max, .0), args...)),
          m_overshoot(kwget(overshoot, 1.0, args...))
    {}
};
