#pragma once

#include <Core/Block.h>
#include <IO/ConnectionTimeouts.h>
#include <IO/ReadWriteBufferFromHTTP.h>
#include <Poco/Net/HTTPBasicCredentials.h>
#include <Poco/URI.h>
#include <Common/LocalDateTime.h>
#include <Dictionaries/DictionaryStructure.h>
#include <Dictionaries/IDictionarySource.h>
#include <Interpreters/Context_fwd.h>
#include <IO/CompressionMethod.h>

namespace Poco
{
class Logger;
}


namespace DB
{
/// Allows loading dictionaries from http[s] source
class HTTPDictionarySource final : public IDictionarySource
{
public:

    struct Configuration
    {
        const std::string url;
        const std::string format;
        const std::string update_field;
        const UInt64 update_lag;
        const HTTPHeaderEntries header_entries;
    };

    HTTPDictionarySource(
        const DictionaryStructure & dict_struct_,
        const Configuration & configuration,
        const Poco::Net::HTTPBasicCredentials & credentials_,
        Block & sample_block_,
        ContextPtr context_);

    HTTPDictionarySource(const HTTPDictionarySource & other);
    HTTPDictionarySource & operator=(const HTTPDictionarySource &) = delete;

    QueryPipeline loadAll() override;

    QueryPipeline loadUpdatedAll() override;

    QueryPipeline loadIds(const std::vector<UInt64> & ids) override;

    QueryPipeline loadKeys(const Columns & key_columns, const std::vector<size_t> & requested_rows) override;

    bool isModified() const override;

    bool supportsSelectiveLoad() const override;

    bool hasUpdateField() const override;

    DictionarySourcePtr clone() const override;

    std::string toString() const override;

private:
    void getUpdateFieldAndDate(Poco::URI & uri);

    // wrap buffer using encoding from made request
    QueryPipeline createWrappedBuffer(std::unique_ptr<ReadWriteBufferFromHTTP> http_buffer);

    LoggerPtr log;

    LocalDateTime getLastModification() const;

    std::chrono::time_point<std::chrono::system_clock> update_time;
    const DictionaryStructure dict_struct;
    const Configuration configuration;
    Poco::Net::HTTPBasicCredentials credentials;
    Block sample_block;
    ContextPtr context;
    ConnectionTimeouts timeouts;
};

}
