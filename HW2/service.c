#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "util.h"

#define ERR_EXIT(s) perror(s), exit(errno);
#define WRITE 1
#define READ 0

static unsigned long secret;
static char service_name[MAX_SERVICE_NAME_LEN];

static inline bool is_manager() {
    return strcmp(service_name, "Manager") == 0;
}

void print_not_exist(char *service_name) {
    printf("%s doesn't exist\n", service_name);
}

void print_receive_command(char *service_name, char *cmd) {
    printf("%s has received %s\n", service_name, cmd);
}

void print_spawn(char *parent_name, char *child_name) {
    printf("%s has spawned a new service %s\n", parent_name, child_name);
}

void print_kill(char *target_name, int decendents_num) {
    printf("%s and %d child services are killed\n", target_name, decendents_num);
}

void print_acquire_secret(char *service_a, char *service_b, unsigned long secret) {
    printf("%s has acquired a new secret from %s, value: %lu\n", service_a, service_b, secret);
}

void print_exchange(char *service_a, char *service_b) {
    printf("%s and %s have exchanged their secrets\n", service_a, service_b);
}

service child[40];
int num_child = 0;

typedef struct command 
{
    char type [20];
    char content [7][MAX_FIFO_NAME_LEN];
    char cmd [MAX_CMD_LEN];
}command;

const int parent_read_fd = 3, parent_write_fd = 4;
int pipe_read[2] = {-1, -1}, pipe_write[2] = {-1, -1}; //for exchange
char name[2][50];

void printcommand(command a)
{
    int aaa;
    memcpy(&aaa, a.content[6], sizeof(int));
    printf("service %s : %s : %s, %s, %s, %s, %d\n", service_name , 
           a.type, a.content[0], a.content[1], a.content[2], a.content[3], aaa );
    return;
}

command read_and_split()
{
    command receive = {0};
    if(is_manager())
    {
        scanf("%s", receive.type);
        if(strcmp(receive.type, "spawn") == 0 || strcmp(receive.type, "exchange") == 0)
        {
            scanf("%s%s", receive.content[0], receive.content[1]);
            sprintf(receive.cmd, "%s %s %s", receive.type, receive.content[0], receive.content[1]);
        }
        else if(strcmp(receive.type, "kill") == 0)
        {
            scanf("%s", receive.content[0]);
            sprintf(receive.cmd, "%s %s", receive.type, receive.content[0]);
        }
    }
    else
        read(parent_read_fd, &receive, sizeof(command));
    return receive;
}

void spawns(char *parent_service, char *new_child_service)
{
    pid_t cpid;
    int pipe_to_parent[2];
    int pipe_to_child[2];
    pipe2(pipe_to_parent, O_CLOEXEC);
    pipe2(pipe_to_child, O_CLOEXEC);
    //printf("aaa:%d %d %d %d\n", pipe_read_from_child[0], pipe_read_from_child[1], pipe_write_to_child[0], pipe_write_to_child[1]); 

    if((cpid = fork()) > 0) //parent 
    {
        close(pipe_to_child[READ]), close(pipe_to_parent[WRITE]);
        strcpy(child[num_child].name, new_child_service);
        child[num_child].read_fd = pipe_to_parent[READ];
        child[num_child].write_fd = pipe_to_child[WRITE];
        child[num_child].pid = cpid;
        num_child++;
    }
    else
    {
        int tmp_fd1 = dup(pipe_to_child[READ]), tmp_fd2 = dup(pipe_to_parent[WRITE]);
        close(pipe_to_child[WRITE]), close(pipe_to_parent[READ]);
        close(pipe_to_child[READ]), close(pipe_to_parent[WRITE]);
        dup2(tmp_fd1, parent_read_fd); //parent_read_fd
        dup2(tmp_fd2, parent_write_fd); //parent_write_fd
        close(tmp_fd1), close(tmp_fd2);
        execl("service","service", new_child_service, NULL);
    }
    return;
}

void suicide()
{
    command receive_child, send_parent, send_child;
    int num_killed = 1;
    for(int i=0 ; i<num_child ; i++)
    {
        strcpy(send_child.type, "suicide");
        write(child[i].write_fd, &send_child, sizeof(command));
        read(child[i].read_fd, &receive_child, sizeof(command));
        wait(NULL);
        int tmp; 
        memcpy(&tmp, receive_child.content[2], sizeof(int));
        num_killed += tmp;
    }
    strcpy(send_parent.type, "suicide_success"); 
    memcpy(send_parent.content[2], &num_killed, sizeof(int));
    //printf("%s: %d\n", service_name, num_killed);
    //printcommand(send_parent);
    if(!is_manager())
        write(parent_write_fd, &send_parent, sizeof(command));
    else
        print_kill(service_name, num_killed-1);
    exit(0);
}

void traverse(command receive_parent)
{
    //printcommand(receive_parent);
    command send_child, receive_child;
    int cnt;
    memcpy(&cnt, receive_parent.content[6], sizeof(int));
    //printf("%d, cntt\n", cnt);
    if(strcmp(receive_parent.content[0], service_name) == 0 || strcmp(receive_parent.content[1], service_name) == 0)
        cnt++;
    memcpy(&send_child, &receive_parent, sizeof(command));
    memcpy(&receive_child, &receive_parent, sizeof(command));
    //printf("%d, cnt\n", cnt);
    if(cnt == 2) 
    {
        strcpy(receive_child.type, "exchange_success");
        if(!is_manager())
            write(parent_write_fd, &receive_child, sizeof(command));
        return;
    }
    memcpy(send_child.content[6], &cnt, sizeof(int));
    memcpy(receive_child.content[6], &cnt, sizeof(int));
    for(int i=0;i<num_child;i++)
    { 
        write(child[i].write_fd, &send_child, sizeof(command));
        read(child[i].read_fd, &receive_child, sizeof(command));
        memcpy(&send_child, &receive_child, sizeof(command));
        if(strcmp(receive_child.type, "exchange_success") == 0)
            break;
    }
    if(!is_manager())
        write(parent_write_fd, &receive_child, sizeof(command));
    return;
}

void finds(command receive_parent, int index)
{
    //printf("from: %s, %d\n",service_name, num_child);
    command receive_child, send_parent, send_child;
    memcpy(receive_parent.content[4], &index, sizeof(int));
    memcpy(&receive_child, &receive_parent, sizeof(command));
    memcpy(&send_parent, &receive_parent, sizeof(command));
    memcpy(&send_child, &receive_parent, sizeof(command));
    bool success = false;
    if( strcmp(service_name, receive_parent.content[index]) == 0 )
    {
        strcpy(send_parent.type, "find_success");
        if(!is_manager())
            write(parent_write_fd, &send_parent, sizeof(command));        
        return;
    }
    for(int i=0;i<num_child;i++)
    {
        strcpy(send_child.type, "exchange");
        write(child[i].write_fd, &send_child, sizeof(command));
        read(child[i].read_fd, &receive_child, sizeof(command));
        if(strcmp(receive_child.type, "find_success") == 0)
        {
            pipe_read[index] = child[i].read_fd;
            pipe_write[index] = child[i].write_fd;
            strcpy(name[index], child[i].name);

            strcpy(send_parent.type, "find_success");
            success = true;
            break;
        }
        memcpy(&send_child, &receive_child, sizeof(command));
        memcpy(&send_parent, &receive_child, sizeof(command));
    }
    if(!success)
        strcpy(send_parent.type, "find_failed");
    if(!is_manager())
        write(parent_write_fd, &send_parent, sizeof(command));
    return;
}

void exchanges(command receive_parent)
{
    command send_parent = receive_parent;
    if(strcmp(receive_parent.content[0], service_name) == 0 )
    {
        memcpy(&send_parent, &receive_parent, sizeof(command));
        int fd1 = open(receive_parent.content[2], O_WRONLY);
        int fd2 = open(receive_parent.content[3], O_RDONLY);
        unsigned long tmp_secret;
        read(fd2, &tmp_secret, sizeof(secret));
        print_acquire_secret(service_name, receive_parent.content[1], tmp_secret);
        write(fd1, &secret, sizeof(secret));
        secret = tmp_secret;
        close(fd1), close(fd2);
        strcpy(send_parent.type, "exchange_success");
    }
    else if(strcmp(receive_parent.content[1], service_name) == 0 )
    {
        memcpy(&send_parent, &receive_parent, sizeof(command));
        int fd2 = open(receive_parent.content[2], O_RDONLY);
        int fd1 = open(receive_parent.content[3], O_WRONLY);
        write(fd1, &secret, sizeof(secret));
        read(fd2, &secret, sizeof(secret));
        print_acquire_secret(service_name, receive_parent.content[0], secret);
        close(fd1), close(fd2);
        strcpy(send_parent.type, "exchange_success");
    }
    return;
}

int main(int argc, char *argv[]) 
{
    pid_t pid = getpid(); 
    command receive_parent, receive_child, send_parent, send_child;
    if (argc != 2) 
    {
        fprintf(stderr, "Usage: ./service [service_name]\n");
        return 0;
    }   

    srand(pid);
    secret = rand();
    setvbuf(stdout, NULL, _IONBF, 0);
    strncpy(service_name, argv[1], MAX_SERVICE_NAME_LEN);
    printf("%s has been spawned, pid: %d, secret: %lu\n", service_name, pid, secret);
    if(!is_manager())
    {
        strcpy(send_parent.type, "spawn_success");
        write(parent_write_fd, &send_parent, sizeof(command));
    }
    //printf("fds: %d %d\n", parent_read_fd, parent_write_fd);

    while(1)
    {
        receive_parent = read_and_split();
        if( strlen(receive_parent.cmd) == 0 && strlen(receive_parent.type) == 0 )
            break;
        if( strcmp(receive_parent.type, "start_exchange") != 0 && strcmp(receive_parent.type, "suicide") != 0  && strcmp(receive_parent.type, "exchange") != 0)
            print_receive_command(service_name, receive_parent.cmd);
        else if(strcmp(receive_parent.type, "suicide") == 0 && strcmp(service_name, receive_parent.content[0]) == 0)
            print_receive_command(service_name, receive_parent.cmd);
        else if(strcmp(receive_parent.type, "exchange") == 0)
        {
            bool tmp_flag;
            memcpy(&tmp_flag, receive_parent.content[5], sizeof(bool));
            if( tmp_flag || is_manager())
                print_receive_command(service_name, receive_parent.cmd);
        }
        if(strcmp(receive_parent.type, "spawn") == 0)
        {
            if(strcmp(receive_parent.content[0], service_name) == 0)
            {
                spawns(receive_parent.content[0], receive_parent.content[1]);
                read(child[num_child-1].read_fd, &receive_child, sizeof(command));
                //printf("check: (%s) ,%s, %s\n", service_name, receive.type, receive.content);
                strcpy(send_parent.type, "spawn_success");
            }
            else
            {
                bool success = false;
                for(int i=0 ; i<num_child ; i++)
                {
                    memcpy(&send_child, &receive_parent, sizeof(command));
                    write(child[i].write_fd, &send_child, sizeof(command));
                    read(child[i].read_fd, &receive_child, sizeof(command));
                    if( strcmp(receive_child.type, "spawn_success") == 0 )
                    {
                        success = true;
                        break;
                    }
                }
                if(success)
                    strcpy(send_parent.type, "spawn_success");
                else
                    strcpy(send_parent.type, "spawn_failed");
            }
            if(!is_manager())
                write(parent_write_fd, &send_parent, sizeof(command));
            if(num_child == 0)
                strcpy(receive_child.type, "spawn_failed");
            if(is_manager() && strcmp(receive_child.type, "spawn_failed") == 0)
                print_not_exist(receive_parent.content[0]);
            else if(is_manager() && strcmp(receive_child.type, "spawn_success") == 0 )
                print_spawn(receive_parent.content[0], receive_parent.content[1]);            
        }
        else if(strcmp(receive_parent.type, "kill") == 0) //0 : target, 1 : print or not, 2 : number of killed
        {
            bool success = false;
            if(strcmp(service_name, receive_parent.content[0]) == 0)
                suicide();
            memcpy(&receive_child, &receive_parent, sizeof(command));
            strcpy(receive_child.type, "kill_failed");
            for(int i=0 ; i<num_child ; i++)
            {
                //printcommand(receive_parent);
                //printf("%s %s\n", child[0].name, child[1].name);
                if(strcmp(receive_parent.content[0], child[i].name) == 0)
                {
                    memcpy(&send_child, &receive_parent, sizeof(command));
                    strcpy(send_child.type, "suicide"); 
                    write(child[i].write_fd, &send_child, sizeof(command));
                    read(child[i].read_fd, &receive_child, sizeof(command));
                    wait(NULL);
                    close(child[i].read_fd), close(child[i].write_fd);
                    strcpy(send_parent.type, "kill_success");
                    for(int j = i+1; j < num_child; j++)
                        child[j-1] = child[j];
                    num_child--;
                    success = true;
                    break;
                }  
                else
                {
                    memcpy(&send_child, &receive_parent, sizeof(command));
                    write(child[i].write_fd, &send_child, sizeof(command));
                    read(child[i].read_fd, &receive_child, sizeof(command));
                    if( strcmp(receive_child.type, "kill_success") == 0 )
                    {
                        int num_killed;
                        memcpy(&num_killed, receive_child.content[2], sizeof(int));
                        success = true;
                        break;
                    }
                }
            }
            memcpy(&send_parent, &receive_child, sizeof(command));
            if(success)
                strcpy(send_parent.type, "kill_success");
            else
                strcpy(send_parent.type, "kill_failed");
            if(!is_manager())
                write(parent_write_fd, &send_parent, sizeof(command));
            else if(is_manager() && strcmp(receive_child.type, "kill_failed") == 0)
                print_not_exist(receive_parent.content[0]);
            else if(is_manager())
            {
                int num_killed;
                memcpy(&num_killed, receive_child.content[2], sizeof(int));
                print_kill(receive_parent.content[0], num_killed-1);
            }
        }
        else if(strcmp(receive_parent.type, "suicide") == 0)
            suicide();
        else if(strcmp(receive_parent.type, "exchange") == 0) 
        //0:first, 1:second, 2:fifo1, 3:fifo2, 4:target 5:print 6:cnt
        {
            char fifo1[MAX_FIFO_NAME_LEN*3], fifo2[MAX_FIFO_NAME_LEN*3];
            bool print_;
            int cnt_;
            memcpy(&print_, receive_parent.content[5], sizeof(bool));
            if(is_manager())
            {
                sprintf(fifo1, "%s_to_%s.fifo", receive_parent.content[0], receive_parent.content[1]);
                mkfifo(fifo1, 0777);
                sprintf(fifo2, "%s_to_%s.fifo", receive_parent.content[1], receive_parent.content[0]);
                mkfifo(fifo2, 0777);
                strcpy(receive_parent.content[2], fifo1);
                strcpy(receive_parent.content[3], fifo2);

                print_ = true;
                memcpy(receive_parent.content[5], &print_, sizeof(bool));
                cnt_ = 0;
                memcpy(receive_parent.content[6], &cnt_, sizeof(bool));

                traverse(receive_parent);

                print_ = false;
                memcpy(receive_parent.content[5], &print_, sizeof(bool));

                memcpy(&send_child, &receive_parent, sizeof(command));
                if(strcmp(service_name, receive_parent.content[0]) == 0)
                {
                    finds(receive_parent, 1);
                    strcpy(send_child.type, "start_exchange");
                    write(pipe_write[1], &send_child, sizeof(command));
                    exchanges(receive_parent);
                    read(pipe_read[1], &receive_child, sizeof(command));
                }
                else if(strcmp(service_name, receive_parent.content[1]) == 0)
                {
                    finds(receive_parent, 0);
                    strcpy(send_child.type, "start_exchange");
                    write(pipe_write[0], &send_child, sizeof(command));
                    exchanges(receive_parent);
                    read(pipe_read[0], &receive_child, sizeof(command));
                }
                else
                {
                    finds(receive_parent, 0);
                    finds(receive_parent, 1);
                    memcpy(&send_child, &receive_parent, sizeof(command));
                    strcpy(send_child.type, "start_exchange");
                    if(pipe_write[0] == pipe_write[1])
                    {
                        write(pipe_write[0], &send_child, sizeof(command));
                        read(pipe_read[0], &receive_child, sizeof(command));
                    }
                    else
                    {
                        write(pipe_write[0], &send_child, sizeof(command));
                        //printf("%s %s\n", name[0], name[1]);
                        write(pipe_write[1], &send_child, sizeof(command));
                        read(pipe_read[0], &receive_child, sizeof(command));
                        read(pipe_read[1], &receive_child, sizeof(command));
                    }   
                }
                unlink(fifo1);
                unlink(fifo2);
                print_exchange(receive_parent.content[0], receive_parent.content[1]);
                pipe_read[0] = pipe_write[0] = pipe_read[1] = pipe_write[1] = -1;
            }
            else if(!print_)
            {
                int idx;
                memcpy(&idx, receive_parent.content[4], sizeof(int));
                finds(receive_parent, idx);
            }
            else
                traverse(receive_parent);
        }
        else if(strcmp(receive_parent.type, "start_exchange") == 0) // 4 : num
        {
            //printcommand(receive_parent);
            //printf("%d %d %d %d %s\n", pipe_read[0] , pipe_write[0] , pipe_read[1] , pipe_write[1], service_name);
            if(pipe_read[0] == -1 && pipe_read[1] == -1)
            {
                exchanges(receive_parent);
                if(!is_manager())
                    write(parent_write_fd, &send_parent, sizeof(command));
            }
            else if(strcmp(receive_parent.content[0], service_name) == 0)
            {
                write(pipe_write[1], &receive_parent, sizeof(command));
                exchanges(receive_parent);
                read(pipe_read[1], &receive_child, sizeof(command));
                if(!is_manager())
                    write(parent_write_fd, &send_parent, sizeof(command));
            }
            else if(strcmp(receive_parent.content[1], service_name) == 0)
            {
                write(pipe_write[0], &receive_parent, sizeof(command));
                exchanges(receive_parent);
                read(pipe_read[0], &receive_child, sizeof(command));
                if(!is_manager())
                    write(parent_write_fd, &send_parent, sizeof(command));
            }
            else if(pipe_read[0] != -1 && pipe_read[1] != -1 && pipe_read[0] != pipe_read[1])
            {
                write(pipe_write[0], &receive_parent, sizeof(command));
                write(pipe_write[1], &receive_parent, sizeof(command));
                read(pipe_read[0], &receive_child, sizeof(command));
                read(pipe_read[1], &receive_child, sizeof(command));
                write(parent_write_fd, &receive_child, sizeof(command));
            }
            else if (pipe_read[0] != -1 && pipe_read[1] != -1 && pipe_read[0] == pipe_read[1] )
            {
                write(pipe_write[0], &receive_parent, sizeof(command));
                read(pipe_read[0], &receive_child, sizeof(command));
                write(parent_write_fd, &receive_child, sizeof(command));
            }
            else
            {
                if(pipe_read[0] != -1)
                {
                    write(pipe_write[0], &receive_parent, sizeof(command));
                    read(pipe_read[0], &receive_child, sizeof(command));
                    write(parent_write_fd, &receive_child, sizeof(command));
                }
                else
                {
                    write(pipe_write[1], &receive_parent, sizeof(command));
                    read(pipe_read[1], &receive_child, sizeof(command));
                    write(parent_write_fd, &receive_child, sizeof(command));
                }
            }
            pipe_read[0] = pipe_write[0] = pipe_read[1] = pipe_write[1] = -1;
        }
    }
    return 0;
}        
