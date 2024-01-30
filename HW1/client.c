#include "hw1.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)
#define BUFFER_SIZE 4096
#define TYPE_SIZE 20

typedef struct {
    char* ip; // server's ip
    unsigned short port; // server's port
    int conn_fd; // fd to talk with server
    char buf[BUFFER_SIZE]; // data sent by/to server
    size_t buf_len; // bytes used by buf
} client;

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

client cli;
static void init_client(char** argv);

void print_board()
{
    int tp;
    io receive, send;
    record rd;
    strcpy(send.type, "!pull");
    lseek(cli.conn_fd, 0, SEEK_SET);
    write(cli.conn_fd, &send, sizeof(send));
    //assert(strcmp(receive.type, "!bulletin") == 0);
    printf("==============================\n");
    fflush(stdout);
    //fprintf(stderr,"size : %ld\n",receive.size/sizeof(record));
    //printf("sz:%d\n",receive.size/sizeof(record));
    //fprintf(stderr, "num:%d\n", receive.size/sizeof(record));
    while(1)
    {
        //char tmp [FROM_LEN+5] = {'\0'}, tmp2[CONTENT_LEN+5] = {'\0'};
        //memcpy(tmp, rd[i].From, sizeof(char)*FROM_LEN);
        //memcpy(tmp2, rd[i].Content, sizeof(char)*CONTENT_LEN);
        lseek(cli.conn_fd, 0, SEEK_SET);
        read(cli.conn_fd, &receive, sizeof(receive));
        if(strcmp(receive.type , "!end") == 0)
            break;
        memcpy(&rd, &receive.buf, sizeof(record));
        printf("FROM: %s\n", rd.From), fflush(stdout);
        printf("CONTENT:\n%s\n", rd.Content), fflush(stdout);
    }
    printf("==============================\n");
    fflush(stdout);
    return;
}

int main(int argc, char** argv){
    
    // Parse args.
    if(argc!=3){
        ERR_EXIT("usage: [ip] [port]");
    }

    // Handling connection
    init_client(argv);
    fprintf(stderr, "connect to %s %d\n", cli.ip, cli.port);

    printf("==============================\nWelcome to CSIE Bulletin board\n");
    fflush(stdout);
    print_board();
    while(1) // TODO: handle user's input
    {
        char command[20];
        printf("Please enter your command (post/pull/exit): ");
        fflush(stdout);
        scanf("%s", command);
        if(strcmp(command, "exit") == 0)
        {
            io send;
            strcpy(send.type, "!exit");
            lseek(cli.conn_fd, 0, SEEK_SET);
            write(cli.conn_fd, &send, sizeof(send));
            break;
        } 
        else if(strcmp(command, "pull") == 0)
            print_board();
        else if(strcmp(command, "post") == 0)
        {
            io send , receive;
            strcpy(send.type, "!post");
            lseek(cli.conn_fd, 0, SEEK_SET);
            write(cli.conn_fd, &send, sizeof(send));
            lseek(cli.conn_fd, 0, SEEK_SET);
            read(cli.conn_fd, &receive,sizeof(receive));
            if (strcmp(receive.type, "!full") == 0)
                printf("[Error] Maximum posting limit exceeded\n"), fflush(stdout);
            else //if(strcmp(receive.type, "!available") == 0)
            {
                message msg;
                memcpy(&msg, receive.buf, sizeof(msg));
                printf("FROM: ");
                fflush(stdout);
                scanf("%s", msg.rd.From);
                printf("CONTENT:\n");
                fflush(stdout);
                scanf("%s", msg.rd.Content);
                memcpy(send.buf, &msg, sizeof(msg));
                send.size = sizeof(msg);
                strcpy(send.type ,"!finish");
                lseek(cli.conn_fd, 0, SEEK_SET);
                write(cli.conn_fd, &send, sizeof(send));
            }
            //else printf("%s\n",receive.type);
            
        }
    }
    close(cli.conn_fd);
    return 0;
}

static void init_client(char** argv){
    
    cli.ip = argv[1];

    if(atoi(argv[2])==0 || atoi(argv[2])>65536){
        ERR_EXIT("Invalid port");
    }
    cli.port=(unsigned short)atoi(argv[2]);

    struct sockaddr_in servaddr;
    cli.conn_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(cli.conn_fd<0){
        ERR_EXIT("socket");
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(cli.port);

    if(inet_pton(AF_INET, cli.ip, &servaddr.sin_addr)<=0){
        ERR_EXIT("Invalid IP");
    }

    if(connect(cli.conn_fd, (struct sockaddr*)&servaddr, sizeof(servaddr))<0){
        ERR_EXIT("connect");
    }

    return;
}
