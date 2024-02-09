task 1: Event-driven approach


task 2: Thread-driven approach


### Run
- compile
`$ make`

- stockserver
	`$ ./stockserver [port number]`
    
- stockclient
`$ ./stockclient [server's IP address] [port number]`

- multiclient
`$ ./multiclient [server's IP address] [port number] [# of clients]`


### Client Commands
1. `show`
현재 주식의 상태를 보여준다.

2. `buy [stock ID] [# of stocks]`
주식 구매

3. `sell [stock ID] [# of stocks]`
주식 판매

4. `exit`
disconnection with server(주식 장 퇴장)
