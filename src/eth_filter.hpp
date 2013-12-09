#pragma once

#include <system_error>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <linux/filter.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include "kwargs.hpp"

class eth_filter_base
{
    typedef std::vector<uint8_t> buf_type;

    struct eth_dual {
        uint16_t head;
        uint32_t tail;
    } __attribute__((packed));

    int m_fd;
    buf_type m_buf;
    uint16_t m_default = 0xffff; /* accept by default */

  protected:
    enum payload_type : uint8_t {
        type_none = 0,
        type_raw  = 1,
        type_enc  = 2,
        type_rec  = 3,
        type_hlp  = 4,
        type_ack  = 5,
        type_done = 6,
    };

    eth_filter_base(int fd)
        : m_fd(fd)
    {}

    void filter_append(struct sock_filter *bpf, size_t len)
    {
        uint8_t *b = reinterpret_cast<uint8_t *>(bpf);

        m_buf.insert(m_buf.end(), b, b + len);
    }

    void filter_apply()
    {
        uint16_t len;
        struct sock_fprog filter;
        struct sock_filter bpf;

        if (m_buf.size() == 0)
            return;

        bpf = { BPF_RET + BPF_K, 0, 0, m_default }; /* 0: ignore packet by default */
        filter_append(&bpf, sizeof(bpf));

        auto buf = reinterpret_cast<struct sock_filter *>(&m_buf[0]);
        len = m_buf.size()/sizeof(*buf);

        filter = {
            .len = len,
            .filter = buf
        };

        if (setsockopt(m_fd, SOL_SOCKET, SO_ATTACH_FILTER, &filter,
                       sizeof(filter)) < 0)
            throw std::system_error(errno,std::system_category(),
                                    "unable to attach packet filter");
    }

    void filter_add(const uint8_t *src)
    {
        auto s = reinterpret_cast<const struct eth_dual *>(src);

        if (!src)
            return;

        struct sock_filter bpf[] = {
            { BPF_LD + BPF_W + BPF_ABS, 0, 0, 8 },               /* 0: point to byte 8-11  */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 3, htonl(s->tail) }, /* 1: jump to next if not equal */
            { BPF_LD + BPF_H + BPF_ABS, 0, 0, 6 },               /* 2: point to byte 6-7 */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 1, htons(s->head) }, /* 3: jump to next if not equal */
            { BPF_RET + BPF_K, 0, 0, 0xffff},                    /* 4: accept packet */
        };

        filter_append(bpf, sizeof(bpf));
        m_default = 0x0000; /* ignore by default */
    }

    void filter_add(const uint8_t *src, uint8_t type)
    {
        auto s = reinterpret_cast<const struct eth_dual *>(src);

        if (!src)
            return;

        struct sock_filter bpf[] = {
            { BPF_LD + BPF_W + BPF_ABS, 0, 0, 8 },               /* 0: point to byte 8-11  */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 6, htonl(s->tail) }, /* 1: jump to next if not equal */
            { BPF_LD + BPF_H + BPF_ABS, 0, 0, 6 },               /* 2: point to byte 6-7 */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 4, htons(s->head) }, /* 3: jump to next if not equal */
            { BPF_LD + BPF_B + BPF_ABS, 0, 0, 14 },              /* 4: point to type byte */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 1, type },           /* 5: jump to ignore if not equal */
            { BPF_RET + BPF_K, 0, 0, 0xffff},                    /* 6: accept packet */
            { BPF_RET + BPF_K, 0, 0, 0x0000},                    /* 7: ignore packet */
        };

        filter_append(bpf, sizeof(bpf));
        m_default = 0x0000; /* ignore by default */
    }

    void filter_add(const uint8_t *src, uint8_t type1, uint8_t type2)
    {
        auto s = reinterpret_cast<const struct eth_dual *>(src);

        if (!src)
            return;

        struct sock_filter bpf[] = {
            { BPF_LD + BPF_W + BPF_ABS, 0, 0, 8 },               /* 0: point to src[2:6] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 7, htonl(s->tail) }, /* 1: jump to next if not equal */
            { BPF_LD + BPF_H + BPF_ABS, 0, 0, 6 },               /* 2: point to src[0:2] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 5, htons(s->head) }, /* 3: jump to next if not equal */
            { BPF_LD + BPF_B + BPF_ABS, 0, 0, 14 },              /* 4: point to type byte */
            { BPF_JMP + BPF_JEQ + BPF_K, 1, 0, type1 },          /* 5: jump to accept if equal */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 1, type2 },          /* 5: jump to ignore if not equal */
            { BPF_RET + BPF_K, 0, 0, 0xffff},                    /* 6: accept packet */
            { BPF_RET + BPF_K, 0, 0, 0x0000},                    /* 7: ignore packet */
        };

        filter_append(bpf, sizeof(bpf));
        m_default = 0x0000; /* ignore by default */
    }

    void filter_add(const uint8_t *src, const uint8_t *dst)
    {
        auto s = reinterpret_cast<const struct eth_dual *>(src);
        auto d = reinterpret_cast<const struct eth_dual *>(dst);

        if (!src || !dst)
            return;

        struct sock_filter bpf[] = {
            { BPF_LD + BPF_W + BPF_ABS, 0, 0, 2 },                /*  0: point to dst[2:6] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 7, htonl(d->tail) }, /*  1: jump to next if not eq */
            { BPF_LD + BPF_H + BPF_ABS, 0, 0, 0 },                /*  2: point to dst[0:2] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 5, htons(d->head) },  /*  3: jump to next if not eq */
            { BPF_LD + BPF_W + BPF_ABS, 0, 0, 8 },                /*  4: point to src[2:6] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 3, htonl(s->tail) },  /*  5: jump to next if not eq */
            { BPF_LD + BPF_H + BPF_ABS, 0, 0, 6 },                /*  6: point to src[0:2] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 1, htons(s->head) },  /*  7: jump to next if not eq */
            { BPF_RET + BPF_K, 0, 0, 0xffff},                     /* 10: accept packet */
        };

        filter_append(bpf, sizeof(bpf));
        m_default = 0x0000; /* ignore by default */
    }

    void filter_add(const uint8_t *src, const uint8_t *dst, uint8_t type)
    {
        auto s = reinterpret_cast<const struct eth_dual *>(src);
        auto d = reinterpret_cast<const struct eth_dual *>(dst);

        if (!src || !dst)
            return;

        struct sock_filter bpf[] = {
            { BPF_LD + BPF_W + BPF_ABS, 0, 0, 2 },                /*  0: point to dst[2:6] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 10, htonl(d->tail) }, /*  1: jump to next if not eq */
            { BPF_LD + BPF_H + BPF_ABS, 0, 0, 0 },                /*  2: point to dst[0:2] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 8, htons(d->head) },  /*  3: jump to next if not eq */
            { BPF_LD + BPF_W + BPF_ABS, 0, 0, 8 },                /*  4: point to src[2:6] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 6, htonl(s->tail) },  /*  5: jump to next if not eq */
            { BPF_LD + BPF_H + BPF_ABS, 0, 0, 6 },                /*  6: point to src[0:2] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 4, htons(s->head) },  /*  7: jump to next if not eq */
            { BPF_LD + BPF_B + BPF_ABS, 0, 0, 14 },               /*  8: point to type byte */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 1, type },            /*  9: jump to ignore if not eq */
            { BPF_RET + BPF_K, 0, 0, 0xffff},                     /* 10: accept packet */
            { BPF_RET + BPF_K, 0, 0, 0x0000},                     /* 11: ignore packet */
        };

        filter_append(bpf, sizeof(bpf));
        m_default = 0x0000; /* ignore by default */
    }

    void filter_add(const uint8_t *src, const uint8_t *dst, uint8_t type1, uint8_t type2)
    {
        auto s = reinterpret_cast<const struct eth_dual *>(src);
        auto d = reinterpret_cast<const struct eth_dual *>(dst);

        if (!src || !dst)
            return;

        struct sock_filter bpf[] = {
            { BPF_LD + BPF_W + BPF_ABS, 0, 0, 2 },                /*  0: point to dst[2:6] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 11, htonl(d->tail) }, /*  1: jump to next if not eq */
            { BPF_LD + BPF_H + BPF_ABS, 0, 0, 0 },                /*  2: point to dst[0:2] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 9, htons(d->head) },  /*  3: jump to next if not eq */
            { BPF_LD + BPF_W + BPF_ABS, 0, 0, 8 },                /*  4: point to src[2:6] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 7, htonl(s->tail) },  /*  5: jump to next if not eq */
            { BPF_LD + BPF_H + BPF_ABS, 0, 0, 6 },                /*  6: point to src[0:2] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 5, htons(s->head) },  /*  7: jump to next if not eq */
            { BPF_LD + BPF_B + BPF_ABS, 0, 0, 14 },               /*  8: point to type byte */
            { BPF_JMP + BPF_JEQ + BPF_K, 1, 0, type1 },           /*  9: jump to accept if equal */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 1, type2 },           /* 10: jump to ignore if not eq */
            { BPF_RET + BPF_K, 0, 0, 0xffff},                     /* 11: accept packet */
            { BPF_RET + BPF_K, 0, 0, 0x0000},                     /* 12: ignore packet */
        };

        filter_append(bpf, sizeof(bpf));
        m_default = 0x0000; /* ignore by default */
    }

    void filter_add(const uint8_t *src, const uint8_t *dst, uint8_t type1, uint8_t type2, uint8_t type3)
    {
        auto s = reinterpret_cast<const struct eth_dual *>(src);
        auto d = reinterpret_cast<const struct eth_dual *>(dst);

        if (!src || !dst)
            return;

        struct sock_filter bpf[] = {
            { BPF_LD + BPF_W + BPF_ABS, 0, 0, 2 },                /*  0: point to dst[2:6] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 12, htonl(d->tail) }, /*  1: jump to next if not eq */
            { BPF_LD + BPF_H + BPF_ABS, 0, 0, 0 },                /*  2: point to dst[0:2] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 10, htons(d->head) },  /*  3: jump to next if not eq */
            { BPF_LD + BPF_W + BPF_ABS, 0, 0, 8 },                /*  4: point to src[2:6] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 8, htonl(s->tail) },  /*  5: jump to next if not eq */
            { BPF_LD + BPF_H + BPF_ABS, 0, 0, 6 },                /*  6: point to src[0:2] */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 6, htons(s->head) },  /*  7: jump to next if not eq */
            { BPF_LD + BPF_B + BPF_ABS, 0, 0, 14 },               /*  8: point to type byte */
            { BPF_JMP + BPF_JEQ + BPF_K, 2, 0, type1 },           /*  9: jump to accept if equal */
            { BPF_JMP + BPF_JEQ + BPF_K, 1, 0, type2 },           /* 10: jump to accept if equal */
            { BPF_JMP + BPF_JEQ + BPF_K, 0, 1, type3 },           /* 11: jump to ignore if not eq */
            { BPF_RET + BPF_K, 0, 0, 0xffff},                     /* 11: accept packet */
            { BPF_RET + BPF_K, 0, 0, 0x0000},                     /* 12: ignore packet */
        };

        filter_append(bpf, sizeof(bpf));
        m_default = 0x0000; /* ignore by default */
    }

    void filter_size_max(uint32_t size)
    {
        if (size == 0)
            return;

        struct sock_filter bpf[] = {
            { BPF_LD + BPF_W + BPF_LEN,  0, 0, 0 },      /* 0: load packet length */
            { BPF_JMP + BPF_JGT + BPF_K, 0, 1, size },   /* 1: jump to next if less */
            { BPF_RET + BPF_K,           0, 0, 0x0000 }, /* 2: ignore packet */
        };

        filter_append(bpf, sizeof(bpf));
    }

    void filter_size_min(uint32_t size)
    {
        if (size == 0)
            return;

        struct sock_filter bpf[] = {
            { BPF_LD + BPF_W + BPF_LEN,  0, 0, 0},      /* 0: load packet length */
            { BPF_JMP + BPF_JGE + BPF_K, 1, 0, size},   /* 1: jump to next if greater */
            { BPF_RET + BPF_K,           0, 0, 0x0000}, /* 2: ignore packet */
        };

        filter_append(bpf, sizeof(bpf));
    }
};

struct eth_filter_args
{
    static const Kwarg<size_t> filter_min_size;
    static const Kwarg<size_t> filter_max_size;
};

decltype(eth_filter_args::filter_min_size) eth_filter_args::filter_min_size;
decltype(eth_filter_args::filter_max_size) eth_filter_args::filter_max_size;

template<class super>
class eth_filter_edge :
    public super,
    public eth_filter_base,
    public eth_filter_args
{
  public:
    template<typename... Args> explicit
    eth_filter_edge(const Args&... args)
        : super(args...),
          eth_filter_base(super::fd())
    {
        filter_add(super::neighbor_addr(), super::interface_address());
        filter_apply();
    }
};

template<class super>
class eth_filter_dec :
    public super,
    public eth_filter_base,
    public eth_filter_args
{
  public:
    template<typename... Args> explicit
    eth_filter_dec(const Args&... args)
        : super(args...),
          eth_filter_base(super::fd())
    {
        filter_size_min(kwget(filter_min_size, 0, args...));
        filter_size_max(kwget(filter_max_size, 0, args...));

        /* filter encoded and recoded packets from neighbor to this host */
        filter_add(super::neighbor_addr(), super::interface_address(),
                   super::rlnc_enc, super::rlnc_rec);

        /* filter encoded and recoded packets from two-hop neighbor to neighbor */
        filter_add(super::two_hop_addr(), super::neighbor_addr(),
                   super::rlnc_enc, super::rlnc_rec);

        /* filter helper packets from helper to this host */
        filter_add(super::helper_addr(), super::interface_address(),
                   super::rlnc_hlp);

        filter_apply();
    }
};

template<class super>
class eth_filter_rec :
    public super,
    public eth_filter_base,
    public eth_filter_args
{
  public:
    template<typename... Args> explicit
    eth_filter_rec(const Args&... args)
        : super(args...),
          eth_filter_base(super::fd())
    {
        filter_size_min(kwget(filter_min_size, 0, args...));
        filter_size_max(kwget(filter_max_size, 0, args...));

        /* filter encoded, recoded, and ack packets from neighbor to this host */
        filter_add(super::neighbor_addr(), super::interface_address(),
                   super::rlnc_enc, super::rlnc_rec, super::rlnc_ack);

        /* filter encoded and recoded packets from two-hop neighbor to neighbor */
        filter_add(super::two_hop_addr(), super::neighbor_addr(),
                   super::rlnc_enc, super::rlnc_rec);

        /* filter helper packets from helper to this host */
        filter_add(super::helper_addr(), super::interface_address(),
                   super::rlnc_hlp);

        filter_apply();
    }
};

template<class super>
class eth_filter_enc : public super, public eth_filter_base
{
  public:
    template<typename... Args> explicit
    eth_filter_enc(const Args&... args)
        : super(args...),
          eth_filter_base(super::fd())
    {
        filter_add(super::neighbor_addr(), super::interface_address(),
                   super::rlnc_ack, super::rlnc_stop);

        filter_apply();
    }
};

template<class super>
class eth_filter_hlp : public super, public eth_filter_base
{
  public:
    template<typename... Args> explicit
    eth_filter_hlp(const Args&... args)
        : super(args...),
          eth_filter_base(super::fd())
    {
        /* allow encoded and recoded packets sent from destination to source */
        filter_add(super::destination_addr(), super::source_addr(),
                   super::rlnc_enc, super::rlnc_rec);

        /* allow acks sent from source to destination */
        filter_add(super::source_addr(), super::destination_addr(),
                   super::rlnc_ack, super::rlnc_stop);

        filter_apply();
    }
};
