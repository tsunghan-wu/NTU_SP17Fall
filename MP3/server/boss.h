#include <limits.h>
struct pipe_pair{
    char input_pipe[PATH_MAX];
    char output_pipe[PATH_MAX];
};

struct fd_pair{
    int input_fd;
    int output_fd;
};

struct server_config{
    char mine_file[PATH_MAX];
    struct pipe_pair pipes[10];
    int num_miners;
};
typedef enum{
	WORK = 0x00,
	QUIT = 0x01,
	GET = 0x02,
	STATUS = 0x03,
}op;

typedef struct doki_doki{
	uint8_t op;	// operation
	uint8_t treasure;
	uint16_t datalen;
	uint16_t namelen;	
	MD5_CTX mdContext;
}mine_sync;

typedef struct shit{
	uint8_t  treasure;
	uint16_t interval_start; 
	uint16_t interval_end;
	MD5_CTX mdContext;
}Shit;

typedef struct dump_info{
	char location[PATH_MAX];
	int fd;
	int start;
	int end;
	int success;
}Dump;