/*
 * Copyright (c) 2022 Huawei Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __EDDIE_H__
#define __EDDIE_H__

#include <string>
#include <functional>
#include <cstdint>
#include <vector>
#include <map>

struct Link {
    std::string host;
    std::string port;
    std::string path;
    std::map<std::string, std::string> attrs;
};

/**
 * Parse a CoAP link format string into a vector of links.
 * Constrained RESTful Environments (CoRE) Link Format is described in
 * [rfc6690 specification](https://datatracker.ietf.org/doc/html/rfc6690)
 * @param link_format CoAP link format string
 * @return vector of Link resulting from the parse of the link format string.
 */
std::vector<Link> parse_link_format(std::string link_format);

typedef enum {GET, POST, PUT, DELETE} method_t;

typedef enum {
    CREATED = 65,
    DELETED = 66,
    VALID = 67,
    CHANGED = 68,
    CONTENT = 69,
    CONTINUE = 95,
    BAD_REQUEST = 128,
    UNAUTHORIZED = 129,
    BAD_OPTION = 130,
    FORBIDDEN = 131,
    NOT_FOUND = 132,
    METHOD_NOT_ALLOWED = 133,
    NOT_ACCEPTABLE = 134,
    REQUEST_ENTITY_INCOMPLETE = 136,
    CONFLICT = 137,
    PRECONDITION_FAILED = 140,
    REQUEST_ENTITY_TOO_LARGE = 141,
    UNSUPPORTED_CONTENT_FORMAT = 143,
    UNPROCESSABLE_ENTITY = 150,
    INTERNAL_SERVER_ERROR = 160,
    NOT_IMPLEMENTED = 161,
    BAD_GATEWAY = 162,
    SERVICE_UNAVAILABLE = 163,
    GATEWAY_TIMEOUT = 164,
    PROXYING_NOT_SUPPORTED = 165,
} response_code_t;

typedef struct {
    char const * dst_host;
    const char * dst_port;
    method_t method = GET;
    const char * path = nullptr;
    const char * query = nullptr;
    const uint8_t * data = nullptr;
    size_t data_length = 0;
    bool confirmable = true;
    unsigned int content_format = 0;
} request_t;

typedef struct {
    std::string data;
    response_code_t status_code;
    std::string src_host;
    std::string src_port;
    std::string location_path;
} message_t;

class EddieResource {
public:

    /**
     * Implement this method to define a get handler
     * @param request The GET request data
     * @return Response to the GET request
     */
	virtual message_t render_get(request_t &request) {
		message_t response;
		response.status_code = NOT_IMPLEMENTED;
		return response;
	}

    /**
     * Implement this method to define a post handler
     * @param request The POST request data
     * @return Response to the POST request
     */
	virtual message_t render_post(request_t &request) {
		message_t response;
		response.status_code = NOT_IMPLEMENTED;
		return response;
	}

    /**
     * Implement this method to define a put handler
     * @param request The PUT request data
     * @return Response to the PUT request
     */
	virtual message_t render_put(request_t &request) {
		message_t response;
		response.status_code = NOT_IMPLEMENTED;
		return response;
	}

    /**
     * Implement this method to define a delete handler
     * @param request The DELETE request data
     * @return Response to the DELETE request
     */
	virtual message_t render_delete(request_t &request) {
		message_t response;
		response.status_code = NOT_IMPLEMENTED;
		return response;
	}

    /**
     * Implement this method to return the resource path
     * @return resource path as an array of C-strings terminated by nullptr.
     * For example the path "lamp/brightness" is returned as ["lamp", "brightness", nullptr]
     */
    virtual const char * const * get_path() {
        return nullptr;
    }

    /**
     * Implement this method to return the resource attributes
     * @return resource attributes as an array of C-strings terminated by nullptr.
     * For example the attributes "lt=90000&rt=eddie.lamp" is returned as ["lt=90000", "rt=eddie.lamp", nullptr]
     */
    virtual const char * const * get_attributes() {
        return nullptr;
    }

	message_t request_handler(request_t &request) {
		switch (request.method) {
		case GET:
            return render_get(request);
		case POST:
			return render_post(request);
		case PUT:
			return render_put(request);
		case DELETE:
			return render_delete(request);
		}
        message_t response;
        response.status_code = METHOD_NOT_ALLOWED;
        return response;
	}

    std::string get_path_str() {
        const char * const * path = get_path();
        std::string res;

        if (path[0] == nullptr) return "";
        else res = path[0];

        for (int i=1; path[i]; i++)
            res += "/" + std::string(path[i]);

        return res;
    }

    std::vector<std::string> get_attributes_str() {
        const char * const * attributes = get_attributes();
        std::vector<std::string> res;
        for (int i=0; attributes[i]; i++) {
            res.emplace_back(attributes[i]);
        }
        return res;
    }
};

#endif