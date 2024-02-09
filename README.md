3. How to run
compile

$ make
stockserver 실행

$ ./stockserver [port number]
stockclient 실행

$ ./stockclient [server's IP address] [port number]
multiclient 실행

$ ./multiclient [server's IP address] [port number] [# of clients]



4. Allowed Commands
주식 내역 보기 (주식 테이블 출력)
>> show
주식 주문하기
>> buy [stock ID] [# of stocks]
주식 판매하기
>> sell [stock ID] [# of stocks]
주식장 나가기 (클라이언트 접속 종료)
>> exit
