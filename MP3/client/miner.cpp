#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/md5.h>
#include <queue>
#include <string>
#include <iostream>
#include "miner.h"
using namespace std;
MD5_CTX bug;
int md5sum(char* str, int treasure, MD5_CTX mdContext, int len){
    unsigned char c[MD5_DIGEST_LENGTH];
    char ans[60] = {};
    MD5_CTX temp;
    memcpy(&temp, &mdContext, sizeof(MD5_CTX));
    MD5_Update(&temp, str, len);

    memcpy(&bug, &temp, sizeof(MD5_CTX));
    // memcpy(&bug, &mdContext, sizeof(MD5_CTX));
    MD5_Final (c,&temp);
    int ptr = 0;

    for(int i = 0; i < MD5_DIGEST_LENGTH; i++)  sprintf(&ans[ptr], "%02x", c[i]), ptr += 2;
    int zero = -1;
    for(int i = 0; i <  MD5_DIGEST_LENGTH; i++){
        if(ans[i] != '0') break;
        zero++;
    }
    if(zero == treasure){
        //puts("-------------");
        //puts(ans);
        //puts("I got it");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv){
    /* parse arguments */
    if (argc != 4){
        fprintf(stderr, "Usage: %s CONFIG_FILE\n", argv[0]);
        exit(1);
    }
    char *name = argv[1];
    char *input_pipe = argv[2];      // read     // /tmp/user_in  -> client從這裡讀
    char *output_pipe = argv[3];     // write    // /tmp/user_out -> client寫入這裡

    /* initialize data for select() */
    int maxfd = 50;
    fd_set readset, working_readset;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    FD_ZERO(&readset);

    /* create named pipes */
    int ret;
    ret = mkfifo(input_pipe, 0644);
    assert (!ret);
    ret = mkfifo(output_pipe, 0644);
    assert (!ret);
    /* open pipes */
    int input_fd = 0, output_fd = 0;

    /* my mine status */
    mine_sync server, client;
    /* loop assignment */
    int LOGIN = 0, NEED_TO_WORK = 0, FIRST_WORK = 0;
    queue<string> trial;
    char tmpstr[1<<10];
    MD5_CTX working_context;
    Shit Status;
    while(1){
        // first time
        if(LOGIN == 0){
            // reset some variables
            input_fd = open(input_pipe, O_RDONLY);
            assert (input_fd >= 0);
            FD_SET(input_fd, &readset); // listen to server
            output_fd = open(output_pipe, O_WRONLY);
            assert (output_fd >= 0);
            FIRST_WORK = 0;
            NEED_TO_WORK = 0;
            LOGIN = 1;
            memset(&Status, 0, sizeof(Status));
            memset(&client, 0, sizeof(client));
            memset(&working_context, 0, sizeof(MD5_CTX));
            //fprintf(stderr, "----------------first time----------------------\n");
            read(input_fd, &Status, sizeof(Status));
            puts("BOSS is mindful.");
            //fprintf(stderr, "interval = %d->%d\n", Status.interval_start, Status.interval_end);
            queue<string> tmp;
            trial.swap(tmp);                      
        }
        while(1){
            FD_ZERO(&working_readset);
            memcpy(&working_readset, &readset, sizeof(readset));             
            select(maxfd, &working_readset, NULL, NULL, &timeout);
            if(FD_ISSET(input_fd, &working_readset)){   // if server send mesg to me
                read(input_fd, &server, sizeof(mine_sync));
                if(server.op == QUIT){
                    printf("BOSS is at rest.\n");
                    LOGIN = 0;
                    close(input_fd);
                    close(output_fd);
                    break;
                }
                else if(server.op == STATUS){
                    if(FIRST_WORK){
                        unsigned char c[MD5_DIGEST_LENGTH];
                        MD5_Final (c,&bug);
                        printf("I'm working on ");
                        for(int i = 0; i < MD5_DIGEST_LENGTH; i++) printf("%02x", c[i]);
                        puts("");                         
                    }                  
                }
                else if(server.op == WORK){
                    char finder[256] = {};
                    read(input_fd, finder, server.namelen);
                    //fprintf(stderr, "-----I got work-----\n");
                    //fprintf(stderr, "treasure = %d\n", server.treasure);
                    // Update treasure
                    Status.treasure = server.treasure;
                    // Update MD5
                    memcpy(&Status.mdContext, &server.mdContext, sizeof(MD5_CTX));
                    memcpy(&working_context, &server.mdContext, sizeof(MD5_CTX));
                    MD5_CTX Sta = working_context;
                    //memcpy(&Sta, &working_context, sizeof(MD5_CTX));
                    if(FIRST_WORK){ // if it is not the first work
                        if(strncmp(finder, name, strlen(name)) == 0){
                            printf("I win a %d-treasure! ", server.treasure);
                        }
                        else{
                            printf("%s wins a %d-treasure! ", finder, server.treasure);
                        }
                        unsigned char cur_best[MD5_DIGEST_LENGTH];
                        MD5_Final(cur_best, &Sta);
                        for(int i = 0; i < MD5_DIGEST_LENGTH; i++) printf("%02x", cur_best[i]);
                        puts("");                         
                    }
                    queue<string> tmp;
                    trial.swap(tmp);
                    //fprintf(stderr, "interval = %d->%d\n", Status.interval_start, Status.interval_end);
                    for(int i = Status.interval_start; i < Status.interval_end; i++){
                        string r;
                        trial.push(r.assign(1, i));
                    }
                    NEED_TO_WORK = 1;
                    FIRST_WORK = 1;
                }
            }
            if(NEED_TO_WORK == 1){
                /* md5 trial */
                string out = trial.front();
                trial.pop();
                memset(tmpstr, 0, sizeof(tmpstr));
                for(int i = 0; i < out.size(); i++){
                    tmpstr[i] = out[i];
                }
                /* count md5 */
                working_context = Status.mdContext;
                //memcpy(&working_context, &Status.mdContext, sizeof(MD5_CTX));          
                int ret = md5sum(tmpstr, Status.treasure, working_context, out.size());
                if(ret == 1){
                    /* update client data structure */    
                    client.treasure+=1;
                    client.op = GET;
                    client.datalen = out.length();
                    client.namelen = strlen(name);
                    client.treasure = Status.treasure+1;
                    MD5_Update(&working_context, tmpstr, out.length());
                    memcpy(&client.mdContext, &working_context, sizeof(MD5_CTX));
                    /* write to server */
                    write(output_fd, &client, sizeof(mine_sync));
                    write(output_fd, tmpstr, out.length());
                    write(output_fd, name, strlen(name));
                    NEED_TO_WORK = 0;
                }
                else{
                    if(trial.size() > 10000000)  continue;
                    for(int i = 0; i < 256; i++){
                        string cur = out;
                        cur += i;
                        trial.push(cur);
                    }                
                }                
            }      
        }
    }
    return 0;
}