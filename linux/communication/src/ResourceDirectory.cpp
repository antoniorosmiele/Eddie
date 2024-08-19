/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileCopyrightText: Huawei Inc.
 * SPDX-FileCopyrightText: Politecnico Di Milano
 */

#include "ResourceDirectory.h"
#include "common.h"

#include <coap3/coap.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <arpa/inet.h>
#include <map>
#include <coap3/uri.h>

#define COAP_IP4_MULTICAST "224.0.1.187"
#define COAP_IP6_MULTICAST_LOCAL_LINK "FF02::FD"
#define COAP_IP6_MULTICAST_SITE_LOCAL "FF05::FD"
#define RD_DEFAULT_PORT "5683"
#define DEFAULT_SECTOR_NAME "sectoreddie"

ThreadSafeMap<std::string, coap_resource_t *> *ResourceDirectory::rd_endpoints = new ThreadSafeMap<std::string, coap_resource_t *>();
ThreadSafeMap<std::string, coap_resource_t *> *ResourceDirectory::rd_endpoints_by_ep_d = new ThreadSafeMap<std::string, coap_resource_t *>();

coap_resource_t *ResourceDirectory::create_endpoint(
        const std::unordered_map<std::string, std::string> &params,
        const std::string &data
) {
    coap_resource_t *new_endpoint;

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

    std::ostringstream oss;
    oss << now;

    std::string new_ep_name = std::string("rd/") + oss.str();
    new_endpoint = coap_resource_init(coap_make_str_const(new_ep_name.c_str()), 0);

    for (const auto &param: params) {
        coap_add_attr(new_endpoint, coap_make_str_const(param.first.c_str()),
                      coap_make_str_const(param.second.c_str()), 0);
    }

    auto userdata = malloc(data.size());
    if (!userdata) {
        return nullptr;
    }
    memset(userdata, 0, data.size());
    memcpy(userdata, data.c_str(), data.size());

    coap_resource_set_userdata(new_endpoint, userdata);

    return new_endpoint;
}

std::string ResourceDirectory::add_endpoint_to_rd(const std::string &ep_d_key,
                                                  coap_context_t *context,
                                                  const std::unordered_map<std::string, std::string> &params,
                                                  const std::string &data) {
    auto res = rd_endpoints_by_ep_d->get(ep_d_key);

    if (res) {
        //auto uri = std::string(reinterpret_cast<const char *>(coap_resource_get_uri_path(res)->s));
        //free(coap_resource_get_userdata(res));
        //coap_delete_resource(context, res);
        //rd_endpoints_by_ep_d->erase(ep_d_key);
        //rd_endpoints->erase(uri);

        auto dataOld = std::string((char *) coap_resource_get_userdata(res));

        //LOG_DBG("Old string for RD:%s", dataOld.c_str());

        std::string newData = dataOld.append(data);

        //LOG_DBG("New string for RD:%s", newData.c_str());
        
        auto userdata = malloc(newData.size());
        
        if (!userdata)
        {
            return "";
        }

        memset(userdata, 0, newData.size());
        memcpy(userdata, newData.c_str(), newData.size());
        free(coap_resource_get_userdata(res));
        coap_resource_set_userdata(res,userdata);

        auto ep_name = std::string(reinterpret_cast<const char *>(coap_resource_get_uri_path(res)->s));
        //LOG_DBG("New complete string:%s",std::string((char *) coap_resource_get_userdata(res)).c_str());
        return ep_name;
    }

    auto new_endpoint = create_endpoint(params, data);

    if (new_endpoint == nullptr) {
        return "";
    }

    auto ep_name = std::string(reinterpret_cast<const char *>(coap_resource_get_uri_path(new_endpoint)->s));

    // ADD HANDLERS
    coap_register_handler(new_endpoint, COAP_REQUEST_POST, hnd_post_rd_endpoint);
    coap_register_handler(new_endpoint, COAP_REQUEST_DELETE, hnd_delete_rd_endpoint);

    coap_add_resource(context, new_endpoint);
    rd_endpoints->set(ep_name, new_endpoint);
    rd_endpoints_by_ep_d->set(ep_d_key, new_endpoint);

    return ep_name;
}

std::list<std::string>
ResourceDirectory::endpoints_to_string_list_filtered(std::map<std::string, std::string> filters) {
    std::list<std::string> results;
    std::list<std::string> empty;
    auto endpoints = rd_endpoints->get_all();
    std::unique_lock<decltype(rd_endpoints->m_)> lock(rd_endpoints->m_);

    size_t buf_size = 1000;
    auto *buf = static_cast<unsigned char *>(malloc(buf_size * sizeof(unsigned char)));
    size_t len;

    for (auto endpoint: endpoints) {
        size_t offset = 0;
        len = buf_size;
        coap_print_status_t print_result;

        auto it = filters.begin();
        int ok_to_return = 1;

        while (it != filters.end() && ok_to_return) {
            if (it->first.empty() || it->second.empty()) {
                free(buf);
                return empty;
            }
            auto key = it->first;
            auto value = it->second;

            coap_attr_t *attr = coap_find_attr(endpoint, coap_make_str_const(key.c_str()));
            if (!attr) {
                ok_to_return = 0;
            } else {
                auto src = std::string(reinterpret_cast<const char *>(coap_attr_get_value(attr)->s));

                if (value.back() == '*') {
                    auto dst = std::string(value, 0, value.length() - 1);
                    ok_to_return = match(src, dst, 1);
                } else {
                    ok_to_return = match(src, value, 0);
                }
            }

            it++;
        }

        if (ok_to_return) {
            print_result = coap_print_link(endpoint, buf, &len, &offset);

            if (print_result & COAP_PRINT_STATUS_ERROR) {
                free(buf);
                empty.emplace_back("error");
                return empty;
            }

            while (print_result & COAP_PRINT_STATUS_TRUNC) {
                buf_size *= 2;
                buf = static_cast<unsigned char *>(realloc(buf, buf_size));
                len = buf_size;
                offset = 0;
                print_result = coap_print_link(endpoint, buf, &len, &offset);
            }

            std::string partial = std::string(reinterpret_cast<const char *>(buf), 0, len);
            auto lt = partial.find("lt=");
            auto end = partial.find(';', lt + 1);

            if (end == std::string::npos) {
                partial = partial.substr(0, lt);
            } else {
                partial.erase(lt, end - lt + 1);
            }

            results.push_back(partial);
        }
    }

    free(buf);
    return results;
}

std::list<std::string>
ResourceDirectory::rd_resources_to_string_list_filtered(std::map<std::string, std::string> filters) {
    std::list<std::string> results;
    std::list<std::string> empty;
    auto endpoints = rd_endpoints->get_all();
    std::unique_lock<decltype(rd_endpoints->m_)> lock(rd_endpoints->m_);

    if (filters.empty()) {
        // return the whole list of resources
        for (auto endpoint: endpoints) {
            auto data = std::string((char *) coap_resource_get_userdata(endpoint));
            auto data_list = split(data, ',');

            auto base = coap_find_attr(endpoint, coap_make_str_const("base"));

            if (!base) {
                empty.emplace_back("error"); //TODO: throw exception instead
                return empty;
            }

            auto base_val = std::string(reinterpret_cast<const char *>(coap_attr_get_value(base)->s));
            for (const auto &data_elem: data_list) {
                auto to_add = "<" + base_val + std::string(data_elem, 1);

                if (to_add.back() == ',') {
                    results.emplace_back(to_add, 0, to_add.length() - 1);
                } else {
                    results.push_back(to_add);
                }
            }
        }

        return results;
    }

    // apply filters
    auto href = filters.find("href");
    for (auto endpoint: endpoints) {
        auto base = coap_find_attr(endpoint, coap_make_str_const("base"));
        auto base_val = std::string((const char *) (coap_attr_get_value(base)->s));
        auto resources_in_endpoint = split(std::string((char *) coap_resource_get_userdata(endpoint)), ',');
        auto resources_matched = std::map<int, std::string>();
        int match_attr_in_ep = 0;

        // extract resources from every endpoint and put it in a matched map
        for (int i = 0; i < resources_in_endpoint.size(); i++) {
            auto to_add = "<" + base_val + std::string(resources_in_endpoint[i], 1);
            resources_matched.insert(std::make_pair(i, to_add));
        }

        // check the href filter, both for endpoint and resource
        if (href != filters.end()) {
            auto href_val = href->second;

            if (href_val.find("://") == std::string::npos) {
                // the query is for sure on the endpoint level
                auto ep_uri = std::string((const char *) coap_resource_get_uri_path(endpoint)->s);
                if (ep_uri.find(href_val) == std::string::npos) {
                    // href does not match -> remove this endpoint from the result
                    resources_matched = {};
                }
            } else {
                // do the matching both for endpoint and resources
                if (base_val != href_val) {
                    // no match for the endpoint, continue matching with resources
                    for (const auto& pair : resources_matched) {
                        if (pair.second.find(href_val) == std::string::npos) {
                            resources_matched.erase(pair.first);
                        }
                    }
                }
            }

            filters.erase(href);
        }

        // apply the other filters
        for (auto it = filters.begin(); it != filters.end() && !resources_matched.empty(); it++) {
            if (it->first.empty() || it->second.empty()) {
                empty.emplace_back("error");
                return empty;
            }

            auto key = it->first;
            auto value = it->second;
            auto ep_attr = coap_find_attr(endpoint, coap_make_str_const(key.c_str()));

            if (ep_attr) {
                // filter found in endpoint, do the matching
                auto ep_attr_val = std::string((const char *) coap_attr_get_value(ep_attr)->s);

                if (value.back() == '*') {
                    auto dst = std::string(value, 0, value.length() - 1);
                    match_attr_in_ep = match(ep_attr_val, dst, 1);
                } else {
                    match_attr_in_ep = match(ep_attr_val, value, 0);
                }
            }

            if (!match_attr_in_ep) {
                // matching failed at endpoint level, do at resource level
                for (int i = 0; i < resources_in_endpoint.size(); i++) {
                    auto resource = resources_in_endpoint[i];

                    auto token_position = resource.find(key);
                    if (token_position) {
                        // token is here, do the match
                        auto token_value_end = resource.find(';', token_position);
                        auto token_value = std::string(resource, token_position + key.length() + 1,
                                                       token_value_end - (key.length() + 1) - token_position);

                        if (token_value.front() == '\"' && token_value.back() == '\"')
                            token_value = std::string(token_value, 1, token_value.length() - 2);

                        int val_match;
                        if (value.back() == '*') {
                            auto dst = std::string(value, 0, value.length() - 1);
                            val_match = match(token_value, dst, 1);
                        } else {
                            val_match = match(token_value, value, 0);
                        }

                        if (!val_match) {
                            resources_matched.erase(i);
                        }
                    } else {
                        // token is not present, remove the resource from the map
                        resources_matched.erase(i);
                    }
                }
            }

            // add the remaining resources in the return variable
            for (const auto &pair: resources_matched) results.push_back(pair.second);
        }
    }

    return results;
}

void ResourceDirectory::hnd_post_rd(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                                    const coap_string_t *query, coap_pdu_t *response) {
    
    //LOG_DBG("hnd_post_rd");                                    
    if (!query) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    size_t data_len;
    const u_int8_t *data;
    coap_get_data(request, &data_len, &data);
    //LOG_DBG("coap_get_data executed");
    //LOG_DBG("Data for directory:%s", std::string(reinterpret_cast<const char *>(data), data_len).c_str());
    if (!data) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    std::string data_str = std::string(reinterpret_cast<const char *>(data), data_len);
    //LOG_DBG("data publishing resource:%s",data_str.c_str());

    if (data_str.empty()) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    size_t buf_len = query->length + 128;
    unsigned char buf[buf_len];
    int n_opts = coap_split_query((const uint8_t *) query->s, query->length, buf, &buf_len);

    if (n_opts < 0) {
        LOG_ERR("Problem parsing query");
        return;
    }

    std::vector<std::string> sub_queries;
    unsigned char *p_buf = buf;
    while (n_opts--) {
        std::string option = std::string(reinterpret_cast<const char *>(coap_opt_value(p_buf)), coap_opt_length(p_buf));
        sub_queries.emplace_back(option);
        p_buf += coap_opt_size(p_buf);
    }

    std::unordered_map<std::string, std::string> query_map;
    for (const auto &token: sub_queries) {
        std::vector<std::string> sub_query = split(token, '=');
        query_map.insert(std::make_pair(sub_query[0], sub_query[1]));
    }

    auto ep = query_map.find("ep");
    if (ep == query_map.end()) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    auto base = query_map.find("base");
    if (base == query_map.end()) {
        auto remote = coap_session_get_addr_remote(session);
        unsigned char tmp_buf[64];
        size_t length = coap_print_addr(remote, tmp_buf, 64);
        std::string base_address = "coap://" + std::string(reinterpret_cast<const char *>(tmp_buf), length);
        query_map.insert(std::make_pair("base", base_address));
    }

    auto lt = query_map.find("lt");
    if (lt == query_map.end()) {
        const std::string default_lt = "90000"; // 25 hours
        query_map.insert(std::make_pair("lt", default_lt));
    }

    auto d = query_map.find("d");
    std::string d_name;
    if (!(d == query_map.end())) d_name = d->second;
    else d_name = DEFAULT_SECTOR_NAME;

    std::string ep_sector_key = ep->second + "@" + d_name;

    std::string ep_name = add_endpoint_to_rd(ep_sector_key, coap_session_get_context(session), query_map, data_str);

    if (ep_name.empty()) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_INTERNAL_ERROR);
    }

    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);

    {
        unsigned char _b[1024];
        unsigned char *b = _b;
        size_t tmp_buf_len = sizeof(_b);
        int n_seg;

        n_seg = coap_split_path(reinterpret_cast<const uint8_t *>(ep_name.c_str()),
                                ep_name.length(),
                                b,
                                &tmp_buf_len);

        while (n_seg--) {
            coap_add_option(response,
                            COAP_OPTION_LOCATION_PATH,
                            coap_opt_length(b),
                            coap_opt_value(b));
            b += coap_opt_size(b);
        }
    }
}

void
ResourceDirectory::hnd_get_lookup_res(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                                      const coap_string_t *query, coap_pdu_t *response) {
    std::string query_str;

    if (query) {
        query_str = std::string(reinterpret_cast<const char *>(query->s));
    }

    unsigned char content_type_buf[3];

    std::vector<std::string> sub_queries = split(query_str, '&');
    std::map<std::string, std::string> query_map;

    for (const auto &token: sub_queries) {
        std::vector<std::string> sub_query = split(token, '=');
        query_map.insert(std::make_pair(sub_query[0], sub_query[1]));
    }

    auto page = query_map.find("page");
    auto count = query_map.find("count");

    if (count != query_map.end() && page == query_map.end()) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    std::string page_val;
    std::string count_val;
    if (page == query_map.end()) {
        page_val = "-1";
    } else {
        page_val = page->second;
        query_map.erase("page");
    }

    if (count == query_map.end()) {
        count_val = "-1";
    } else {
        count_val = count->second;
        query_map.erase("count");
    }

    std::list<std::string> results = rd_resources_to_string_list_filtered(query_map);

    if (results.empty()) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
        coap_add_option(response,
                        COAP_OPTION_CONTENT_TYPE,
                        coap_encode_var_safe(content_type_buf, sizeof(content_type_buf),
                                             COAP_MEDIATYPE_APPLICATION_LINK_FORMAT),
                        content_type_buf);
        return;
    }

    if (results.front() == "error") {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_INTERNAL_ERROR);
        return;
    }

    if (count_val != "-1" && page_val == "-1") {
        // return only the first `count_val` number of strings
        auto begin = results.begin();
        std::advance(begin, stoi(count_val));

        results.erase(begin, results.end());
    } else if (count_val != "-1" && page_val != "-1") {
        // return `count_val` number of strings, starting from the `count_val` * `page_val`
        auto begin = results.begin();
        std::advance(begin, stoi(count_val) * stoi(page_val));
        auto end = begin;
        std::advance(end, stoi(count_val));

        results.erase(results.begin(), begin);
        results.erase(end, results.end());
    }

    std::string result;
    for (const auto &item: results) {
        result += (item + ",");
    }

    // remove the last ','
    if (!result.empty())
        result.pop_back();

    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);

    coap_add_option(response,
                    COAP_OPTION_CONTENT_TYPE,
                    coap_encode_var_safe(content_type_buf, sizeof(content_type_buf),
                                         COAP_MEDIATYPE_APPLICATION_LINK_FORMAT),
                    content_type_buf);
    coap_add_data_large_response(resource,session,request,response,query,COAP_MEDIATYPE_TEXT_PLAIN, 1, 0, result.length(), coap_make_str_const(result.c_str())->s,NULL, NULL);
}

void ResourceDirectory::hnd_get_lookup_ep(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                                          const coap_string_t *query, coap_pdu_t *response) {
    std::string query_str;

    if (query) {
        query_str = std::string(reinterpret_cast<const char *>(query->s));
    }

    unsigned char content_type_buf[3];

    std::vector<std::string> sub_queries = split(query_str, '&');
    std::map<std::string, std::string> query_map;

    for (const auto &token: sub_queries) {
        std::vector<std::string> sub_query = split(token, '=');
        query_map.insert(std::make_pair(sub_query[0], sub_query[1]));
    }

    auto page = query_map.find("page");
    auto count = query_map.find("count");

    if (count != query_map.end() && page == query_map.end()) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    std::string page_val;
    std::string count_val;
    if (page == query_map.end()) {
        page_val = "-1";
    } else {
        page_val = page->second;
        query_map.erase("page");
    }

    if (count == query_map.end()) {
        count_val = "-1";
    } else {
        count_val = count->second;
        query_map.erase("count");
    }

    std::list<std::string> results = endpoints_to_string_list_filtered(query_map);

    if (results.empty()) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
        coap_add_option(response,
                        COAP_OPTION_CONTENT_TYPE,
                        coap_encode_var_safe(content_type_buf, sizeof(content_type_buf),
                                             COAP_MEDIATYPE_APPLICATION_LINK_FORMAT),
                        content_type_buf);
        return;
    }

    if (results.front() == "error") {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_INTERNAL_ERROR);
        return;
    }


    if (count_val != "-1" && page_val == "-1") {
        // return only the first `count_val` number of strings
        auto begin = results.begin();
        std::advance(begin, stoi(count_val));

        results.erase(begin, results.end());
    } else if (count_val != "-1" && page_val != "-1") {
        // return `count_val` number of strings, starting from the `count_val` * `page_val`
        auto begin = results.begin();
        std::advance(begin, stoi(count_val) * stoi(page_val));
        auto end = begin;
        std::advance(end, stoi(count_val));

        results.erase(results.begin(), begin);
        results.erase(end, results.end());
    }

    std::string result;
    for (const auto &item: results) {
        result += (item + ",");
    }

    // remove the last ','
    if (!result.empty())
        result.pop_back();

    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);

    coap_add_option(response,
                    COAP_OPTION_CONTENT_TYPE,
                    coap_encode_var_safe(content_type_buf, sizeof(content_type_buf),
                                         COAP_MEDIATYPE_APPLICATION_LINK_FORMAT),
                    content_type_buf);
    coap_add_data_large_response(resource,session,request,response,query,COAP_MEDIATYPE_TEXT_PLAIN, 1, 0, result.length(), coap_make_str_const(result.c_str())->s,NULL, NULL);
}

void
ResourceDirectory::hnd_post_rd_endpoint(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                                        const coap_string_t *query, coap_pdu_t *response) {
    if (!query) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_BAD_REQUEST);
        return;
    }

    std::string query_str = std::string(reinterpret_cast<const char *>(query->s));
    std::vector<std::string> sub_queries = split(query_str, '&');
    std::unordered_map<std::string, std::string> query_map;

    for (const auto &token: sub_queries) {
        std::vector<std::string> sub_query = split(token, '=');
        query_map.insert(std::make_pair(sub_query[0], sub_query[1]));
    }

    auto lt = query_map.find("lt");
    std::string lt_value;
    if (lt == query_map.end()) {
        lt_value = "90000"; // 25 hours
    } else {
        lt_value = lt->second;
        query_map.erase("lt");
    }

    auto uri_path = std::string(reinterpret_cast<const char *>(coap_resource_get_uri_path(resource)->s));
    auto rd_endpoint = rd_endpoints->get(uri_path);

    coap_add_attr(rd_endpoint,
                  coap_make_str_const("lt"),
                  coap_make_str_const(lt_value.c_str()), 0);

    for (const auto &pair: query_map) {
        coap_add_attr(rd_endpoint,
                      coap_make_str_const(pair.first.c_str()),
                      coap_make_str_const(pair.second.c_str()), 0);
    }

    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CHANGED);
}

void
ResourceDirectory::hnd_delete_rd_endpoint(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                                          const coap_string_t *query, coap_pdu_t *response) {
    auto uri_path = std::string(reinterpret_cast<const char *>(coap_resource_get_uri_path(resource)->s));
    auto rd_endpoint = rd_endpoints->get(uri_path);
    rd_endpoints->erase(uri_path);
    auto ep = coap_find_attr(rd_endpoint, coap_make_str_const("ep"));
    auto d = coap_find_attr(rd_endpoint, coap_make_str_const("d"));
    std::string key;

    if (ep) {
        key += std::string(reinterpret_cast<const char *>(coap_attr_get_value(ep)->s));
        key += "@";
    } else {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_INTERNAL_ERROR);
        return;
    }

    if (d)
        key += std::string(reinterpret_cast<const char *>(coap_attr_get_value(d)->s));

    rd_endpoints->erase(uri_path);
    rd_endpoints_by_ep_d->erase(key);
    coap_delete_resource(coap_session_get_context(session), resource);

    coap_pdu_set_code(response, COAP_RESPONSE_CODE_DELETED);
}

int ResourceDirectory::init_server_context() {
    coap_startup();

    coap_address_t dst;
    if (resolve_address("::", RD_DEFAULT_PORT, &dst) < 0) {
        LOG_ERR("Failed to resolve address");
        coap_free_context(this->context);
        coap_cleanup();
        return -1;
    }

    this->context = coap_new_context(nullptr);
    coap_new_endpoint(context, &dst, COAP_PROTO_UDP);

    // be available on multicast groups
    coap_join_mcast_group(this->context, COAP_IP4_MULTICAST);
    coap_join_mcast_group(this->context, COAP_IP6_MULTICAST_LOCAL_LINK);
    coap_join_mcast_group(this->context, COAP_IP6_MULTICAST_SITE_LOCAL);

    return 0;
}

ResourceDirectory::ResourceDirectory(const std::string &ip, coap_context_t *server_context) {

    if (server_context != nullptr)
        this->context = server_context;
    else
        this->init_server_context();

    if (this->context == nullptr) {
        // TODO: throw exception
        return;
    }

    coap_resource_t *resource_dir, *rd_lookup_res, *rd_lookup_ep;

    resource_dir = coap_resource_init(coap_make_str_const("rd"), 0);
    coap_register_handler(resource_dir, COAP_REQUEST_POST, hnd_post_rd);
    coap_add_attr(resource_dir, coap_make_str_const("rt"), coap_make_str_const("\"core.rd\""), 0);
    coap_add_attr(resource_dir, coap_make_str_const("ct"), coap_make_str_const("40"), 0);
    coap_add_resource(this->context, resource_dir);

    rd_lookup_res = coap_resource_init(coap_make_str_const("rd-lookup/res"), 0);
    coap_register_handler(rd_lookup_res, COAP_REQUEST_GET, hnd_get_lookup_res);
    coap_add_attr(rd_lookup_res, coap_make_str_const("rt"), coap_make_str_const("\"core.rd-lookup-res\""), 0);
    coap_add_attr(rd_lookup_res, coap_make_str_const("ct"), coap_make_str_const("40"), 0);
    coap_resource_set_get_observable(rd_lookup_res, 1);
    coap_add_resource(this->context, rd_lookup_res);

    rd_lookup_ep = coap_resource_init(coap_make_str_const("rd-lookup/ep"), 0);
    coap_register_handler(rd_lookup_ep, COAP_REQUEST_GET, hnd_get_lookup_ep);
    coap_add_attr(rd_lookup_ep, coap_make_str_const("rt"), coap_make_str_const("\"core.rd-lookup-ep\""), 0);
    coap_add_attr(rd_lookup_ep, coap_make_str_const("ct"), coap_make_str_const("40"), 0);
    coap_add_resource(this->context, rd_lookup_ep);

    LOG_DBG("Initialized resource directory");
}

ResourceDirectory::~ResourceDirectory() {
    set_quit(true);
    rd_endpoints->delete_all();
    rd_endpoints_by_ep_d->delete_all();

    delete rd_endpoints;
    delete rd_endpoints_by_ep_d;
}

int ResourceDirectory::run() {
    LOG_INFO("Running resource directory");
    while (!this->get_quit()) {
        coap_io_process(this->context, COAP_IO_WAIT);
    }

    return 0;
}
