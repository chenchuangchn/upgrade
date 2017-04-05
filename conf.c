#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cJSON.h"
#include "conf.h"

#define UPGRADECMD "upgradecmd"
#define PROGRAMS "programs"
#define NAME "name"
#define VERSION "version"
#define DOWNLOADNAME "downloadname"
#define UPDATECMD "updatecmd"
#define RUNCMD "runcmd"

#define PAGESIZE 512

int cmp_md5_files(char *path1, char *path2)
{
	int fd1, fd2, ret;
	char buf[33] = {0};
	char buf2[33] = {0};
	
	fd1 = open(path1, O_RDONLY);
	if(-1 == fd1) {
		perror("open path1");
		ret = -1;
		goto failed;
	}
	fd2 = open(path2, O_RDONLY);
	if(-1 == fd2) {
		perror("open path2");
		goto open2;
	}
	if(-1 == read(fd1, buf, 32)) {
		perror("read fd1");
		goto end;
	}
	if(-1 == read(fd2, buf2, 32)) {
		perror("read fd2");
		ret = -1;
		goto end;
	}
	
	ret = strncmp(buf, buf2, 32);
	printf("ret = %d\n", ret);

end:
	close(fd1);
open2:
	close(fd2);
failed:
	return ret;
}


void copy(char * src, char * dst){
	FILE *in_fd = fopen(src,"rb"); 
	if(!in_fd) {
		perror("in_fd");
		return;
	}
	FILE *out_fd = fopen(dst,"wb"); 
	if(!out_fd) {
		perror("out_fd");
		return;
	}

	if(-1 == access(dst, W_OK)) {
		perror("access");
		return;
	}
	
	char buf[PAGESIZE];
	size_t n = 0;

	while (n != EOF) {
		n = fread(buf, 1, PAGESIZE, in_fd);
		if (n == 0)
			break;
		fwrite(buf, 1, n, out_fd);
	}
	fclose(in_fd);
	fclose(out_fd);
}

struct conf * loadconf(cJSON *json){
	if(!json){
		return NULL;
	}
	cJSON *jprograms= cJSON_GetObjectItem(json,PROGRAMS);
	int programcount = cJSON_GetArraySize(jprograms); 

	struct program * programs = (struct program *)malloc(sizeof(struct program)*programcount);
	memset(programs, 0, sizeof(struct program) * programcount); 

	cJSON * name;
	cJSON * version;
	cJSON * downloadname;
	cJSON * updatecmd;
	cJSON * runcmd;


	int i = 0;
	for(;i<programcount; i++){ 
		cJSON * subitem = cJSON_GetArrayItem(jprograms, i);
		name = cJSON_GetObjectItem(subitem, NAME);
		if(name && name->type == cJSON_String){
			programs[i].name = strdup(name->valuestring);
		}
		version = cJSON_GetObjectItem(subitem, VERSION);
		if(version && version->type == cJSON_String){
			programs[i].version = atoi(version->valuestring);
		}
		downloadname = cJSON_GetObjectItem(subitem, DOWNLOADNAME);
		if(downloadname && downloadname->type == cJSON_String){
			programs[i].downloadname = strdup(downloadname->valuestring);
		} 
		updatecmd  = cJSON_GetObjectItem(subitem, UPDATECMD);
		if(updatecmd && updatecmd->type == cJSON_String){
			programs[i].updatecmd = strdup(updatecmd->valuestring);
		}
		runcmd = cJSON_GetObjectItem(subitem, RUNCMD);
		if(runcmd && runcmd->type == cJSON_String){
			programs[i].runcmd = strdup(runcmd->valuestring);
		}
	}

	cJSON * jupgradecmd = cJSON_GetObjectItem(json, UPGRADECMD);
	struct conf * conf = (struct conf *) malloc(sizeof(struct conf));
	memset(conf, 0, sizeof(struct conf));
	if(jupgradecmd && jupgradecmd->type == cJSON_String){
		conf->upgradecmd = strdup(jupgradecmd->valuestring);
	}else{
		return NULL;
	}
	conf->programcount = programcount;
	conf->programs = programs;

	return conf;
}

int includeprogram(struct conf * conflocal, struct program * program){
	int i ;
	for(i = 0; i < conflocal->programcount; i++){
		if(strlen(conflocal->programs[i].name) == strlen(program->name) && 
		   0 == strcmp(conflocal->programs[i].name, program->name) &&
		   conflocal->programs[i].version == program->version){
			return 1;
		}
	}

	return 0;
}

struct list_head * getconf(struct conf * conflocal, struct conf * confurl){ 
	if(conflocal == NULL || confurl == NULL){
		return NULL;
	} 

	struct list_head * head = (struct list_head *)malloc(sizeof(struct list_head));
	INIT_LIST_HEAD(head);

	int i;
	for(i = 0; i < confurl->programcount; i++){
		if(!includeprogram(conflocal, &confurl->programs[i])){ 
			list_add_tail(&confurl->programs[i].list, head);
		}
	}

	return head;
}

void destroyprogram( struct program * program ){
	if(program){
		free(program->name);
		free(program->updatecmd);
		free(program->runcmd);
	}
}

void destroyconf(struct conf * conf){ 
	if(conf){
		int i = 0;
		for(; i < conf->programcount; i++){
			destroyprogram(&conf->programs[i]);
		}
		free(conf->upgradecmd);
		free(conf->programs);
	}
	free(conf);
}

int downloadprogram(struct list_head * head)
{
	struct program * program;
	char cmd[128];
	char md5_name[64];
	char md5_name_down[64];
	int ret = 1;

	struct list_head * pos, *n;
	list_for_each_safe(pos, n, head) { 
		program = list_entry(pos, struct program, list); 
		remove(program->downloadname);
		printf("====== updatecmd...======\n");
		if (0 == system(program->updatecmd)) {
			memset(cmd, 0, 128);
			memset(md5_name, 0, 64);
			memset(md5_name_down, 0, 64);
			snprintf(md5_name, 64, "%s.md5", program->name);
			printf("%s\n", md5_name);
			snprintf(md5_name_down, 64, "%s.md5.download", program->name);
			printf("%s\n", md5_name_down);
			snprintf(cmd, 128, "md5sum -b %s > %s", program->downloadname, md5_name);
			printf("%s\n", cmd);
			//system(cmd);
			if((0 == system(cmd)) && (0 == cmp_md5_files(md5_name, md5_name_down))) {
				printf("copy %s to %s...\n", program->downloadname, program->name);
				ret = 0;
				copy(program->downloadname, program->name); 
			
				printf("[downloadprogram] remove...\n");
				int rt = remove(program->downloadname);
				if(rt)
					fprintf(stdout, "%d %s\n", rt, strerror(errno));
				printf("[downloadprogram] program->runcmd..\n");
				ret = system(program->runcmd);
			}
			
		}
		else {
			printf("[downloadprogram] updaecmd is failed\n");
			return -1;
		}
		//system(program->runcmd);
	}
	return ret;
}

void runprograms(struct conf * urlconf){
	int i;
	for(i = 0; i < urlconf->programcount; i++){ 
		system(urlconf->programs[i].runcmd);
	}
}

