# Architecture

### Signal/Slot, Stream/Sink

basic concepts are Signal and Slot

```c++
struct Signal<T> {
    void connect(Slot<T> slot);
    void disconnect(Slot<T> slot);
}

struct Slot<T> : Callable<T> {
    void invoke(T);
    void operator()(T);
}
```

Stream<T> and Sink<T> concepts correspond to unidirectional communication channel from Subscriber and Publisher points of view.

Stream could operate over reliable (e.g. TCP) or unreliable media (UDP/Multicast)

StreamState reflects logical stream state. 
If used with stateful transport (tcp) each state corresponds to one or many [TCP states](https://users.cs.northwestern.edu/~agupta/cs340/project2/TCPIP_State_Transition_Diagram.pdf) (SYN-SENT, ESTABLISHED, CLOSING, etc)

Stream implementations could use following messages
'Open': stream opening request, containing filter of events Subscriber wich to receive from Publisher
'

``` java
enum StreamState {                                                                         //tcp
    Closed,                 // initial, from: Closing                                      CLOSED
                            // to: Pending upon open()
    
    Pending,                // from: Closed, Failed                               
                            // tcp connecting                                              SYN-SENT         
                            // or tcp connected, 'Open' sent                               ESTABLISHED
                            // to: Closing upon close()
                            // to: Pending upon reopen()
                            // to: Stale upon tcp connect timeout
                            // to: Stale upon 'Ack' timeout

    Open,                   // from: Pending
                            // tcp connected
                            // 'Open' was sent, 'Ack'/'Data' was received.                 TCP: ESTABLISHED

    Stale,                  // from: Pending, Open
                            // timeout while waiting for 'Ack'/'Data'.                     TCP: ESTABLISHED, SYN-SENT 
                            // to: Pending (upon Subscriber's reopen()).                   TCP: ESTABLISHED 
                            // to: Closing (upon Subscriber's close())
    
    Closing,                // from: Open, Pending
                            // 'Close' was sent to Publisher                               TCP: ESTABLISHED
                            // 

    Failed                  // unrecoverable state (max reconnections attempts, unrecoverable error received from server), TCP: CLOSED
};
enum SinkState : StreamState {
    Closed,                 // sink is closed
    Open,                   // 'Open' received from subscriber
                            // 'Ack' COULD be sent using 'Ack' or 'Data'. TCP: SYN-SENT/ESTABLISHED
    Stale,                  // waiting 'HeartBeat' from subscriber timed out, some kind of recovery process could start
    Closing                 // upon user request, 'Close' was sent to subscriber, TCP: ESTABLISHED, CLOSING
    Failed                  // sink recovery process failed
};

```

```c++
using Seq=std::size_t;
struct Stream<T> : Signal<T>, Sequenced<Seq>
{
    Seq sequence();         /// last received seq
    StreamState state();    /// stream state
}
struct Sink<T> : Slot<T>, Sequenced<Seq>
{
    void invoke(T);
    void operator()(T);
    Seq sequence();
    SinkState state();
}
```

