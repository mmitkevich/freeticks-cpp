        
 on_instr          on_tick
  |                   |
InstrStrm --+-- BestPriceStrm
            |
         Protocol
            |
    MdClient<Conn<UDP>> 
            |
       +----+----+             conn.write()
       |         |
    Conn<UDP>  Conn<UDP>
       |         |             sock.write()
    Sock<UDP>  Sock<UDP>
       |         |
     ~~~~~~~~~~~~~~~~~~
         MEDIA
     ~~~~~~~~~~~~~~~~~~    
       |         |             
     Sock<UDP>  Sock<UDP>       
       |         |             sock.read()
    Conn<UDP>   Conn<UDP>
       \         /
       MdServer<Conn<UDP>>