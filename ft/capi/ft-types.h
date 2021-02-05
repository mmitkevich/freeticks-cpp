#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)

typedef uint16_t ft_len_t;
typedef int64_t  ft_flags_t;
typedef struct fd_it_s {
    uint64_t low;
    uint64_t high;
} ft_id_t;
typedef unsigned long long ft_time_t;
typedef int64_t  ft_qty_t;
typedef uint16_t ft_slen_t;
typedef uint64_t ft_seq_t;
typedef unsigned char  ft_byte_t;
typedef char ft_char_t;
typedef int8_t ft_side_t;
typedef uint16_t ft_msg_type_t;
typedef uint8_t ft_topic_t;
typedef uint8_t ft_event_t;
typedef uint8_t ft_instrument_type_t;
typedef uint16_t ft_status_t;
typedef ft_id_t ft_instrument_id_t;
typedef ft_id_t ft_request_id_t;
typedef ft_id_t ft_order_id_t;

/// events occur in topics and form messages
typedef struct {
    ft_event_t ft_event;
    ft_topic_t ft_topic;
} ft_msgtype_t;

/// message has header
typedef struct  {
    ft_slen_t ft_len;               // length of the message fixed size
    ft_msgtype_t ft_type;           // message type (event, topic)
    uint16_t ft_schema;             // schema
    uint16_t ft_version;            // schema version
    ft_seq_t  ft_seq;               // sender-generated message sequence number    
    ft_time_t ft_recv_time;         // receive timestamp
    ft_time_t ft_send_time;         // send timestamp
    ft_flags_t ft_flags;            // reserved: arbitrary binary flags
} ft_hdr_t;

enum ft_topic_enum {
    FT_TOPIC_EMPTY = 0,
    FT_TOPIC_BESTPRICE = 1,
    FT_TOPIC_INSTRUMENT = 2,
    FT_TOPIC_ORDERSTATUS = 3,
    FT_TOPIC_STATISTICS = 4,    
    FT_TOPIC_CANDLE = 5,
    FT_TOPIC_STREAM = 6    // state: stale/failed/etc
};

enum ft_event_enum {
    FT_EVENT_EMPTY = 0,
    FT_EVENT_SNAPSHOT = 1,
    FT_EVENT_UPDATE = 2
};

/// common event types, applied to L1/L2/L3 market data
enum ft_mod_enum {
    FT_ADD       = 1,   // L3: add order, L2: insert level
    FT_DEL       = 2,   // L3: remove order, L2: remove level
    FT_MOD       = 3,   // L1, L2: update aggregated level, L3: modify order
    FT_CLEAR     = 4,   // L3, L2, L1: clear book (or one side)
};

enum ft_order_req_enum {
    FT_ORDER_ADD    = 1,
    FT_ORDER_DEL    = 2,
    FT_ORDER_MOD    = 3
};

enum ft_tick_enum {
    FT_TICK_ADD = 1,
    FT_TICK_DEL = 2,
    FT_TICK_MOD = 3,
    FT_TICK_CLEAR = 4,
    FT_TICK_FILL = 5
};

/// generic status
enum ft_status_enum {
    FT_STATUS_UNKNOWN = 0,
    FT_STATUS_PENDING = 1,
    FT_STATUS_OK = 2,
    FT_STATUS_FAILED = 3
};

/// order status
enum ft_order_status_enum {
    FT_ORDER_STATUS_UNKNOWN      = 0,    // not sent yet
    FT_ORDER_STATUS_PENDING_NEW  = 1,    // sent    
    FT_ORDER_STATUS_NEW          = 2,    // active     
    FT_ORDER_STATUS_FAILED       = 3,    // final: failed    
    FT_ORDER_STATUS_PENDING_CANCEL = 4,  // cancel sent
    FT_ORDER_STATUS_CANCELED     = 5,    // final: canceled
    FT_ORDER_STATUS_PART_FILLED  = 7,    // partially filled
    FT_ORDER_STATUS_FILLED       = 8,    // final: filled
};


enum ft_request_enum {
    FT_REQ_SUBSCRIBE    = 1,    // with 2 reserved for FT_SUBSCRIBE_RESPONSE
    FT_RES_SUBSCRIBE    = 2,
    FT_REQ_UNSUBSCRIBE  = 3,
    FT_RES_UNSUBSCRIBE  = 4,
    FT_REQ_UNSUBSCRIBE_ALL  = 5,
    FT_RES_UNSUBSCRIBE_ALL  = 6,

    FT_REQ_ORDER    = 17,
    FT_RES_ORDER    = 18
};


// messages binary layouts

/// market data L2 aggregated level update or fill update or L3 order log update
struct ft_price_t {
    ft_flags_t ft_flags;
    ft_event_t ft_event;
    ft_id_t ft_client_id;               // client order id
    ft_id_t ft_server_id {};            // server order or fill id    
    ft_time_t ft_timestamp {};          // exchange timestamp        
    ft_side_t ft_side;                  // BUY +1, SELL -1, UNKNOWN 0
    ft_slen_t ft_level;                 // index of the L2 price level to update
    ft_qty_t ft_qty;                    // order qty (Place: original, Replace: new, Report: active)
    ft_qty_t ft_price;                  // price
};

/// market data update
struct ft_tick_t {
    ft_hdr_t ft_hdr;                    // SBE-compatible header
    ft_instrument_id_t ft_instrument_id;           // hash of instrument symbol or some other object
    ft_instrument_id_t ft_venue_instrument_id;       // venue-specific numeric instrument id, e.g. 100500
    ft_slen_t ft_item_len;              // SBE: blockLength
    ft_slen_t ft_items_count;           // SBE: numInGroup
    ft_char_t ft_items[0];                // data block, SBE: "repeating group"
};

// instrument update
typedef struct  {
    ft_hdr_t ft_hdr;
    ft_instrument_id_t ft_instrument_id;             // listed instrument numeric id, e.g. MSFT@NASDSAQ != MSFT@NYSE
    ft_instrument_id_t ft_venue_instrument_id;       // venue-specific numeric instrument id, e.g. 100500
    ft_instrument_type_t ft_instrument_type;    // type code
    ft_slen_t ft_symbol_len;              // well-known symbol, e.g. MSFT
    ft_slen_t ft_exchange_len;            // exchange name, e.g. XNAS
    ft_slen_t ft_venue_symbol_len;        // venue-specific symbol, e.g. BBG1230ABCDE
    ft_char_t   ft_items[0];
} ft_instrument_t;

/// subscription or unsubscription request
typedef struct  {
    ft_hdr_t ft_hdr;
    ft_instrument_id_t ft_instrument_id;           // hash of instrument symbol or some other object    
    ft_request_id_t ft_request_id;                  // client-generated request id
    ft_slen_t ft_symbol_len;
    ft_char_t   ft_symbol[0];
} ft_subscribe_t;

/// response on arbitrary request
typedef struct  {
    ft_hdr_t ft_hdr;
    ft_id_t ft_instrument_id;           // hash of instrument symbol or some other object    
    ft_id_t ft_request_id {};
    ft_id_t ft_response_id {};            // server-generated response id (could identifiy request as well)
    ft_status_t ft_status;
    ft_time_t ft_timestamp;             // exchange timestamp (e.g. gateway timestamp)
    ft_slen_t ft_message_len;           // error message
    ft_char_t ft_message[0];
} ft_response_t;

/// order request
typedef struct {
    ft_hdr_t ft_hdr;
    ft_instrument_id_t ft_instrument_id;
    ft_order_id_t ft_client_order_id;               // client order id
    ft_order_id_t ft_server_order_id;               // exchange order id
    ft_order_id_t ft_linked_order_id;               // reserved: linked id (client order id of replaced order, etc)
    ft_time_t ft_timestamp {};          // exchange timestamp                    
    ft_side_t ft_side;                  // BUY +1, SELL -1, UNKNOWN 0
    ft_qty_t ft_qty;                    // original qty
    ft_qty_t ft_price;                  // price
    ft_slen_t ft_symbol_len;            // symbol len
    ft_char_t ft_symbol[0];             // symbol characters
} ft_order_t;

/// order update
struct ft_execution_t {
    ft_hdr_t ft_hdr;
    ft_id_t ft_instrument_id;    
    ft_order_id_t ft_client_order_id;         // client order id
    ft_order_id_t ft_server_order_id;         // server-generated order id
    ft_order_id_t ft_linked_order_id;               // reserved: linked id (client order id of replaced order, etc)    
    ft_time_t ft_timestamp {};          // exchange timestamp                    
    ft_side_t ft_side;                  // BUY +1, SELL -1, UNKNOWN 0
    ft_qty_t ft_qty;                    // order active qty
    ft_qty_t ft_price;                  // price
    ft_qty_t ft_orig_qty;               // order original qty
    ft_qty_t ft_fill_qty;               // fill qty
    ft_id_t  ft_fill_id;                // fill id
};

/// instrument status update
typedef struct  {
    ft_hdr_t ft_hdr;
    ft_id_t ft_instrument_id;
    ft_time_t ft_timestamp;                     // exchange timetamp        
    ft_status_t  ft_instrument_status;          // numeric status
    ft_slen_t ft_message_len;                   // string status
    ft_char_t ft_message[0]; 
} ft_instrument_status_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif