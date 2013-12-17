#pragma once

#include <vector>

#include "kwargs.hpp"

template<typename errors_type>
class budgets
{
    enum {
        e1 = 0,
        e2,
        e3,
        e4
    };

  protected:
    static inline bool r_test(errors_type &errors)
    {
        return (1 - errors[e2]) < (errors[e3] - errors[e1]*errors[e3]);
    }

    static inline double r_val(size_t g, errors_type &errors)
    {
        double nom, denom;

        if (r_test(errors)) {
            return 1.0/(errors[e3] - errors[e1]*errors[e3]);
        } else {
            nom = g - g*errors[e2] - g*errors[e3] + g*errors[e1]*errors[e3];
            denom = 1 + errors[e1]*errors[e2]*errors[e3];
            denom -= errors[e2];
            denom -= errors[e1]*errors[e3];
            return nom/denom;
        }
    }

    static inline double helper_threshold(size_t g, errors_type &errors)
    {
        double r = r_val(g, errors);

        return r - r*errors[e1];
    }

    static inline double helper_budget(size_t g, errors_type &errors)
    {
        double nom, denom, r = r_val(g, errors);

        nom = errors[e3]*r - r + g;
        denom = 2 - errors[e2] - errors[e3];

        return nom/denom;
    }

    static inline double helper_credits(size_t g, errors_type &errors)
    {
        return 1/(1 - errors[e1]);
    }

    static inline double source_credits(size_t g, errors_type &errors)
    {
        return 1/(1 - errors[e3]*errors[e1]);
    }

    static inline double source_budget(size_t g, errors_type &errors)
    {
        double nom, denom, r = r_val(g, errors);

        nom = g + r - r*errors[e2];
        denom = 2 - errors[e3] - errors[e2];

        return nom/denom;
    }

    static inline double relay_credits(size_t g, errors_type &errors)
    {
        return 1;
        return 1/(1 - errors[e3]*errors[e1]);
    }

    static inline double relay_budget(size_t g, errors_type &errors)
    {
        return source_budget(g, errors) - g*(1 - errors[e4]);
    }
};

template<typename errors_type>
class budgets_base : public budgets<errors_type>
{
  protected:
    double m_credits;
    double m_threshold;
    double m_budget = 0;
    double m_max;

  protected:
    bool decrease_budget()
    {
        m_budget--;
        return m_budget >= 1;
    }

    void increase_budget()
    {
        m_budget += m_credits;
    }

    double threshold()
    {
        return m_threshold;
    }

    double budget()
    {
        return m_budget;
    }
};

template<class super>
class source_budgets : public super, public budgets_base<typename super::errors_type>
{
    typedef budgets_base<typename super::errors_type> base;

  protected:
    template<typename... Args> explicit
    source_budgets(const Args&... args)
        : super(args...)
    {
        base::m_credits = base::source_credits(super::rlnc_symbols(),
                                               super::errors_info());
        base::m_max = base::source_budget(super::rlnc_symbols(),
                                          super::errors_info());
    }

    void increment(size_t b = 0)
    {
        super::increment();
        base::m_budget = 0;
    }
};

template<class super>
class helper_budgets : public super, public budgets_base<typename super::errors_type>
{
    typedef budgets_base<typename super::errors_type> base;

  protected:
    template<typename... Args> explicit
    helper_budgets(const Args&... args)
        : super(args...)
    {
        base::m_credits = base::helper_credits(super::rlnc_symbols(),
                                               super::errors_info());
        base::m_max = base::helper_budget(super::rlnc_symbols(),
                                          super::errors_info());
        base::m_threshold = base::helper_threshold(super::rlnc_symbols(),
                                                   super::errors_info());
    }

    void increment(size_t b = 0)
    {
        (void) b;
        super::increment();
        base::m_budget = 0;
    }
};

template<class super>
class relay_budgets : public super, public budgets_base<typename super::errors_type>
{
    typedef budgets_base<typename super::errors_type> base;

  protected:
    template<typename... Args> explicit
    relay_budgets(const Args&... args)
        : super(args...)
    {
        base::m_credits = base::relay_credits(super::rlnc_symbols(),
                                              super::errors_info());
        base::m_max = base::relay_budget(super::rlnc_symbols(),
                                         super::errors_info());
    }

    void increment(size_t b = 0)
    {
        (void) b;
        super::increment();
        base::m_budget = 0;
    }
};
