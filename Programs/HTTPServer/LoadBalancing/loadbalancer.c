#include<err.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include "queue.h"
void * thread_function();
void * handle_connection(void * args);
void * healthCheck(int fd);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;

size_t rReceived;
uint8_t done = 0;
int interval;



typedef struct serversObj{
    
    int port;
    int fd;
    int healthfd;
    char response[100];
    int requests;
    int errors;
    
} serverObj;

serverObj *servers;
size_t serverCount;

/*
 * client_connect takes a port number and establishes a connection as a client.
 * connectport: port number of server to connect to
 * returns: valid socket if successful, -1 otherwise
 */
int client_connect(uint16_t connectport) {
    int connfd;
    struct sockaddr_in servaddr;

    connfd=socket(AF_INET,SOCK_STREAM,0);
    if (connfd < 0)
        return -1;
    memset(&servaddr, 0, sizeof servaddr);

    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(connectport);

    /* For this assignment the IP address can be fixed */
    inet_pton(AF_INET,"127.0.0.1",&(servaddr.sin_addr));

    if(connect(connfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) < 0)
        return -1;
    return connfd;
}

/*
 * server_listen takes a port number and creates a socket to listen on 
 * that port.
 * port: the port number to receive connections
 * returns: valid socket if successful, -1 otherwise
 */
int server_listen(int port) {
    int listenfd;
    int enable = 1;
    struct sockaddr_in servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
        return -1;
    memset(&servaddr, 0, sizeof servaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
        return -1;
    if (bind(listenfd, (struct sockaddr*) &servaddr, sizeof servaddr) < 0)
        return -1;
    if (listen(listenfd, 500) < 0)
        return -1;
    return listenfd;
}

/*
 * bridge_connections send up to 100 bytes from fromfd to tofd
 * fromfd, tofd: valid sockets
 * returns: number of bytes sent, 0 if connection closed, -1 on error
 */
//int counter = 0;
int bridge_connections(int fromfd, int tofd) {
    char recvline[100];
    int n = recv(fromfd, recvline, 100, 0);
    //counter += n;
    printf("%d\n", n);
    if (n < 0) {
        printf("connection error receiving\n");
        return -1;
    } else if (n == 0) {
        printf("receiving connection ended\n");
        return 0;
    }
    recvline[n] = '\0';
    n = send(tofd, recvline, n, 0);
    if (n < 0) {
        printf("connection error sending\n");
        return -1;
    } else if (n == 0) {
        printf("sending connection ended\n");
        return 0;
    }
    return n;
}


void * healthCheck(int fd){
    char health[200];
    int n = 0;
    
    strcpy(health, "GET /healthcheck HTTP/1.1\r\n\r\n");
    n = strlen(health);

    
    n = send(fd, health, n, 0);
    
    return NULL;
    
    
}

//Sends helath checks to servers every R requests.
void * healthThreadFunction(){
    //int fds[serverCount];
    char recvLine[100];
    char response[100] = "";
    int n = 1;
    int contentL = 0;
        
    
    while(1){
        if(!(rReceived %interval == 0 && done == 0)){
            continue;
        }
        
        if(rReceived == 0){
            continue;
        }
        
        done = 1;
        printf("GOT HERE\n");
        
        for(size_t i = 0; i < serverCount; i++){
            if ((servers[i].healthfd = client_connect(servers[i].port)) < 0){
                err(1, "failed connecting");
            }
            healthCheck(servers[i].healthfd);
            
        }
        
        
        for(size_t i=0;i<serverCount;i++){
            n = 1;
            
            while(n>0){
                n = recv(servers[i].healthfd, recvLine, 100, 0);
                strcpy(response,strcat(response,recvLine));
                memset(recvLine,'\0',100);
            }
            strcpy(servers[i].response, response);
            sscanf(servers[i].response, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n%d\n%d", &contentL, &servers[i].errors, &servers[i].requests);
            printf("num of requests: %d\n", servers[i].requests);
            memset(response,'\0',100);
            close(servers[i].healthfd);
        }
        
    }
    
    printf("--%s--\n",servers[0].response);
    printf("--%s--\n",servers[1].response);
    
    return NULL;
}
//Sends a helthcheck to servers every 5 seconds.
void * healthThreadFunction2(){
    char recvLine[100];
    char response[100] = "";
    int n = 1;
    int contentL = 0;
    
    while(1){
        
        for(size_t i = 0; i < serverCount; i++){
            if ((servers[i].healthfd = client_connect(servers[i].port)) < 0){
                err(1, "failed connecting");
            }
            healthCheck(servers[i].healthfd);
            
        }
        
        
        for(size_t i=0;i<serverCount;i++){
            n = 1;
            
            while(n>0){
                n = recv(servers[i].healthfd, recvLine, 100, 0);
                strcpy(response,strcat(response,recvLine));
                memset(recvLine,'\0',100);
            }
            strcpy(servers[i].response, response);
            sscanf(servers[i].response, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n%d\n%d", &contentL, &servers[i].errors, &servers[i].requests);
            memset(response,'\0',100);
            close(servers[i].healthfd);
        }
        sleep(5);
    }
    
    
    return NULL;
}



/*
 * bridge_loop forwards all messages between both sockets until the connection
 * is interrupted. It also prints a message if both channels are idle.
 * sockfd1, sockfd2: valid sockets
 */
void bridge_loop(int sockfd1, int sockfd2) {
    fd_set set;
    struct timeval timeout;

    int fromfd, tofd;
    while(1) {
        // set for select usage must be initialized before each select call
        // set manages which file descriptors are being watched
        FD_ZERO (&set);
        FD_SET (sockfd1, &set);
        FD_SET (sockfd2, &set);

        // same for timeout
        // max time waiting, 5 seconds, 0 microseconds
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        // select return the number of file descriptors ready for reading in set
        switch (select(FD_SETSIZE, &set, NULL, NULL, &timeout)) {
            case -1:
                printf("error during select, exiting\n");
                return;
            case 0:
                printf("both channels are idle, waiting again\n");
                continue;
            default:
                if (FD_ISSET(sockfd1, &set)) {
                    fromfd = sockfd1;
                    tofd = sockfd2;
                } else if (FD_ISSET(sockfd2, &set)) {
                    fromfd = sockfd2;
                    tofd = sockfd1;
                } else {
                    printf("this should be unreachable\n");
                    return;
                }
        }
        if (bridge_connections(fromfd, tofd) <= 0){
            close(fromfd);
            close(tofd);
            return;
        }
    }
}
//Each thread grabs this function and takes an fd from the queue and calls handle_connection
void * thread_function(){
    
    while(true){
        int *pclient_sockd;
        pthread_mutex_lock(&mutex);
        if((pclient_sockd = dequeue()) == NULL){
            pthread_cond_wait(&condition_var,&mutex);
            pclient_sockd = dequeue();
        }
        pthread_mutex_unlock(&mutex);
        if(pclient_sockd != NULL){
            handle_connection(pclient_sockd);
        }
    }
}

//get appropriate server and calls bride_loop on the two file descriptors
void * handle_connection(void * args) {
    int client_sockd = *((int*)args);
    client_sockd += 0;
    int min;
    serverObj toSend;
    min = servers[0].requests;
    toSend = servers[0];
    for(size_t i = 0; i< serverCount; i++){
        if(servers[i].requests < min){
            toSend = servers[i];
            min = servers[i].requests;
        }
    }
    
    
    
    if ((toSend.fd = client_connect(toSend.port)) < 0)
            err(1, "failed connecting"); 
    
    
    
    bridge_loop(client_sockd,toSend.fd);
    
    

    return NULL;
}

int main(int argc,char **argv) {
    int16_t opt;
    char *threads = NULL;
    char * interv = NULL;
    
    while((opt = getopt(argc, argv, "R:N:")) != -1){  
        switch(opt){  
        
            case 'N':  
                threads = optarg; 
                break;  
                
            case 'R': 
                interv = optarg;
                //logSig = 1;
                break;
                
            case '?':
                break;
        }  
    }

    
    uint16_t listenPortCount = (argc - optind) - 1;
    int listenfd;
    int iServ = 0;
    uint16_t listenport;
    serverCount = listenPortCount;
    servers = malloc(listenPortCount * sizeof(serverObj));
    listenport = atoi(argv[optind]);
    for(; (optind+1)<argc; optind++){
        servers[iServ].port = atoi(argv[optind+1]);
        iServ++;
    }

    if (argc < 3) {
        printf("missing arguments: usage %s port_to_connect port_to_listen", argv[0]);
        return 1;
    }
    interval = 5;
    if (interv != NULL){
        interval = atoi(interv);
    }

    // Remember to validate return values
    // You can fail tests for not validating
    
    if ((listenfd = server_listen(listenport)) < 0)
        err(1, "failed listening");
    
    
    pthread_t health;
    pthread_create(&health, NULL, healthThreadFunction, NULL);
    pthread_t health2;
    pthread_create(&health2, NULL, healthThreadFunction2, NULL);
    

    
    
    uint8_t threadCount = 4;
    if(threads != NULL){
        threadCount = atoi(threads);
        
    }
    
    pthread_t pool[threadCount];

    
    for(size_t i = 0; i < threadCount; i++){
        pthread_create(&pool[i], NULL, thread_function, NULL);
    }
    
    while (true) {
        /*
         * 1. Accept Connection
         */
        int acceptfd = accept(listenfd, NULL, NULL);
        rReceived++;
        if(rReceived % interval == 0){
            done = 0;
        }
        
        
        // Remember errors happen

        /*
         * 2. Read HTTP Message
         */
        //read_http_response(&message);

        /*
         * 3. Process Request
         */
        
        
        //enqueue
        int *pclient_sockd = malloc(sizeof(int));
        *pclient_sockd = acceptfd;
        
        pthread_mutex_lock(&mutex);
        enqueue(pclient_sockd);
        pthread_cond_signal(&condition_var);
        pthread_mutex_unlock(&mutex);
        
        
        
    }

}