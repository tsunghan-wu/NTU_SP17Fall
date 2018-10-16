#include <stdio.h>
#include <openssl/md5.h>

void md5sum(char*filename, char *ans){
    unsigned char c[MD5_DIGEST_LENGTH];
    int i;
    FILE *inFile = fopen (filename, "rb");
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];
    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);
    int ptr = 0;
    for(i = 0; i < MD5_DIGEST_LENGTH; i++)	sprintf(&ans[ptr], "%02x", c[i]), ptr += 2;
    fclose (inFile);
	return;
}
