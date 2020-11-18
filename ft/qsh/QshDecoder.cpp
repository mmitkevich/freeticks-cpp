#include "ft/core/Tick.hpp"
#include "ft/utils/Common.hpp"
#include "ft/core/StreamStats.hpp"
#include "QshDecoder.hpp"
#include "toolbox/sys/Log.hpp"

#include <cstring>
#include <fstream>

using namespace ft::qsh;

std::int64_t QshDecoder::read_leb128() {
    std::int64_t result = 0;
    int shift  = 0;
    int byte;
    do {
        byte = input_stream().get();
        if(byte == EOF) {
            throw std::runtime_error("EOF in read_leb128");
        }
        result |= ((byte & 0x7F) << shift);
        shift += 7;
    }while(byte & 0x80);

    if((byte & 0x40)!=0) {
        result |= ((-1ULL) << shift);
    }
    return result;
}

std::int64_t QshDecoder::read_uleb128() {
    std::int64_t result = 0;
    int shift  = 0;
    int byte;
    do {
        byte = input_stream().get();
        if(byte == EOF) {
            throw std::runtime_error("EOF in read_uleb128");
        }
        result |= ((byte & 0x7F) << shift);
        shift += 7;            
    } while(byte & 0x80);
    return result;
}

std::int64_t QshDecoder::read_relative(std::int64_t previous) {
    previous += read_leb128();
    return previous;
}

std::string QshDecoder::read_string() {
    std::int64_t size = read_uleb128();
    std::string result(size, '\0');
    input_stream().read(result.data(), size);
    if(input_stream().fail())
        throw std::runtime_error("EOF in read_string");
    return result;
}

std::int64_t QshDecoder::read_growing(std::int64_t previous) {
    std::int64_t delta = read_uleb128();
    if(delta == 268435455) {
        std::int64_t delta2 = read_leb128();
        return previous + delta2;
    }else {
        return previous + delta;
    }
}

ftu::HundredNanos QshDecoder::read_datetime() {
    std::int64_t val;
    input_stream().read((char*)&val, sizeof(val));
    if(input_stream().fail())
        throw std::runtime_error("EOF in read_datetime");
    return ftu::HundredNanos(val);
}

ftu::HundredNanos QshDecoder::read_grow_datetime(ftu::HundredNanos previous) {
    std::int64_t pval = previous.count();
    std::int64_t val = read_growing(pval / 10000) * 10000;
    return ftu::HundredNanos(val);
}


int QshDecoder::read_byte() {
    int byte = input_stream().get();
    if(byte == EOF)
        throw std::runtime_error("EOF in read_byte");
    return byte;
}

std::uint16_t QshDecoder::read_uint16() {
    std::int16_t val;
    input_stream().read((char*)&val, sizeof(val));
    if(input_stream().fail())
        throw std::runtime_error("EOF in read_datetime");
    return val;
}

void QshDecoder::read_header() {
    char buf[19];
    input_stream().read(buf, sizeof(buf));
    if(input_stream().fail() || 
        0!=std::memcmp("MdGateway History Data", buf, sizeof(buf)))
        throw std::runtime_error("not qscalp file");
    int v = input_stream().get();
    if(v!=4)
        throw std::runtime_error("only qscalp version 4 is supported");
    state_.app = read_string();
    state_.comment = read_string();
    state_.frame_ts = state_.ctime = read_datetime();
    FT_TRACE("ctime: " << state_.ctime.to_string())
    state_.nstreams = read_byte();
    for(int i=0;i<state_.nstreams;i++) {
        int stream_id = read_byte();
        if(stream_id != Stream::Messages) {
            state_.stream_ids[i] = stream_id;
            std::size_t stream_index =  (stream_id >> 4) - 1;
            if(stream_index<0 || stream_index>=7)
                throw std::runtime_error("invalid stream id");
            state_.instruments[stream_index] = read_string();
            FT_TRACE("stream 0x"<<std::hex<<stream_id<<" ins "<<state_.instruments[stream_index]);
        }
    }
}

bool QshDecoder::read_frame_header() {
    if(input_stream().eof())
        return false;
    FT_TRACE("ofs "<<input_stream().tellg()<<" last frame ts "<<state_.frame_ts);
    state_.ts = state_.frame_ts = read_grow_datetime(state_.frame_ts);
    if(state_.nstreams>1)
        state_.cur_stream = read_byte();
    FT_TRACE("frame: frame_ts "<<frame_ts<< " cur_stream "<<std::hex<<cur_stream)
    return true;
}

void QshDecoder::run() {
    input_stream().exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
    try {
        read_header();
        while(read_frame_header()) {
            int stream_id = state_.stream_ids[state_.cur_stream];
            switch(stream_id) {
                case OrdLog: read_order_log(); break;
                default: throw std::runtime_error("invalid stream id");
            }
            ticks().stats().on_received();
        }
    }catch(std::ios_base::failure& fail) {
        // rethrow everything except eof
        if(input_stream().bad())
            throw fail;
    }
}

void QshDecoder::read_order_log() {

    int flags = read_byte();
    std::uint16_t plaza_flags = read_uint16();
    QshTick ti {};
    auto& order = ti[0];
    auto& fill = ti[1];
    FT_TRACE("flags 0x"<<std::hex<<flags<<" plaza_flags 0x"<<plaza_flags);
    if(plaza_flags & (PLAZA_ADD|PLAZA_MOVE|PLAZA_COUNTER)) {
        ti.type(core::TickType::Tick);
        order.type(core::TickType::Add);
    }
    if(plaza_flags & (PLAZA_CANCEL|PLAZA_MOVE)) {
        ti.type(core::TickType::Tick);
        order.type(core::TickType::Remove);
    }
    if(plaza_flags & PLAZA_FILL) {
        fill.type(core::TickType::Fill);
    }
    if(flags & OL_TIMESTAMP) {
        state_.ts = read_grow_datetime(state_.ts);
        FT_TRACE("exchange_timestamp "<<e.timestamp)
    }
    Timestamp ts(state_.ts);
    ti.recv_time(ts);
    ti.send_time(ts);
    if(flags & OL_ID) {
        if(plaza_flags & PLAZA_ADD) {
            state_.exchange_id = read_growing(state_.exchange_id);
            order.server_id(state_.exchange_id);
        } else {
            order.server_id(read_relative(state_.exchange_id));
        }
        FT_TRACE("exchange_id "<<e.exchange_id)
    } else {
        order.server_id(state_.exchange_id);
    }
    if(flags&OL_PRICE) {
        state_.price = read_relative(state_.price);
        FT_TRACE("price "<<e.price)
    }
    order.price(state_.price);
    if(flags&OL_AMOUNT) {
        order.qty(read_leb128());
        FT_TRACE("qty "<<e.qty)
    }
    if(plaza_flags & PLAZA_FILL) {
        if(flags&OL_AMOUNT_LEFT) {
            fill.qty(read_leb128());
            FT_TRACE("qty_left "<<e.qty_left)
        }
        if(flags&OL_FILL_ID) {
            state_.fill_id = read_growing(state_.fill_id);
            FT_TRACE("trade_id "<<d.fill_id)
        }
        fill.server_id(state_.fill_id);        
        if(flags&OL_FILL_PRICE) {
            state_.fill_price = read_relative(state_.fill_price);
            FT_TRACE("trade_price "<<e.fill_price)
        }
        fill.price(state_.fill_price);
        if(flags&OL_OPEN_INTEREST) {
            state_.open_interest = read_relative(state_.open_interest);
            FT_TRACE("open_interest "<<e.open_interest)
        }
        //fill.open_interest(state_.open_interest);
    }
    if(ti.type()==core::TickType::Invalid) {
        TOOLBOX_WARNING<<"qsh: invalid flags "<<std::hex<<flags<<" plaza "<<std::hex<<plaza_flags;
        ticks().stats().on_rejected(flags);
    }else {
        ticks().invoke(ti.as_size<1>());
    }
}
