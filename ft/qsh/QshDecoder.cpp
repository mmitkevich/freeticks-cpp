#include <chrono>
#include <cstring>
#include <fstream>
#include "ft/utils/Trace.hpp"
#include "QshDecoder.hpp"

using namespace ft::qsh;

std::int64_t QshDecoder::read_leb128() {
    std::int64_t result = 0;
    int shift  = 0;
    int byte;
    do {
        byte = input_stream_.get();
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
        byte = input_stream_.get();
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
    input_stream_.read(result.data(), size);
    if(input_stream_.fail())
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

Ticks QshDecoder::read_datetime() {
    std::int64_t val;
    input_stream_.read((char*)&val, sizeof(val));
    if(input_stream_.fail())
        throw std::runtime_error("EOF in read_datetime");
    return Ticks(val);
}

Ticks QshDecoder::read_grow_datetime(Ticks previous) {
    std::int64_t pval = previous.count();
    std::int64_t val = read_growing(pval / 10000) * 10000;
    return Ticks(val);
}


int QshDecoder::read_byte() {
    int byte = input_stream_.get();
    if(byte == EOF)
        throw std::runtime_error("EOF in read_byte");
    return byte;
}

std::uint16_t QshDecoder::read_uint16() {
    std::int16_t val;
    input_stream_.read((char*)&val, sizeof(val));
    if(input_stream_.fail())
        throw std::runtime_error("EOF in read_datetime");
    return val;
}

void QshDecoder::read_header() {
    char buf[19];
    input_stream_.read(buf, sizeof(buf));
    if(input_stream_.fail() || 
        0!=std::memcmp("MdGateway History Data", buf, sizeof(buf)))
        throw std::runtime_error("not qscalp file");
    int v = input_stream_.get();
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
    if(input_stream_.eof())
        return false;
    FT_TRACE("ofs "<<input_stream_.tellg()<<" last frame ts "<<state_.frame_ts);
    state_.ts = state_.frame_ts = read_grow_datetime(state_.frame_ts);
    if(state_.nstreams>1)
        state_.cur_stream = read_byte();
    FT_TRACE("frame: frame_ts "<<frame_ts<< " cur_stream "<<std::hex<<cur_stream)
    return true;
}

void QshDecoder::decode() {
    input_stream_.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
    total_count_ = 0;
    try {
        read_header();
        while(read_frame_header()) {
            int stream_id = state_.stream_ids[state_.cur_stream];
            switch(stream_id) {
                case OrdLog: read_order_log(); break;
                default: throw std::runtime_error("invalid stream id");
            }
            total_count_++;
        }
    }catch(std::ios_base::failure& fail) {
        // rethrow everything except eof
        if(input_stream_.bad())
            throw fail;
    }
}

void QshDecoder::read_order_log() {
    int flags = read_byte();
    std::uint16_t plaza_flags = read_uint16();
    core::TickMessage e{};
    e.flags = 0;
    FT_TRACE("flags 0x"<<std::hex<<flags<<" plaza_flags 0x"<<plaza_flags);
    if(plaza_flags & (PLAZA_ADD|PLAZA_MOVE|PLAZA_COUNTER)) {
        e.flags |= TickMessage::Place;
    }
    if(plaza_flags & (PLAZA_CANCEL|PLAZA_MOVE)) {
        e.flags |= TickMessage::Cancel;
    }
    if(plaza_flags & PLAZA_FILL) {
        e.flags |= TickMessage::Fill;
    }
    if(flags & OL_TIMESTAMP) {
        state_.ts = read_grow_datetime(state_.ts);
        e.timestamp = e.exchange_timestamp = state_.ts.to_time_point();
        FT_TRACE("exchange_timestamp "<<e.timestamp)
    } else {
        e.timestamp = e.exchange_timestamp = state_.ts.to_time_point();
    }
    if(flags & OL_ID) {
        if(plaza_flags & PLAZA_ADD)
            e.exchange_id = state_.exchange_id = read_growing(state_.exchange_id);
        else 
            e.exchange_id = read_relative(state_.exchange_id);
        FT_TRACE("exchange_id "<<e.exchange_id)
    } else {
        e.exchange_id = state_.exchange_id;
    }
    if(flags&OL_PRICE) {
        e.price = state_.price = read_relative(state_.price);
        FT_TRACE("price "<<e.price)
    } else {
        e.price = state_.price;
    }
    if(flags&OL_AMOUNT) {
        e.qty = read_leb128();
        FT_TRACE("qty "<<e.qty)
    }
    if(plaza_flags & PLAZA_FILL) {
        if(flags&OL_AMOUNT_LEFT) {
            e.qty_left = read_leb128();
            FT_TRACE("qty_left "<<e.qty_left)
        }
        if(flags&OL_FILL_ID) {
            e.fill_id = state_.fill_id = read_growing(state_.fill_id);
            FT_TRACE("trade_id "<<e.fill_id)
        } else {
            e.fill_id = state_.fill_id;
        }
        if(flags&OL_FILL_PRICE) {
            e.fill_price = state_.fill_price = read_relative(state_.fill_price);
            FT_TRACE("trade_price "<<e.fill_price)
        } else {
            e.fill_price = state_.fill_price;
        }
        if(flags&OL_OPEN_INTEREST) {
            e.open_interest = state_.open_interest = read_relative(state_.open_interest);
            FT_TRACE("open_interest "<<e.open_interest)
        } else {
            e.open_interest = state_.open_interest;
        }
    }
    if(e.flags==0) {
        std::cerr<<"UNKNOWN flags "<<std::hex<<flags<<" plaza "<<std::hex<<plaza_flags<<std::endl;
    }else {
        if(on_tick_)
            on_tick_(e);
    }
}
