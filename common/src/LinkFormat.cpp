/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 */

#include "eddie.h"

#include <string>
#include <map>

#define COAP_DEFAULT_SCHEME   "coap"
#define COAP_DEFAULT_PORT   "5683"
#define COAPS_DEFAULT_PORT   "5684"

#define ISEQUAL_CI(a,b) \
   ((a) == (b) || (islower(b) && ((a) == ((b) - 0x20))))

/*
 * The following function was taken and modified from the libcoap library licensed under
 * BSD-2-clause (see LICENSES/BSD-2-clause.txt file)
 * Copyright (c) 2010--2021, Olaf Bergmann and others
 */
int coap_split_uri(std::string &str_var, std::string &host, std::string &port, std::string &path) {
    const char *p, *q;
    size_t len = str_var.length();
    int res = 0;

    if (str_var.empty())
        return -1;

    port = COAP_DEFAULT_PORT;

    /* search for scheme */
    p = str_var.c_str();
    if (*p == '/') {
        q = p;
        goto path;
    }

    q = COAP_DEFAULT_SCHEME;
    while (len && *q && ISEQUAL_CI(*p, *q)) {
        ++p; ++q; --len;
    }

    /* If q does not point to the string end marker '\0', the schema
     * identifier is wrong. */
    if (*q) {
        res = -1;
        goto error;
    }

    /* There might be an additional 's', indicating the secure version: */
    if (len && (*p == 's')) {
        ++p;
        --len;
        port = COAPS_DEFAULT_PORT;
    }

    /* There might be and addition "+tcp", indicating reliable transport: */
    if (len>=4 && p[0] == '+' && p[1] == 't' && p[2] == 'c' && p[3] == 'p' ) {
        p += 4;
        len -= 4;
    }
    q = "://";
    while (len && *q && *p == *q) {
        ++p; ++q; --len;
    }

    if (*q) {
        res = -2;
        goto error;
    }

    /* p points to beginning of Uri-Host */
    q = p;
    if (len && *p == '[') {        /* IPv6 address reference */
        ++p;

        while (len && *q != ']') {
            ++q; --len;
        }

        if (!len || *q != ']' || p == q) {
            res = -3;
            goto error;
        }

        host = std::string(p, q-p);
        ++q; --len;
    } else {                        /* IPv4 address or FQDN */
        while (len && *q != ':' && *q != '/' && *q != '?') {
            ++q;
            --len;
        }

        if (p == q) {
            res = -3;
            goto error;
        }

        host = std::string(p, q-p);
    }

    /* check for Uri-Port */
    if (len && *q == ':') {
        p = ++q;
        --len;

        while (len && isdigit(*q)) {
            ++q;
            --len;
        }

        if (p < q) {                /* explicit port number given */
            int uri_port = 0;

            while (p < q)
                uri_port = uri_port * 10 + (*p++ - '0');

            /* check if port number is in allowed range */
            if (uri_port > 65535) {
                res = -4;
                goto error;
            }

            port = std::to_string(uri_port);
        }
    }

    path:                 /* at this point, p must point to an absolute path */

    if (!len)
        goto end;

    if (*q == '/') {
        p = ++q;
        --len;

        while (len && *q != '?') {
            ++q;
            --len;
        }

        if (p < q) {
            path = std::string(p, q-p);
            p = q;
        }
    }

    /* Uri_Query */
    if (len && *p == '?') {
        ++p;
        --len;
        len = 0;
    }

    end:
    return len ? -1 : 0;

    error:
    return res;
}

std::vector<Link> parse_link_format(std::string link_format) {
    std::vector<Link> links;

    std::string::iterator end = link_format.end();
    for ( std::string::iterator it=link_format.begin(); it!=end; ++it) {
        if (*it == ' ') continue;
        if (*it != '<') {
            // TODO: throw exception
            break;
        }

        std::string uri;
        for (++it; (it != end) && (*it != '>'); ++it) {
            uri += *it;
        }

        std::map<std::string, std::string> link_params;
        
        for (++it; (it != end) && (*it != ','); ++it) {
            if (*it == ' ' || *it == ';') continue;

            std::string key;
            for (;(it != end) && (*it != '=') && (*it != ';') && (*it != ','); ++it)
                key += *it;

            if (it == end || *it == ',') break;
            it++;

            std::string value_str;
            if (*it == '"') {
                // Quoted string.
                char lastc = *it;
                for (++it; it != end; ++it) {
                    if (*it == '"') {
                        if (lastc != '\\') {
                            // End of the string.
                            break;
                        }
                        // Quoted quote mark.
                        value_str.pop_back();
                    }
                    value_str += *it;
                    lastc = *it;
                }
            } else {
                // Unquoted string.
                for (; (it != end) && (*it != ';') && (*it != ',') && (*it != ' '); ++it) {
                    value_str += *it;
                }
            }

            link_params.insert(std::pair<std::string, std::string>(key, value_str));

            if (it == end || *it == ',') break;
        }

        std::string host, port, path;
        coap_split_uri(uri, host, port, path);

        links.push_back({host, port, path, link_params});
        if (it == end) break;
    }

    return links; 
}