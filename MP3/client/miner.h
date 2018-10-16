#include <stdio.h>
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
