#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <limits.h>
#define BUFSIZE 32768
#define BODYSIZE 16384
#define HEADERSIZE 4096
#define PORT 2020
#define MAXCLIENTNUM 32768
#define PYTHONPATH "/sbin/python3"
typedef struct {
    int conn_fd;  // fd to talk with client
    char inbuf[BUFSIZE];  // data sent by client
    char outbuf[BUFSIZE]; // data sent to client
    size_t inbuf_len;
    size_t outbuf_len;
} request;
typedef struct{
    char method[8];
    char target[64];
    char version[16];
    char headers[HEADERSIZE];
    char body[BODYSIZE];
} HTTPReq; //http request
typedef struct{
    char version[16];
    char status[4];
    char reason[16];
    char headers[HEADERSIZE];
    char body[BODYSIZE];
} HTTPRes; //http response
request* clients[MAXCLIENTNUM];
int client_num;
int sessionId;

int init_server(unsigned short port);
HTTPReq* parse_request(char* req_lines, int length);
void encapsulate_response(HTTPRes* res, char* mes, size_t* mes_len);
void handle_request(HTTPReq* req, HTTPRes* res);
void get_sessionId(char* headers, char* sessionId);
int classify_url(char* str){
    int len = strlen(str);
    int last_slash_pos = 0;
    for (int i=0;i<len;i++){
        if (str[i]=='/'){
            last_slash_pos = i;
        }
    }
    if (strcmp(str+last_slash_pos+1,"messages.json")==0) return 1;
    else if (strcmp(str+last_slash_pos+1,"register.html")==0) return 2;
    else if (strcmp(str+last_slash_pos+1,"homepage.html")==0) return 3;
    else if (strcmp(str+last_slash_pos+1,"bulletin_board.html")==0) return 4;
    else return 0;
}

int main(int argc, char* argv[]){
    client_num=0;
    sessionId=0;
    int listening_fd = init_server(PORT);
    //fprintf(stderr, "start of error file\n");
    //fflush(stderr);

    //for I/O multiplexing
    int maxfd;
    fd_set readfds; 

    //for accepting new clients
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    int conn_fd = 0;
    char host_addr[1024]; //client host address
    while (1){
        FD_ZERO(&readfds);
        FD_SET(listening_fd, &readfds);  
        maxfd = listening_fd;
        for (int i=0;i<client_num;i++){
            if (clients[i] == NULL) continue;
            FD_SET(clients[i]->conn_fd,&readfds);
            if (clients[i]->conn_fd > maxfd){
                maxfd = clients[i]->conn_fd;
            }
        }
        int s = select(maxfd+1, &readfds, NULL, NULL, NULL);
        if (s < 0 && errno!=EINTR){
            perror("select error");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(listening_fd, &readfds)){
            conn_fd = accept(listening_fd, (struct sockaddr*)&client_addr,(socklen_t*)&client_addr_len);
            if (conn_fd<0){
                if (errno == EINTR || errno == EAGAIN) continue; // try again
                if (errno == ENFILE) {
                    perror("out of file descriptor table\n");
		            fflush(stderr);
                    continue;
                }
                perror("accept failed\n");
		        fflush(stderr);
                continue;
                //exit(EXIT_FAILURE);
            }
            strcpy(host_addr,inet_ntoa(client_addr.sin_addr));
            printf("Accept a new connection from %s (id: %d)\n",host_addr,client_num);
            clients[client_num] = (request*)malloc(sizeof(request));
            clients[client_num]->conn_fd = conn_fd;
            clients[client_num]->inbuf_len = 0;
            clients[client_num]->outbuf_len = 0;
            client_num++;
	        fflush(stdout);
        }

        for (int i=0;i<client_num;i++){
            if (clients[i] == NULL) continue;
            int fd = clients[i]->conn_fd;
            if (FD_ISSET(fd, &readfds)){
                int read_bytesnum = read(fd, clients[i]->inbuf,BUFSIZE);
                if (read_bytesnum < 0){
                    perror("reading from socket failed\n");
		            fflush(stderr);
                    continue;
                }
                else if (read_bytesnum == 0){
                    close(fd);
                    request* old = clients[i];
                    free(old);
                    clients[i] = NULL;
                    printf("Client %d disconnected.\n",i);
                }
                else{
                    clients[i]->inbuf_len = (size_t)read_bytesnum;
                    clients[i]->inbuf[clients[i]->inbuf_len] = '\0';
                    printf("--------------------------------------------------\n");
                    printf("get message with %d characters from client %d:\n",read_bytesnum,i);
                    printf("%s\n",clients[i]->inbuf);
                    HTTPReq* req = parse_request(clients[i]->inbuf, clients[i]->inbuf_len);
                    HTTPRes* res = (HTTPRes*)malloc(sizeof(HTTPRes));
                    handle_request(req,res);
                    encapsulate_response(res, clients[i]->outbuf,&(clients[i]->outbuf_len));
                    printf("--------------------------------------------------\n");
                    printf("%s",clients[i]->outbuf);
                    send(clients[i]->conn_fd,clients[i]->outbuf,clients[i]->outbuf_len,0);
                    printf("HTTP Response message sent to client %d!\n",i);
                    free(req);
                    free(res);

                    //close connection
                    close(clients[i]->conn_fd);
                    request* old = clients[i];
                    free(old);
                    clients[i] = NULL;
                    printf("Client %d disconnected.\n",i);
                }
		        fflush(stdout);
            }
        } 
    }
}

int init_server(unsigned short port){
    int listening_fd;
    struct sockaddr_in addr;
    listening_fd = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (listening_fd < 0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    int tmp = 1;
    if (setsockopt(listening_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, (void*)&tmp, sizeof(tmp))<0){
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket created.\n");

    if (bind(listening_fd, (struct sockaddr*)&addr,sizeof(addr))<0){
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Socket binded to port %d\n",port);

    if (listen(listening_fd, 1024)<0){
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    printf("Listening...\n");
    fflush(stdout);
    return listening_fd;
}

void handle_request(HTTPReq* req, HTTPRes* res){
    if (req==NULL){
        strcpy(res->version, "HTTP/1.1");
        strcpy(res->status, "400");
        strcpy(res->reason, "Bad Request");
        strcpy(res->headers,"Connection: close\r\n");
        strcpy(res->body,"");
    }
    else{
        int req_file_fd = open(req->target,O_RDONLY);
        if (req_file_fd < 0){
            strcpy(res->version, req->version);
            strcpy(res->status, "404");
            strcpy(res->reason,"Not Found");
            strcpy(res->headers,"Connection: close\r\n");
            strcpy(res->body,"");
        }
        else{
            if (strcmp(req->method,"POST")==0){
                if (classify_url(req->target)==1){
                    char sessionId_str[64];
                    get_sessionId(req->headers,sessionId_str);

                    if (fork()==0){
                        //child process
                        execl(PYTHONPATH,"python3","./update_messages.py","--new_message",req->body,"--sessionId",sessionId_str,NULL);
                    }
                    else{
                        wait(NULL);
                        strcpy(res->headers,"Connection: close\r\n");
                    }
                }
                else if (classify_url(req->target)==2){
                    if (fork()==0){
                        //child process
                        execl(PYTHONPATH,"python3","./add_user.py","--new_user",req->body,NULL);
                    }
                    else{
                        wait(NULL);
                        strcpy(res->headers,"Connection: close\r\n");
                    }
                }
                else if (classify_url(req->target)==3){
                    char sessionId_str[64];
                    sprintf(sessionId_str,"%d",sessionId);
                    //printf("session id: %s\n",sessionId_str);
                    if (fork()==0){
                        execl(PYTHONPATH,"python3","./login.py","--login_user",req->body,"--sessionId",sessionId_str,NULL);
                    }
                    else{
                        int wstatus;
                        wait(&wstatus);
                        //printf("%d\n",*wstatus);
                        
                        if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus)==0){
                            sprintf(res->headers,"Connection: close\r\nSet-Cookie: sessionId=%s\r\n",sessionId_str);
                            //printf("headers:\n%s\n",res->headers);
                            sessionId++;
                        }
                        else{
                            strcpy(res->headers,"Connection: close\r\nSet-Cookie: sessionId=; Max-Age=0\r\n");
                        }
                    }  
                }
                else {
                    char sessionId_str[64];
                    get_sessionId(req->headers,sessionId_str);
                    if (fork()==0){
                        execl(PYTHONPATH,"python3","./logout.py","--sessionId",sessionId_str,NULL);
                    }
                    else{
                        wait(NULL);
                        strcpy(res->headers,"Connection: close\r\nSet-Cookie: sessionId=; Max-Age=0\r\n");
                    }
                }
                ssize_t req_file_size = read(req_file_fd, res->body, BODYSIZE);
                if (req_file_size < 0){
                    //read error
                    strcpy(res->version, req->version);
                    strcpy(res->status, "200");
                    strcpy(res->reason,"OK");
                    strcpy(res->body,"");
                }
                else{
                    strcpy(res->version, req->version);
                    strcpy(res->status, "200");
                    strcpy(res->reason,"OK");
                    res->body[req_file_size]='\0';
                }
            }
            else if (strcmp(req->method,"GET")==0){
                ssize_t req_file_size = read(req_file_fd, res->body, BODYSIZE);
                if (req_file_size < 0){
                    //read error
                    strcpy(res->version, req->version);
                    strcpy(res->status, "200");
                    strcpy(res->reason,"OK");
                    strcpy(res->headers,"Connection: close\r\n");
                    strcpy(res->body,"");
                }
                else{
                    strcpy(res->version, req->version);
                    strcpy(res->status, "200");
                    strcpy(res->reason,"OK");
                    strcpy(res->headers,"Connection: close\r\n");
                    res->body[req_file_size]='\0';
                }
            }
            else{
                strcpy(res->version, req->version);
                strcpy(res->status, "200");
                strcpy(res->reason,"OK");
                strcpy(res->headers,"Connection: close\r\n");
                strcpy(res->body,"");
            }
            close(req_file_fd);
        }
    }
}

HTTPReq* parse_request(char* req_lines, int length){
    if (length<3) return NULL;
    HTTPReq* req = (HTTPReq*)malloc(sizeof(HTTPReq));
    int state = 0; //0: method; 1: target; 2: version; 3: headers; 4: body
    int start = 0; //start position of each field 
    for (int i=0; i<length; i++){
        if (state==0 && req_lines[i]==' '){
            strncpy(req->method,req_lines+start,i-start);
            req->method[i-start]='\0';
            //printf("%s\n",req->method);
            state++;
            start=i+1;
        }
        else if (state==1 && req_lines[i]==' '){
            strncpy(req->target,req_lines+start,i-start);
            req->target[i-start]='\0';
            //printf("%s\n",req->target);
            state++;
            start=i+1;
        }
        else if (state>=2 && state<4 && i+3>=length){
            free(req);
            return NULL;
        }
        else if (state==2 && req_lines[i]=='\r' && req_lines[i+1]=='\n' && req_lines[i+2]=='\r' && req_lines[i+3]=='\n'){
            // the request does not contain headers
            strncpy(req->version, req_lines+start, i-start);
            req->version[i-start]='\0';
            strcpy(req->headers, "");
            //printf("%s\n",req->version);
            //printf("%s\n",req->headers);
            state+=2;
            start=i+4;
        }
        else if (state==2 && req_lines[i]=='\r' && req_lines[i+1]=='\n'){
            strncpy(req->version, req_lines+start, i-start);
            req->version[i-start]='\0';
            //printf("%s\n",req->version);
            state+=1;
            start=i+2;
        }
        else if (state==3 && req_lines[i]=='\r' && req_lines[i+1]=='\n' && req_lines[i+2]=='\r' && req_lines[i+3]=='\n'){
            strncpy(req->headers, req_lines+start, i+2-start);
            req->headers[i+2-start]='\0';
            //printf("%s\n",req->headers);
            state+=1;
            start=i+4;
        }
    }
    //printf("state: %d\n",state);
    if (state==4){
        strncpy(req->body, req_lines+start,length-start);
        req->body[length-start]='\0';
        //printf("%s\n",req->body);
        return req;
    }
    else{
        free(req);
        return NULL;
    }

    /*
    HTTPReq* req = (HTTPReq*)malloc(sizeof(HTTPReq));
    char* t = req_lines;
    char* s = req_lines;
    while (*t != ' ') t=t+1;
    strncpy(req->method, s, t-s);
    req->method[t-s] = '\0';
    t = t+1;
    s = t;
    while(*t != ' ') t=t+1;
    strncpy(req->target, s, t-s);
    req->target[t-s] = '\0';
    t = t+1;
    s = t;
    while(*t != '\r') t=t+1;
    strncpy(req->version, s, t-s);
    req->version[t-s] = '\0';
    strcpy(req->headers, "");
    return req;
    */
}

void encapsulate_response(HTTPRes* res, char* mes, size_t* mes_len){
    int t=0;
    strcpy(mes, res->version);
    t = strlen(mes);
    mes[t++] = ' ';
    strcpy(mes+t, res->status);
    t = strlen(mes);
    mes[t++] = ' ';
    strcpy(mes+t, res->reason);
    t = strlen(mes);
    mes[t++] = '\r';
    mes[t++] = '\n';
    strcpy(mes+t, res->headers);
    t = strlen(mes);
    mes[t++] = '\r';
    mes[t++] = '\n';
    strcpy(mes+t, res->body);
    t = strlen(mes);
    mes[t]='\0';
    *mes_len = (size_t)t;
}

void get_sessionId(char* headers, char* sessionId){
    int len = strlen(headers);
    int start = 0; //start of a header
    char header[HEADERSIZE];
    short have_cookie_header = 0;
    for (int i=0; i<len-1; i++){
        if (headers[i]=='\r' && headers[i+1]=='\n'){
            strncpy(header,headers+start,i-start);
            header[i-start]='\0';
            if (strlen(header)>=6 && strncmp(header,"Cookie",6)==0){
                have_cookie_header = 1;
                break;
            }
            start = i+2;
        }
    }
    if (have_cookie_header==0){
        strcpy(sessionId,"");
    }
    else if (strncmp(header+8,"sessionId=",10)==0){
        strcpy(sessionId,header+18);
    }
    else{
        strcpy(sessionId,"");
    }
    //printf("%s\n",sessionId);
}
