/*
    Copyright 2020 The Silkrpc Authors

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "bitmap.hpp"

#include <climits>
#include <memory>
#include <utility>
#include <vector>

#include <boost/endian/conversion.hpp>

#include <silkrpc/common/log.hpp>
#include <silkrpc/common/util.hpp>
#include <silkworm/node/silkworm/db/bitmap.hpp>

namespace silkrpc::ethdb::bitmap {

using Roaring64Map = roaring::Roaring64Map;

// Implement the Roaring64Map::fastunion for vector of unique_ptr
static Roaring64Map fast_or(size_t n, const std::vector<std::unique_ptr<Roaring64Map>>& inputs) {
    Roaring64Map result;
    for (size_t k = 0; k < n; ++k) {
        result |= *inputs[k];
    }
    return result;
}

boost::asio::awaitable<Roaring64Map> get(core::rawdb::DatabaseReader& db_reader, const std::string& table, silkworm::Bytes& key, uint32_t from_block, uint32_t to_block) {
    std::vector<std::unique_ptr<Roaring64Map>> chuncks;

    silkworm::Bytes from_key{key.begin(), key.end()};
    from_key.resize(key.size() + sizeof(uint16_t));
    boost::endian::store_big_u16(&from_key[key.size()], 0);
    SILKRPC_DEBUG << "table: " << table << " key: " << key << " from_key: " << from_key << "\n";

    core::rawdb::Walker walker = [&](const silkworm::Bytes& k, const silkworm::Bytes& v) {
        SILKRPC_TRACE << "k: " << k << " v: " << v << "\n";
        auto same_key = k.starts_with(key);
        if (!same_key) {
            return false;
        }
        
        auto chunck = std::make_unique<Roaring64Map>(silkworm::db::bitmap::parse(v));
        SILKRPC_TRACE << "chunck: " << chunck->toString() << "\n";
        
        if (chunck->maximum() >= from_block && chunck->minimum() <= to_block) {
            chuncks.push_back(std::move(chunck));
        }
        return true;
    };
    co_await db_reader.walk(table, from_key, key.size() * CHAR_BIT, walker);

    auto result{fast_or(chuncks.size(), chuncks)};
    SILKRPC_DEBUG << "result: " << result.toString() << "\n";
    co_return result;
}

} // namespace silkrpc::ethdb::bitmap
