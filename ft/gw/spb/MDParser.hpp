#pragma once

#include <boost/mp11/tuple.hpp>
#include <cassert>
#include <array>
#include <string_view>
#include "toolbox/util/Slot.hpp"
#include "toolbox/sys/Log.hpp"
#include "dto/MarketData.hpp"
#include <boost/mp11.hpp>

namespace ft::gw::spb {
namespace mp = boost::mp11;
using namespace mp;

namespace tbu = toolbox::util;

template<typename TypeList>
class MDParser {
    template<typename T> 
    struct Handler : tbu::BasicSlot<const T&> {
        using Base = tbu::BasicSlot<const T&>;
        using Message = T;
        using Base::Base;
        using Base::operator();
        using Base::operator bool;
        using Base::operator=;
    };
    using HandlerList = mp_rename<mp_transform<Handler, TypeList>, std::tuple>;
public:
    void parse(std::string_view data) {
        auto ptr = data.begin();
        auto end = data.end();
        while(ptr<end) {
            const dto::Frame *frame = reinterpret_cast<const dto::Frame*>(ptr);
            if(frame->size<=0) {
                TOOLBOX_DEBUG << "["<<(ptr-data.begin())<<"] invalid size "<<frame->size;
                break;
            }
            bool found = false;
            mp::tuple_for_each(callbacks_, [&](auto &cb) {
                using CB = std::decay_t<decltype(cb)>;
                if(CB::Message::msgid==frame->msgid) {
                    found = true;
                    TOOLBOX_DEBUG << "["<<(ptr-data.begin())<<".."<<(ptr+frame->size-data.begin())<<"] found msgid "<<frame->msgid;
                    if(cb)
                        cb(*reinterpret_cast<const typename CB::Message*>(frame));
                }
            });
            if(!found) {
                TOOLBOX_DEBUG << "["<<(ptr-data.begin())<<"] unknown msgid "<<frame->msgid;
            }
            ptr += frame->size;
        }
    }

    template<typename T>
    auto subscribe(tbu::BasicSlot<const T&> fn) { std::get< mp_find<TypeList, T>::value >(callbacks_) = fn; }
    HandlerList callbacks_;
};

}