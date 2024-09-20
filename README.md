

  
Task 1: Event-driven approach
- select() 함수를 통한 I/O Multiplexing

Task 2: Thread-based approach
- POSIX thread를 활용하여 멀티스레드를 통한 동시성 구현
- 세마포어, 뮤텍스를 활용한 멀티스레드 동기화 메커니즘 구현

공통사항: 소켓 프로그래밍을 활용한 서버-클라이언트 통신 구현

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
