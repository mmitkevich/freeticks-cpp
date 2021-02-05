CREATE TABLE default.spb_taq
(
    `LocalTime` DateTime64(9, 'Europe/Moscow'),
    `Time` DateTime64(9, 'Europe/Moscow'),
    `InstrumentId` UUID,
    `Symbol` String,
    `VenueInstrumentId` UUID,
    `Side` Enum8('EMPTY' = 0, 'BUY' = 1, 'SELL' = -1),
    `Price` Float64,
    `Qty` Float64,
    `Event` Enum8('ADD' = 1, 'DELETE' = 2, 'MODIFY' = 3, 'CLEAR' = 4, 'TRADE' = 5),
    `Seq` UInt64,
    `Date` Date DEFAULT toDate(LocalTime)
)
ENGINE = MergeTree
ORDER BY (LocalTime, Seq, Symbol)
PARTITION BY (Date)
SETTINGS index_granularity = 8192