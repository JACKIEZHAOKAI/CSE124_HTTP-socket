## build HTTP client -server connection 
https://cseweb.ucsd.edu/~gmporter/classes/wi19/cse124/projects/pa1/

HTTP model
https://cseweb.ucsd.edu/~gmporter/classes/wi19/cse124/post/2019/01/12/tritonhttp-specification/


## HTTPServer.cc
#### 1    搭建TCP socket 可以直接套用模板
create     setopt    bind     listen     looping{    accept    handleRequest}
https://www.geeksforgeeks.org/socket-programming-cc/

#### 2    handleRequest 
    2.1        reads multiple requests into a string     like R1/R2/R3...   
    2.2       looping to break the requests String and do
        2.21    parsing request 
        2.22    framing response 
        2.23    send header
        2.24    send files         (only if 200 allowed )


### 3   Testing
    单个request发送
        合法文件路径    相对路径    
        不同文件类型    jpg png  html txt  ….
    multiple  requests     一起发送
cat  <(printf "GET /testing.txt HTTP/1.1\r\nHost: localhost\r\n\r\n") <(printf "GET /testing2.txt HTTP/1.1\r\nHost: localhost\r\n\r\n") <(printf "GET /testing3.txt HTTP/1.1\r\nHost: localhost\r\n\r\n") <(printf "GET /testing4.txt HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")   | nc localhost 9000
    multi threading 多用户同时请求        
    handle timeout    5seconds无连接？部分数据收到？



### 项目收获总结：
    1      整理思路    找资源  
        先大致理解概念    理清思路    读参考代码框架    借鉴模板
        这部分在开始的时候花费过多时间，比如读网站的project内容有很多困惑
        没写过后端代码不知道从何下手，对项目的预算时间没有合理估量
        
    2    开始写代码    分模块    
                    按照功能可以在写的过程中封装函数    也可以最后统一refactor code
          代码最重要是逻辑思维    和代码框架    适当备注每个过程的主要任务
                有些不熟悉的system call可以先读manual    然后单独测试    测好了拿来用
                代码逻辑理顺了可以直接分部测试，不需要完全写完再测试，可以分阶段测试。

    3    测试代码 handle edge cases
                根据函数的功能    brainstorming
