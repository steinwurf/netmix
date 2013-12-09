#pragma once

#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <string>
#include <system_error>
#include <vector>

template<class buffer_ptr>
class sock
{
    typedef buffer_ptr buf_ptr;

  protected:
    virtual int fd() = 0;

    virtual struct sockaddr *sa_send()
    {
        return NULL;
    }

    virtual struct sockaddr *sa_recv()
    {
        return NULL;
    }

    virtual uint32_t sa_send_len()
    {
        return 0;
    }

    virtual uint32_t *sa_recv_len()
    {
        return NULL;
    }

  public:
    sock()
    {}

    void sock_send_buf(int size)
    {
        if (setsockopt(fd(), SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) < 0)
            throw std::system_error(errno, std::system_category(),
                                    "unable to set buffer size");
    }

    bool read_pkt(buf_ptr &buf)
    {
        return read_pkt(buf, buf->max_len());
    }

    bool read_pkt(buf_ptr &buf, size_t len, size_t offset = 0)
    {
        int res = recvfrom(fd(), buf->head() + offset, len, 0, sa_recv(),
                           sa_recv_len());

        if (res > 0) {
            buf->push(res);
            return true;
        }

        if (res == 0)
            throw std::runtime_error("connection closed");

        if (res < 0 && errno == EAGAIN)
            return false;

        throw std::system_error(errno, std::system_category(),
                                "unable to recv packet");
    }

    bool write_pkt(buf_ptr &buf)
    {
        int res = sendto(fd(), buf->head(), buf->len(), 0,
                         sa_send(), sa_send_len());

        if (res > 0) {
            buf->pull(res);
            return true;
        }

        if (res < 0 && errno == EAGAIN)
            return false;

        throw std::system_error(errno, std::system_category(),
                                "unable to write packet");
    }
};
