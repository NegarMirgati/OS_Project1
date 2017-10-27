#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>
#include "server.h"

#define MAXDATASIZE 200
#define BACKLOG 10     // how many pending connections queue will hold
int active = 1 ;
char file_name[MAXDATASIZE];

int main(int argc, char* argv[])
{


    int counter, numbytes;
    int active = 0 ;
    char buf[MAXDATASIZE];
    if(parameter_err(argc)) return 0 ;
    
    char*  PORT = argv[1] ;
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    //socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        //char msg[] = "getaddrinfo: %s\n";
        write(STDERR_FILENO, gai_strerror(rv),strlen(gai_strerror(rv))-1 );
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server : socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (bind_err(p)) return 1 ;
    if (listen_err(sockfd)) return 1;
 
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    const char msg[] = "server: waiting for connections...\n";
    write(STDOUT_FILENO, msg, strlen(msg));

    run(sockfd, new_fd, their_addr, s);
    char message[MAXDATASIZE] ;
    printf("%c",recv_msg(new_fd));
    return 0;
}


void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

char *append_str (char *str1 , char *str2){
    char * new_str ;
    if((new_str = malloc(strlen(str1)+strlen(str2)+1)) != NULL){
        new_str[0] = '\0';   // ensures the memory is an empty string
        strcat(new_str,str1);
        strcat(new_str,str2);
        return new_str;
}
   else
    abort(); 
}

char* itoa(int i, char b[]){
    char const digit[] = "0123456789";
    char* p = b;
    if(i<0){
        *p++ = '-';
        i *= -1;
    }
    int shifter = i;
    do{ //Move to where representation ends
        ++p;
        shifter = shifter/10;
    }while(shifter);
    *p = '\0';
    do{ //Move back, inserting digits as u go
        *--p = digit[i%10];
        i = i/10;
    }while(i);
    return b;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

 int parameter_err(int argc){

    if(argc==1){
        const char msg[] = "\n valid operation server [port number]\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return 1;
    }
    return 0 ;
 }

 int bind_err(struct addrinfo* p){

    if (p == NULL)  {
        char msg[] = "server: failed to bind\n";
        write(STDERR_FILENO, msg, strlen(msg));
        return 1 ;
    }
    return 0 ;
 }

 int listen_err(int sockfd){
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        return 1;
    }
    return 0;

 }

void run(int sockfd, int new_fd, struct sockaddr_storage their_addr, char* s){

    socklen_t sin_size;
    while(active) {  
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);
        on_new_connection(new_fd);       
        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            if (send(new_fd, "\n#CONNECTION STABLISHED", 23, 0) == -1)
                write(STDERR_FILENO, "send error", 10);
       
            close(new_fd);
            exit(0);
        }
        on_new_message(new_fd);
        close(new_fd);  // parent doesn't need this


    }
 }

void send_msg (int indentifier, char* msg)
 {

   send(indentifier, msg, strlen(msg), 0);
 }

char recv_msg(int indentifier)
 {
    char buff[MAXDATASIZE];
    recv(indentifier, buff, MAXDATASIZE, 0);
    return *buff;
 }

void stop(){
    char msg[] = "Server is already stopped.\n";
    if(!active)
        write(STDERR_FILENO, msg, strlen(msg));
    active = 0 ;
 }

void on_new_connection(int indentifier){
    char* msg = "\nNEW CONNECTION : ";
    char tmp[5] = {0x0} ;
    write(STDOUT_FILENO, msg, strlen(msg));
    sprintf(tmp,"%4d", indentifier);
    write(STDOUT_FILENO, tmp, strlen(tmp));
 }

void on_standard_input(char* line)
{
  char* msg = "STDIN" ;
  char* quit = ":q";
  write(STDOUT_FILENO, msg, strlen(msg));
  write(STDOUT_FILENO, line, strlen(line));
  if (strcmp(line,quit))
    stop();
}

int on_new_message(int sockfd){
    int numbytes;
    char buf[MAXDATASIZE];
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        return 1;
    }
    
    char* temp = "\nNEW MESSAGE FROM";
    char id[5] = {0x0} ;
    sprintf(id,"%4d", sockfd);
    write(STDOUT_FILENO, temp, strlen(temp));
    write(STDOUT_FILENO, id, strlen(id));
    write(STDOUT_FILENO, ": ",2);
    write(STDOUT_FILENO, buf, strlen(buf));
    parse(buf, sockfd);
    return 0;

}

void parse(char input[],int sockfd){

     char *token = strtok(input, ",");
     if(!strcmp(token,"filename"))
     {
        token = strtok(NULL, ",");
        memcpy(file_name, token ,strlen(token));
        return;
     }
}
