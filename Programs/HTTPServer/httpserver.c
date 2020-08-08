#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <unistd.h> // write
#include <string.h> // memset
#include <stdlib.h> // atoi
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include "queue.h"
 
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
uint16_t logfd = 0;
size_t gOffset = 0;
uint8_t logSig = 0;
size_t entries = 0;
size_t errors = 0;
 
void * handle_connection(void * args);
void * thread_function();

#define BUFFER_SIZE 4000

struct httpObject {
    /*
        Create some object 'struct' to keep track of all
        the components related to a HTTP message
        NOTE: There may be more member variables you would want to add
    */
    char method[10];         // PUT, HEAD, GET
    char filename[28];      // what is the file we are worried about
    char httpversion[8];    // HTTP/1.1
    size_t content_length; // example: 13
    uint16_t status_code;
    uint8_t buffer[BUFFER_SIZE];
    int client_sockd;
};

/*
    \brief 1. Want to read in the HTTP message/ data coming in from socket
    \param client_sockd - socket file descriptor
    \param message - object we want to 'fill in' as we read in the HTTP message
*/
void read_http_response(struct httpObject* message) {

    // Read the contents of the header and store them in the buffer.
    memset(message->buffer,'\0',BUFFER_SIZE);
    size_t bytes = recv(message->client_sockd, message->buffer, BUFFER_SIZE, 0);
    bytes = bytes + 0;
    char* rest = (char*)message->buffer;

        // Read the header contents into a string in order to be formatted with strtok
        char* req = strtok_r(rest, "\r\n",&rest);

            sscanf(req, "%[A-Z] /%[^ ] %s", message->method, message->filename,message->httpversion);

       
        
        message->content_length = 0;
        
        // Increment the token variable to the correct line to get the content length if there is
        // a put request.
        if(strcmp(message->method,"PUT") == 0){
            
            req = strtok_r(NULL, "\r\n",&rest);
            req = strtok_r(NULL, "\r\n",&rest);
            req = strtok_r(NULL, "\r\n",&rest);
            req = strtok_r(NULL, "\r\n",&rest);
            
            sscanf(req,"Content-Length: %lu", &message->content_length);
        }

    
    
    //Comments for testing purposes. 
    
    printf("\n%s\n",message->method);
    printf("%s\n",message->filename);
    printf("%s\n",message->httpversion);
    printf("%ld\n",message->content_length);
    printf("---------\n");

    

    return;
}

/*
    \brief 2. Want to process the message we just recieved
*/
void process_request(struct httpObject* message) {
    
    size_t bytes = 0;
    
    char status[32];
    char response[512];
    size_t offset = 0;
    
    uint8_t log_buf[BUFFER_SIZE];
    char firstLine [1024];
    
    uint8_t count = 0;
    size_t bytesRead = 0;
    size_t contentPos = 0;
    size_t temp = 0;
    size_t contentCount = message->content_length;
    size_t contentFill = message->content_length;
    uint8_t modBuf = 1;
    
    uint8_t content = 0;
    if(message->content_length != 0){
        content = 1;
    }
    
    // String including all valid characters a request can consist of.
    const char* test = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
        
        
    // Test if request header name only includes valid characters.
    // If not, send appropriate response, close the socket, and log data if necessary.
    if(strlen(message->filename) > 27 || message->filename[strspn(message->filename,test)] != '\0' || 
            (strcmp(message->method,"PUT") != 0 && strcmp(message->method,"HEAD") != 0 && strcmp(message->method,"GET") != 0) ||
            strcmp(message->httpversion,"HTTP/1.1") != 0){
        message->status_code = 400;
        sprintf(response, "%s %d Bad Request\r\nContent-Length: %d\r\n\r\n", message->httpversion,
        message->status_code, 0);
        write(message->client_sockd, response, strlen(response));
        close(message->client_sockd);
        
        if(logSig == 1){
            count += snprintf(firstLine, sizeof(firstLine), "FAIL: %s /%s --- response %d\n========\n", 
                message->method,message->filename,message->status_code);
            
            
            pthread_mutex_lock(&logMutex);
            offset = gOffset;
            gOffset += count;
            pthread_mutex_unlock(&logMutex);
            pwrite(logfd, firstLine, strlen(firstLine), offset);
            offset += strlen(firstLine);
            memset(log_buf, '\0',BUFFER_SIZE);
                  
            entries++;
            errors++;
        }

        return;
    }
    
    // PUT request handling
    if(strcmp(message->method,"PUT") == 0){
        
        
        // Assign correct labels for the reponse.
        strcpy(status,"Created");
        message->status_code = 201;
        
        
        // If a healthcheck request is sent as a PUT request, send 403 error.
        if(strcmp(message->filename,"healthcheck") == 0){
            message->status_code = 403;
            strcpy(status, "Forbidden");
            sprintf(response, "%s %d %s\r\nContent-Length: %d\r\n\r\n", message->httpversion,
            message->status_code, status, 0);
            write(message->client_sockd, response, strlen(response));
            close(message->client_sockd);
            
            if(logSig == 1){
                count += snprintf(firstLine, sizeof(firstLine), "FAIL: %s /%s --- response %d\n========\n", 
                        message->method,message->filename,message->status_code);
            
                pthread_mutex_lock(&logMutex);
                offset = gOffset;
                gOffset += count;
                pthread_mutex_unlock(&logMutex);
                
                pwrite(logfd, firstLine, strlen(firstLine), offset);
                offset += strlen(firstLine);
                memset(log_buf, '\0',BUFFER_SIZE);
                
                entries++;
                errors++;
            }
            
            return;
        }
        
        
        
        struct stat statRes;
        stat(message->filename, &statRes);
        mode_t bits = statRes.st_mode;
        
        // Create a stat struct to test if the file already exists in the directory
        // if not, set appropriate response labels and log if necessary.
        struct stat exist;
        if(stat(message->filename, &exist) == 0){
            strcpy(status, "OK");
            message->status_code = 200;
            
            if((bits & S_IRUSR) == 0){
                message->status_code = 403;
                strcpy(status, "Forbidden");
                sprintf(response, "%s %d %s\r\nContent-Length: %d\r\n\r\n", message->httpversion,
                message->status_code, status, 0);
                write(message->client_sockd, response, strlen(response));
                close(message->client_sockd);
                
                if(logSig == 1){
                    count += snprintf(firstLine, sizeof(firstLine), "FAIL: %s /%s --- response %d\n========\n", 
                            message->method,message->filename,message->status_code);
                
                    pthread_mutex_lock(&logMutex);
                    offset = gOffset;
                    gOffset += count;
                    pthread_mutex_unlock(&logMutex);
                    
                    pwrite(logfd, firstLine, strlen(firstLine), offset);
                    offset += strlen(firstLine);
                    memset(log_buf, '\0',BUFFER_SIZE);
                    
                    entries++;
                    errors++;
                }
                
                return;
            
            
            }
            
        }
        
        
        // Test if the file could not be opened for reading for whatever reason.
        // If so, send the appropriate response and close the socket and log if necessary.
        if (open(message->filename, O_WRONLY|O_CREAT|O_TRUNC,0644) == -1){
            message->status_code = 500;
            sprintf(response, "%s %d Internal Server Error\r\nContent-Length: %d\r\n\r\n", message->httpversion,
            message->status_code, 0);
            write(message->client_sockd, response, strlen(response));
            close(message->client_sockd);
            
            if(logSig == 1){
                count += snprintf(firstLine, sizeof(firstLine), "FAIL: %s /%s --- response %d\n========\n", 
                message->method,message->filename,message->status_code);
            
                pthread_mutex_lock(&logMutex);
                offset = gOffset;
                gOffset += count;
                pthread_mutex_unlock(&logMutex);
                
                pwrite(logfd, firstLine, strlen(firstLine), offset);
                offset += strlen(firstLine);
                memset(log_buf, '\0',BUFFER_SIZE);
                
                entries++;
                errors++;
            }
            
            return;
        }
        
        // Create file descriptor and open file for reading content. 
        int fd = open(message->filename, O_RDWR|O_CREAT|O_TRUNC,0644);
        
        memset(message->buffer,'\0',BUFFER_SIZE);
        
        uint8_t testCount = 0;
        testCount++;
        

        count += snprintf(firstLine, 100, "%s /%s length %ld\n", 
                    message->method,message->filename,message->content_length);
        if(logSig == 1){
            if(message->content_length%20 == 0){
                modBuf = 0;
            }
            entries++;
            pthread_mutex_lock(&logMutex);
            offset = gOffset;
            gOffset += count + 9 + (69* (message->content_length/20)) + ((message->content_length%20)*3 + 9) * modBuf * content;
            pthread_mutex_unlock(&logMutex);
            pwrite(logfd, firstLine, strlen(firstLine), offset);
            offset += strlen(firstLine);
            memset(log_buf, '\0',BUFFER_SIZE);
        }
        // Loop to write contents of buffer into a file and log data if logSig == 1.
        while(contentFill > 0){
            if(message->content_length != 0){
            bytes = recv(message->client_sockd, message->buffer, BUFFER_SIZE, 0);
            }
            
            write(fd, message->buffer, bytes);
                
            if(logSig == 1){
                for(size_t i=0;i<bytes;i++){
                temp = 0;
                if(i%20 == 0){
                    temp += snprintf((char*)log_buf + strlen((char*)log_buf), sizeof(log_buf),"%08ld", bytesRead);
                }
                
                temp += snprintf((char*)log_buf + strlen((char*)log_buf), sizeof(log_buf)," %02x", message->buffer[i]);
                bytesRead++;
                
                if((i+1)%20 == 0){
                    temp += snprintf((char*)log_buf + strlen((char*)log_buf),sizeof(log_buf),"\n");
                }
                if(contentPos == message->content_length && (i+1)%20 == 0){
                    temp += snprintf((char*)log_buf + strlen((char*)log_buf),sizeof(log_buf),"========\n");
                }else if(contentPos == message->content_length && (i+1)%20 != 0){
                    temp += snprintf((char*)log_buf + strlen((char*)log_buf),sizeof(log_buf),"\n========\n");
                }
                
                    
                pwrite(logfd, log_buf, strlen((char*)log_buf), offset);
                offset += temp;
                memset(log_buf, '\0', BUFFER_SIZE);
                
                }   

            }
            
            memset(message->buffer,'\0',BUFFER_SIZE);
            
            contentFill -= bytes;
            
            
            contentCount -= bytes;
            
        
        }
        if(message->content_length == 0){
            temp += snprintf((char*)log_buf + strlen((char*)log_buf),sizeof(log_buf),"========\n");
            pwrite(logfd, log_buf, strlen((char*)log_buf), offset);
            offset += temp;
            memset(log_buf, '\0', BUFFER_SIZE);
        }
        if(bytes == 0){
            content = 0;
        }

        
        // Close the file descriptor, send the response message and close the socket.
        close(fd);
        sprintf(response, "%s %d %s\r\nContent-Length: %d\r\n\r\n", message->httpversion,
            message->status_code, status, 0);
        write(message->client_sockd, response, strlen(response));
        close(message->client_sockd);
    }
    
    
    
    // GET and HEAD request handling
    if(strcmp(message->method,"GET") == 0 || strcmp(message->method,"HEAD") == 0){
        uint8_t gOrP = 0;
        
        struct stat st;
        strcpy(status, "OK");
        message->status_code = 200;
        
        if(strcmp(message->method,"GET") == 0){
            gOrP = 1;
        }
        
        // If a heatlhcheck request is sent as a HEAD request, send a 403 response
        if(strcmp(message->method,"HEAD") == 0 && strcmp(message->filename,"healthcheck") == 0){
            message->status_code = 403;
            strcpy(status, "Forbidden");
            sprintf(response, "%s %d %s\r\nContent-Length: %d\r\n\r\n", message->httpversion,
            message->status_code, status, 0);
            write(message->client_sockd, response, strlen(response));
            close(message->client_sockd);
            
            if(logSig == 1){
                count += snprintf(firstLine, sizeof(firstLine), "FAIL: %s /%s --- response %d\n========\n", 
                        message->method,message->filename,message->status_code);
            
                pthread_mutex_lock(&logMutex);
                offset = gOffset;
                gOffset += count;
                pthread_mutex_unlock(&logMutex);
                
                pwrite(logfd, firstLine, strlen(firstLine), offset);
                offset += strlen(firstLine);
                memset(log_buf, '\0',BUFFER_SIZE);
                
                entries++;
                errors++;
            }
            
            return;
        }
        
        
        // If a healthcheck request is sent, handle it and log it if logSig == 1
        if(strcmp(message->method,"GET") == 0 && strcmp(message->filename,"healthcheck") == 0){
            
            // If logSig == 0, send a 404 response.
            if(logSig == 0){
                message->status_code = 404;
                strcpy(status, "Not Found");
                sprintf(response, "%s %d %s\r\nContent-Length: %d\r\n\r\n", message->httpversion,
                message->status_code, status, 0);
                write(message->client_sockd, response, strlen(response));
                close(message->client_sockd);
                           
                return;
            }
            
            
            char healthInfo[128];
            
            pthread_mutex_lock(&logMutex);
            snprintf(healthInfo, sizeof(healthInfo), "%ld\n%ld", errors, entries);
            pthread_mutex_unlock(&logMutex);
            
            snprintf(response, sizeof(response), "%s %d %s\r\nContent-Length: %ld\r\n\r\n", message->httpversion,
                    message->status_code, status, strlen(healthInfo));
                    
            write(message->client_sockd, response, strlen(response));
            write(message->client_sockd, healthInfo, strlen(healthInfo));
            close(message->client_sockd);
            
            
            count += snprintf(firstLine, sizeof(firstLine), "%s /%s length %ld\n", 
                    message->method,message->filename,strlen(healthInfo));
            
            pthread_mutex_lock(&logMutex);
            offset = gOffset;
            gOffset += count + 9 + ((69* (strlen(healthInfo)/20)) + ((strlen(healthInfo)%20)*3 + 9))* gOrP;
            pthread_mutex_unlock(&logMutex);
            pwrite(logfd, firstLine, strlen(firstLine), offset);
            offset += strlen(firstLine);
            memset(log_buf, '\0',BUFFER_SIZE);
            
            
            
            contentPos = 0;
            for(size_t i=0;i<strlen(healthInfo);i++){
                temp = 0;
                if(i%20 == 0){
                    temp += snprintf((char*)log_buf, sizeof(log_buf),"%08ld", bytesRead);
                }
                
                temp += snprintf((char*)log_buf + strlen((char*)log_buf), sizeof(log_buf)," %02x", message->buffer[i]);
                bytesRead++;
                
                if((i+1)%20 == 0  && (i+1 != strlen(healthInfo))){
                    temp += snprintf((char*)log_buf + strlen((char*)log_buf),sizeof(log_buf),"\n");
                }
                
                pwrite(logfd, log_buf, strlen((char*)log_buf), offset);
                offset += temp;
                memset(log_buf, '\0', BUFFER_SIZE);
            }
                
            temp = 0;
            temp += snprintf((char*)log_buf + strlen((char*)log_buf),sizeof(log_buf),"\n========\n");
            pwrite(logfd, log_buf, strlen((char*)log_buf), offset);
            offset += temp;
            memset(log_buf, '\0', BUFFER_SIZE);
            
            
            return;
            
        }
        
        // If the file cannot be opnened for reading, send the appropriate response
        // and close the socket.
        if (open (message->filename,O_RDONLY) == -1){
            message->status_code = 404;
            strcpy(status, "Not Found");
            sprintf(response, "%s %d %s\r\nContent-Length: %d\r\n\r\n", message->httpversion,
            message->status_code, status, 0);
            write(message->client_sockd, response, strlen(response));
            close(message->client_sockd);
            
            if(logSig == 1){
                count += snprintf(firstLine, sizeof(firstLine), "FAIL: %s /%s --- response %d\n========\n", 
                        message->method,message->filename,message->status_code);
            
                pthread_mutex_lock(&logMutex);
                offset = gOffset;
                gOffset += count;
                pthread_mutex_unlock(&logMutex);
                
                pwrite(logfd, firstLine, strlen(firstLine), offset);
                offset += strlen(firstLine);
                memset(log_buf, '\0',BUFFER_SIZE);
                
                entries++;
                errors++;
            }
            
            return;
        }
        
        // Create stat struct in order to test the persmissions of a file.
        struct stat statRes;
        stat(message->filename, &statRes);
        mode_t bits = statRes.st_mode;
        
        // Test if the file requested has the correct permissions. If not, send the appropriate
        // reponse and close the socket.
        if((bits & S_IRUSR) == 0){
            message->status_code = 403;
            strcpy(status, "Forbidden");
            sprintf(response, "%s %d %s\r\nContent-Length: %d\r\n\r\n", message->httpversion,
            message->status_code, status, 0);
            write(message->client_sockd, response, strlen(response));
            close(message->client_sockd);
            
            if(logSig == 1){
                count += snprintf(firstLine, sizeof(firstLine), "FAIL: %s /%s --- response %d\n========\n", 
                        message->method,message->filename,message->status_code);
            
                pthread_mutex_lock(&logMutex);
                offset = gOffset;
                gOffset += count;
                pthread_mutex_unlock(&logMutex);
                
                pwrite(logfd, firstLine, strlen(firstLine), offset);
                offset += strlen(firstLine);
                memset(log_buf, '\0',BUFFER_SIZE);
                
                entries++;
                errors++;
            }
            
            return;
            
            
        }
        

            
        memset(message->buffer,'\0',BUFFER_SIZE);
        uint16_t fd = open(message->filename, O_RDONLY);
            
        // get the size of the file being sent to the client
        fstat(fd, &st);
        ssize_t size = st.st_size;
        message->content_length = size;
        contentFill = message->content_length;
        if(message->content_length != 0){
            content = 1;
        }
            
        sprintf(response, "%s %d %s\r\nContent-Length: %ld\r\n\r\n", message->httpversion,
            message->status_code, status, size);
        write(message->client_sockd, response, strlen(response));
        
        count += snprintf(firstLine, sizeof(firstLine), "%s /%s length %ld\n", 
                    message->method,message->filename,message->content_length);
        
        if(logSig == 1){
            if(message->content_length%20 == 0){
                modBuf = 0;
            }
            entries++;
            pthread_mutex_lock(&logMutex);
            offset = gOffset;
            gOffset += count + 9 + ((69* (message->content_length/20)) + (((message->content_length%20)*3 + 9)* modBuf)) * gOrP * content;
            pthread_mutex_unlock(&logMutex);
            pwrite(logfd, firstLine, strlen(firstLine), offset);
            offset += strlen(firstLine);
            memset(log_buf, '\0',BUFFER_SIZE);
        }
            
        // If there is a GET request, get contents from from and write them to the client 
        // from the buffer
        // If logSig == 1, them log the request as well.
        if(strcmp(message->method,"GET") == 0){

            
            
            while(contentFill > 0){
                
                bytes = read(fd, message->buffer, BUFFER_SIZE);
                
                write(message->client_sockd, message->buffer, bytes);
                
            if(logSig == 1){
                for(size_t i=0;i<bytes;i++){
                temp = 0;
                if(i%20 == 0){
                    temp += snprintf((char*)log_buf + strlen((char*)log_buf), sizeof(log_buf),"%08ld", bytesRead);
                }
                
                temp += snprintf((char*)log_buf + strlen((char*)log_buf), sizeof(log_buf)," %02x", message->buffer[i]);
                contentPos++;
                bytesRead++;
                
                if((i+1)%20 == 0){
                    temp += snprintf((char*)log_buf + strlen((char*)log_buf),sizeof(log_buf),"\n");
                }
                if(contentPos == message->content_length && (i+1)%20 == 0){
                    temp += snprintf((char*)log_buf + strlen((char*)log_buf),sizeof(log_buf),"========\n");
                }else if(contentPos == message->content_length && (i+1)%20 != 0){
                    temp += snprintf((char*)log_buf + strlen((char*)log_buf),sizeof(log_buf),"\n========\n");
                }
                
                    
                pwrite(logfd, log_buf, strlen((char*)log_buf), offset);
                offset += temp;
                memset(log_buf, '\0', BUFFER_SIZE);
                
                }

            }

                
                memset(message->buffer,'\0',BUFFER_SIZE);
                
                contentFill -= bytes;
            
            
                contentCount -= bytes;
                
                
            }
            
            if(message->content_length == 0){
            temp += snprintf((char*)log_buf + strlen((char*)log_buf),sizeof(log_buf),"========\n");
            pwrite(logfd, log_buf, strlen((char*)log_buf), offset);
            offset += temp;
            memset(log_buf, '\0', BUFFER_SIZE);
            }
            
            if(bytes == 0){
                content = 0;
            }   
            
                
                
        }else{
            if(logSig == 1){
                temp = 0;
                temp += snprintf((char*)log_buf + strlen((char*)log_buf),sizeof(log_buf),"========\n");
                pwrite(logfd, log_buf, strlen((char*)log_buf), offset);
                offset += temp;
                memset(log_buf, '\0', BUFFER_SIZE);
            }
        }
        
        close(message->client_sockd);
        
    }
    
    
    return;
}


// Function to dequeue a socket file descriptor off of the queue, and assign a thread to it.
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




/*
    Each thread picks up this function, which calls other functions to handle the request.
*/
void * handle_connection(void * args) {
    int client_sockd = *((int*)args);
    
    struct httpObject message;
    message.client_sockd = client_sockd;
    
    read_http_response(&message);
    
    process_request(&message);
    
    

    return NULL;
}




int main(int argc, char **argv) {
    int16_t opt;
    char *threads = NULL;
    char *logName = NULL;
    
    while((opt = getopt(argc, argv, "l:N:")) != -1){  
        switch(opt){  
        
            case 'N':  
                threads = optarg; 
                break;  
                
            case 'l': 
                logName = optarg;
                logSig = 1;
                break;
                
            case '?':
                break;
        }  
    }
    
    if(logSig == 1){
        logfd = open(logName, O_RDWR|O_CREAT|O_TRUNC,0666);
    }
    
    /*
        Create sockaddr_in with server information
    */
    char* port = argv[optind];
    
    if(port == NULL){
        fprintf(stderr,"%s","No port Specified!\n");
        exit(1);
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t addrlen = sizeof(server_addr);

    /*
        Create server socket
    */
    int server_sockd = socket(AF_INET, SOCK_STREAM, 0);

    // Need to check if server_sockd < 0, meaning an error
    if (server_sockd < 0) {
        //perror("socket");
    }

    /*
        Configure server socket
    */
    int enable = 1;

    /*
        This allows you to avoid: 'Bind: Address Already in Use' error
    */
    int ret = setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    /*
        Bind server address to socket that is open
    */
    ret = bind(server_sockd, (struct sockaddr *) &server_addr, addrlen);

    /*
        Listen for incoming connections
    */
    ret = listen(server_sockd, 5); // 5 should be enough, if not use SOMAXCONN

    if (ret < 0) {
        return EXIT_FAILURE;
    }

    /*
        Connecting with a client
    */
    struct sockaddr client_addr;
    socklen_t client_addrlen;


    uint8_t threadCount = atoi("4");
    if(threads != NULL && atoi(threads) != 0){
        threadCount = atoi(threads);
    }
    pthread_t pool[threadCount];

    
    for(size_t i = 0; i < threadCount; i++){
        pthread_create(&pool[i], NULL, thread_function, NULL);
    }
    
    
    while (true) {
        //printf("[+] server is waiting...\n");
        /*
         * 1. Accept Connection
         */
        int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
        
        
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
        *pclient_sockd = client_sockd;
        
        pthread_mutex_lock(&mutex);
        enqueue(pclient_sockd);
        pthread_cond_signal(&condition_var);
        pthread_mutex_unlock(&mutex);
        
        
        
    }

    return EXIT_SUCCESS;
}

