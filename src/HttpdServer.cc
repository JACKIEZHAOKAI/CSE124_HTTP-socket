/*  Project 1: HTTP client server communicaiton 
    author: ZHAOKAI XU,
            ZICHAO WU,
            XUAN ZHANG 
*/
#include "HttpdServer.hpp"
using namespace std;

const int BUFSIZE = 8192;
const char* LINE_END = "\r\n";

const string OK200 = "200 OK\r\n";
const string ERR400 = "400 Client Error\r\n";
const string ERR403 = "403 Forbidden\r\n";
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
    // log->info("minetype is {}",minetypePath);
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

    // log->info("requestMessage {}", requestMessage);
    char *line = strtok(requestMessage, LINE_END);
    string init_line = line;
    
    // ========== break the initLine ================= 
    string symlinkpath="";
    clntRequest->request = init_line.substr(0, init_line.find(" "));

    string rest = init_line.substr( (clntRequest->request).length() + 1);
    symlinkpath = rest.substr(0, rest.find(" "));

    clntRequest->version = rest.substr(symlinkpath.length() + 1);


    line = strtok(NULL, LINE_END);        
    bool hasHost=false;
 
    // =========== find realpath ====================
    // concate doc_root to req uri
    string abosolutePath = DOC_ROOT + symlinkpath;
    //convert to abosolute path and store in uri
    char *actualpath;
    actualpath = realpath(abosolutePath.c_str(), NULL);
    if (actualpath == NULL)
    {
        log->error("error in path resolution");
        clntRequest->requestErr = true;
        return;
    }

    // Default to index.html if requesting docroot
    if (strcmp(actualpath, &DOC_ROOT[0])==0) {
        strcat(actualpath, "/index.html");
    }

    clntRequest->uri = actualpath;

    while (line)
    {
        string temp = line;
        //check line by line if connection closed  OR host missing
        int pos = temp.find(": ");
        //if valid  key value pair
        if (pos == -1)
        {
            log->error("key value pair invalid");
            clntRequest->requestErr = true;
            return;
        }
        string key = temp.substr(0, pos);
        string value = temp.substr(key.length() + 2);

        //check if contains host in keys, Do not care lower or upper case
        if ( strcasecmp (key.c_str(), "host") == 0 )
            hasHost = true;
        
        //check if connection is closed, Do not care lower or upper case
        if (strcasecmp(key.c_str(), "connection") == 0 && strcasecmp(value.c_str(), "connection: close") == 0)
        {
            log->info("clinet closing connection");
            clntRequest->connectionClosed = true;
        }
        //next line
        line = strtok(NULL, LINE_END);
    }

    if (hasHost==false){
        log->error("clinet request has no host");
        clntRequest->requestErr = true;
    }
    
    //free memory
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

    // Any error in request header parsing is 400 client error
    if (clntRequest->requestErr)
    {
        response->resCode = ERR400;

    }

    struct stat filestat;

    // Check if file exists, or if accessing file outside DOC_ROOT directory
    if ( (stat(clntRequest->uri.c_str(), &filestat) < 0 && (clntRequest->requestErr == false))
        || string(clntRequest->uri).rfind(DOC_ROOT, 0) != 0)
    { 
        clntRequest->requestErr = true;
        log->error("error in stat() to get info from the file");
        response->resCode = ERR404;
    }

    // No file read permission
    if ((filestat.st_mode & S_IROTH) == 0 && (clntRequest->requestErr == false))
    { 
        clntRequest->requestErr = true;
        log->error("No file read permission");
        response->resCode = ERR403;
    }

    // No errors - 200 OK !
    if (!clntRequest->requestErr)
    {
        struct tm *gmt = gmtime(&(filestat.st_mtime));

        char lastMode_charArray[100];
        strcpy(lastMode_charArray,response->lastMode.c_str());

        strftime(lastMode_charArray, 1024, "Last-Modified: %a, %d %b %y %H:%M:%S GMT\r\n", gmt);

        response->lastMode = lastMode_charArray;
        response->resCode = OK200;
        response->contLen = "Content-Length: " + to_string(filestat.st_size) + LINE_END;

        int pos = (clntRequest->uri).rfind(".");

        // Get MIME type if existt
        if (pos >= 0)
        {
            string postfix = clntRequest->uri.substr(pos, string::npos);
            // MIME type do not exist in mime.types
            if (minetypeMap.find(postfix) == minetypeMap.end())
            {
                log->error("MIME type do not exist in mime.types");
            }
            else
            { // MIME type exists in mime.types
                log->info("reading mine type");
                response->conType = "Content-Type: " + minetypeMap[postfix] + LINE_END;
            }
        }
        response->Server = SERVER_NAME;
    }

    if (clntRequest->connectionClosed)
    {   
        response->connect = CONN_CLOSED;
    }

    response->header = clntRequest->version + " " + response->resCode + response->Server + response->lastMode
     + response->contLen + response->conType + response->connect + LINE_END;

    return;
}

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
        log->error("error in stat() to get info from the file");
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

int HttpdServer::readClientRequests(string& clntMessage,int client_sock){
    
    auto log = logger();
    
    char buffer[BUFSIZE];
    ssize_t clntRqstSize;
         
    while (true) {
        // read data from socket
        clntRqstSize = recv(client_sock, buffer, BUFSIZE, 0);
        //check edge case
        if (clntRqstSize == -1)
        {
            log->error("error in received message crush");
            return -1;
        }
        if (clntRqstSize == 0)
        {
            log->error("peer client close connection");
            return 0;
        }
        //store buffer into clntMessage
        clntMessage += string(buffer);
        // clear the buffer
        memset(buffer, '\0', BUFSIZE);
        //store and break if read all messages from client
        if (clntRqstSize < BUFSIZE){
            break;
        }
    }
    return 1;
}

// handle clinet request and make a response
void  HttpdServer::handle_client(int client_sock)
{
    auto log = logger();

    string clntMessage = "";
    // ==============   0 read all clinet requests into clntMessag=======
    int res = readClientRequests(clntMessage, client_sock);
    //inf recv wrong request 
    if (res == 0 || res ==  -1){
        //send file
        // log->info("start sendResponseFile()...");
        // sendResponseFile(clntRequest->uri, client_sock);
        return;
    }

    size_t position = 0;
    string rest = clntMessage;
    string clientMsg="";        // a single clientMsg does not include \r\n\r\n

    //handle request by request 
     while(1){
        //go to the next request , split by "\r\n\r\n"
        position = rest.find( "\r\n\r\n");
        if(position == string::npos)
            break;
        clientMsg =  rest.substr(0, position);
        rest = rest.substr(position+4);

        //    ============  1  parseMessage   ============
        HTTPRequest *clntRequest = new HTTPRequest();
        log->info("start parseMessage()...");        
        parseMessage(&clientMsg[0], clntRequest);

        //    ============ 2  frameResponse ============
        HTTPResponse *response = new HTTPResponse();
        log->info("start frameResponse()...");
        frameResponse(clntRequest, response);
    
        //   ===========   3  send response to client ==========
        //send file header
        log->info("start sendResponseHeader()...");
        sendResponseHeader(response, client_sock);

        if (!clntRequest->requestErr) {
            //send file
            log->info("start sendResponseFile()...");
            sendResponseFile(clntRequest->uri, client_sock);
        } else {
            log->error("client error 400");
            break;
        }
    }
    //close client socket
    close(client_sock);
    log->info("warning: client sock {} closed after 5 seconds", client_sock);

    return;
}

void HttpdServer::launch()
{
    // ========== parse config file into a mimetype hashmap ==========
    storeMineTypesIntoMap();

    //================== set up a server =========================
    auto log = logger();

    log->info("Launching web server");
    log->info("Port: {}", port);
    log->info("doc_root: {}", DOC_ROOT);

    // Put code here that actually launches your webserver...

    // Build a server address structure
    // struct sockaddr_in {
    //     short            sin_family;   // e.g. AF_INET
    //     unsigned short   sin_port;     // e.g. htons(3490)
    //     struct in_addr   sin_addr;     // see struct in_addr, below
    //     char             sin_zero[8];  // zero this if you want to
    // };
    struct sockaddr_in serv_addr;             // Local address
    memset(&serv_addr, 0, sizeof(serv_addr)); // clear the address structure

    /* setup the host_addr structure for use in bind call */
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    short portNumber = (short)stoi(port);    //cast string to short
    serv_addr.sin_port = htons(portNumber);

    // 1    create a socket - Get the file descriptor!
    int server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock < 0) {
        log->error("ERROR create socket");
        exit(1);
    }
    // 2    bind to an address -What port am I on?
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
    vector<thread *> workers; //thread stack created in heap to handle connections

    // 4   handle request 
    while (true)
    {
        struct sockaddr_in clnt_addr;
        char clientName[16];

        struct timeval recvTimeout;
        recvTimeout.tv_sec = 5;         //set receive timeout to be 5 seconds 
        recvTimeout.tv_usec = 0;
        int optval=1;

        // 5.1    try to accept the connection from a client.
        socklen_t clntAddrLen = sizeof(clnt_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&clnt_addr, &clntAddrLen);
        auto flag1 = inet_ntop(AF_INET, &clnt_addr.sin_addr.s_addr, clientName, sizeof(clientName));
        auto flag2 = setsockopt (client_sock, SOL_SOCKET, SO_RCVTIMEO , (char *)&recvTimeout, sizeof(recvTimeout) );
        auto flag3 = setsockopt (server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof (optval));

        // go to next connection IF this clinet socket failed
        if (client_sock == -1)
        {
            log->error("can not resolve this client socket ");
        }
        // go to next connection IF read invalid client address
        else if ( flag1 == NULL)
        {
            log->error("Unable to get client address");
        }
        else if ( flag2 < 0){
            log->error("failed to set timeout for this client");
        }
        else if ( flag3 < 0) {
            log->error("setsockopt released");
        }
        else{
            //5.2   handle multi clienet connections using multi-threading
            log->info("Handling client {}", clientName);
            thread *t = spawn(client_sock);
            t->detach();
            workers.push_back(t);
        }
    }
    // 6    close server to releases data.
    close(server_sock);
}




