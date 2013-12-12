#pragma once

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <net/if.h>
#include <arpa/inet.h>
#include <getopt.h>

enum {
    e1 = 0,
    e2,
    e3,
    e4,
    e_max
};

struct side_args
{
    char interface[IFNAMSIZ] = "lo";
    char *neighbor     = NULL;
    char *helper       = NULL;
    char *two_hop      = NULL;
    double errors[e_max]    = {0.1, 0.1, 0.5, 0.75};
};

struct args
{
    struct side_args src;
    struct side_args dst;
    char   address[20] = "localhost";
    char   port[20]    = "15887";
    size_t min_size    = 0;
    size_t max_size    = 0;
    size_t symbols     = 100;
    size_t symbol_size = 1450;
    ssize_t timeout    = 20;
    int    help        = 0;
};

static struct args args;

static struct option options[] = {
    {"src_interface", required_argument, NULL, 1},
    {"dst_interface", required_argument, NULL, 2},
    {"src_neighbor",  required_argument, NULL, 3},
    {"dst_neighbor",  required_argument, NULL, 4},
    {"src_helper",    required_argument, NULL, 5},
    {"dst_helper",    required_argument, NULL, 6},
    {"src_two_hop",   required_argument, NULL, 7},
    {"dst_two_hop",   required_argument, NULL, 8},
    {"symbols",       required_argument, NULL, 9},
    {"symbol_size",   required_argument, NULL, 10},
    {"src_e1",        required_argument, NULL, 11},
    {"dst_e1",        required_argument, NULL, 12},
    {"src_e2",        required_argument, NULL, 13},
    {"dst_e2",        required_argument, NULL, 14},
    {"src_e3",        required_argument, NULL, 15},
    {"dst_e3",        required_argument, NULL, 16},
    {"src_e4",        required_argument, NULL, 17},
    {"dst_e4",        required_argument, NULL, 18},
    {"timeout",       required_argument, NULL, 19},
    {"min_size",      required_argument, NULL, 20},
    {"max_size",      required_argument, NULL, 21},
    {"address",       required_argument, NULL, 22},
    {"port",          required_argument, NULL, 23},
    {"help",          no_argument, &args.help, 1},
    {0}
};

#define OPTIONS_NUM sizeof(options)/sizeof(options[0]) - 1

static const char *args_desc[OPTIONS_NUM] = {
    "interface to receive packets on",
    "interface to send packet on",
    "address of source neighbor",
    "address of destination neighbor",
    "address of source helper",
    "address of destination helper",
    "address of source two-hop neighbor",
    "address of destination two-hop neighbor",
    "number of symbols in one block",
    "number of bytes in one symbol",
    "e1 for source neighbor",
    "e1 for destination neighbor",
    "e2 for source helper",
    "e2 for destination helper",
    "e3 for source helper",
    "e3 for destination helper",
    "e4 for source two-hop",
    "e4 for destination two-hop",
    "milliseconds before retransmitting",
    "minimum packet size to read",
    "maximum packet size to read",
    "local address to bind to",
    "local port to bind to",
    "show this help",
};

static inline int args_usage(char *name)
{
    struct option *opt;
    const char *desc;
    size_t i;

    printf("Usage: %s [options]\n", name);
    printf("\n");
    printf("Available options:\n");

    for (i = 0; i < OPTIONS_NUM; ++i) {
        opt = options + i;
        desc = args_desc[i];

        printf("  --%-20s %s\n", opt->name, desc);
    }

    return EXIT_SUCCESS;
}

static inline int parse_args(int argc, char **argv)
{
    signed char c;

    while ((c = getopt_long_only(argc, argv, "", options, NULL)) != -1) {
        switch (c) {
            case 1:
                strncpy(args.src.interface, optarg, IFNAMSIZ);
                break;
            case 2:
                strncpy(args.dst.interface, optarg, IFNAMSIZ);
                break;
            case 3:
                args.src.neighbor = optarg;
                break;
            case 4:
                args.dst.neighbor = optarg;
                break;
            case 5:
                args.src.helper = optarg;
                break;
            case 6:
                args.dst.helper = optarg;
                break;
            case 7:
                args.src.two_hop = optarg;
                break;
            case 8:
                args.dst.two_hop = optarg;
                break;
            case 9:
                args.symbols = atoi(optarg);
                break;
            case 10:
                args.symbol_size = atoi(optarg);
                break;
            case 11:
                args.src.errors[0] = strtod(optarg, NULL);
                break;
            case 12:
                args.dst.errors[0] = strtod(optarg, NULL);
                break;
            case 13:
                args.src.errors[1] = strtod(optarg, NULL);
                break;
            case 14:
                args.dst.errors[1] = strtod(optarg, NULL);
                break;
            case 15:
                args.src.errors[2] = strtod(optarg, NULL);
                break;
            case 16:
                args.dst.errors[2] = strtod(optarg, NULL);
                break;
            case 17:
                args.src.errors[3] = strtod(optarg, NULL);
                break;
            case 18:
                args.dst.errors[3] = strtod(optarg, NULL);
                break;
            case 19:
                args.timeout = atoi(optarg);
                break;
            case 20:
                args.min_size = atoi(optarg);
                break;
            case 21:
                args.max_size = atoi(optarg);
                break;
            case 22:
                strncpy(args.address, optarg, 20);
                break;
            case 23:
                strncpy(args.port, optarg, 20);
                break;
            case '?':
                args_usage(argv[0]);
                return -1;
        }
    }

    for (ssize_t index = optind; index < argc; index++)
        printf("Non-option argument %s\n", argv[index]);

    return 0;
}
