### 1    build TCP socket model 
steps: create     setopt    bind     listen     looping{    accept    handleRequest}
ref: https://www.geeksforgeeks.org/socket-programming-cc/

### 2    handleRequest 
    2.1        reads multiple requests into a string     like R1/R2/R3...   
    2.2       looping to break the requests String and do
        2.21    parsing request 
        2.22    framing response 
        2.23    send header
        2.24    send files         (only if 200 allowed )


### 3   Testing
    3.1     sending single request
        illegal path?   relative path? 
        file permission check
        different file types:    jpg png  html txt 
    
    3,2   multiple  requests in one client message
    EX cat  <(printf "GET /testing.txt HTTP/1.1\r\nHost: localhost\r\n\r\n") <(printf "GET /testing2.txt HTTP/1.1\r\nHost: localhost\r\n\r\n") <(printf "GET /testing3.txt HTTP/1.1\r\nHost: localhost\r\n\r\n") <(printf "GET /testing4.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")   | nc localhost 9000
    
    3.3     multi threading to handle multi users requesting and the same time       
    
    3.4     handle timeout. 
            set time out to be 5 seconds for each user, if no request recieved in 5 sec, cut connection.


### Project summary, Can do better next time?
    1   Overview, 
        find relative resources to understand, find TCP socket programming tamplate
        break projects into steps, 
          
    2   Coding 
        apply encapusalation and modularization to design classes and functions 
        focus on programming logic 
            for those new methods to be used, like sys calls, read manual, test individually and then apply in project
        TDD test driven development. Do not test all together, write part and test, then write the following

    3   Testing to handle edge cases
        brainstorming to test based on functionalities  
        think deeply
        
