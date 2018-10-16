#define MAX_FD 1025
#define MAX_P 4
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>

#include <iostream>
#include <algorithm>
#include <vector>
#include <string>

#include "common.h"
#include "json.hpp"
using json = nlohmann::json;
using namespace std;
typedef pair<int,int> ii;
Client clients[1<<11];
char ret[] = "{\"cmd\":\"try_match\"}\n";
char quit[] = "{\"cmd\":\"quit\"}\n";
char inform[] = "{\"cmd\":\"other_side_quit\"}\n";
char match[] = "{\"cmd\": \"matched\"";
char receive[] = "{\"cmd\": \"receive_message\"";
char input_pipe[] = "./mama _in";
char output_pipe[] = "./mama _out";
Process process_pool[10];
struct BOSS mama;	
vector<int> try_list;
vector<int> waiting_list;
void remove_try_match(int fd){
	fprintf(stderr, "---------------REMOVE---------------\n");
	// Sync child processes state
	for(int i = 0; i < MAX_P; i++){
		int wait_idx = process_pool[i].waiting_list_index;
		if(mama.working_on_fd == fd || waiting_list[wait_idx] == fd){
			process_pool[i].valid_ans = 0;
			cout << "invalid process " << i << "\n" << flush;
		}
	}
	// remove try list
	for(unsigned int i = 0; i < try_list.size(); i++){
		if(fd == try_list[i]){
			try_list.erase(try_list.begin()+i);
			return;
		}
	}
	if(fd == mama.working_on_fd){
		mama.working_on_fd = -1;
		return;
	}
	// in waiting list
	int quit_idx = 4096;
	for(unsigned int i = 0; i < waiting_list.size(); i++){
		if(waiting_list[i] == fd){
			quit_idx = i;
			
		}
	}
	waiting_list.erase(waiting_list.begin()+quit_idx);
	// if next index need to minus
	for(int i = 0; i < MAX_P; i++){
		fprintf(stderr, "P[%d] at %d\n", i, process_pool[i].waiting_list_index);
		if(quit_idx < process_pool[i].waiting_list_index){
			fprintf(stderr, "P[%d]--\n", i);
			process_pool[i].waiting_list_index--;
		}
	}
	// next indexx need to minus?
	if(quit_idx < mama.assign_next_index){
		mama.assign_next_index--;
		fprintf(stderr, "next minus\n");
	}
	// if answer need to minus
	if(quit_idx < mama.solution && mama.solution != 4096){
		mama.solution--;
		fprintf(stderr, "solution minus\n");
	}
	return;
}
int Initialize_socket(int port){
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	assert(sockfd >= 0);
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr)); // 清零初始化，不可省略
	server_addr.sin_family = PF_INET;              // 位置類型是網際網路位置
	server_addr.sin_addr.s_addr = INADDR_ANY;      // INADDR_ANY 是特殊的IP位置，表示接受所有外來的連線
	server_addr.sin_port = htons(port);           // 在 44444 號 TCP 埠口監聽新連線
	// 使用 bind() 將伺服socket"綁定"到特定 IP 位置
	if(bind(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1){
		fprintf(stderr, "socket fail\n");
		exit(0);
	}
	// 呼叫 listen() 告知作業系統這是一個伺服socket
	int retval = listen(sockfd, 5);
	assert(!retval);
	return sockfd;
}
void Client::get_info(string target, string name, int age, string gender, string intro, string filter){
	status = TRY_MATCH;
	me.age = age;
	strcpy(me.name, name.c_str());
	strcpy(me.gender, gender.c_str());
	strcpy(me.introduction, intro.c_str());
	filter_function = filter;
	json_package = match;
	json_package += &target[18];
	// cout << "json package\n" << "fd = " << my_fd << "\n" << json_package << "\n" << flush;
}
int Client::new_msg(char *input, int size){
	tcp_buffer.append(input, 0, size);
	string::size_type pos;
	while((pos = tcp_buffer.find_first_of('\n')) != string::npos){
		// parse json
		string target = tcp_buffer.substr(0, pos);
		json Get = json::parse(target);
		tcp_buffer.erase(tcp_buffer.begin(), tcp_buffer.begin()+pos+1);
		//cout << "-----------------------json-----------------------\n" << target << "\n";
		if(Get["cmd"] == "try_match"){
			get_info(target, Get["name"], Get["age"], Get["gender"], 
					 Get["introduction"], Get["filter_function"]);
			send(my_fd, ret, strlen(ret), 0);
			return 1;
		}
		else if(Get["cmd"] == "quit"){
			return 2;
		}
		else if(Get["cmd"] == "send_message"){	// send to friend
			string QQ = target + "\n";
			send(my_fd, QQ.c_str(), QQ.length(), 0);
			string XD = receive;
			XD += &target[21];
			XD += "\n";
			send(friend_fd, XD.c_str(), XD.length(), 0);			
		}
	}
	return 0;
}
void open_fifo(){
	for(int i = 0; i < MAX_P; i++){
		input_pipe[6] = 48 + i;
		output_pipe[6] = 48 + i;
		int pip;
		pip = mkfifo(input_pipe, 0644);
		assert (!pip);
		pip = mkfifo(output_pipe, 0644);
		assert (!pip);			
	}	
}
int Client::compile_code(){
	// write code
	string filename = to_string(my_fd) + ".c";	// [fd].c
	string data = "struct User{ char name[33];\n unsigned int age;\n char gender[7];\n char introduction[1025];};\n" + filter_function;
	int code = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	write(code, data.c_str(), data.length());
	close(code);
	// gcc -fPIC -O2 -std=c11 [fd].c -shared -o [fd].so
	string command = "gcc -fPIC -O2 -std=c11 " + filename + " -shared -o " + to_string(my_fd) + ".so";
	// cout << "FLAG = " << my_fd << "\n" << flush;

	int result = system(command.c_str());
	if(result != 0)
		return 1;
	return 0;
}
void Client::start_chat(int new_friend){
	friend_fd = new_friend;
	//cout << "Friend --> " << my_fd << "," << new_friend << "\n" << flush;
	//cout << ">>> TEST\n" <<  clients[friend_fd].json_package << flush;
	memcpy(&friends, &clients[new_friend].me, sizeof(struct User));
	//fprintf(stderr, "--------------------Success--------------------\n");
	// pass json package
	status = MATCH;
	string tmp =  clients[friend_fd].json_package + "\n";
	send(my_fd, tmp.c_str(), tmp.length(), 0);
}
void Process::GO_TO_WORK(int wait_idx){
	// change something
	status = WORK;
	waiting_list_index = wait_idx;
	job_fd.first = mama.working_on_fd;
	job_fd.second = waiting_list[wait_idx];
	valid_ans = 1;
	// preprocess
	string file_one = to_string(job_fd.first) + ".so";
	string file_two = to_string(job_fd.second) + ".so";
	cout << "fileone = " << file_one << "\n" << flush;
	cout << "filetwo = " << file_two << "\n" << flush;
	int path1 = file_one.length();
	int path2 = file_two.length();
	// send
	write(input_fd, &path1, sizeof(uint8_t));
	write(input_fd, &path2, sizeof(uint8_t));
	write(input_fd, file_one.c_str(), file_one.length());
	write(input_fd, &clients[job_fd.first].me, sizeof(struct User));
	write(input_fd, file_two.c_str(), file_two.length());
	write(input_fd, &clients[job_fd.second].me, sizeof(struct User));
}
void assign_jobs(int process_index){
	if(mama.need_to_Assign_job == 0)	return;
	if(process_pool[process_index].status == WORK)	return;
	assert(process_pool[process_index].status == IDLE);
	process_pool[process_index].GO_TO_WORK(mama.assign_next_index);
	mama.assign_next_index++;
	return;
}
void judge_answer(){
	if(mama.has_job == 0 || mama.need_to_Assign_job == 1)	return;
	for(int i = 0; i < MAX_P; i++){
		if(process_pool[i].status == WORK)	return;
	}
	cout << "------------HAS CONCLUSION------------\n" << flush;
	if(mama.working_on_fd == -1){
		fprintf(stderr, "Nothing happen\n");
		mama.init();
		return;
	}
	if(mama.solution < 4096){
		fprintf(stderr, "Someone match\n");
		int client1 = mama.working_on_fd;
		int client2 = waiting_list[mama.solution];
		fprintf(stderr, "client1 = %d, client2 = %d\n", client1, client2);
		clients[client1].start_chat(client2);
		clients[client2].start_chat(client1);
		waiting_list.erase(waiting_list.begin()+mama.solution);
	}
	else{
		fprintf(stderr, "Match Fail\n");
		waiting_list.push_back(mama.working_on_fd);
	}
	mama.init();
	return;
}
void Process::receive_from_child(){
	int result = 0;
	if(read(output_fd, &result, sizeof(uint8_t)) == 0)
		return;
	status = IDLE;
	if(valid_ans == 0 || result == 0)	return;
	assert(result == 1);
	mama.solution = min(mama.solution, waiting_list_index);
	return;
}
void Process::init_process(int index, pid_t child){
	input_pipe[6] = 48 + index;
	output_pipe[6] = 48 + index; 
	input_fd = open(input_pipe, O_WRONLY);	// for write
	output_fd = open(output_pipe, O_RDONLY);	// for read
	assert(input_fd >= 0 && output_fd >= 0);
	status = IDLE;
	PID = child;
	waiting_list_index = -1;
	valid_ans = 0;
}
int main(int argc, char const *argv[]){
	if(argc <= 1){
		fprintf(stderr, "need port\n");
	}
	// Socket
	int sockfd = Initialize_socket(atoi(argv[1]));
	fd_set readset, working_readset;
	FD_ZERO(&readset);
	FD_SET(sockfd, &readset);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
	// fifo + child process
	open_fifo();
	for(int i = 0; i < MAX_P; i++){
		pid_t child = fork();
		if(child == 0){
			child_process(i);
		}
		else{
			process_pool[i].init_process(i, child);
			cout << "init process " << i << " success\n" << flush;
			FD_SET(process_pool[i].output_fd, &readset);    // set read read FD_SET
		}
	}
	cout << "Open socket and fifo\n" << flush;
	mama.init();
	while (1){
		memcpy(&working_readset, &readset, sizeof(fd_set));
		if(select(MAX_FD, &working_readset, NULL, NULL, &timeout)){
			for (int fd = 0; fd < MAX_FD; fd += 1){ // 用迴圈列舉描述子
				// 排除沒有事件的描述子
				if (!FD_ISSET(fd, &working_readset))
					continue;
				if (fd == sockfd){	// new connection
					struct sockaddr_in client_addr;
					socklen_t addrlen = sizeof(client_addr);
					int client_fd = accept(fd, (struct sockaddr*) &client_addr, &addrlen);
					cout << "--------New Connection--------->" << client_fd << "\n"; 
					if(client_fd >= 0){
						FD_SET(client_fd, &readset); // 加入新創的描述子，用於和客戶端連線
						clients[client_fd].my_fd = client_fd;
					}
				}        
				else{
					int flage = 0;
					for(int i = 0; i < MAX_P; i++){
						if(process_pool[i].output_fd == fd){
							// cout << "I get worker's response\n";
							cout << "I am process " << i << "\n" << flush;
							process_pool[i].receive_from_child();
							flage = 1;
						}
					}
					if(flage)	continue;

					// 這裏的描述子來自 accept() 回傳值，用於和客戶端連線

					char buffer[1<<13] = "\0";
					ssize_t sz = recv(fd, buffer, (1<<13), 0); // 接收資料
					//cout << buffer << " test" << endl;

					if (sz == 0){  // recv() 回傳值爲零表示客戶端已關閉連線
						// close fd
						cout << "close fd = " << fd << "\n";
						if(clients[fd].status == MATCH){
							send(clients[fd].friend_fd, inform, strlen(inform), 0);
						}
						if(clients[fd].status == TRY_MATCH){
							cout << "/C remove from match\n" << flush;
							remove_try_match(fd);
						}
						clients[fd].done();
						close(fd);
						FD_CLR(fd, &readset);
					}
					else if(sz < 0){ // 發生錯誤
						//fprintf(stderr, "Doki Doki\n");
					}
					else{ // sz > 0，表示有新資料讀入
						int next_step = clients[fd].new_msg(buffer, sz);
						if(next_step == 1){		//assign job
							// GCC
							int compile_result = clients[fd].compile_code();
							cout << "<<<<< " << fd << " send try match msg\n" << flush;
							if(compile_result == 0){
								try_list.push_back(fd);
							}
							else{
								fprintf(stderr, "Compile Error !\n");
							}
						}
						else if(next_step == 2){
							send(fd, quit, strlen(quit), 0);
							if(clients[fd].status == MATCH){
								int ffd = clients[fd].friend_fd;
								send(ffd, inform, strlen(inform), 0);
								clients[ffd].status = IDLE;	
							}
							else if(clients[fd].status == TRY_MATCH){
								cout << "/Q remove from match\n" << flush;
								remove_try_match(fd);								
							}
							clients[fd].status = IDLE;
						}					
					}
				}
			}			
		}
		// has answer?
		judge_answer();
		// try match function
		if(waiting_list.size() == 0 && try_list.size() > 0){
			waiting_list.push_back(try_list[0]);
			try_list.erase(try_list.begin());
		}
		// wait child pid
		for(int i = 0; i < MAX_P; i++){
			int tmp = waitpid(process_pool[i].PID,NULL,WNOHANG);
			if(tmp == process_pool[i].PID){
				fprintf(stderr, "Process [%d] dead\n", i);
				close(process_pool[i].input_fd);
				close(process_pool[i].output_fd);
				FD_CLR(process_pool[i].output_fd, &readset);
				// refork process
				pid_t child = fork();
				if(child == 0){
					child_process(i);
				}
				else{
					process_pool[i].init_process(i, child);
					FD_SET(process_pool[i].output_fd, &readset); // set read read FD_SET
				}
			}
		}
		// need to assign job ?
		if(try_list.size() > 0 && mama.has_job == 0){
			mama.need_to_Assign_job = 1;
			mama.has_job = 1;
			mama.working_on_fd = try_list[0];
			fprintf(stderr, "I am working on %d\n", mama.working_on_fd);
			try_list.erase(try_list.begin());
		}
		for(int i = 0; i < MAX_P; i++){
			if(waiting_list.size() == 0 || mama.assign_next_index >= int(waiting_list.size()) || mama.working_on_fd == -1 || mama.solution < 4096){
				mama.need_to_Assign_job = 0;
			}
			assign_jobs(i);
		}
	}
	close(sockfd);
	return 0;
}
