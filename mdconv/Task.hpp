#pragma once
#include "ft/utils/Common.hpp"
#include <chrono>

namespace ft {


template<typename GatewayT>
class Task
{
public:
    Task(std::string_view taskid, std::unique_ptr<GatewayT> gateway)
    : taskid_(taskid)
    , gateway_(std::move(gateway))
    {}

    GatewayT &gateway() { return *gateway_; }

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
            gateway().report(std::cout);
            auto elapsed = std::chrono::system_clock::now() - start_ts;
            auto total_received = gateway().stats().total_received;
            TOOLBOX_INFO << taskid() << " received " << total_received
                << " in " << elapsed.count()/1e9 <<" s" 
                << " at "<< (1e3*total_received/elapsed.count()) << " mio/s";
        };
        try {
            gateway().input(input);            
            gateway().filter(opts.filter);
            gateway().run();
            finally();            
        }catch(std::runtime_error &e) {
            finally();
            TOOLBOX_INFO << "'" << gateway().input() << "' " << e.what();
            throw;
        }
    }
private:
    std::unique_ptr<GatewayT> gateway_;
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