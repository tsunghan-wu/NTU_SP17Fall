#include <stdio.h>
#include <dlfcn.h>
#include <limits.h>
#include <iostream>
#include "common.h"
void child_process(int number){
	char input_pipe[] = "./mama _in";
	char output_pipe[] = "./mama _out";
	input_pipe[6] = 48 + number;
	output_pipe[6] = 48 + number;
	fd_set readset, working_readset;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    FD_ZERO(&readset);

	int input_fd = open(input_pipe, O_RDONLY);
	assert (input_fd >= 0);
	FD_SET(input_fd, &readset); // listen to server
	int output_fd = open(output_pipe, O_WRONLY);
	assert (output_fd >= 0);
	int maxfd = 50;

	char file_one[PATH_MAX+1];
	char file_two[PATH_MAX+1];

	struct User one;
	struct User two;
	while(1){
		FD_ZERO(&working_readset);
		memcpy(&working_readset, &readset, sizeof(readset)); 
		select(maxfd, &working_readset, NULL, NULL, &timeout);
		if(FD_ISSET(input_fd, &working_readset)){
			memset(file_one, 0, sizeof(file_one));
			memset(file_two, 0, sizeof(file_two));
			file_one[0] = file_two[0] = '.';
			file_one[1] = file_two[1] = '/';
			memset(&one, 0, sizeof(struct User));
			memset(&two, 0, sizeof(struct User));
			int len1 = 0, len2 = 0;
			int get = read(input_fd, &len1, sizeof(uint8_t));
			if(get == 0)	continue;
			read(input_fd, &len2, sizeof(uint8_t));
			read(input_fd, &file_one[2], len1);
			read(input_fd, &one, sizeof(struct User));
			read(input_fd, &file_two[2], len2);
			read(input_fd, &two, sizeof(struct User));
			// cout << "--------- Client Process ---------\n" << flush;
			// cout << "length1 = " << len1 << " length2 = " << len2 << "\n" << flush;
			// cout << "I am process " << number << "\n" << flush;
			// Dynamic linking
			// cout << "miner client1 = " << file_one << "\n";
			// cout << "miner client2 = " << file_two << "\n";
			// First people

			void* first = dlopen(file_one, RTLD_LAZY);
			assert(NULL != first);
			dlerror();
			int (*Filter_1)(struct User) = (int (*)(struct User)) dlsym(first, "filter_function");
			const char *dlsym_error = dlerror();
		    if (dlsym_error){
		        fprintf(stderr, "Cannot load symbol 'filter_function': %s\n", dlsym_error);
		        dlclose(first);
		    }
		    int result = Filter_1(two);
		    dlclose(first);
		    if(result != 0){
		    	// do again
		    	void* second = dlopen(file_two, RTLD_LAZY);
				assert(NULL != second);
				dlerror();
				int (*Filter_2)(struct User) = (int (*)(struct User)) dlsym(second, "filter_function");
				const char *dlsym_error = dlerror();
			    if (dlsym_error){
			        fprintf(stderr, "Cannot load symbol 'filter_function': %s\n", dlsym_error);
			        dlclose(second);
			    }
			    result = Filter_2(one);
			    dlclose(second);
		    }
		    if(result != 0){
		    	result = 1;
		    	cout << "try match success\n";
		    	write(output_fd, &result, sizeof(uint8_t));
		    }
		    else{
		    	cout << "try match fail\n";
		    	write(output_fd, &result, sizeof(uint8_t));
		    }
		}
	}
	_exit(0);
}