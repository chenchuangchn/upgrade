#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "cJSON.h"
#include "conf.h"
#include "list.h"

#define LOCALCONFPATH "./upgrade.json"
#define URLCONFPATH "./upgrade.json.download"

//#define LOCALCONFPATH "/home/root/upgrade.json"
//#define URLCONFPATH "/home/root/upgrade.json.download"

cJSON * loadconffile(char * path){ 
	FILE *f;
	long len;
	char *data;

	f=fopen(path,"rb");
	if(!f) {
		perror("loadconffile:");
		return NULL;
	}
	fseek(f,0,SEEK_END);
	len=ftell(f);
	fseek(f,0,SEEK_SET);
	data=(char*)malloc(len+1);
	fread(data,1,len,f);
	data[len]='\0';
	fclose(f); 
	
	cJSON *json;
	json=cJSON_Parse(data);
	free(data); 
	return json;
}

void execmajor(void){

}

int main(){
	int ret;
	if(atexit(execmajor)){
		fprintf(stdout, "register at exit fail\n");
	}
	
	//stage 1. read the conf file and get the remote conf file
	//stage 2. commpare the two conf files
	//stage 3. if is equal then exit if not equal excute the new conf
	if(access(URLCONFPATH, F_OK) != -1){
		remove(URLCONFPATH);
	}
	cJSON * localjson = loadconffile(LOCALCONFPATH);
	if(!localjson) {
		printf("localjson is NULL\n");
		return -1;
	}
	struct conf * localconf = loadconf(localjson);
	int rt  = system(localconf->upgradecmd);
	int rt2 = system("md5sum upgrade.json.download > upgrade.json.md5");
	int rt3 = cmp_md5_files("./upgrade.json.md5.download", "./upgrade.json.md5");
	printf("rt = %d\n", rt);
	printf("rt2 = %d\n", rt2);
	printf("rt3 = %d\n", rt3);
	if((rt == 0) && (rt2 == 0) && (rt3 == 0)) {
		cJSON * urljson = loadconffile(URLCONFPATH);
		if(!urljson) {
			printf("urljson is NULL\n");
			return -1;
		}
		printf("loadconf...\n");
		struct conf * urlconf = loadconf(urljson);
		cJSON_Delete(localjson);
		cJSON_Delete(urljson);
		printf("getconf...\n");
		struct list_head * head = getconf(localconf, urlconf); 
		if(head && !list_empty(head)) {
			printf("downloadprogram..\n");
			ret = downloadprogram(head);
		}
		free(head);
		if(0 == ret)
			copy(URLCONFPATH, LOCALCONFPATH);
		remove(URLCONFPATH); 

		//runprograms(urlconf);
		destroyconf(urlconf);
	}else { 
		runprograms(localconf);
	}
	if(0 == rt)
		system("rm ./upgrade.json.md5.download");
	destroyconf(localconf);
	return 0;
}
