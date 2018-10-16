#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <string.h>
#include <assert.h>
#include <string>
using namespace std;
typedef pair<int,int> ii;
enum Status{
  IDLE = 0x90,
  TRY_MATCH = 0x91,
  MATCH = 0x92,
  WORK = 0x93,
};
struct User {
	char name[33];
	unsigned int age;
	char gender[7];
	char introduction[1025];
};
void child_process(int number);
struct BOSS{
	int has_job;
	int assign_next_index;
	int need_to_Assign_job;
	int solution;
	int num_of_rm;
	int working_on_fd;
	void init(){
		has_job = 0;
		assign_next_index = 0;
		need_to_Assign_job = 0;
		solution = 4096;
		working_on_fd = -1;
	}
};
class Client{

public:
	int friend_fd;
	int my_fd;
	Status status;
	string tcp_buffer;
	string filter_function;
	string json_package;
	struct User me;
	struct User friends;
	int new_msg(char*,int);
	void get_info(string,string,int,string,string,string);
	void handle_quit();
	void start_chat(int);
	int compile_code();
	void done(){
		status = IDLE;
		tcp_buffer.clear();
		filter_function.clear();
		json_package.clear();
		memset(&me, 0, sizeof(struct User));
		memset(&friends, 0, sizeof(struct User));
	}
	Client(){
		status = IDLE;
	}
};
class Process{

public:
	int input_fd;	// for write
	int output_fd;	// for read
	pid_t PID;
	Status status;
	int waiting_list_index;
	int valid_ans;
	ii job_fd;
	void init_process(int, pid_t);
	void GO_TO_WORK(int);
	void receive_from_child();
};