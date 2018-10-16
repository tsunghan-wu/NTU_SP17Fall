#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <map>
#include <string>
#include <algorithm>
#include <vector>
#include "list_file.h"
using namespace std; 
char loser_record[] = ".loser_record";
char loser_config[] = ".loser_config";
int cmd(char *s){
	if(strcmp("status", s) == 0)	return 1;
	if(strcmp("commit", s) == 0)	return 2;
	if(strcmp("log", s) == 0)	return 3;
	return 0;
}

int special_case(char *s){
	if(strcmp(s, ".loser_record") == 0)	return 1;
	if(strcmp(s, ".") == 0)	return 1;
	if(strcmp(s, "..") == 0)	return 1;
	return 0;
}
void merge_name(char s[], char *record, char file[]){
	string dir(s);
	dir += "/";
	dir += file;
	for(unsigned int i = 0; i < dir.size(); i++)
		record[i] = dir[i];
	return;
}

void status_commit(char s[], int action){
	// open .loser_record
	char record[300] = "\0";
	merge_name(s, record, loser_record);
	//cout << "record = " << record << "\n";
	FILE *fin, *fout;
	fin = fopen(record, "r");
	// read all file in the dir -> read in the vector<> file_in_dir 
	vector<string> file_in_dir;
	struct FileNames file_names = list_file(s);
	if(file_names.length == -1)	fprintf(stderr, "error\n");
		for (size_t i = 0; i < file_names.length; i++) {
			if(special_case(file_names.names[i]))	continue;
			file_in_dir.push_back(file_names.names[i]);
		}
  	free_file_names(file_names);
	sort(file_in_dir.begin(), file_in_dir.end());

	// store <filename> <md5sum> in map
	map<string,string> files, re_files;
	map<string,string>::iterator it, re_it;
	// if .loser_record didn't exist
	if(!fin){
		if(action == 1)	fout = stdout;
		else{
			if(file_in_dir.size() == 0)	return;
			fout = fopen(record, "a");
			fputs("# commit 1\n", fout);
		}
		fputs("[new_file]\n", fout);
		for (unsigned int i = 0; i < file_in_dir.size(); i++)
			fputs(file_in_dir[i].c_str(), fout), fputs("\n", fout);
		fputs("[modified]\n", fout);
		fputs("[copied]\n", fout);
		if(action == 2){
			for (unsigned int i = 0; i < file_in_dir.size(); i++){
				char md5_check[300] = "\0", tmp2[300] = "\0", F_name[300] = "\0";
				strcpy(F_name,file_in_dir[i].c_str());
				merge_name(s, tmp2, F_name);
				md5sum(tmp2, md5_check);
				files.insert (pair<string,string>(file_in_dir[i],md5_check));
			}
			fputs("(MD5)", fout);
			fputs("\n", fout);
			for(it = files.begin(); it != files.end(); it++)
				fprintf(fout,"%s %s\n", it->first.c_str(), it->second.c_str());
	//	fclose(fin);
	//	fclose(fout);
		}
		return;
	}
	// if .loser_record exist
	else{
		// read .loser_record and get # pos
		fseek(fin, -1, SEEK_END);
		char c;
		while((c = fgetc(fin)) != EOF){
			//printf("> %c", c);
			if(c == '#')
				break;
			fseek(fin, -2, SEEK_CUR);
		}

		int N = 0;
		char tmp[20], buffer[2000], exist_file[256], checksum[256];
		fseek(fin, 1, SEEK_CUR);
		// read commit count
		fscanf(fin, "%s %d\n", tmp, &N);
		// read .loser_record -> (MD5)
		while(fgets(buffer, 2000, fin)){
			//puts(buffer);
			if(!strcmp("(MD5)\n", buffer))
				break;
		}

		// store <filename> <md5sum> in map
		while(fscanf(fin,"%s %s\n", exist_file, checksum) == 2){
			//puts(exist_file);
			//puts(checksum);
			files.insert (pair<string,string>(exist_file,checksum));			
			re_files.insert (pair<string,string>(checksum,exist_file));				
		}
		// check new_file modified copied & copied_src
		vector<string> newfile, modified, copied, copied_src;
		int diff = 0;
		for (unsigned int i = 0; i < file_in_dir.size(); i++){
			char md5_check[300] = "\0", tmp2[300] = "\0", F_name[300] = "\0";
			strcpy(F_name,file_in_dir[i].c_str());
			merge_name(s, tmp2, F_name);
			md5sum(tmp2, md5_check);
			// insert it into the vector
			it = files.find(file_in_dir[i]);
			re_it = re_files.find(md5_check);
			if(it == files.end() && re_it == re_files.end())	//newfile
				newfile.push_back(file_in_dir[i]), diff = 1;

			else if(it == files.end() && re_it != re_files.end())	//copied
				copied.push_back(file_in_dir[i]), copied_src.push_back(re_it->second), diff = 1;
			
			else if(it != files.end() && re_it == re_files.end()){	// modified
				modified.push_back(file_in_dir[i]), diff = 1;
				files.erase (file_in_dir[i]);
			}else{	// another modified
				if(it->second != string(md5_check)){
						files.erase (file_in_dir[i]);
						modified.push_back(file_in_dir[i]), diff = 1;
				}
			}
			files.insert (pair<string,string>(file_in_dir[i],md5_check));
			re_files.insert (pair<string,string>(md5_check,file_in_dir[i]));
		}
		if(!diff && action == 2)	return;
		// sort 3 vector
		sort(newfile.begin(), newfile.end());
		sort(modified.begin(), modified.end());
		sort(copied.begin(), copied.end());
		//free_file_names(file_names);
		
		// print <status> or <commit>
		if(action == 1)	fout = stdout;
		else
			fout = fopen(record, "a"), fprintf(fout,"\n# commit %d\n", N+1);
		fputs("[new_file]", fout);
		fputs("\n", fout);
		for(unsigned int i = 0; i < newfile.size(); i++)
			fputs(newfile[i].c_str(), fout), fputs("\n", fout);
		fputs("[modified]", fout);
		fputs("\n", fout);
		for(unsigned int i = 0; i < modified.size(); i++)
			fputs(modified[i].c_str(), fout), fputs("\n", fout);
		fputs("[copied]", fout);
		fputs("\n", fout);
		for(unsigned int i = 0; i < copied.size(); i++)
			fputs(copied_src[i].c_str(), fout), fprintf(fout," => "), fputs(copied[i].c_str(), fout), fputs("\n", fout);
		// print (MD5)
		if(action == 2){
			fputs("(MD5)", fout);
			fputs("\n", fout);
			for(it = files.begin(); it != files.end(); it++)
				fprintf(fout,"%s %s\n", it->first.c_str(), it->second.c_str());
		}
	}
}
void log(char *number, char *dir){
	char record[300] = "\0";
	merge_name(dir, record, loser_record);
	FILE *fp = fopen(record, "r");
	if(!fp){
		return;
	}
	fseek(fp, 0, SEEK_END);
	char c;
	int num = atoi(number);
	long long tag = ftell(fp) + 1;
	char buffer[1000000] = "\0";
	for(int i = 0; i < num; i++){
		fseek(fp, tag-2, SEEK_SET);
		while((c = fgetc(fp)) != EOF){
			if(c == '#'){
				tag = ftell(fp);
				break;
			}
			fseek(fp, -2, SEEK_CUR);
		}
		//printf("tag = %d", tag);
		printf("#");
		int ptr = 0;
		//buffer[0] = '\0';
		while( (c = fgetc(fp)) != EOF){
			if(c == '#')
				break;
			buffer[ptr++] = c;
			//puts(buffer);
		}
		buffer[--ptr] = '\0';
		printf("%s", buffer);
		if(i == 0)	puts("");
		if(tag == 1)	break;
		if(i != num-1)	puts("");
	}
}

int ALIAS(char s[], char p[]){
	char config[300] = "\0";
	merge_name(s, config, loser_config);
	FILE *fp = fopen(config, "rb");
	if(fp){
		char alias[300], oper[5], name[300];
		while(fscanf(fp, "%s %s %s", alias, oper, name) == 3){
			if(strcmp(alias, p) == 0){
				return cmd(name);
			}
		}
	}
	return 0;
}

int main(int argc, char *argv[]){
	if(argc < 3){
		fprintf(stderr, "error\n");
		exit(1);
	}
	// judge action
	int action = cmd(argv[1]);	// 1-> status 2-> commit 3-> log
	if(!action){	// alias
		action = (argc == 3)? ALIAS(argv[2], argv[1]) : ALIAS(argv[3], argv[2]);
	}
	if(!action){
		fprintf(stderr, "error\n");
		exit(1);
	}
	if(action == 1 || action == 2){
		status_commit(argv[2], action);	
	}
	else{	// log
		log(argv[2], argv[3]);
	}
	return 0;
}

