/*  Project 1: HTTP client server communicaiton 
    author: ZHAOKAI XU,
            ZICHAO WU,
            XUAN ZHANG 
*/
#include "HttpdServer.hpp"
using namespace std;

const int BUFSIZE = 1024;
const char* LINE_END = "\r\n";

const string OK200 = "200 OK\r\n";
const string ERR400 = "400 Client Error\r\n";
const string ERR403 = "403 Permission denied\r\n";
const string ERR404 = "404 Not Found\r\n";
const string CONN_CLOSED = "Connection: close\r\n";
const string SERVER_NAME = "Server: Server(chmod711)\r\n";
string DOC_ROOT;

//set up config for server, including port, doc_root, mineType
HttpdServer::HttpdServer(INIReader &t_config) : config(t_config)
{
    auto log = logger();

    string pstr = config.Get("httpd", "port", "");
    if (pstr == "")
    {
        log->error("port was not in the config file");
        exit(EX_CONFIG);
    }
    port = pstr;

    string doc_root = config.Get("httpd", "doc_root", "");
    if (doc_root == "")
    {
        log->error("doc_root was not in the config file");
        exit(EX_CONFIG);
    }
    if(realpath(doc_root.c_str(),NULL) == NULL ){
        log->error("doc root {} does not exist", DOC_ROOT);
        exit(1);
    }
    DOC_ROOT = realpath(doc_root.c_str(),NULL);

    // check if the mime_types path existed in config file
    string minetypestr = config.Get("httpd", "mime_types", "");
    if (minetypestr == "")
    {
        log->error("mineType was not is the config file");
        exit(EX_CONFIG);
    }
    minetypePath = minetypestr;
    log->info("minetype is {}",minetypePath);
}

// parse message and stored in HTTPRequest, given the msg format:
// <request initial line>[CRLF]
// Key1: Value1[CRLF]
// Key2: Value2[CRLF]
// ...
// KeyN: Value3[CRLF]
// [CRLF]
void HttpdServer::parseMessage(char* requestMessage, HTTPRequest *clntRequest) {
    
    auto log = logger();

    char *line = strtok(requestMessage, LINE_END);

    // ========== break the initLine ================= 
    // check if contains three strings, request path version
    string init_line = line;
    char request_charArray[1000];
    char symlinkpath_charArray[1000];
    char version_charArray[1000];

    //format check
    int para_num = std::count (init_line.begin(), init_line.end(), ' '); 
    if( para_num != 2){
        log->error("error in request header");
        clntRequest->Err400 = true;
        return;
    }

    sscanf(line, "%s %s %s", request_charArray, symlinkpath_charArray,  version_charArray);

    clntRequest->request =  string(request_charArray);
    string symlinkpath = string(symlinkpath_charArray);
    clntRequest->version = string(version_charArray);
    

    line = strtok(NULL, LINE_END);        
    bool hasHost=false;

    // =========== find realpath ====================
    //check if start with / else 400 error
    if(symlinkpath[0] != '/'){
        clntRequest->Err400 = true;
        return;
    }
    // concate doc_root to req uri
    string abosolutePath = DOC_ROOT + symlinkpath;
    //convert to abosolute path and store in uri
    char *actualpath;
    actualpath = realpath(abosolutePath.c_str(), NULL);
    if (actualpath == NULL)
    {
        clntRequest->Err404 = true;
        return;
    }

    // Default to index.html if requesting docroot
    if (strcmp(actualpath, &DOC_ROOT[0])==0) {
        strcat(actualpath, "/index.html");
    }

    clntRequest->uri = actualpath;

    // =========== check key value pairs ===================
    //check line by line if connection closed  OR host missing
    while (line)
    {
        string temp = line;

        int pos = temp.find(": ");
        //if valid  key value pair
        if (pos == -1)
        {
            log->error("key value pair invalid");
            clntRequest->Err400 = true;
            return;
        }
        string key = temp.substr(0, pos);
        string value = temp.substr(key.length() + 2);

        //check if contains host in keys, Do not care lower or upper case
        if ( strcasecmp (key.c_str(), "host") == 0 )
            hasHost = true;
        
        //check if connection is closed, Do not care lower or upper case
        if (strcasecmp(key.c_str(), "connection") == 0 && strcasecmp(value.c_str(), "close") == 0)
        {
            log->info("clinet close connection");
            clntRequest->connectionClosed = true;
        }
        //next line
        line = strtok(NULL, LINE_END);
    }

    if (hasHost==false){
        log->error("clinet request has no host");
        clntRequest->Err400 = true;
    }
   
    return;
}

//frame a response to be sent to client, used stored info in HTTPResponse
// <response initial line>[CRLF]
// Key1: Value1[CRLF]
// Key2: Value2[CRLF]
// ...
// KeyN: Value3[CRLF]
// [CRLF]
// <optional_body>
void HttpdServer::frameResponse(HTTPRequest *clntRequest, HTTPResponse *response)
{
    auto log = logger();


    struct stat filestat;

    //  400 error malform
    if (clntRequest->Err400)
    {
        response->resCode = ERR400;
    }
    // 404  file path incorrect
    // Check if file exists, or if accessing file outside DOC_ROOT directory
    else if ( (stat(clntRequest->uri.c_str(), &filestat) < 0) || string(clntRequest->uri).rfind(DOC_ROOT, 0) != 0)
    { 
        clntRequest->Err404 = true;
        response->resCode = ERR404;
    }
    // 403 No file read permission
    else if ((filestat.st_mode & S_IROTH) == 0)
    { 
        clntRequest->Err403 = true;
        response->resCode = ERR403;
    }
    // No errors - 200 OK !
    else {
        struct tm *gmt = gmtime(&(filestat.st_mtime));

        char lastMode_charArray[100];
        strcpy(lastMode_charArray,response->lastMode.c_str());

        strftime(lastMode_charArray, 1024, "Last-Modified: %a, %d %b %y %T %z\r\n", gmt);

        response->lastMode = lastMode_charArray;
        response->resCode = OK200;
        response->contLen = "Content-Length: " + to_string(filestat.st_size) + LINE_END;

        int pos = (clntRequest->uri).rfind("."); // find file extension

        //deal with MIME types if extension exists
        if (pos >= 0) 
        {
            string postfix = clntRequest->uri.substr(pos, string::npos);
                //if can find mime type
            if (minetypeMap.find(postfix) != minetypeMap.end()) { 
                response->conType = "Content-Type: " + minetypeMap[postfix] + LINE_END;
            } else {
                response->conType = string("Content-Type: application/octet-stream") + LINE_END;
            }
        } 
        else{
            response->conType = string("Content-Type: application/octet-stream") + LINE_END;
        }   
    }

    if (clntRequest->connectionClosed || clntRequest->Err400 )
    {   
        response->connect = CONN_CLOSED;
    }

    response->header ="HTTP/1.1 " + response->resCode + SERVER_NAME + response->lastMode
     + response->contLen + response->conType + response->connect + LINE_END;

    return;
}

// ========== parse config file into a mimetype hashmap ==========
void HttpdServer::storeMineTypesIntoMap()
{
    auto log = logger();
    string line;
    ifstream fd(minetypePath);

    if (fd.is_open())
    {
        while (getline(fd, line))
        {
            string key = line.substr(0, line.find(" "));
            string value = line.substr(key.length() + 1, line.length() - key.length());
            minetypeMap[key] = value;
        }
        fd.close();
    }
    else
    {
        log->error("Can't find the mime.type file");
    }
    return;
}

//send response header to clinet, Success or failed
void HttpdServer::sendResponseHeader(HTTPResponse *response, int client_sock)
{
    send(client_sock, (response->header).c_str(), (response->header).length(), 0);
}

//send file to client if request succeed
void HttpdServer::sendResponseFile(string &actualpath, int client_sock)
{
    auto log = logger();

    struct stat st; // stat to get file info
    size_t fileSize;
    int in_fd; //file descriptor

    //get file size
    if (stat(actualpath.c_str(), &st) != 0)
    {
        return;
    }
    fileSize = st.st_size;

    //open a file
    in_fd = open(actualpath.c_str(), O_RDONLY);

    if (in_fd == -1)
    {
        log->error("error: can not open the file");
        return;
    }

    // sendfile() copies data between one file descriptor and another.
    log->info("sending file ... ");
    sendfile(client_sock, in_fd, NULL, fileSize);
}


// ============= handle clinet request and make a response ===================
void  HttpdServer::handle_client(int client_sock)
{
    auto log = logger();

    string clntMessage;
    char buffer[BUFSIZE];
    memset(buffer, 0, BUFSIZE);
    ssize_t numBytesRcvd = recv(client_sock, buffer, BUFSIZE, 0);
    size_t position = 0;
    string request="";   


    if (numBytesRcvd < 0 && errno == EWOULDBLOCK){
        log->warn("closing socket due to timeout\n");
        close(client_sock);
        return;
    }
    if (numBytesRcvd < 0){
        log->error("recv() failed");
        return;
    }

    // Send received string and receive again until end of stream
    while(numBytesRcvd>0){  

        //append buffer data to clntMessage , clntMessage may contains many requests
        clntMessage += std::string(buffer);
        memset(buffer, 0, BUFSIZE);    
  
        //handle request by request 
        while(1){
            //go to the next request , split by "\r\n\r\n"
            position = clntMessage.find( "\r\n\r\n");

            if(position == string::npos){
                position = 0;
                break;
            }
            request = clntMessage.substr(0, position);
            clntMessage = clntMessage.substr(position+4); 

            //    ============  1  parseMessage   ============
            HTTPRequest *clntRequest = new HTTPRequest();
            log->info("start parseMessage()...");        
            parseMessage(&request[0], clntRequest);

            //    ============ 2  frameResponse ============
            HTTPResponse *response = new HTTPResponse();
            log->info("start frameResponse()...");
            frameResponse(clntRequest, response);
        
            //   ===========   3  send response to client ==========
            //send file header
            log->info("start sendResponseHeader()...");
            sendResponseHeader(response, client_sock);

            if (clntRequest->Err400){
                log->error("client error 400");
                break;
            }
            else if (clntRequest->connectionClosed ){
                log->info("start sendResponseFile()...");
                sendResponseFile(clntRequest->uri, client_sock);
                log->info("connection closed");
                break;
            }
            else if (clntRequest->Err403){
                log->error("client error 403");
            }
            else if (clntRequest->Err404){
                log->error("client error 404");
            }
            else{
                log->info("start sendResponseFile()...");
                sendResponseFile(clntRequest->uri, client_sock);
            }
            //free memory
            delete(clntRequest);
            delete(response);
        }

        numBytesRcvd = recv(client_sock, buffer, BUFSIZE, 0);

        if (numBytesRcvd < 0 && errno == EWOULDBLOCK){
            log->warn("closing socket due to timeout\n");
            close(client_sock);
            return;
        }
        if (numBytesRcvd < 0){
            log->error("recv() failed");
            return;
        }
    }

    //close client socket
    close(client_sock);
    log->warn("client closed");
    return;
}


//================== launch a server =========================
void HttpdServer::launch()
{
    auto log = logger();

    storeMineTypesIntoMap();

    log->info("Launching web server");
    log->info("Port: {}", port);
    log->info("doc_root: {}", DOC_ROOT);
    
    struct sockaddr_in serv_addr;             // Local address
    memset(&serv_addr, 0, sizeof(serv_addr)); // clear the address structure

    /* setup the host_addr structure for use in bind call */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    short portNumber = (short)stoi(port);   
    serv_addr.sin_port = htons(portNumber);

    // 1    create a socket 
    int server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock < 0) {
        log->error("ERROR create socket");
        exit(1);
    }

    //release port optionally
    int optval=1;
    setsockopt (server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof (optval));

    // 2    bind to an address/ port
    if (bind(server_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        log->error("ERROR on binding");
        exit(1);
    }
    // 3    listen on a port, and wait for a connection to be established.
    if (listen(server_sock, 5) < 0)   // set the listen queue to be size 5
    {
        log->error("listen failed");
        exit(1);
    }
    
    log->info("start Waiting for connections...");
    vector<thread *> workers; //thread stack created in heap to handle multiple connections

    // 4   handle request 
    while (true)
    {
        struct sockaddr_in clnt_addr;
        char clientName[16];

        struct timeval timeOut;
        timeOut.tv_sec = 5;         //set receive timeout to be 5 seconds 
        timeOut.tv_usec = 0;
       
        // 4.1  try to accept the connection from a client.
        socklen_t clntAddrLen = sizeof(clnt_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&clnt_addr, &clntAddrLen);
        auto flag1 = inet_ntop(AF_INET, &clnt_addr.sin_addr.s_addr, clientName, sizeof(clientName));
        setsockopt (client_sock, SOL_SOCKET, SO_RCVTIMEO,(char *)&timeOut, sizeof(timeOut) );//set timeout for client connection
        setsockopt (client_sock, SOL_SOCKET, SO_SNDTIMEO,(char *)&timeOut, sizeof(timeOut) );

        if (client_sock == -1)
        {
            log->error("can not resolve this client socket ");
        }
        else if ( flag1 == NULL)
        {
            log->error("Unable to get client address/port");
        }
        else{
            //4.2   handle  clienet using
            log->info("Handling client {}", clientName);
            thread *t = spawn(client_sock);
            t->detach();
            workers.push_back(t);
        }
    }
    // 5    close server 
    close(server_sock);
}




