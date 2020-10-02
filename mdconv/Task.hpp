#pragma once
#include "ft/utils/Common.hpp"
#include <chrono>
#include "ft/core/TickMessage.hpp"

namespace ft {


template<typename GatewayT>
class Task
{
public:
    Task(std::string_view taskid, GatewayT&& gateway)
    : taskid_(taskid)
    , gateway_(gateway)
    {}

    template<typename HandlerT>
    void record(HandlerT handler) {
        gateway_.on_record(handler);
    }

    bool match(std::string_view taskid) const {
        return taskid_ == taskid;
    }
    
    const std::string_view taskid() const {
        return taskid_;
    }

    template<typename OptionsT>
    void run(std::string_view input, const OptionsT &opts) {
        auto start_ts = std::chrono::system_clock::now();
        auto finally = [&] {
            auto elapsed = std::chrono::system_clock::now() - start_ts;
            auto count = gateway_.total_count();
            TOOLBOX_INFO << taskid() << " parsed " << count
                << " records in " << elapsed.count()/1e9 <<" s" 
                << " at "<< (1e3*count/elapsed.count()) << " mio/s";
        };
        try {
            gateway_.input(input);            
            gateway_.filter(opts.filter);
            gateway_.run();
            finally();            
        }catch(std::runtime_error &e) {
            finally();
            TOOLBOX_INFO << "'" << gateway_.input() << "' " << e.what();
            throw;
        }
    }
private:
    GatewayT gateway_;
    std::string taskid_;
};


template<typename...TasksT>
class TaskSet {
public:
    template<typename...ArgsT>
    TaskSet(ArgsT... args)
    : tasks_(std::forward<ArgsT>(args)...)
    {}

    template<typename OptionsT>
    void run(std::string_view taskid, const OptionsT& opts)
    {
        for(auto& input: opts.inputs) {
            bool done = false;
            mp::tuple_for_each(tasks_, [&](auto &task) {
                if(!done && task.match(taskid)) {
                    done = true;
                    try {
                        task.run(input, opts);
                    }catch(std::runtime_error &e) {
                        TOOLBOX_INFO << "'" << input_url_ << "' " << e.what();
                    }
                }
            });
        }
    }
private:
    std::tuple<TasksT...> tasks_;
    std::string input_url_;
};

template<typename...TasksT>
auto make_task_set(TasksT... tasks) { return TaskSet<TasksT...>(std::forward<TasksT>(tasks)...); }

} // ft::md