#include <Common/Exception.h>
#include <Common/ReplicasReconnector.h>
#include <algorithm>
#include <ctime>
#include <random>
#include <thread>
#include <pcg_random.hpp>
#include <mysqlxx/PoolWithFailover.h>
#include <Common/randomSeed.h>
#include <IO/WriteBufferFromString.h>
#include <IO/Operators.h>

namespace DB::ErrorCodes
{
    extern const int ALL_CONNECTION_TRIES_FAILED;
}

using namespace mysqlxx;

auto connectionReestablisher(std::weak_ptr<Pool> pool, bool shareable)
{
    return [weak_pool = pool, shareable](UInt64 interval_milliseconds)
    {
        auto shared_pool = weak_pool.lock();
        if (!shared_pool)
            return false;

        if (shareable && shared_pool.use_count() == 2)
            return true;

        if (!shared_pool->isOnline())
        {
            try
            {
                shared_pool->get();
                Poco::Util::Application::instance().logger().information("Reestablishing connection to " + shared_pool->getDescription() + " has succeeded.");
            }
            catch (const Poco::Exception & e)
            {
                if (interval_milliseconds >= 1000)
                    Poco::Util::Application::instance().logger().warning("Reestablishing connection to " + shared_pool->getDescription() + " has failed: " + e.displayText());
            }
            catch (...)
            {
                if (interval_milliseconds >= 1000)
                    Poco::Util::Application::instance().logger().warning("Reestablishing connection to " + shared_pool->getDescription() + " has failed.");
            }
        }

        return true;
    };
}


PoolWithFailover::PoolWithFailover(
        const Poco::Util::AbstractConfiguration & config_,
        const std::string & config_name_,
        const unsigned default_connections_,
        const unsigned max_connections_,
        const size_t max_tries_)
    : max_tries(max_tries_)
    , shareable(config_.getBool(config_name_ + ".share_connection", false))
    , wait_timeout(UINT64_MAX)
    , bg_reconnect(config_.getBool(config_name_ + ".background_reconnect", false))
{
    if (config_.has(config_name_ + ".replica"))
    {
        Poco::Util::AbstractConfiguration::Keys replica_keys;
        config_.keys(config_name_, replica_keys);
        for (const auto & replica_config_key : replica_keys)
        {
            /// There could be another elements in the same level in configuration file, like "password", "port"...
            if (replica_config_key.starts_with("replica"))
            {
                std::string replica_name = config_name_ + "." + replica_config_key;

                int priority = config_.getInt(replica_name + ".priority", 0);

                replicas_by_priority[priority].emplace_back(
                    std::make_shared<Pool>(config_, replica_name, default_connections_, max_connections_, config_name_.c_str()));

                if (bg_reconnect)
                    DB::ReplicasReconnector::instance().add(connectionReestablisher(std::weak_ptr(replicas_by_priority[priority].back()), shareable));
            }
        }

        /// PoolWithFailover objects are stored in a cache inside PoolFactory.
        /// This cache is reset by ExternalDictionariesLoader after every SYSTEM RELOAD DICTIONAR{Y|IES}
        /// which triggers massive re-constructing of connection pools.
        static thread_local pcg64_fast rnd_generator(randomSeed());
        for (auto & [_, replicas] : replicas_by_priority)
        {
            if (replicas.size() > 1)
                std::shuffle(replicas.begin(), replicas.end(), rnd_generator);
        }
    }
    else
    {
        replicas_by_priority[0].emplace_back(
            std::make_shared<Pool>(config_, config_name_, default_connections_, max_connections_));
        if (bg_reconnect)
            DB::ReplicasReconnector::instance().add(connectionReestablisher(std::weak_ptr(replicas_by_priority[0].back()), shareable));
    }

}


PoolWithFailover::PoolWithFailover(
        const std::string & config_name_,
        const unsigned default_connections_,
        const unsigned max_connections_,
        const size_t max_tries_)
    : PoolWithFailover{Poco::Util::Application::instance().config(),
            config_name_, default_connections_, max_connections_, max_tries_}
{
}


PoolWithFailover::PoolWithFailover(
        const std::string & database,
        const RemoteDescription & addresses,
        const std::string & user,
        const std::string & password,
        const std::string & ssl_ca,
        const std::string & ssl_cert,
        const std::string & ssl_key,
        unsigned default_connections_,
        unsigned max_connections_,
        size_t max_tries_,
        uint64_t wait_timeout_,
        size_t connect_timeout_,
        size_t rw_timeout_,
        bool bg_reconnect_)
    : max_tries(max_tries_)
    , shareable(false)
    , wait_timeout(wait_timeout_)
    , bg_reconnect(bg_reconnect_)
{
    /// Replicas have the same priority, but traversed replicas are moved to the end of the queue.
    for (const auto & [host, port] : addresses)
    {
        replicas_by_priority[0].emplace_back(std::make_shared<Pool>(database,
            host, user, password, port, ssl_ca, ssl_cert, ssl_key,
            /* socket_ = */ "",
            connect_timeout_,
            rw_timeout_,
            default_connections_,
            max_connections_));
        if (bg_reconnect)
            DB::ReplicasReconnector::instance().add(connectionReestablisher(std::weak_ptr(replicas_by_priority[0].back()), shareable));
    }

}


PoolWithFailover::PoolWithFailover(const PoolWithFailover & other)
    : max_tries{other.max_tries}
    , shareable{other.shareable}
    , wait_timeout(other.wait_timeout)
    , bg_reconnect(other.bg_reconnect)
{
    if (shareable)
    {
        replicas_by_priority = other.replicas_by_priority;
    }
    else
    {
        for (const auto & priority_replicas : other.replicas_by_priority)
        {
            Replicas replicas;
            replicas.reserve(priority_replicas.second.size());
            for (const auto & pool : priority_replicas.second)
            {
                replicas.emplace_back(std::make_shared<Pool>(*pool));
                if (bg_reconnect)
                    DB::ReplicasReconnector::instance().add(connectionReestablisher(std::weak_ptr(replicas.back()), shareable));
            }
            replicas_by_priority.emplace(priority_replicas.first, std::move(replicas));
        }

    }
}

PoolWithFailover::Entry PoolWithFailover::get()
{
    Poco::Util::Application & app = Poco::Util::Application::instance();
    std::lock_guard locker(mutex);

    /// If we cannot connect to some replica due to pool overflow, than we will wait and connect.
    PoolPtr * full_pool = nullptr;

    struct ErrorDetail
    {
        int code;
        std::string description;
    };

    std::unordered_map<std::string, ErrorDetail> replica_name_to_error_detail;

    for (size_t try_no = 0; try_no < max_tries; ++try_no)
    {
        full_pool = nullptr;

        for (auto & priority_replicas : replicas_by_priority)
        {
            Replicas & replicas = priority_replicas.second;
            for (size_t i = 0, size = replicas.size(); i < size; ++i)
            {
                PoolPtr & pool = replicas[i];

                if (bg_reconnect && !pool->isOnline())
                    continue;

                try
                {
                    Entry entry = shareable ? pool->get(wait_timeout) : pool->tryGet();

                    if (!entry.isNull())
                    {
                        /// Move all traversed replicas to the end of queue.
                        /// (No need to move replicas with another priority)
                        std::rotate(replicas.begin(), replicas.begin() + i + 1, replicas.end());

                        return entry;
                    }
                }
                catch (const Poco::Exception & e)
                {
                    if (e.displayText().contains("mysqlxx::Pool is full")) /// NOTE: String comparison is trashy code.
                    {
                        full_pool = &pool;
                    }

                    app.logger().warning("Connection to " + pool->getDescription() + " failed: " + e.displayText());

                    replica_name_to_error_detail.insert_or_assign(pool->getDescription(), ErrorDetail{e.code(), e.displayText()});

                    continue;
                }

                app.logger().warning("Connection to " + pool->getDescription() + " failed.");
            }
        }

        if (replicas_by_priority.size() > 1)
            app.logger().error("Connection to all mysql replicas failed " + std::to_string(try_no + 1) + " times");
        else
            app.logger().error("Connection to mysql failed " + std::to_string(try_no + 1) + " times");
    }

    if (full_pool)
    {
        app.logger().error("All connections failed, trying to wait on a full pool " + (*full_pool)->getDescription());
        return (*full_pool)->get(wait_timeout);
    }

    DB::WriteBufferFromOwnString message;

    for (auto it = replicas_by_priority.begin(); it != replicas_by_priority.end(); ++it)
    {
        for (auto jt = it->second.begin(); jt != it->second.end(); ++jt)
        {
            message << (it == replicas_by_priority.begin() && jt == it->second.begin() ? "" : ", ") << (*jt)->getDescription();

            if (auto error_detail_it = replica_name_to_error_detail.find(((*jt)->getDescription()));
                error_detail_it != replica_name_to_error_detail.end())
            {
                const auto & [code, description] = error_detail_it->second;
                message << ", ERROR " << code  << " : " << description;
            }
        }
    }


    if (replicas_by_priority.size() > 1)
        throw DB::Exception(DB::ErrorCodes::ALL_CONNECTION_TRIES_FAILED, "Connections to all mysql replicas failed: {}", message.str());
    throw DB::Exception(DB::ErrorCodes::ALL_CONNECTION_TRIES_FAILED, "Connections to mysql failed: {}", message.str());
}
