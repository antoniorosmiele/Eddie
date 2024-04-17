/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#ifndef COMMON_H
#define COMMON_H

#include "eddie.h"

#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <list>
#include <coap3/coap.h>
#include <cstdio>
#include <regex>

#define LOG_DBG(_str, ...) printf(_str "\n", ##__VA_ARGS__)
#define LOG_INFO(_str, ...) printf(_str "\n", ##__VA_ARGS__)
#define LOG_ERR(_str, ...) printf(_str "\n", ##__VA_ARGS__)

/**
 * Resolve coap_address_t from destination host and port.
 * @param host Ip or host name to be resolved
 * @param service Port number of the host to be resolved
 * @param dst The result from the address resolve is written in this parameter
 * @return Size of the resolved address and stored in dst.
 */
int resolve_address(const char *host, const char *service, coap_address_t *dst);

/**
 * Get ip of this node by looking at the addresses assigned to the network interfaces
 * @return Returns the first ipv6 found in the first non loopback network interface
 */
std::string get_local_node_ip();

/**
 * Convert a libcoap method into eddie's method_t
 * @param method coap method
 * @return The corresponding eddie method_t
 */
method_t coap_method_to_method(coap_pdu_code_t method);

/**
 * Convert a eddie's method_t into the corresponding libcoap method
 * @param method the method to convert into libcoap method
 * @return libcoap method
 */
coap_pdu_code_t method_to_coap_method(method_t method);

/**
 * Check if dst is equal to src or is its prefix
 * @param src source string
 * @param dst destination string
 * @param is_prefix if this is true, check if dst is a prefix of src
 * @return True if there is a match, false otherwise
 */
bool match(const std::string &src, const std::string &dst, int is_prefix);

/**
 * Given ip and port number, make URL in the format "coap://<ip>:<port number>
 * @param ip IP address
 * @param port Port number
 * @return The resulting url
 */
std::string ip_to_url(const std::string& ip, const std::string& port);

/**
 * Split a string in different substrings, given the delimiter character.
 * @param s             the string to analyze
 * @param delimiter     the delimiter character of every substring
 */
std::vector<std::string> split(const std::string &s, char delimiter);

/**
 * Percent-encode a url
 * @param value String to encode
 * @return The resulting percent encoded url
 */
std::string url_encode(const std::string &value);

/**
 * Structure that holds responses received by the client.
 * @tparam Key      message_id of the response
 * @tparam Value    pointer to the response in memory
 */
template<class Key, class Value>
class ThreadSafeMap {
    std::unordered_map<Key, Value> c_;

public:
    std::mutex m_;

    Value get(Key const &k) {
        std::unique_lock<decltype(m_)> lock(m_);
        Value elem = c_[k];
        return elem;
    }

    bool exists(Key const &k) {
        std::unique_lock<decltype(m_)> lock(m_);
        return c_.find(k) != c_.end();
    }

    std::list<Value> get_all() {
        std::unique_lock<decltype(m_)> lock(m_);
        std::list<Value> return_list;

        for(auto const& imap: c_)
            return_list.push_back(imap.second);

        return return_list;
    }

    template<class Value2>
    void set(Key const &k, Value2 &&v) {
        std::unique_lock<decltype(m_)> lock(m_);
        c_[k] = std::forward<Value2>(v);
    }

    void erase(Key const &k) {
        std::unique_lock<decltype(m_)> lock(m_);
        c_.erase(k);
    }

    void delete_all() {
        std::unique_lock<decltype(m_)> lock(m_);
        c_.erase(c_.begin(), c_.end());
    }

    int empty() {
        std::unique_lock<decltype(m_)> lock(m_);
        return c_.empty();
    }
};

template<class Key, class Value>
class ThreadSafeStorage {
    std::unordered_multimap<Key, Value> mm_;
    std::unordered_set<Key> s_;

public:
    std::mutex m_;

    std::list<Key> get_valid_keys() {
        std::unique_lock<decltype(m_)> lock(m_);

        std::list<Key> return_container;
        for (auto key: s_) return_container.push_back(key);

        return return_container;
    }
    /**
     * Add a new key to the set of valid keys
     * @param k the key to add
     */
    void insert_valid_key(Key const &k) {
        std::unique_lock<decltype(m_)> lock(m_);
        s_.insert(k);
    }

    /**
     * Remove a key from the set of valid keys.
     * As a side effect, all pairs in the unordered_multimap are removed
     *
     * @param k the key to remove
     */
    void remove_valid_key (Key const &k) {
        std::unique_lock<decltype(m_)> lock(m_);
        s_.erase(k);
        mm_.erase(k);
    }

    /**
     * Check if the provided key is contained in the set of valid keys.
     * @param k     the key to find in the set of valid keys
     * @return      true if the key is found, false otherwise
     */
    int contains_key(Key const &k) {
        std::unique_lock<decltype(m_)> lock(m_);
        auto result = s_.find(k);
        return result != s_.end();
    }

    template<class Value2>
    void insert_new_message(Key const &k, Value2 &&v) {
        std::unique_lock<decltype(m_)> lock(m_);

        if (s_.find(k) != s_.end()) {
            mm_.insert(std::make_pair(k, v));
        }
    }

    std::list<Value> get(Key const &k) {
        std::unique_lock<decltype(m_)> lock(m_);
        std::list<Value> return_list;

        if (s_.find(k) != s_.end()) {
            auto items = mm_.equal_range(k);

            for (auto item = items.first; item != items.second; ++item) {
                return_list.push_back(item->second);
            }
        }

        return return_list;
    }

    void delete_all() {
        std::unique_lock<decltype(m_)> lock(m_);
        s_.erase(s_.begin(), s_.end());
        mm_.erase(mm_.begin(), mm_.end());
    }

    int empty() {
        std::unique_lock<decltype(m_)> lock(m_);
        return mm_.empty();
    }
};

#endif