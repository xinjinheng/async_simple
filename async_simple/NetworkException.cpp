/*
 * Copyright (c) 2022, Alibaba Group Holding Limited;
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "async_simple/NetworkException.h"
#include <asio/error.hpp>

namespace async_simple {

std::unique_ptr<NetworkException> convertAsioErrorToException(
    const asio::error_code& ec, const std::string& operation) {
    std::string message = operation + " failed";
    
    if (ec == asio::error::operation_aborted) {
        return std::make_unique<ConnectionClosedException>(message, ec);
    }
    else if (ec == asio::error::eof) {
        return std::make_unique<ConnectionClosedException>(message, ec);
    }
    else if (ec == asio::error::connection_reset) {
        return std::make_unique<ConnectionClosedException>(message, ec);
    }
    else if (ec == asio::error::connection_aborted) {
        return std::make_unique<ConnectionClosedException>(message, ec);
    }
    else if (ec == asio::error::timed_out) {
        return std::make_unique<ConnectionTimeoutException>(message, ec);
    }
    else if (ec == asio::error::connection_refused) {
        return std::make_unique<ConnectionTimeoutException>(message, ec);
    }
    else if (ec.category() == asio::error::system_category && 
             (ec.value() == ECONNRESET || ec.value() == ENOTCONN || 
              ec.value() == EPIPE || ec.value() == ESHUTDOWN)) {
        return std::make_unique<ConnectionClosedException>(message, ec);
    }
    else if (ec.category() == asio::error::system_category && 
             (ec.value() == EIO || ec.value() == EBADF || 
              ec.value() == EINVAL || ec.value() == ENOSPC)) {
        return std::make_unique<ReadWriteException>(message, ec);
    }
    else {
        // Default to ReadWriteException for other errors
        return std::make_unique<ReadWriteException>(message, ec);
    }
}

}  // namespace async_simple
