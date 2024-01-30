#include "hw1.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)
#define BUFFER_SIZE 4096
#define TYPE_SIZE 20

#define read_lock(fd, offset, whence, len) \
        lock_reg((fd), F_SETLK, F_RDLCK, (offset), (whence), (len))
#define write_lock(fd, offset, whence, len) \
        lock_reg((fd), F_SETLK, F_WRLCK, (offset), (whence), (len))
#define unlock(fd, offset, whence, len) \
        lock_reg((fd), F_SETLK, F_UNLCK, (offset), (whence), (len))
int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
    struct flock lock;
    lock.l_type = type; /* F_RDLCK, F_WRLCK, F_UNLCK */
    lock.l_start = offset; /* byte offset, relative to l_whence */
    lock.l_whence = whence; /* SEEK_SET, SEEK_CUR, SEEK_END */
    lock.l_len = len; /* #bytes (0 means to EOF) */
    return(fcntl(fd, cmd, &lock));
}

typedef struct
{
    char type [TYPE_SIZE];
    char buf [BUFFER_SIZE];
    int size;
} io;

typedef struct 
{
    record rd;
    int pos;
} message;

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[BUFFER_SIZE];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    int id;
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list
int num_locked = 0;
int last = RECORD_NUM - 1;
int bulletin_fd;
struct pollfd fds[MAX_CLIENTS+1];
int locked [RECORD_NUM] ;

// initailize a server, exit for error
static void init_server(unsigned short port);

// initailize a request instance
static void init_request(request* reqP);

// free resources used by a request instance
static void free_request(request* reqP);

int find_available()
{
    int index = (last+1)%RECORD_NUM;
    for(int i=0;i<RECORD_NUM;i++)
    {
        if(locked[index] == 1)
            index = (index + 1) % RECORD_NUM;
        else if(write_lock(bulletin_fd, index*sizeof(record), SEEK_SET, sizeof(record)) != -1) 
        {
            unlock(bulletin_fd, index*sizeof(record), SEEK_SET, sizeof(record));
            return index;
        }
        else 
            index = (index + 1) % RECORD_NUM;
    }
    return -1;
}

int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        ERR_EXIT("usage: [port]");
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[BUFFER_SIZE];
    record* bulletinbuf;
    int buf_len;

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));
    memset(locked, 0, sizeof(locked));
    for (int i=0;i<MAX_CLIENTS+1;i++)  fds[i].events = POLLIN, fds[i].fd = -1;

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);
    bulletin_fd = open(RECORD_PATH, O_RDWR | O_CREAT, 0700);
    bulletinbuf = (record*)malloc((RECORD_NUM) * sizeof(record));
    memset(bulletinbuf, 0, sizeof(record)*RECORD_NUM);
    lseek(bulletin_fd, 0, SEEK_SET);
    read(bulletin_fd, bulletinbuf, sizeof(record)*RECORD_NUM);
    lseek(bulletin_fd, 0, SEEK_SET);
    write(bulletin_fd, bulletinbuf, (RECORD_NUM) * sizeof(record));
    fds[0].fd = svr.listen_fd;

    while (1) {
        // TODO: Add IO multiplexing
        int num_in = poll(fds, MAX_CLIENTS+1, -1);
        if (num_in > 0)
        {
            if(fds[0].revents & POLLIN)
            {
                // Check new connection
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
                fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
                for(int i=1;i<=MAX_CLIENTS;i++)
                {
                    if(fds[i].fd == -1)
                    {
                        fds[i].fd = conn_fd;
                        break;
                    }
                }
            }
            // TODO: handle requests from clients
            for(int i=1;i<=MAX_CLIENTS;i++)
            {
                if(fds[i].revents & POLLIN)
                {
                    conn_fd = fds[i].fd;
                    io receive;
                    lseek(requestP[conn_fd].conn_fd, 0, SEEK_SET);
                    read(requestP[conn_fd].conn_fd, &receive, sizeof(receive));
                    //fprintf(stderr, "request : %s\n", receive.type);
                    if(strcmp(receive.type, "!post") == 0)
                    {
                        io send;
                        //printf("%d ", last);
                        int tmp = find_available();
                        //printf("%d\n", tmp);
                        if (tmp == -1) 
                        {
                            strcpy(send.type, "!full");
                            lseek(requestP[conn_fd].conn_fd, 0, SEEK_SET);
                            write(requestP[conn_fd].conn_fd, &send, sizeof(send));
                        }
                        else
                        {
                            write_lock(bulletin_fd, tmp*sizeof(record), SEEK_SET, sizeof(record));
                            locked[tmp] = 1;
                            for(int i=0;i<RECORD_NUM;i++) fprintf(stderr, "%d", locked[i]);
                            fprintf(stderr, "\n");
                            strcpy(send.type, "!available");
                            message msg;
                            memcpy(&msg.pos, &tmp, sizeof(int));
                            memcpy(&send.buf, &msg, sizeof(msg));
                            send.size = sizeof(msg);
                            lseek(requestP[conn_fd].conn_fd, 0, SEEK_SET);
                            write(requestP[conn_fd].conn_fd, &send, sizeof(send));
                            //read_lock (bulletin_fd, tmp*sizeof(record), SEEK_SET, sizeof(record));
                            last = tmp;
                        }
                    }
                    else if(strcmp(receive.type, "!exit") == 0)
                    {
                        close(requestP[conn_fd].conn_fd);
                        free_request(&requestP[conn_fd]);
                        fds[i].fd = -1;
                    }
                    else if(strcmp(receive.type, "!pull") == 0)
                    {
                        record sd[MAX_CLIENTS];
                        io send;
                        int num_locked = 0, index = 0;
                        lseek(bulletin_fd, 0, SEEK_SET);
                        read(bulletin_fd, bulletinbuf, sizeof(record)*RECORD_NUM);
                        
                        for(int i=0;i<RECORD_NUM;i++)
                        {
                            //fprintf(stderr, "%d %s %s\n",i, bulletinbuf[i].From,  bulletinbuf[i].Content);
                            if(locked[i] == 1 || write_lock(bulletin_fd, i*sizeof(record), SEEK_SET, sizeof(record)) == -1) 
                                num_locked ++;
                            else
                            {
                                unlock(bulletin_fd, i*sizeof(record), SEEK_SET, sizeof(record));
                                //fprintf(stderr, "%d %s\n",i, bulletinbuf[i].From);
                                if(strlen(bulletinbuf[i].From) != 0)
                                {
                                    memcpy(&send.buf, &bulletinbuf[i], sizeof(record));
                                    send.size = sizeof(record);
                                    strcpy(send.type, "!record");
                                    lseek(requestP[conn_fd].conn_fd, 0, SEEK_SET);
                                    write(requestP[conn_fd].conn_fd, &send, sizeof(send));
                                    memcpy(&sd[index], &bulletinbuf[i] , sizeof(record));
                                    index++;
                                }
                            }
                        }
                        send.size = 0;
                        strcpy(send.type,"!end");
                        //for(int i=0;i<index;i++) printf("%s %s\n",sd[i].Content, sd[i].From);
                        lseek(requestP[conn_fd].conn_fd, 0, SEEK_SET);
                        write(requestP[conn_fd].conn_fd, &send, sizeof(send));
                        if(num_locked > 0)
                            printf("[Warning] Try to access locked post - %d\n", num_locked), fflush(stdout);
                    }
                    else if(strcmp(receive.type, "!finish") == 0)
                    {
                        message msg;
                        //lseek(bulletin_fd, 0, SEEK_SET);
                        //read(bulletin_fd, bulletinbuf, sizeof(record)*RECORD_NUM);
                        memcpy(&msg, receive.buf, sizeof(msg));
                        //memcpy(&bulletinbuf[msg.pos], &msg.rd, sizeof(msg.rd));
                        lseek(bulletin_fd, (msg.pos)*sizeof(record), SEEK_SET);
                        int ret = write(bulletin_fd, &msg.rd, sizeof(record));
                            //lseek(bulletin_fd, (msg.pos)*sizeof(record), SEEK_SET);
                            //record ttt;
                            //read(bulletin_fd, &ttt, sizeof(record));
                            //fprintf(stderr, "%s %s\n", ttt.From, ttt.Content);
                        fprintf(stderr, "wrt : %d\n", ret);
                        unlock(bulletin_fd, (msg.pos)*sizeof(record), SEEK_SET, sizeof(record));
                        locked[msg.pos] = 0;
                        printf("[Log] Receive post from %s\n", msg.rd.From);
                        fprintf(stderr, "%d %s %s\n", msg.pos, msg.rd.From, msg.rd.Content);
                        fflush(stdout);
                    }
                }
            }
        }
        //lseek(bulletin_fd, 0, SEEK_SET);
        //write(bulletin_fd, bulletinbuf, sizeof(record)*RECORD_NUM);
        /*for(int i=0;i<RECORD_NUM;i++)
            fprintf(stderr, "from : %s \t content : %s\n", bulletinbuf[i].From, bulletinbuf[i].Content);
        fprintf(stderr, "\n");*/
    }
    free(requestP);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
}

static void free_request(request* reqP) {
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

    // Get file descripter table size and initialize request table
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
