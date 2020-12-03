#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t ft_len_t;
typedef int64_t  ft_flags_t;
typedef uint64_t ft_id_t;
typedef unsigned long long ft_time_t;
typedef int64_t  ft_qty_t;
typedef uint16_t ft_slen_t;
typedef uint64_t ft_seq_t;
typedef unsigned char  ft_byte;
typedef char ft_char;
typedef int8_t ft_side_t;
typedef uint16_t ft_msg_type_t;
typedef uint8_t ft_topic_t;
typedef uint8_t ft_event_t;

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
    ft_seq_t  ft_seq;          // sender-generated message sequence number    
    ft_time_t ft_recv_time;         // receive timestamp
    ft_time_t ft_send_time;         // send timestamp
    ft_flags_t ft_flags;            // reserved: arbitrary binary flags
} ft_hdr_t;

enum ft_topic_enum {
    FT_TICK         = 1,
    FT_OHLC         = 9,
    FT_INSTRUMENT   = 17,
    FT_STATISTICS   = 33
};

enum ft_message_enum {
    // transport
    FT_SYN          = 1,
    FT_FIN          = 2,
    FT_ACK          = 3,
    FT_NACK         = 4,
    FT_HB           = 5,

    // requests: pub/sub
    FT_SUBSCRIBE    = 17,
    FT_UNSUBSCRIBE  = 18,
    FT_UNSUBSCRIBE_ALL = 19,

    // subscription status
    FT_STERAM_STALE = 6,
    FT_STREAM_FAIL  = 7,

    // response: general request status
    FT_REQ_ACK      = 9,
    FT_REQ_FAIL     = 10,


    // topics: L1, L2, L3
    FT_ADD       = 33,   // L3: add order
    FT_REMOVE    = 34,   // L3: remove order
    FT_CLEAR     = 35,   // L3, L2, L1: clear book (or one side)
    FT_UPDATE    = 36,   // L1, L2: update aggregated level

    // requests: order
    FT_ORDER_PLACE  = 65,   // L3: place order(s)
    FT_ORDER_CANCEL = 66,   // L3: cancel order(s)
    FT_ORDER_REPLACE = 67,  // L3: atomically cancel old order and place new 

    // stream: executions 
    FT_EXEC_FILL         = 97,      // active/filled report    
    FT_EXEC_PARTIAL_FILL = 98,     // active/filled report    
    FT_EXEC_FAILED       = 99
}; 

// messages binary layouts

/// market data L2 aggregated level update or fill update or L3 order log update
struct ft_price_t {
    ft_flags_t ft_flags;
    ft_event_t ft_type;
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
    ft_id_t ft_instrument_id;       // hash of instrument symbol or some other object
    ft_slen_t ft_item_len;              // SBE: blockLength
    ft_slen_t ft_items_count;           // SBE: numInGroup
    ft_byte ft_items[0];                // data block, SBE: "repeating group"
};

// instrument metadata update
typedef struct  {
    ft_hdr_t ft_hdr;
    ft_id_t ft_instrument_id;       // hash of instrument symbol or some other object
    ft_slen_t ft_symbol_len;
    ft_char   ft_symbol[0];
} ft_instrument_t;

/// subscription or unsubscription request
typedef struct  {
    ft_hdr_t ft_hdr;
    ft_id_t ft_client_id;           // client-generated request id
    ft_id_t ft_instrument_id;       // hash of instrument symbol or some other object
    ft_slen_t ft_symbol_len;
    ft_char   ft_symbol[0];
} ft_subscribe_t;

/// response on arbitrary request
typedef struct  {
    ft_hdr_t ft_hdr;
    ft_id_t ft_client_id {};            // client-generated request id
    ft_id_t ft_server_id {};            // server-generated response id (could identifiy request as well)
    ft_time_t ft_timestamp;             // exchange timestamp (e.g. gateway timestamp)
    ft_id_t ft_instrument_id;           // hash of instrument symbol or some other object
} ft_response_t;

/// order request
typedef struct {
    ft_hdr_t ft_hdr;
    ft_id_t ft_client_id;               // client order id
    ft_id_t ft_server_id;               // exchange order id
    ft_id_t ft_linked_id;               // reserved: linked id (client order id of replaced order, etc)
    ft_side_t ft_side;                  // BUY +1, SELL -1, UNKNOWN 0
    ft_qty_t ft_qty;                    // original qty
    ft_qty_t ft_price;                  // price
    ft_id_t ft_instrument_id;           // hash of instrument symbol or some other object
    ft_slen_t ft_symbol_len;            // symbol len
    ft_char ft_symbol[0];                // symbol characters
} ft_order_t;

/// order update
struct ft_execution_t {
    ft_hdr_t ft_hdr;
    ft_id_t ft_client_id;               // client order id
    ft_id_t ft_server_id;               // server-generated order id
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
    ft_time_t ft_timestamp;         // exchange timetamp
    ft_id_t  ft_status;             // numeric status
    ft_slen_t ft_message_len;       // string status
    ft_char   ft_message[0]; 
} ft_status_t;

#ifdef __cplusplus
}
#endif