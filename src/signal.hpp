#pragma once

#include <csignal>
#include <cstring>
#include <system_error>
#include <functional>

class signal
{
    static bool m_running;

    static void install_signals()
    {
        struct sigaction act;

        memset(&act, 0, sizeof(act));
        act.sa_handler = &handler;

        if (sigaction(SIGTERM, &act, 0))
            throw std::system_error(errno, std::system_category(),
                                    "unable to install signal");

        if (sigaction(SIGINT, &act, 0))
            throw std::system_error(errno, std::system_category(),
                                    "unable to install signal");
    }

    static void handler(int /*sig*/)
    {
        m_running = false;
    }

  public:
    signal()
    {
        install_signals();
    }

    static bool running()
    {
        return m_running;
    }
};

bool signal::m_running = true;
