#pragma once

#include <mbgl/storage/response.hpp>
#include <mbgl/util/optional.hpp>
#include <mbgl/util/chrono.hpp>

namespace mbgl {
namespace http {

static Duration errorRetryTimeout(Response::Error::Reason failedRequestReason, uint32_t failedRequests, optional<Timestamp> retryAfter = {}) {

    if (failedRequestReason == Response::Error::Reason::Server) {
        // Retry after one second three times, then start exponential backoff.
        return Seconds(failedRequests <= 3 ? 1 : 1u << std::min(failedRequests - 3, 31u));
    } else if (failedRequestReason == Response::Error::Reason::Connection) {
        // Immediate exponential backoff.
        assert(failedRequests > 0);
        return Seconds(1u << std::min(failedRequests - 1, 31u));
    } else if (failedRequestReason == Response::Error::Reason::RateLimit) {
        if (retryAfter) {
            return *retryAfter - util::now();
        } else {
            //Default
            return Seconds(5);
        }
    } else {
        // No error, or not an error that triggers retries.
        return Duration::max();
    }
}

static Duration expirationTimeout(optional<Timestamp> expires, uint32_t expiredRequests) {
    if (expiredRequests) {
        return Seconds(1u << std::min(expiredRequests - 1, 31u));
    } else if (expires) {
        return std::max(Seconds::zero(), *expires - util::now());
    } else {
        return Duration::max();
    }
} 

} // namespace http
} // namespace mbgl
