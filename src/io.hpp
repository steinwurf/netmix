#pragma once

#include <sys/epoll.h>
#include <fcntl.h>
#include <functional>
#include <system_error>
#include <vector>

class io
{
    typedef std::function<void(int)> io_cb;

    struct epoll_info {
        io_cb read = NULL;
        io_cb write = NULL;
        struct epoll_event ev;

        epoll_info() {}
        epoll_info(io_cb r, io_cb w, struct epoll_event e)
            : read(r), write(w), ev(e)
        {}
        bool operator!() const { return !read && !write; }
    };

    std::vector<struct epoll_info> m_epoll_info;
    static const size_t MAX_EVENTS = 2;
    struct epoll_event m_events[MAX_EVENTS], *m_event;
    int m_epoll;

    static void set_non_blocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);

        if (flags < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to receive file descriptor flags");

        flags |= O_NONBLOCK;

        if (fcntl(fd, F_SETFL, flags) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to set file descriptor flags");
    }

  public:
    io()
    {
        if ((m_epoll = epoll_create(1)) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to create epoll file descriptor");
    }

    void disable_read(int fd)
    {
        struct epoll_event *ev;
        size_t max = fd + 1;

        if (m_epoll_info.size() < max || !m_epoll_info[fd]) {
            std::cerr << "tried to disable unknown fd: " << fd << std::endl;
            return;
        }

        ev = &m_epoll_info[fd].ev;
        ev->events &= ~EPOLLIN;

        if (epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, ev) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to modify file descriptor in epoll");
    }

    void enable_read(int fd)
    {
        struct epoll_event *ev;
        size_t max = fd + 1;

        if (m_epoll_info.size() < max || !m_epoll_info[fd]) {
            std::cerr << "tried to enable unknown fd: " << fd << std::endl;
            return;
        }

        ev = &m_epoll_info[fd].ev;
        ev->events |= EPOLLIN;

        if (epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, ev) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to modify file descriptor in epoll");
    }

    void disable_write(int fd)
    {
        struct epoll_event *ev;
        size_t max = fd + 1;

        if (m_epoll_info.size() < max || !m_epoll_info[fd]) {
            std::cerr << "tried to disable unknown fd: " << fd << std::endl;
            return;
        }

        ev = &m_epoll_info[fd].ev;
        ev->events &= ~EPOLLOUT;

        if (epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, ev) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to modify file descriptor in epoll");
    }

    void enable_write(int fd)
    {
        struct epoll_event *ev;
        size_t max = fd + 1;

        if (m_epoll_info.size() < max || !m_epoll_info[fd]) {
            std::cerr << "tried to enable unknown fd: " << fd << std::endl;
            return;
        }

        ev = &m_epoll_info[fd].ev;
        ev->events |= EPOLLOUT;

        if (epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, ev) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to modify file descriptor in epoll");
    }

    void add_cb(int fd, io_cb read, io_cb write)
    {
        struct epoll_event *ev;
        size_t max = fd + 1;

        set_non_blocking(fd);

        if (m_epoll_info.size() < max)
            m_epoll_info.resize(max);

        m_epoll_info[fd] = {read, write, {0}};
        ev = &m_epoll_info[fd].ev;
        ev->data.fd = fd;
        ev->events |= read ? EPOLLIN : 0;
        ev->events |= write ? EPOLLOUT : 0;

        if (epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, ev) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to add file descriptor to epoll");
    }

    void del_cb(int fd)
    {
        size_t max = fd + 1;

        if (m_epoll_info.size() < max) {
            std::cerr << "tried to delete unknown fd: " << fd << std::endl;
            return;
        }

        m_epoll_info[fd] = {NULL, NULL, {0}};

        if (epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, NULL) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to delete file descriptor from epoll");
    }

    int wait(int timeout = -1)
    {
        int nfds = epoll_wait(m_epoll, m_events, MAX_EVENTS, timeout);

        if (nfds == -1 && errno == EINTR)
            return 1;

        if (nfds == 0)
            return 0;

        if (nfds == -1)
            throw std::system_error(errno, std::system_category(),
                                    "error from epoll_wait");

        for (ssize_t n = 0; n < nfds; ++n) {
            m_event = &m_events[n];
            int fd = m_event->data.fd;
            struct epoll_info *info = &m_epoll_info[fd];

            try {
                if (m_events->events & EPOLLIN && info->read)
                    info->read(fd);

                if (m_events->events & EPOLLOUT && info->write)
                    info->write(fd);

            } catch (std::bad_function_call &e) {
                std::cerr << "bad function ptr for fd: " << fd << std::endl;
                return -1;
            }

            if (m_events->events & EPOLLERR) {
                std::cerr << "fd: " << fd << std::endl;
                throw std::runtime_error("error on fd");
            }

            if (m_events->events & EPOLLHUP) {
                std::cerr << "fd hangup: " << fd << std::endl;
                del_cb(fd);
            }
        }

        return nfds;
    }
};
