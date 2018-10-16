#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <openssl/md5.h>
#include <algorithm>
#include "boss.h"
using namespace std;
char str[(1<<30)+1024]; 
Dump dump_information[1<<6];
int num_of_dump = 0;
int load_config_file(struct server_config *config, char *path){
    FILE *fp = fopen(path, "r");
    if(!fp)  return 0;
    fscanf(fp, "MINE: %s\n", config->mine_file);
    config->num_miners = 0;
    while(fscanf(fp, "MINER: %s %s\n", config->pipes[config->num_miners].input_pipe, 
                                       config->pipes[config->num_miners].output_pipe) == 2){
            config->num_miners++;
    }   
    return 0;
}

int main(int argc, char **argv){
    /* sanity check on arguments */
    char name[1<<10];
    if (argc != 2){
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }
    /* load config file */
    struct server_config config;
    load_config_file(&config, argv[1]);

    /* initialize data for select() */
    int maxfd = 100;
    fd_set readset;
    fd_set working_readset;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    FD_ZERO(&readset);
    FD_SET(STDIN_FILENO, &readset);

    /* miner protocol*/
    mine_sync server, get;
    Shit First_Job;
    memset(&First_Job, 0, sizeof(First_Job));
    memset(&server, 0, sizeof(server));
    memset(str, 0, sizeof(str));
    /* first setting */
    struct fd_pair client_fds[config.num_miners];
    int interval = 256 / config.num_miners;
    /* handle mine MD5 */
    MD5_Init (&server.mdContext);
    long long bytes = 0;
    int READ = 0;
    char input[(1<<13)+1] = {};
    /* first job */
    MD5_Init (&First_Job.mdContext);
    First_Job.treasure = 0;
    for (int ind = 0; ind < config.num_miners; ind += 1){
        First_Job.interval_start = ind*interval;
        if(ind != config.num_miners-1)
            First_Job.interval_end = (ind+1)*interval;
        else
            First_Job.interval_end = 256;       
        /* open FIFO and send msg */
        client_fds[ind].input_fd = open(config.pipes[ind].input_pipe, O_WRONLY);
        assert (client_fds[ind].input_fd >= 0);

        write(client_fds[ind].input_fd, &First_Job, sizeof(First_Job));  // first job

        client_fds[ind].output_fd = open(config.pipes[ind].output_pipe, O_RDONLY);
        assert (client_fds[ind].output_fd >= 0);
        FD_SET(client_fds[ind].output_fd, &readset);    // set read read FD_SET
    }
    char cur[50] = {};
    char cmd[PATH_MAX+10] = {};
    int ret;
    long long need_to_write;
    MD5_CTX working_context;
    int READ_FILE = 0;
    FILE *fmine = fopen(config.mine_file, "rb");
    while (1){
        int assign_jobs = 0;
        /* READ FILE */
        if(!READ_FILE){
            READ = fread(input, 1, 4096, fmine);
            if(READ == 0){
                //fprintf(stderr, "my bytes = %lld\n", bytes);
                READ_FILE = 1;
                assign_jobs = 1;
                //fprintf(stderr, "READ DONE\n"); 
            }
            else{
                for(int i = 0; i < READ; i++)
                    str[bytes+i] = input[i];
                MD5_Update(&server.mdContext, input, READ);
                memset(input, 0, sizeof(input));
                bytes += READ;
            }                    
        }

        FD_ZERO(&working_readset);
        memcpy(&working_readset, &readset, sizeof(readset)); 
        select(maxfd, &working_readset, NULL, NULL, &timeout);
        if (FD_ISSET(STDIN_FILENO, &working_readset)){
            memset(cmd, 0, sizeof(cmd));
            read(STDIN_FILENO, cmd, sizeof(cmd));
            cmd[strlen(cmd)-1]='\0';
            if (!strncmp(cmd, "status", 6)){
                // handle md5 + stdout
                if(server.treasure == 0){
                    puts("best 0-treasure in 0 bytes");
                }
                else{
                    memcpy(&working_context, &server.mdContext, sizeof(MD5_CTX));
                    unsigned char c[MD5_DIGEST_LENGTH];
                    MD5_Final (c,&working_context);
                    printf("best %d-treasure ", server.treasure);
                    for(int i = 0; i < MD5_DIGEST_LENGTH; i++) printf("%02x", c[i]);
                    printf(" in %lld bytes\n", bytes);
                    // write to client                        
                }
                server.op = STATUS;
                for(int ind = 0; ind < config.num_miners; ind++){
                    write(client_fds[ind].input_fd, &server, sizeof(server));
                }              
            }
            else if (!strncmp(cmd, "dump", 4)){
                int new_one = 1;
                for(int i = 0; i < num_of_dump; i++){
                    if(!strncmp(&cmd[5], dump_information[i].location, strlen(dump_information[i].location)) && dump_information[i].success == 0){
                        if(READ_FILE)
                            dump_information[i].end = bytes;
                        else
                            dump_information[i].end = 0;
                        new_one = 0;
                        break;
                    }
                }
                if(new_one){
                    memset(&dump_information[num_of_dump], 0, sizeof(Dump));
                    strcpy(dump_information[num_of_dump].location, &cmd[5]);
                    if(READ_FILE)
                        dump_information[num_of_dump].end = bytes;
                    else
                        dump_information[num_of_dump].end = 0;
                    dump_information[num_of_dump].fd = -1;
                    num_of_dump++;                    
                }
            }
            else{
                assert(!strncmp(cmd, "quit", 4));
                server.op = QUIT;
                for(int ind = 0; ind < config.num_miners; ind++){
                    write(client_fds[ind].input_fd, &server, sizeof(server));
                    close(client_fds[ind].input_fd);
                    close(client_fds[ind].output_fd);
                }
                for(int i = 0; i < num_of_dump; i++){
                    if(dump_information[i].success == 1){
                        close(dump_information[i].fd);
                        continue;
                    } 
                    if(dump_information[i].fd < 0){
                       while((dump_information[i].fd = open(dump_information[i].location, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_NONBLOCK, 0644)) < 0);
                    }
                    if(dump_information[i].fd > 0){
                        while(dump_information[i].start < dump_information[i].end){
                            need_to_write = min(dump_information[i].end - dump_information[i].start, 1024);
                            ret = write(dump_information[i].fd, str+dump_information[i].start, need_to_write);
                            if(ret >= 0){
                                dump_information[i].start += ret;   
                            }       
                        }                       
                    }
                    close(dump_information[i].fd);
                }
                return 0;
            }
        }
        for(int i = 3; i < maxfd; i++){
            if(FD_ISSET(i, &working_readset)){
                memset(&get, 0, sizeof(get));
                memset(cur, 0, sizeof(cur));
                memset(name, 0, sizeof(name));
                //fprintf(stderr, "--------------------------------\n"); 
                read(i, &get, sizeof(get));
                //fprintf(stderr, "datalen = %d, namelen = %d\n", get.datalen, get.namelen);
                read(i, cur, get.datalen);
                read(i, name, get.namelen);
                if(get.treasure > server.treasure){
                    /* update information -> treasure + string + MD5 */
                    server.treasure = get.treasure;   // treasure
                    server.namelen = strlen(name);
                    //fprintf(stderr, "read %d bytes\n", get.datalen);
                    for(int i = 0; i < get.datalen; i++)
                        str[bytes+i] = cur[i];
                    //strncpy(str+bytes, cur, get.datalen);   //string
                    bytes += get.datalen;
                    //fprintf(stderr, "bytes = %d\n", bytes);
                    memcpy(&server.mdContext, &get.mdContext, sizeof(MD5_CTX));
                    /* print MD5 */
                    memcpy(&working_context, &get.mdContext, sizeof(MD5_CTX));
                    unsigned char found[MD5_DIGEST_LENGTH];
                    MD5_Final(found, &working_context);
                    printf("A %d-treasure discovered! ", server.treasure);
                    for(int i = 0; i < MD5_DIGEST_LENGTH; i++) printf("%02x", found[i]);
                    puts("");                                                  
                    assign_jobs = 1;
                    break;                    
                }
            }
        }
        if(assign_jobs == 1){
            for (int ind = 0; ind < config.num_miners; ind += 1){
                /* assign jobs */
                server.op = WORK;
                write(client_fds[ind].input_fd, &server, sizeof(mine_sync));  // write protocol
                write(client_fds[ind].input_fd, name, strlen(name));            
            }   
        }
        for(int i = 0; i < num_of_dump; i++){
            if(dump_information[i].success == 1)    continue;
            if(dump_information[i].fd < 0){
               dump_information[i].fd = open(dump_information[i].location, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_NONBLOCK, 0644);
            }
            if(dump_information[i].fd > 0){
                need_to_write = min(dump_information[i].end - dump_information[i].start, 1024);
                ret = write(dump_information[i].fd, str+dump_information[i].start, need_to_write);
                if(ret >= 0){
                    dump_information[i].start += ret;    
                }                
                if(dump_information[i].start == dump_information[i].end){
                    dump_information[i].success = 1;
                    //fprintf(stderr, "write done\n");
                }
                //fprintf(stderr, "file = %s, cur = %d\n", dump_information[i].location, dump_information[i].start);
            }
        }
    }
    return 0;
}
