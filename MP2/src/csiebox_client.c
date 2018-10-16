#include "csiebox_client.h"
#include "csiebox_common.h"
#include "connect.h"
#include <sys/mman.h>
#include <sys/file.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#define REG_S_FLAG (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)//permission you can use to create new directory
#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))
static int parse_arg(csiebox_client* client, int argc, char** argv);
static int login(csiebox_client* client);
int ptr = 0;
int f_ptr = 0;
struct allpath{
  char name[300];
  struct stat info;
  int used;
};
struct watch{
  char name[300];
  int used;
  int wd;
};
struct allpath cdir_path[305];
struct watch watch_dir[305];
//read config file, and connect to server
void csiebox_client_init(
  csiebox_client** client, int argc, char** argv) {
  csiebox_client* tmp = (csiebox_client*)malloc(sizeof(csiebox_client));
  if (!tmp) {
    fprintf(stderr, "client malloc fail\n");
    return;
  }
  memset(tmp, 0, sizeof(csiebox_client));
  if (!parse_arg(tmp, argc, argv)) {
    fprintf(stderr, "Usage: %s [config file]\n", argv[0]);
    free(tmp);
    return;
  }
  int fd = client_start(tmp->arg.name, tmp->arg.server);
  if (fd < 0) {
    fprintf(stderr, "connect fail\n");
    free(tmp);
    return;
  }
  tmp->conn_fd = fd;
  *client = tmp;
}
int ishardlink(int num){
  if(cdir_path[num].info.st_nlink == 1) return -1;
  if(S_ISDIR(cdir_path[num].info.st_mode)) return -1;
  for(int i = 0; i < num; i++){
    if(cdir_path[i].used == 0)  continue;
    //if(S_ISDIR(cdir_path[i].info.st_mode))  continue;
    if((cdir_path[i].used == 1) && (cdir_path[i].info.st_ino == cdir_path[num].info.st_ino)){
      return i;
    }
  }
  return -1;
}
int syncmeta(char *path, csiebox_client *client){
  fprintf(stderr, "sync meta : %s\n", path);
  // init meta protocol
  csiebox_protocol_meta req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  req.message.body.pathlen = strlen(path);
  lstat(path, &req.message.body.stat);
  // deal with all file
  // file -> md5 file data 
  if(S_ISREG(req.message.body.stat.st_mode)){
    md5_file(path, req.message.body.hash);
  }
  // send to server
  send_message(client->conn_fd, &req, sizeof(req));
  send_message(client->conn_fd, path, strlen(path));
  // receive outcome
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  recv_message(client->conn_fd, &header, sizeof(header));
  if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
      header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META &&
      header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      fprintf(stderr, "Receive OK from server\n");
      return 0; //if OK -> done
  }
  return 1; // need to sync file
}
void syncfile(int num, csiebox_client *client){
  // init
  fprintf(stderr, "sync file : %s\n", cdir_path[num].name);
  csiebox_protocol_file req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
  req.message.header.req.client_id = client->client_id;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);

  lstat(cdir_path[num].name, &req.message.body.stat);
  req.message.body.datalen = req.message.body.stat.st_size;
  req.message.body.pathlen = strlen(cdir_path[num].name);
  // send header to server
  send_message(client->conn_fd, &req, sizeof(req));
  // write into buffer
  char buffer[5000]="\0";
  memset(buffer, 0, 5000);
  if(S_ISREG(cdir_path[num].info.st_mode)){
    int filedes = open(cdir_path[num].name, O_RDONLY | O_RSYNC);
    if(filedes < 0){
      fprintf(stderr, "file cannot be opened\n");      
    }
    fprintf(stderr, "filesize = %d\n", cdir_path[num].info.st_size);
    read(filedes, buffer, cdir_path[num].info.st_size);
    fprintf(stderr, "buffer = %s\n", buffer);
    close(filedes);
  }
  else{
    int slinksuc = readlink(cdir_path[num].name, buffer, cdir_path[num].info.st_size);
    if(slinksuc < 0){
      fprintf(stderr, "softlink cannot be read\n");
    }
  }
  // send data to server
  send_message(client->conn_fd, buffer, cdir_path[num].info.st_size);
  send_message(client->conn_fd, cdir_path[num].name, strlen(cdir_path[num].name));
  //free(buffer);
}
void synchlink(int src, int target, csiebox_client *client){
  csiebox_protocol_hardlink req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK;
  req.message.header.req.client_id = client->client_id;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  req.message.body.srclen = strlen(cdir_path[src].name);
  req.message.body.targetlen = strlen(cdir_path[target].name);
  send_message(client->conn_fd, &req, sizeof(req));
  send_message(client->conn_fd, cdir_path[src].name, strlen(cdir_path[src].name));
  send_message(client->conn_fd, cdir_path[target].name, strlen(cdir_path[target].name));
}
void sendfile(int i, char *path, csiebox_client *client){
  int hardlinksrc = ishardlink(i);
  if(hardlinksrc >= 0){
    synchlink(hardlinksrc, i, client);
  }
  else{
    if(S_ISREG(cdir_path[i].info.st_mode)){
      // fprintf(stderr, "handle file : %s\n", cdir_path[i].name);
      // fprintf(stderr, "file size = %d\n", cdir_path[i].info.st_size);
      syncfile(i, client);
    }
    else if(S_ISLNK(cdir_path[i].info.st_mode)){
      fprintf(stderr, "soft link !!! %s\n", cdir_path[i].name);
      syncfile(i, client);
    }
  }  
}
int get_dir_name(char *des, int wd){
  char tmp[400] = "\0";
  for(int i = 0; i < f_ptr; i++){
    if(watch_dir[i].wd == wd && watch_dir[i].used == 1){
      strcpy(des, watch_dir[i].name);
      return 1;
    }
  }
  return 0;
}
void go_rm(csiebox_client *client, char *pathname, char *dirname, int fd){
  csiebox_protocol_rm req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_RM;
  req.message.header.req.client_id = client->client_id;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  req.message.body.pathlen = strlen(pathname);
  send_message(client->conn_fd, &req, sizeof(req));
  fprintf(stderr, "pathname = %s\n", pathname);
  send_message(client->conn_fd, pathname, strlen(pathname));
  for(int i = 0; i < ptr; i++){
    if((strlen(pathname) == strlen(cdir_path[i].name) && !strcmp(pathname, cdir_path[i].name))){
      cdir_path[i].used = 0;
    }
  }
  for(int i = 0; i < f_ptr; i++){
    if((strlen(pathname) == strlen(watch_dir[i].name) && !strcmp(pathname, watch_dir[i].name))){
      watch_dir[i].used = 0;
      inotify_rm_watch(fd, watch_dir[i].wd);
    }

  }
}
void push_back_to_array(const char *pathname){
    strcpy(cdir_path[ptr].name, pathname);
    lstat(pathname, &cdir_path[ptr].info);
    cdir_path[ptr].used = 1;
    ptr++;  
}
void add_to_inotify(const char *path, int fd){
    int wd = inotify_add_watch(fd, path, IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY);
    strcpy(watch_dir[f_ptr].name, path);
    watch_dir[f_ptr].wd = wd;
    watch_dir[f_ptr].used = 1;
    f_ptr++;
}
void inotify(csiebox_client * client, int fd){
  int length, i = 0;
  int wd;
  char buffer[EVENT_BUF_LEN];
  memset(buffer, 0, EVENT_BUF_LEN);
  while ((length = read(fd, buffer, EVENT_BUF_LEN)) > 0) {
    i = 0;
    while (i < length) {
      fprintf(stderr, "--------------\n");
      struct inotify_event* event = (struct inotify_event*)&buffer[i];
      printf("event: (%d, %d, %s)\ntype: ", event->wd, strlen(event->name), event->name);
      // get dirpath
      char dirname[400] = "\0";
      memset(dirname, 0, 400);
      get_dir_name(dirname, event->wd);
      // get filepath
      char pathname[400] = "\0";
      memset(pathname, 0, 400);
      snprintf(pathname, sizeof(pathname),"%s/%s", dirname, event->name);

      fprintf(stderr, "monitor filepath = %s\n", pathname);
      struct stat tmp; 
      lstat(pathname, &tmp);
      if(event->mask & IN_ATTRIB) {
        printf("attrib ");
        for(int i = 0; i < ptr; i++){
          if(tmp.st_ino == cdir_path[i].info.st_ino){
            lstat(cdir_path[i].name, &cdir_path[i].info);
            //fprintf(stderr, "monitor filepath = %s\n", cdir_path[i].name);
            syncmeta(cdir_path[i].name, client);
          }
        }        
      }
      if (event->mask & IN_MODIFY) {
        printf("modify ");
        for(int i = 0; i < ptr; i++){
          if(tmp.st_ino == cdir_path[i].info.st_ino){
            lstat(cdir_path[i].name, &cdir_path[i].info);
            //fprintf(stderr, "monitor filepath = %s\n", cdir_path[i].name);
            sendfile(i, cdir_path[i].name, client);
          }
        }      
      }
      if (event->mask & IN_CREATE) {
        printf("create ");
        // modify itself
        push_back_to_array(pathname);
        if(S_ISDIR(tmp.st_mode)){
          add_to_inotify(pathname, fd);
        }   
        int ret = syncmeta(pathname, client);  
        if(ret == 1){
          sendfile(ptr-1, pathname, client);
        }
        // dir
        for(int i = 0; i < ptr; i++){
          if((strlen(dirname) == strlen(cdir_path[i].name)) &&
            (!strcmp(dirname, cdir_path[i].name))){
            lstat(cdir_path[i].name, &cdir_path[i].info);
            fprintf(stderr, "update mother_dir = %s\n", cdir_path[i].name);
            syncmeta(cdir_path[i].name, client);
          }
        }                 
      }
      if (event->mask & IN_DELETE) {
        printf("delete ");
        struct stat tmp;
        lstat(pathname, &tmp);
        fprintf(stderr, "pathname = %s\n", pathname);
        go_rm(client, pathname, dirname, fd);
        // dir
        for(int i = 0; i < ptr; i++){
          if((strlen(dirname) == strlen(cdir_path[i].name)) &&
            (!strcmp(dirname, cdir_path[i].name))){
            lstat(cdir_path[i].name, &cdir_path[i].info);
            fprintf(stderr, "update mother_dir = %s\n", cdir_path[i].name);
            syncmeta(cdir_path[i].name, client);
          }
        }
      }
      // type
      if (event->mask & IN_ISDIR) {
        printf("dir\n");
      } else {
        printf("file\n");
      }
      i += EVENT_SIZE + event->len;
    }
    memset(buffer, 0, EVENT_BUF_LEN);
  }
}


void traverse(char *dirname, int depth, char *lpath, int *maxdepth, int fd, csiebox_client * client){
    DIR *dir;
    struct dirent *entry;
    if (!(dir = opendir(dirname))){
      fprintf(stderr, "Directory open fail\n");
      return;      
    }
    // sync directory's meta
    syncmeta(dirname, client);
    push_back_to_array(dirname);
    add_to_inotify(dirname, fd);
    // open dir and read it
    while ((entry = readdir(dir)) != NULL) {
        // do not need to deal with that
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        // get curent path
        char path[500] = "\0";
        snprintf(path, sizeof(path),"%s/%s", dirname, entry->d_name);
        struct stat tmp;
        lstat(path, &tmp);
        // if dir -> recursively traverse
        if (S_ISDIR(tmp.st_mode)) {
            traverse(path, depth + 1, lpath, maxdepth, fd, client);
        }
        else{
          // handle maxdepth
          if(S_ISREG(tmp.st_mode) && depth > *maxdepth){
            *maxdepth = depth;
            strcpy(lpath, path+8);
            fprintf(stderr, "longest path = %s\n", lpath);
          }

          push_back_to_array(path);

          if(syncmeta(path, client)){
            fprintf(stderr, "I need to sync file %s\n", path);
            sendfile(ptr-1, path, client);  
          }
        }          
    }
    syncmeta(dirname, client);    
    closedir(dir);
}
int csiebox_client_run(csiebox_client* client) {
  if (!login(client)) {
    fprintf(stderr, "login fail\n");
    return 0;
  }
  fprintf(stderr, "login success\n");
  // get home dir
  char *home = client->arg.path;
  // create longestPath.txt
  char lpathstr[500] = "\0";
  int lpath_filedes = open("../cdir/longestPath.txt", 
                            O_WRONLY | O_CREAT | O_RSYNC, REG_S_FLAG);
  // initial inotify
  int fd = inotify_init();
  fprintf(stderr, "inotify_start, fd = %d\n", fd);
  if (fd < 0) {
    perror("inotify_init");
  }
  // traverse cdir
  int maxdepth = 0;
  traverse(home, 1, lpathstr, &maxdepth, fd, client);
  // write longestpath 
  write(lpath_filedes, lpathstr, sizeof(lpathstr));
  close(lpath_filedes);
  // inotify
  inotify(client, fd);
  return 1;
}

void csiebox_client_destroy(csiebox_client** client) {
  csiebox_client* tmp = *client;
  *client = 0;
  if (!tmp) {
    return;
  }
  close(tmp->conn_fd);
  free(tmp);
}

//read config file
static int parse_arg(csiebox_client* client, int argc, char** argv) {
  if (argc != 2) {
    return 0;
  }
  FILE* file = fopen(argv[1], "r");
  if (!file) {
    return 0;
  }
  fprintf(stderr, "reading config...\n");
  size_t keysize = 20, valsize = 20;
  char* key = (char*)malloc(sizeof(char) * keysize);
  char* val = (char*)malloc(sizeof(char) * valsize);
  ssize_t keylen, vallen;
  int accept_config_total = 5;
  int accept_config[5] = {0, 0, 0, 0, 0};
  while ((keylen = getdelim(&key, &keysize, '=', file) - 1) > 0) {
    key[keylen] = '\0';
    vallen = getline(&val, &valsize, file) - 1;
    val[vallen] = '\0';
    fprintf(stderr, "config (%d, %s)=(%d, %s)\n", keylen, key, vallen, val);
    if (strcmp("name", key) == 0) {
      if (vallen <= sizeof(client->arg.name)) {
        strncpy(client->arg.name, val, vallen);
        accept_config[0] = 1;
      }
    } else if (strcmp("server", key) == 0) {
      if (vallen <= sizeof(client->arg.server)) {
        strncpy(client->arg.server, val, vallen);
        accept_config[1] = 1;
      }
    } else if (strcmp("user", key) == 0) {
      if (vallen <= sizeof(client->arg.user)) {
        strncpy(client->arg.user, val, vallen);
        accept_config[2] = 1;
      }
    } else if (strcmp("passwd", key) == 0) {
      if (vallen <= sizeof(client->arg.passwd)) {
        strncpy(client->arg.passwd, val, vallen);
        accept_config[3] = 1;
      }
    } else if (strcmp("path", key) == 0) {
      if (vallen <= sizeof(client->arg.path)) {
        strncpy(client->arg.path, val, vallen);
        accept_config[4] = 1;
      }
    }
  }
  free(key);
  free(val);
  fclose(file);
  int i, test = 1;
  for (i = 0; i < accept_config_total; ++i) {
    test = test & accept_config[i];
  }
  if (!test) {
    fprintf(stderr, "config error\n");
    return 0;
  }
  return 1;
}

static int login(csiebox_client* client) {
  csiebox_protocol_login req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_LOGIN;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  memcpy(req.message.body.user, client->arg.user, strlen(client->arg.user));
  md5(client->arg.passwd,
      strlen(client->arg.passwd),
      req.message.body.passwd_hash);
  if (!send_message(client->conn_fd, &req, sizeof(req))) {
    fprintf(stderr, "send fail\n");
    return 0;
  }
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  if (recv_message(client->conn_fd, &header, sizeof(header))) {
    if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
        header.res.op == CSIEBOX_PROTOCOL_OP_LOGIN &&
        header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) {
      client->client_id = header.res.client_id;
      return 1;
    } else {
      return 0;
    }
  }
  return 0;
}
