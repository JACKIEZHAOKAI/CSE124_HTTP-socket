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

using namespace std;

// Defined class to store HTTP request components
class HTTPRequest
{
  public:
    string request = "";
    string uri = "";
    string version = ""; 
    //error while reading the clinet request 
    // bool 404Error =false;
    bool requestErr = false; 
    bool hasHost = false;
    bool connectionClosed = false;
};

// Defined class to store HTTP Response components
class HTTPResponse
{
  public:
    string resCode = "";
    string Server = "";
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
    int readClientRequests(std::string&, int);
    void handle_client(int);
    void launch();

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
