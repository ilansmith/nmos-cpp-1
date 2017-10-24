#ifndef NMOS_CPP_NODE_MAIN_GATE_H
#define NMOS_CPP_NODE_MAIN_GATE_H

#include <atomic>
#include <ostream>
#include <boost/algorithm/string/replace.hpp>
#include "nmos/logging_api.h"

namespace
{
    inline slog::omanip_function error_log_format(const slog::async_log_message& message)
    {
        return slog::omanip([&](std::ostream& os)
        {
            auto category = nmos::get_category_stash(message.stream());
            os
                << slog::put_timestamp(message.timestamp()) << ": "
                << slog::put_severity_name(message.level()) << ": "
                << message.thread_id() << ": "
                << (category.empty() ? "" : category + ": ")
                << boost::replace_all_copy(message.str(), "\n", "\n\t") // indent multi-line messages
                << std::endl;
        });
    }

    class main_gate : public slog::base_gate
    {
    public:
        main_gate(std::ostream& error_log, std::ostream& access_log, nmos::experimental::log_model& model, std::mutex& mutex, std::atomic<slog::severity>& level)
            : service({ error_log, access_log, model, mutex }), level(level) {}
        virtual ~main_gate() {}

        virtual bool pertinent(slog::severity level) const { return this->level <= level; }
        virtual void log(const slog::log_message& message) const { service(message); }

    private:
        struct service_function
        {
            std::ostream& error_log;
            std::ostream& access_log;
            nmos::experimental::log_model& model;
            std::mutex& mutex;

            typedef const slog::async_log_message& argument_type;
            void operator()(argument_type message) const
            {
                std::lock_guard<std::mutex> lock(mutex);
                error_log << error_log_format(message);
                if (nmos::categories::access == nmos::get_category_stash(message.stream()))
                {
                    access_log << nmos::common_log_format(message);
                }
                nmos::experimental::log_to_model(model, message);
            }
        };

        mutable slog::async_log_service<service_function> service;
        std::atomic<slog::severity>& level;
    };
}

#endif