#include "csiebox_server.h"
#include "csiebox_common.h"
#include "connect.h"
#include <utime.h>
#include <sys/time.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>

static int parse_arg(csiebox_server* server, int argc, char** argv);
static void handle_request(csiebox_server* server, int conn_fd);
static int get_account_info(csiebox_server* server,  const char* user, csiebox_account_info* info);
static void login(csiebox_server* server, int conn_fd, csiebox_protocol_login* login);
static void logout(csiebox_server* server, int conn_fd);
static char* get_user_homedir(csiebox_server* server, csiebox_client_info* info);

#define DIR_S_FLAG (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)//permission you can use to create new file
#define REG_S_FLAG (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)//permission you can use to create new directory

//read config file, and start to listen
void csiebox_server_init(csiebox_server** server, int argc, char** argv) {
  csiebox_server* tmp = (csiebox_server*)malloc(sizeof(csiebox_server));
  if (!tmp) {
    fprintf(stderr, "server malloc fail\n");
    return;
  }
  memset(tmp, 0, sizeof(csiebox_server));
  if (!parse_arg(tmp, argc, argv)) {
    fprintf(stderr, "Usage: %s [config file]\n", argv[0]);
    free(tmp);
    return;
  }
  int fd = server_start();
  if (fd < 0) {
    fprintf(stderr, "server fail\n");
    free(tmp);
    return;
  }
  tmp->client = (csiebox_client_info**)
      malloc(sizeof(csiebox_client_info*) * getdtablesize());
  if (!tmp->client) {
    fprintf(stderr, "client list malloc fail\n");
    close(fd);
    free(tmp);
    return;
  }
  memset(tmp->client, 0, sizeof(csiebox_client_info*) * getdtablesize());
  tmp->listen_fd = fd;
  *server = tmp;
}

//wait client to connect and handle requests from connected socket fd
int csiebox_server_run(csiebox_server* server) {
  int conn_fd, conn_len;
  struct sockaddr_in addr;
  while (1) {
    memset(&addr, 0, sizeof(addr));
    conn_len = 0;
    // waiting client connect
    conn_fd = accept(server->listen_fd, (struct sockaddr*)&addr, (socklen_t*)&conn_len);
    if (conn_fd < 0) {
      if (errno == ENFILE) {
          fprintf(stderr, "out of file descriptor table\n");
          continue;
        } else if (errno == EAGAIN || errno == EINTR) {
          continue;
        } else {
          fprintf(stderr, "accept err\n");
          fprintf(stderr, "code: %s\n", strerror(errno));
          break;
        }
    }
    // handle request from connected socket fd
    handle_request(server, conn_fd);
  }
  return 1;
}

void csiebox_server_destroy(csiebox_server** server) {
  csiebox_server* tmp = *server;
  *server = 0;
  if (!tmp) {
    return;
  }
  close(tmp->listen_fd);
  int i = getdtablesize() - 1;
  for (; i >= 0; --i) {
    if (tmp->client[i]) {
      free(tmp->client[i]);
    }
  }
  free(tmp->client);
  free(tmp);
}

//read config file
static int parse_arg(csiebox_server* server, int argc, char** argv) {
  if (argc != 2) {
    return 0;
  }
  FILE* file = fopen(argv[1], "r");
  if (!file) {
    return 0;
  }
  //fprintf(stderr, "reading config...\n");
  size_t keysize = 20, valsize = 20;
  char* key = (char*)malloc(sizeof(char) * keysize);
  char* val = (char*)malloc(sizeof(char) * valsize);
  ssize_t keylen, vallen;
  int accept_config_total = 2;
  int accept_config[2] = {0, 0};
  while ((keylen = getdelim(&key, &keysize, '=', file) - 1) > 0) {
    key[keylen] = '\0';
    vallen = getline(&val, &valsize, file) - 1;
    val[vallen] = '\0';
    //fprintf(stderr, "config (%d, %s)=(%d, %s)\n", keylen, key, vallen, val);
    if (strcmp("path", key) == 0) {
      if (vallen <= sizeof(server->arg.path)) {
        strncpy(server->arg.path, val, vallen);
        accept_config[0] = 1;
      }
    } else if (strcmp("account_path", key) == 0) {
      if (vallen <= sizeof(server->arg.account_path)) {
        strncpy(server->arg.account_path, val, vallen);
        accept_config[1] = 1;
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
void change_path(char *serverpath, char *clientpath){
  char path[400] = "\0";
  snprintf(path, sizeof(path), "%s/%s", serverpath, clientpath+8);
  strcpy(serverpath, path);
  //strcat(serverpath, "/");
  //strcat(serverpath, clientpath+8);
}

void sym_update_time(char *filename, long mtime, long atime){
  fprintf(stderr, "mtime = %llu, atime = %llu\n", mtime, atime);
  struct timeval new_times[2];
  memset(new_times, 0, sizeof(new_times));
  new_times[0].tv_sec = atime;
  new_times[1].tv_sec = mtime;
  int suc = lutimes(filename, new_times);
   if(suc == -1)
     fprintf(stderr, "error = %s", strerror(errno));
}

void update_time(char *filename, long mtime, long atime){
  fprintf(stderr, "mtime = %llu, atime = %llu\n", mtime, atime);
  struct timeval new_times[2];
  memset(new_times, 0, sizeof(new_times));
  new_times[0].tv_sec = atime;
  new_times[1].tv_sec = mtime;
  int suc = utimes(filename, new_times);
  if(suc == -1)
    fprintf(stderr, "errno = %s", strerror(errno));
}
int get_client_meta(int conn_fd, csiebox_protocol_meta* meta, char *serverpath) {
  //recv path from client
  char client_path[400]="\0";
  //char client_path[400] = "\0";
  memset(client_path, 0, 400);
  recv_message(conn_fd, client_path, meta->message.body.pathlen);
  // server path
  change_path(serverpath, client_path);
  fprintf(stderr, "What i get is %s\n", serverpath);
  // initiate header
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
  header.res.datalen = 0;
  // Judge file type
  if(S_ISLNK(meta->message.body.stat.st_mode)){ // softlink -> more
    header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
  }
  else if(S_ISDIR(meta->message.body.stat.st_mode)){  // Dir -> mkdir
    mkdir(serverpath, meta->message.body.stat.st_mode);
    chmod(serverpath, meta->message.body.stat.st_mode);
    update_time(serverpath, meta->message.body.stat.st_mtime, meta->message.body.stat.st_atime);
    header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;    
  }
  else if(S_ISREG(meta->message.body.stat.st_mode)){  // reg file -> hash -> memcpy
    uint8_t hash[MD5_DIGEST_LENGTH];
    int fd = open(serverpath, O_RDONLY);
    if (fd < 0) { // doesn't exist -> sync file
      header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
    }
    else{ // exist -> check hash 
      md5_file(serverpath, hash);
      if (memcmp(meta->message.body.hash, hash, sizeof(hash)) != 0) {  // if hash X -> syncfile
        header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
        fprintf(stderr, "need to syncfile\n");
      }
      else{
        chmod(serverpath, meta->message.body.stat.st_mode);
        fprintf(stderr, "need not to sync file\n");
        fprintf(stderr, "mtime = %llu, atime = %llu\n", 
          meta->message.body.stat.st_mtime, meta->message.body.stat.st_atime);
        update_time(serverpath, meta->message.body.stat.st_mtime, meta->message.body.stat.st_atime);
        header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
      }      
    }
  }
  send_message(conn_fd, &header, sizeof(header));
  //free(client_path);
}
int get_client_file(int conn_fd,csiebox_protocol_file *file, char *serverpath){
  //recv data
  char buffer[5000]="\0";
  memset(buffer, 0, 5000);
  recv_message(conn_fd, buffer, file->message.body.datalen);
  // recv client path
  char client_path[400]="\0";
  memset(client_path, 0, 400);
  recv_message(conn_fd, client_path, file->message.body.pathlen);
  // server path 
  change_path(serverpath, client_path);
  fprintf(stderr, "sync file_path %s\n", serverpath);
  // write to file
  if(S_ISREG(file->message.body.stat.st_mode)){
    fprintf(stderr, "regular file : %s\n", serverpath);
    int newfiledes = open(serverpath, O_WRONLY | O_TRUNC | O_CREAT, file->message.body.stat.st_mode);
    write(newfiledes, buffer, file->message.body.datalen);
    close(newfiledes);
    update_time(serverpath, file->message.body.stat.st_mtime, file->message.body.stat.st_atime);
  }
  else{
    symlink(buffer, serverpath);
    sym_update_time(serverpath, file->message.body.stat.st_mtime, file->message.body.stat.st_atime);
  }
}
int handle_remove(int conn_fd, csiebox_protocol_rm *rm, char *serverpath){
  //recv data
  char client_path[400] = "\0";
  fprintf(stderr, "pathlen = %d\n", rm->message.body.pathlen); 
  recv_message(conn_fd, client_path, rm->message.body.pathlen);
  fprintf(stderr, "client path = %s\n", client_path);
  change_path(serverpath, client_path);
  fprintf(stderr, "server path = %s\n", serverpath);
  struct stat tmp;
  lstat(serverpath, &tmp);
  if(S_ISDIR(tmp.st_mode)){
    fprintf(stderr, "serverpath = %s\n", serverpath);
    if (rmdir(serverpath) < 0) fprintf(stderr, "-1\n");
  }
  else{
    unlink(serverpath);
  }
}
int get_client_hlink(int conn_fd, csiebox_protocol_hardlink *hardlink, char *srcserverpath){
  //printf("%s\n", "<SYNC HLINK>");
  char src_client_path[400]="\0";
  char target_client_path[400]="\0";  
  memset(src_client_path, 0, 400);
  memset(target_client_path, 0, 400);
  // recv src file and target file
  recv_message(conn_fd, src_client_path, hardlink->message.body.srclen);
  recv_message(conn_fd, target_client_path, hardlink->message.body.targetlen);
  //fprintf(stderr, "srcbuf = %s targetbuf = %s\n", srcbuf, targetbuf);
  // change to server path
  char targetserverpath[400] = "\0";
  strcpy(targetserverpath, srcserverpath);
  change_path(targetserverpath, target_client_path);
  change_path(srcserverpath, src_client_path);
  // link
  struct stat tmp;
  lstat(srcserverpath, &tmp); 
  link(srcserverpath, targetserverpath);
  update_time(targetserverpath, tmp.st_mtime, tmp.st_atime);
}
//this is where the server handle requests, you should write your code here
static void handle_request(csiebox_server* server, int conn_fd) {
  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  while (recv_message(conn_fd, &header, sizeof(header))) {
    fprintf(stderr, "------------------------\n");
    if (header.req.magic != CSIEBOX_PROTOCOL_MAGIC_REQ) {
      continue;
    }
    switch (header.req.op) {
      case CSIEBOX_PROTOCOL_OP_LOGIN:
        fprintf(stderr, "login\n");
        csiebox_protocol_login req;
        if (complete_message_with_header(conn_fd, &header, &req)) {
          login(server, conn_fd, &req);
        }
        break;
      case CSIEBOX_PROTOCOL_OP_SYNC_META:  
        fprintf(stderr, "sync meta\n");
        csiebox_protocol_meta meta;
        if (complete_message_with_header(conn_fd, &header, &meta)) {
          char* homedir = malloc(300);
          memset(homedir, 0, 300);
          homedir = get_user_homedir(server, server->client[conn_fd]);
          //fprintf(stderr, "homedir = %s\n", homedir);
          get_client_meta(conn_fd, &meta, homedir);
        }
        break;
      case CSIEBOX_PROTOCOL_OP_SYNC_FILE:
        fprintf(stderr, "sync file\n");
        csiebox_protocol_file file;
        if (complete_message_with_header(conn_fd, &header, &file)) {
          char* homedir = malloc(300);
          memset(homedir, 0, 300);
          homedir = get_user_homedir(server, server->client[conn_fd]);
          get_client_file(conn_fd, &file, homedir);
        }
        break;
      case CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK:
        fprintf(stderr, "sync hardlink\n");
        csiebox_protocol_hardlink hardlink;
        if (complete_message_with_header(conn_fd, &header, &hardlink)) {
          char* homedir = malloc(300);
          memset(homedir, 0, 300);
          homedir = get_user_homedir(server, server->client[conn_fd]);
          get_client_hlink(conn_fd, &hardlink, homedir);
        }
        break;
      case CSIEBOX_PROTOCOL_OP_SYNC_END:
        fprintf(stderr, "sync end\n");
        csiebox_protocol_header end;
          //====================
          //        TODO
          //====================
        break;
      case CSIEBOX_PROTOCOL_OP_RM:
        fprintf(stderr, "rm\n");
        csiebox_protocol_rm rm;
        if (complete_message_with_header(conn_fd, &header, &rm)) {
          char* homedir = malloc(300);
          memset(homedir, 0, 300);
          homedir = get_user_homedir(server, server->client[conn_fd]);
          handle_remove(conn_fd, &rm, homedir);
        }
        break;
      default:
        fprintf(stderr, "unknown op %x\n", header.req.op);
        break;
    }
  }
  fprintf(stderr, "end of connection\n");
  logout(server, conn_fd);
}

//open account file to get account information
static int get_account_info(
  csiebox_server* server,  const char* user, csiebox_account_info* info) {
  FILE* file = fopen(server->arg.account_path, "r");
  if (!file) {
    return 0;
  }
  size_t buflen = 100;
  char* buf = (char*)malloc(sizeof(char) * buflen);
  memset(buf, 0, buflen);
  ssize_t len;
  int ret = 0;
  int line = 0;
  while ((len = getline(&buf, &buflen, file) - 1) > 0) {
    ++line;
    buf[len] = '\0';
    char* u = strtok(buf, ",");
    if (!u) {
      fprintf(stderr, "illegal form in account file, line %d\n", line);
      continue;
    }
    if (strcmp(user, u) == 0) {
      memcpy(info->user, user, strlen(user));
      char* passwd = strtok(NULL, ",");
      if (!passwd) {
        fprintf(stderr, "illegal form in account file, line %d\n", line);
        continue;
      }
      md5(passwd, strlen(passwd), info->passwd_hash);
      ret = 1;
      break;
    }
  }
  free(buf);
  fclose(file);
  return ret;
}

//handle the login request from client
static void login(csiebox_server* server, int conn_fd, csiebox_protocol_login* login) {
  int succ = 1;
  csiebox_client_info* info = (csiebox_client_info*)malloc(sizeof(csiebox_client_info));
  memset(info, 0, sizeof(csiebox_client_info));
  if (!get_account_info(server, login->message.body.user, &(info->account))) {
    fprintf(stderr, "cannot find account\n");
    succ = 0;
  }
  if (succ && memcmp(login->message.body.passwd_hash, info->account.passwd_hash, MD5_DIGEST_LENGTH) != 0) {
    fprintf(stderr, "passwd miss match\n");
    succ = 0;
  }

  csiebox_protocol_header header;
  memset(&header, 0, sizeof(header));
  header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
  header.res.op = CSIEBOX_PROTOCOL_OP_LOGIN;
  header.res.datalen = 0;
  if (succ) {
    if (server->client[conn_fd]) {
      free(server->client[conn_fd]);
    }
    info->conn_fd = conn_fd;
    server->client[conn_fd] = info;
    header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
    header.res.client_id = info->conn_fd;
    char* homedir = get_user_homedir(server, info);
    mkdir(homedir, DIR_S_FLAG);
    free(homedir);
  } else {
    header.res.status = CSIEBOX_PROTOCOL_STATUS_FAIL;
    free(info);
  }
  send_message(conn_fd, &header, sizeof(header));
}

static void logout(csiebox_server* server, int conn_fd) {
  free(server->client[conn_fd]);
  server->client[conn_fd] = 0;
  close(conn_fd);
}

static char* get_user_homedir(
  csiebox_server* server, csiebox_client_info* info) {
  char* ret = (char*)malloc(sizeof(char) * PATH_MAX);
  memset(ret, 0, PATH_MAX);
  sprintf(ret, "%s/%s", server->arg.path, info->account.user);
  return ret;
}

