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
#include <stdbool.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    // you don't need to change this.
    int id; // 1-20
    int conversation;  // used by handle_read to know if the header is read or not.
    int errnum; // 1: locked, 2: operation failed
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";
const char* welcome_message = "Please enter the id (to check how many masks can be ordered):\n";
const char* lock_message = "Locked.\n";
const char* failed_message = "Operation failed.\n";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

typedef struct {
    int id; //customer id, 1-20
    int adultMask;
    int childrenMask;
} Order;
Order orderP[25];

void init_order() {for(int i=1;i<=20;i++) orderP[i].id=0;}

Order* orderBuf;
void init_orderBuf() {
    orderBuf = (Order*) malloc(sizeof(Order) * maxfd);
}

int ordertype[25]; // 0 for adult, 1 for children

int mapConn_fd[25]; // used for order
void init_mapConn_fd() {for(int i=1;i<=20;i++) mapConn_fd[i]=-1;}

bool ordering[25] = {false};
int reading[25] = {0};

typedef struct {
    int id;
    int adultMask;
    int childrenMask;
} Reading;
Reading* readList;

void init_readList() {
    readList = (Reading*) malloc(sizeof(Reading) * maxfd);
    for(int i=0;i<maxfd;i++) readList[i].id=0;
}

int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
    struct flock lock;

    lock.l_type = type;
    lock.l_start = offset;
    lock.l_whence = whence;
    lock.l_len = len;

    return (fcntl(fd,cmd,&lock));
}

#define read_lock(fd,offset,whence,len) \
    lock_reg((fd),F_SETLK,F_RDLCK,(offset),(whence),(len))
#define write_lock(fd,offset,whence,len) \
    lock_reg((fd),F_SETLK,F_WRLCK,(offset),(whence),(len))
#define un_lock(fd,offset,whence,len) \
    lock_reg((fd),F_SETLK,F_UNLCK,(offset),(whence),(len))

pid_t lock_test(int fd, int type, off_t offset, int whence, off_t len)
{
    struct flock lock;
    lock.l_type = type;
    lock.l_start = offset;
    lock.l_whence = whence;
    lock.l_len = len;

    fcntl(fd,F_GETLK,&lock);

    if(lock.l_type == F_UNLCK) return(0);
    return (lock.l_pid);
}

#define is_write_lockable(fd,offset,whence,len) \
    (lock_test((fd),F_WRLCK, (offset), (whence), (len)) == 0)

void locked(int conn_fd){
    requestP[conn_fd].errnum = 1;
}
void failed(int conn_fd){
    requestP[conn_fd].errnum = 2;
}

void handle_read(request* reqP) {
    char buf[512];
    read(reqP->conn_fd, buf, sizeof(buf));
    memcpy(reqP->buf, buf, strlen(buf));
}

bool isnum(char* s)
{
    int l=strlen(s);
    for(int i=0;i<l;i++) if(s[i]<'0' || s[i]>'9') return false;
    return true;
}

int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd = open("preorderRecord",O_RDWR);
    if(file_fd==-1) ERR_EXIT("open");
    // fprintf(stderr,"file_fd=%d\n",file_fd);
    char buf[512];
    int buf_len;

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));
    init_order();
    init_readList();
    init_mapConn_fd();
    init_orderBuf();

    /* if timeout is needed
    struct timeval timeout;
    timeout.tv_sec=0; timeout.tv_usec=0;
    */

    fd_set master_rfds, working_rfds, master_wfds, working_wfds;
    FD_ZERO(&master_rfds);
    FD_ZERO(&master_wfds);

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);
    FD_SET(svr.listen_fd,&master_rfds);

    while (1) {
        // TODO: Add IO multiplexing
        memcpy(&working_rfds,&master_rfds,sizeof(master_rfds));
        memcpy(&working_wfds,&master_wfds,sizeof(master_wfds));
        select(maxfd,&working_rfds,&working_wfds,NULL,NULL);
        
        // Check new connection
        if(FD_ISSET(svr.listen_fd, &working_rfds)){
            clilen = sizeof(cliaddr);
            conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
            if (conn_fd < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;  // try again
                if (errno == ENFILE) {
                    (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                    continue;
                }
                ERR_EXIT("accept");
            }
            requestP[conn_fd].conn_fd = conn_fd;
            strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
            FD_SET(conn_fd, &master_wfds);
            fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
        }

        // File Read
        if(FD_ISSET(file_fd, &working_rfds)){
            for(int conn_fd=0;conn_fd<maxfd;conn_fd++){
                if(readList[conn_fd].id){
                    pread(file_fd,&(orderBuf[conn_fd]),sizeof(Order),(readList[conn_fd].id-1)*sizeof(Order));
                    readList[conn_fd].adultMask = orderBuf[conn_fd].adultMask;    
                    readList[conn_fd].childrenMask = orderBuf[conn_fd].childrenMask;
                    readList[conn_fd].id=0;
                    FD_SET(conn_fd, &master_wfds); 
                }
            }
            FD_CLR(file_fd, &master_rfds);
        }

        // File Write
        if(FD_ISSET(file_fd, &working_wfds)){
            for(int i=1;i<=20;i++){
                if(orderP[i].id>0){
                    pwrite(file_fd, &(orderP[i]), sizeof(Order), (i-1)*sizeof(Order));
                    orderP[i].id=0;
                    FD_SET(mapConn_fd[i], &master_wfds);
                }
            }
            FD_CLR(file_fd, &master_wfds);
        }

        //Read

        for(conn_fd=0;conn_fd<maxfd;conn_fd++){ // STDIN, STDOUT, STDERR ??
            if(requestP[conn_fd].conn_fd!=-1 && conn_fd!=svr.listen_fd && conn_fd!=file_fd && FD_ISSET(conn_fd, &working_rfds)){
                handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
#ifdef READ_SERVER
                if(strcmp(requestP[conn_fd].buf,"902001")<0 || strcmp(requestP[conn_fd].buf,"902020")>0){
                    failed(conn_fd);
                    FD_SET(conn_fd, &master_wfds);
                } else {
                    requestP[conn_fd].id = atoi(requestP[conn_fd].buf)%100;
                    if(ordering[requestP[conn_fd].id]){
                        locked(conn_fd);
                        FD_SET(conn_fd, &master_wfds);
                    }
                    else{
                        errno = 0;
                        read_lock(file_fd,(requestP[conn_fd].id-1)*sizeof(Order),SEEK_SET,sizeof(Order));
                        if(errno == EACCES || errno == EAGAIN){
                            locked(conn_fd);
                            FD_SET(conn_fd, &master_wfds);
                        } else {
                            reading[requestP[conn_fd].id]++;
                            readList[conn_fd].id=requestP[conn_fd].id;
                            FD_SET(file_fd, &master_rfds);
                        }
                    }
                }
#else
                //fprintf(stderr, "write server: %s, conversation id: %d\n", requestP[conn_fd].buf, requestP[conn_fd].conversation);
                if(requestP[conn_fd].conversation==1){
                    if(strcmp(requestP[conn_fd].buf,"902001")<0 || strcmp(requestP[conn_fd].buf,"902020")>0){
                        failed(conn_fd);
                        FD_SET(conn_fd, &master_wfds);
                    } else {
                        requestP[conn_fd].id = atoi(requestP[conn_fd].buf)%100;
                        if(ordering[requestP[conn_fd].id]){
                            locked(conn_fd);
                            FD_SET(conn_fd, &master_wfds);
                        } else {
                            errno = 0;
                            write_lock(file_fd,(requestP[conn_fd].id-1)*sizeof(Order),SEEK_SET,sizeof(Order));
                            //perror("ERROR");
                            if(errno == EACCES || errno == EAGAIN){
                                locked(conn_fd);
                                FD_SET(conn_fd, &master_wfds);
                            } else {
                                mapConn_fd[requestP[conn_fd].id]=conn_fd;
                                ordering[requestP[conn_fd].id]=true;
                                readList[conn_fd].id=requestP[conn_fd].id;
                                FD_SET(file_fd, &master_rfds);
                            }
                        }
                    }
                } else {
                    char* token;
                    token = strtok(requestP[conn_fd].buf, " ");
                    //fprintf(stderr,"token=%s\n",token);
                    if(strcmp(token, "adult")==0){
                        token = strtok(NULL, "\r\n");
                        int x;
                        if(!isnum(token) || (x=atoi(token))<0 || x>readList[conn_fd].adultMask){
                            failed(conn_fd);
                            FD_SET(conn_fd, &master_wfds);
                        }
                        else{
                            orderP[requestP[conn_fd].id].id = requestP[conn_fd].id;
                            orderP[requestP[conn_fd].id].adultMask = readList[conn_fd].adultMask-x;
                            orderP[requestP[conn_fd].id].childrenMask = readList[conn_fd].childrenMask;
                            ordertype[requestP[conn_fd].id]=0;
                            FD_SET(file_fd, &master_wfds);
                        }
                        //fprintf(stderr,"go into adult... token=%s\n",token);
                    } else if(strcmp(token, "children")==0){
                        token = strtok(NULL, "\r\n");
                        int x;
                        if(!isnum(token) || (x=atoi(token))<0 || x>readList[conn_fd].childrenMask){
                            failed(conn_fd);
                            FD_SET(conn_fd, &master_wfds);
                        }
                        else{
                            orderP[requestP[conn_fd].id].id = requestP[conn_fd].id;
                            orderP[requestP[conn_fd].id].adultMask = readList[conn_fd].adultMask;
                            orderP[requestP[conn_fd].id].childrenMask = readList[conn_fd].childrenMask-x;
                            ordertype[requestP[conn_fd].id]=1;
                            FD_SET(file_fd, &master_wfds);
                        }
                        //fprintf(stderr,"go into children... token=%s\n",token);
                    } else {
                        failed(conn_fd);
                        FD_SET(conn_fd, &master_wfds);
                    }
                }
#endif
                requestP[conn_fd].conversation++;
                //fprintf(stderr, "conversation id=%d\n", requestP[conn_fd].conversation);
                FD_CLR(conn_fd, &master_rfds);
            }
        }
        
        // Write

        for(conn_fd=0;conn_fd<maxfd;conn_fd++){
            if(requestP[conn_fd].conn_fd!=-1 && conn_fd!=file_fd && FD_ISSET(conn_fd, &working_wfds)){
                int to_close=0;
                if(requestP[conn_fd].errnum){
                    if(requestP[conn_fd].errnum==1){
                        write(requestP[conn_fd].conn_fd, lock_message, strlen(lock_message));
                    } else {
                        write(requestP[conn_fd].conn_fd, failed_message, strlen(failed_message));
                    }
                    to_close=1;
                }
                else if(requestP[conn_fd].conversation==0){
                    write(requestP[conn_fd].conn_fd, welcome_message, strlen(welcome_message));
                }
                else {
#ifdef READ_SERVER      
                    sprintf(buf,"You can order %d adult mask(s) and %d children mask(s).\n",
                        readList[conn_fd].adultMask,readList[conn_fd].childrenMask);
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));   
                    to_close=1; 
#else 
                    if(requestP[conn_fd].conversation==2){
                        sprintf(buf,"You can order %d adult mask(s) and %d children mask(s).\nPlease enter the mask type (adult or children) and number of mask you would like to order:\n",
                            readList[conn_fd].adultMask,readList[conn_fd].childrenMask);
                        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                    } else {
                        int id=requestP[conn_fd].id;
                        //fprintf(stderr,"adult = %d, children = %d\n",orderP[id].adultMask, orderP[id].childrenMask);
                        if(ordertype[requestP[conn_fd].id]==0){
                            sprintf(buf,"Pre-order for 9020%02d successed, %d adult mask(s) ordered.\n",
                                requestP[conn_fd].id,readList[conn_fd].adultMask-orderP[id].adultMask);
                        } else {
                            sprintf(buf,"Pre-order for 9020%02d successed, %d children mask(s) ordered.\n",
                                requestP[conn_fd].id,readList[conn_fd].childrenMask-orderP[id].childrenMask);
                        }
                        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
                        to_close=1;
                    }
#endif                    
                }

                FD_CLR(conn_fd, &master_wfds);
                if(to_close){
#ifdef READ_SERVER
                    reading[requestP[conn_fd].id]--;
                    if(reading[requestP[conn_fd].id]==0 && \
                        is_write_lockable(file_fd, (requestP[conn_fd].id-1)*sizeof(Order), SEEK_SET, sizeof(Order)))
                    {
                        un_lock(file_fd, (requestP[conn_fd].id-1)*sizeof(Order), SEEK_SET, sizeof(Order));
                    }
#else
                    un_lock(file_fd, (requestP[conn_fd].id-1)*sizeof(Order), SEEK_SET, sizeof(Order));
                    ordering[requestP[conn_fd].id]=false;
#endif
                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                } else {
                    requestP[conn_fd].conversation++;
                    FD_SET(conn_fd, &master_rfds);
                }      
            }
        }
        
    }
    free(requestP);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
    reqP->conversation = 0;
    reqP->errnum = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}
