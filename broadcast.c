#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BROADCASTVREQUEST "DISCOVER_CEGATEWAY_REQUEST"
#define BROADCASTVRESPONSE "DISCOVER_CEGATEWAY_RESPONSE"
#define STRSIZE 13
#define PAGESIZE 32

static char *xlate_seq2str(char *str)
{
	int i;
	char *out_ptr = NULL;
	char *subp[3];
	unsigned long long seqnum_h, seqnum_m, seqnum_l;
	unsigned long long seqnum;
    char *seqstr = NULL;
	
    seqstr = calloc(STRSIZE, 1);
    if(NULL == seqstr) {
        printf("caloc error\n");
        return NULL;
    }
	for(i = 0; i < 3; str = NULL, i++) {
		subp[i] = strtok_r(str, ".", &out_ptr);
		if(NULL == subp[i])
			break;
		printf("--> %s\n", subp[i]);
	}
	if(0 == i) {
		printf("no match\n");
		return NULL;
	}
	
	seqnum_h = strtoul(subp[0] + 4, NULL, 10);
	seqnum_m = strtoul(subp[1], NULL, 10);
	seqnum_l = strtoul(subp[2], NULL, 10);
	
	seqnum = (seqnum_h << (5 * 8)) | (seqnum_m << (4 * 8)) | seqnum_l;
    
	printf("seqnum:%llx\n", seqnum);
    snprintf(seqstr, STRSIZE, "%llx", seqnum);
    printf("seqstr:%s\n", seqstr);
	
	return seqstr;
}

static char *load_seq2string(char *path)
{
	int fd, size;
	char *ret;
	char buf[PAGESIZE] = {0};
	
	fd = open(path, O_RDONLY);
	if(-1 == fd) {
		perror("load_sequence:open");
		return NULL;
	}
	
	size = read(fd, buf, PAGESIZE);
	if(-1 == size) {
		perror("read");
		return NULL;
	}
	else if(size == PAGESIZE) {
		buf[PAGESIZE - 1] = 0;
		printf("warning:read size maybe overflow\n");
	}
	
    ret = xlate_seq2str(buf);
	
	return ret;
}


int main()
{
    char *sbuf = NULL;
	daemon(1,1);
	setvbuf(stdout, NULL, _IONBF, 0); 
	fflush(stdout); 

	// 绑定地址
	struct sockaddr addrto;
	bzero(&addrto, sizeof(struct sockaddr));
	((struct sockaddr_in *)&addrto)->sin_family = AF_INET;
	((struct sockaddr_in *)&addrto)->sin_addr.s_addr = htonl(INADDR_ANY);
	((struct sockaddr_in *)&addrto)->sin_port = htons(8888);

	struct sockaddr from;
	bzero(&from, sizeof(struct sockaddr));
	//	((struct sockaddr_in *)&from)->sin_family = AF_INET;
	//	((struct sockaddr_in *)&from)->sin_addr.s_addr = htonl(INADDR_ANY);
	//	((struct sockaddr_in *)&from)->sin_port = htons(8888);

	int sock = -1;
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
	{   
		fprintf(stdout, "socket error\n");
		return -1;
	}   

	const int opt = 1;
	//设置该套接字为广播类型，
	int nb = 0;
	nb = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&opt, sizeof(opt));
	if(nb == -1)
	{
		fprintf(stdout, "set socket error...\n");
		return -1;
	}

	if(bind(sock,(struct sockaddr *)&(addrto), sizeof(struct sockaddr)) == -1) 
	{   
		fprintf(stdout, "bind error...\n");
		return -1;
	}

	int len = sizeof(struct sockaddr);

    sbuf = load_seq2string("/home/root/dstseq.txt");

	while(1)
	{
		//从广播地址接受消息
		char smsg[128] = {0};
		int ret=recvfrom(sock, smsg, 100, 0, (struct sockaddr*)&from,(socklen_t*)&len);
		if(ret<=0)
		{
			fprintf(stdout, "read error....\n");
			continue;
		}
		else
		{		
			char *recip = inet_ntoa(((struct sockaddr_in *)&from)->sin_addr);
			printf("from:%s>>>%s\n",recip, smsg);
			//if(0 == strcmp(BROADCASTVREQUEST, (const char *)smsg)){
			if(0 == strncmp(BROADCASTVREQUEST, (const char *)smsg, strlen(BROADCASTVREQUEST))){
				if (sendto(sock, sbuf, STRSIZE, 0,  
							(struct sockaddr *) &from, sizeof(struct sockaddr)) <= 0) {  
					fprintf(stdout, "sendto error!\n");  
					continue;
				}
				fprintf(stdout, "send ok\n");
			}
			//back res
		}
		fprintf(stdout, "hhh\n");
	}

	return 0;
}

