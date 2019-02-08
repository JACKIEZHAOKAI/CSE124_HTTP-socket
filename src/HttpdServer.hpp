#ifndef HTTPDSERVER_HPP
#define HTTPDSERVER_HPP
#include "inih/INIReader.h"
#include "logger.hpp"
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <sysexits.h>
#include <sys/sendfile.h>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <algorithm>
#include <bits/stdc++.h> // string to char *
#include <sys/time.h>
#include <thread>
#include <algorithm>   
#include <vector> 
using namespace std;

class HTTPRequest
{
  public:
    string request = "";
    string uri = "";
    string version = ""; 
    
    bool Err400 = false; 
    bool Err403 = false; 
    bool Err404 = false; 
    bool hasHost = false;
    bool connectionClosed = false;
};

class HTTPResponse
{
  public:
    string resCode = "";
    string lastMode = "";
    string conType = "";
    string contLen = string("Content-Length: 0") + "\r\n";
    string header = "";
    string connect = "";
};


class HttpdServer {
  public:
    HttpdServer(INIReader &t_config);
    void parseMessage(char*, HTTPRequest *);
    void frameResponse(HTTPRequest*, HTTPResponse*);
    void storeMineTypesIntoMap();
    void sendResponseHeader(HTTPResponse*, int);
    void sendResponseFile(std::string&, int);
    void handle_client(int);
    void launch();
    //multi threading
    thread* spawn(int &client_sock){
        return new thread([this,client_sock]{this->handle_client(client_sock);});
    };

  protected:
    INIReader &config;
    string port;
    string doc_root;
    string minetypePath; // added class field
    unordered_map<string, string> minetypeMap;
};

#endif // HTTPDSERVER_HPP
