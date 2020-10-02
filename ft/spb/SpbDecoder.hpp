#pragma once

#include "ft/utils/Common.hpp"

namespace ft::spb {

class Frame;

template<typename FrameT, typename TypeListT>
class SpbDecoder : public ftu::SignalTuple<TypeListT>
{
    using Base = ftu::SignalTuple<TypeListT>;
public:
    using TypeList = TypeListT;
    using Frame = FrameT;
public:
    
    using Base::Base;
    using Base::connect;
    using Base::disconnect;

    void decode(std::string_view data) {
        auto ptr = data.begin();
        auto end = data.end();
        while(ptr < end) {
            const Frame &frame = *reinterpret_cast<const Frame*>(ptr);

            FT_ASSERT(frame.size>=0);
            
            if(frame.size<=0) {
                TOOLBOX_DEBUG << "["<<(ptr-data.begin())<<"] invalid size "<<frame.size;
                break;
            }

            bool found = false;

            Base::for_each([&](auto &cb) {
                using CB = std::decay_t<decltype(cb)>;
                if(CB::Message::msgid == frame.msgid) {
                    found = true;
                    TOOLBOX_DEBUG << "["<<(ptr-data.begin())<<".."<<(ptr+frame.size-data.begin())<<"] found msgid "<<frame.msgid;
                    if(cb)
                        cb(*reinterpret_cast<const typename CB::Message*>(ptr));
                }
            });
            if(!found) {
                TOOLBOX_DEBUG << "["<<(ptr-data.begin())<<"] unknown msgid "<<frame.msgid;
            }
            ptr += sizeof(Frame) + frame.size;
        }
    }
   
private:

};

}