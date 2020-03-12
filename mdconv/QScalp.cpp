#include "QScalp.hpp"
#include "TickMessage.hpp"
#include <chrono>
#include <cstring>
#include <fstream>
#include "util/Trace.hpp"

std::int64_t QScalp::read_leb128(std::istream &input) {
    std::int64_t result = 0;
    int shift  = 0;
    int byte;
    do {
        byte = input.get();
        if(byte == EOF) {
            throw std::runtime_error("EOF in read_leb128");
        }
        result |= ((byte & 0x7F) << shift);
        shift += 7;
    }while(byte & 0x80);

    if(byte & 0x40) {
        result |= ((-1ULL) << shift);
    }
    return result;
}

std::int64_t QScalp::read_uleb128(std::istream &input) {
    std::int64_t result = 0;
    int shift  = 0;
    int byte;
    do {
        byte = input.get();
        if(byte == EOF) {
            throw std::runtime_error("EOF in read_uleb128");
        }
        result |= ((byte & 0x7F) << shift);
        shift += 7;            
    } while(byte & 0x80);
    return result;
}

std::int64_t QScalp::read_relative(std::istream &input, std::int64_t previous) {
    previous += read_leb128(input);
    return previous;
}

std::string QScalp::read_string(std::istream& input) {
    std::int64_t size = read_uleb128(input);
    std::string result(size, '\0');
    input.read(result.data(), size);
    if(input.fail())
        throw std::runtime_error("EOF in read_string");
    return result;
}

std::int64_t QScalp::read_growing(std::istream& input, std::int64_t previous) {
    std::int64_t delta = read_uleb128(input);
    if(delta == 268435455) {
        std::int64_t delta2 = read_leb128(input);
        return previous + delta2;
    }else {
        return previous + delta;
    }
}

Ticks QScalp::read_datetime(std::istream& input) {
    std::int64_t val;
    input.read((char*)&val, sizeof(val));
    if(input.fail())
        throw std::runtime_error("EOF in read_datetime");
    return Ticks(val);
}

Ticks QScalp::read_grow_datetime(std::istream& input, Ticks previous) {
    std::int64_t val = read_growing(input, previous.count()/100)*100;
    return Ticks(val);
}

void QScalp::read_header(std::istream& input) {
    char buf[19];
    input.read(buf, sizeof(buf));
    if(input.fail() || 
        0!=std::memcmp("QScalp History Data", buf, sizeof(buf)))
        throw std::runtime_error("not qscalp file");
    int v = input.get();
    if(v!=4)
        throw std::runtime_error("only qscalp version 4 is supported");
    app = read_string(input);
    comment = read_string(input);
    ts = frame_ts = ctime = read_datetime(input);
    FT_TRACE("ctime: " << ctime.to_string())
    nstreams = read_byte(input);
    for(int i=0;i<nstreams;i++) {
        int stream_id = read_byte(input);
        if(stream_id != Stream::Messages) {
            stream_ids[i] = stream_id;
            std::size_t stream_index =  (stream_id >> 4) - 1;
            if(stream_index<0 || stream_index>=7)
                throw std::runtime_error("invalid stream id");
            instruments[stream_index] = read_string(input);
            FT_TRACE("stream 0x"<<std::hex<<stream_id<<" ins "<<instruments[stream_index]);
        }
    }
}

int QScalp::read_byte(std::istream& input) {
    int byte = input.get();
    if(byte == EOF)
        throw std::runtime_error("EOF in read_byte");
    return byte;
}

std::uint16_t QScalp::read_uint16(std::istream& input) {
    std::int16_t val;
    input.read((char*)&val, sizeof(val));
    if(input.fail())
        throw std::runtime_error("EOF in read_datetime");
    return val;
}

bool QScalp::read_frame_header(std::istream& input) {
    if(input.eof())
        return false;
    FT_TRACE("ofs "<<input.tellg());
    frame_ts = read_grow_datetime(input, frame_ts);
    if(nstreams>1)
        cur_stream = read_byte(input);
    FT_TRACE("frame: frame_ts "<<ts.to_string()<< " cur_stream "<<std::hex<<cur_stream)
    return true;
}

void QScalp::read(std::istream &input, OnRecord on_record) {
    read_header(input);
    while(read_frame_header(input)) {
        int stream_id = stream_ids[cur_stream];
        TickMessage e {};
        switch(stream_id) {
            case OrdLog: read_ord_log(input, e); break;
            default: throw std::runtime_error("invalid stream id");
        }
        on_record(e);
    }
}
enum PlazaFlags {
    PLAZA_ADD = 1<<2,
    PLAZA_FILL = 1<<3,
    PLAZA_BUY = 1<<4,
    PLAZA_SELL = 1<<5,
    PLAZA_SNAPSHOT = 1<<6,
    PLAZA_QUOTE = 1<<7,
    PLAZA_COUNTER = 1<<8,
    PLAZA_NONSYSTEM = 1<<9,
    PLAZA_END_OF_TX = 1<<10,
    PLAZA_FOC = 1<<11,
    PLAZA_MOVE = 1<<12,
    PLAZA_CANCEL = 1<<13,
    PLAZA_CANCEL_GROUP = 1<<14,
    PLAZA_CROSS = 1<<15
};
enum OrdLogFlags {
    OL_TIMESTAMP = 1<<0,
    OL_ID = 1<<1,
    OL_PRICE = 1<<2,
    OL_AMOUNT = 1<<3,
    OL_AMOUNT_LEFT = 1<<4,
    OL_FILL_ID = 1<<5,
    OL_FILL_PRICE = 1<<6,
    OL_OPEN_INTEREST = 1<<7
};
void QScalp::read_ord_log(std::istream &input, TickMessage &e) {
    int flags = read_byte(input);
    std::uint16_t plaza_flags = read_uint16(input);
    e.flags = 0;
    FT_TRACE("flags 0x"<<std::hex<<flags<<" plaza_flags 0x"<<plaza_flags);
    if(plaza_flags & (PLAZA_ADD|PLAZA_MOVE|PLAZA_COUNTER)) {
        e.flags |= TickMessage::Place;
    }
    if(plaza_flags & (PLAZA_CANCEL|PLAZA_CANCEL_GROUP|PLAZA_MOVE)) {
        e.flags |= TickMessage::Cancel;
    }
    if(plaza_flags & PLAZA_FILL) {
        e.flags |= TickMessage::Fill;
    }
    if(e.flags==0) {
        assert(false);
    }
    if(flags & OL_TIMESTAMP) {
        ts = read_grow_datetime(input, ts);
        e.timestamp = e.exchange_timestamp = ts.to_time_point();
        FT_TRACE("exchange_timestamp "<<e.timestamp)G
    } else {
        e.timestamp = e.exchange_timestamp = ts.to_time_point();
    }
    if(flags & OL_ID) {
        if(plaza_flags & PLAZA_ADD)
            e.exchange_id = exchange_id = read_growing(input, exchange_id);
        else 
            e.exchange_id = read_relative(input, exchange_id);
        FT_TRACE("exchange_id "<<e.exchange_id)
    } else {
        e.exchange_id = exchange_id;
    }
    if(flags&OL_PRICE) {
        e.price = price = read_relative(input, price);
        FT_TRACE("price "<<e.price)
    } else {
        e.price = price;
    }
    if(flags&OL_AMOUNT) {
        e.qty = read_leb128(input);
        FT_TRACE("qty "<<e.qty)
    }
    if(plaza_flags & PLAZA_FILL) {
        if(flags&OL_AMOUNT_LEFT) {
            e.qty_left = read_leb128(input);
            FT_TRACE("qty_left "<<e.qty_left)
        }
        if(flags&OL_FILL_ID) {
            e.fill_id = fill_id = read_growing(input, fill_id);
            FT_TRACE("trade_id "<<e.fill_id)
        } else {
            e.fill_id = fill_id;
        }
        if(flags&OL_FILL_PRICE) {
            e.fill_price = fill_price = read_relative(input, fill_price);
            FT_TRACE("trade_price "<<e.fill_price)
        } else {
            e.fill_price = fill_price;
        }
        if(flags&OL_OPEN_INTEREST) {
            e.open_interest = open_interest = read_relative(input, open_interest);
            FT_TRACE("open_interest "<<e.open_interest)
        } else {
            e.open_interest = open_interest;
        }
    }
}

void QScalp::parse(std::string fname, OnRecord on_record) {
    FT_TRACE(format_time_point(std::chrono::system_clock::now()))
    std::ifstream ifs(fname);
    QScalp reader;    
    reader.read(ifs, on_record);
}