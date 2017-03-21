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
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/ioctl.h>

/*
** Communication Format
** 
** remote --> gateway: "request:yymmssssssss:port;"
** ** if it's an auto-bed, the port is a non-zeore value, 
** ** otherwise, the value of port is 0.
** 
** gateway --> remote: "respose:phone/bed:ip;" 
*/


#define BROADCASTVREQUEST "DISCOVER_CEGATEWAY_REQUEST"
#define BROADCASTVRESPONSE "DISCOVER_CEGATEWAY_RESPONSE"
#define STRSIZE 13
#define RECVSIZE 64
#define SENDSIZE 64
#define REQ_TYPE "request"
#define RSP_TYPE "response"
#define MAC_SIZE	18	
#define IP_SIZE 	16
#define	SEQ_PATH	"/home/root/dstseq.txt"
//#define	SEQ_PATH	"./dstseq.txt"



struct remote_request {
	char *seq_str;
	unsigned short port;
};


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
	char *retstr, *buf = NULL;
	
	fd = open(path, O_RDONLY);
	if(-1 == fd) {
		perror("load_seq2string:open");
		return NULL;
	}

	buf = calloc(RECVSIZE, 1);
	if(NULL == buf) {
		printf("calloc is fail\n");
		return NULL;
	}
	
	size = read(fd, buf, RECVSIZE);
	if(-1 == size) {
		perror("read");
		return NULL;
	}
	else if(size == RECVSIZE) {
		buf[RECVSIZE - 1] = 0;
		printf("warning:read size maybe overflow\n");
	}
	retstr = xlate_seq2str(buf);	
	
	return retstr;
}

int find_next_request(char *msg, char **buf)
{
	char *start_substr = "request", *end_substr = ";";
	char *start, *end;
	int len;
	
	start = strstr(msg, start_substr);
	end = strstr(msg, end_substr);
	if(!start || !end) {
		printf("uncomplete message\n");
		return 0;
	}
	len = end - start + 1;
	if(RECVSIZE - 1 < len) {
		printf("sub-message is too long\n");
		return 0;
	}
	
	strncpy(*buf, start, len);
	
	return len;
}

int parse_one_message(char *msg, struct remote_request *req)
{
	int i;
	char *out_ptr = NULL;
	char *subp[3];

	for(i = 0; i < 3; msg = NULL, i++) {
		subp[i] = strtok_r(msg, ":", &out_ptr);
		if(NULL == subp[i])
			break;
		printf("--> %s\n", subp[i]);
	}
	if(0 == i) {
		printf("[parse_recv_message]no match\n");
		return -1;
	}

	if(0 == strncmp(subp[0], REQ_TYPE, strlen(REQ_TYPE))) {
		req->seq_str = subp[1];
		req->port = atoi(subp[2]);
		printf("req->seq_str:%s\n", req->seq_str);
		printf("req->port:%d\n", req->port);
		return 0;
	}
	else {
		printf("[parse_recv_message] type isn't match\n");
		return 1;
	}
}

int get_local_ip(const char *eth_inf, char *ip)  
{  
    int sd;  
    struct sockaddr_in sin;  
    struct ifreq ifr;  
  
    sd = socket(AF_INET, SOCK_DGRAM, 0);  
    if (-1 == sd)  
    {  
        printf("socket error: %s\n", strerror(errno));  
        return -1;        
    }  
  
    strncpy(ifr.ifr_name, eth_inf, IFNAMSIZ);  
    ifr.ifr_name[IFNAMSIZ - 1] = 0;  
      
    // if error: No such device  
    if (ioctl(sd, SIOCGIFADDR, &ifr) < 0)  
    {  
        printf("ioctl error: %s\n", strerror(errno));  
        close(sd);  
        return -1;  
    }  
  
    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));  
    snprintf(ip, IP_SIZE, "%s", inet_ntoa(sin.sin_addr));  
  
    close(sd);  
    return 0;  
}  

int encode_send_message(char *buf, unsigned short port, char *itf)
{
	char ip[IP_SIZE];
	
	printf("encode_send_message...\n");
	strcat(buf, RSP_TYPE);
	strcat(buf, ":");
	if(0 == port) 
		strcat(buf, "phone");
	else
		strcat(buf, "bed");
	strcat(buf, ":");
	if(-1 == get_local_ip(itf, ip)) {
		printf("[encode_send_message] get_local_ip error\n");
		return -1;
	}
	printf("ip:%s\n", ip);
	strcat(buf, ip);
	strcat(buf, ";");

	return strlen(buf);
}

int send2remote(int sockfd, const void *buf, size_t len, 
		unsigned short port, struct sockaddr_in *dest_addr)
{
	int ret;
	
	if(0 == port) {
		ret = sendto(sockfd, buf, len, 0, (struct sockaddr *)dest_addr, sizeof(struct sockaddr));
	}
	else {
		dest_addr->sin_addr.s_addr = htonl(INADDR_BROADCAST);			
		dest_addr->sin_port = htons(port);
		ret = sendto(sockfd, buf, len, 0, (struct sockaddr *)dest_addr, sizeof(struct sockaddr));
	}
	return ret;
}

int main(int argc, char **argv)
{
    char *localstr = NULL;
	int rlen;
	
	daemon(1,1);
	setvbuf(stdout, NULL, _IONBF, 0); 
	fflush(stdout); 

	// 缂佹垵鐣鹃崷鏉挎絻
	struct sockaddr addrto;
	bzero(&addrto, sizeof(struct sockaddr));
	((struct sockaddr_in *)&addrto)->sin_family = AF_INET;
	((struct sockaddr_in *)&addrto)->sin_addr.s_addr = htonl(INADDR_ANY);
	((struct sockaddr_in *)&addrto)->sin_port = htons(8888);

	struct sockaddr_in from;
	bzero(&from, sizeof(struct sockaddr));

	int sock = -1;
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
	{   
		fprintf(stdout, "socket error\n");
		return -1;
	}   

	const int opt = 1;
	//鐠佸墽鐤嗙拠銉ヮ殰閹恒儱鐡ф稉鍝勭畭閹绢厾琚崹瀣剁礉
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

    localstr = load_seq2string(SEQ_PATH);
	if(NULL == localstr)
		return -1;

	printf("localseq:%s\n", localstr);

	while(1)
	{
		//娴犲骸绠嶉幘顓炴勾閸р偓閹恒儱褰堝☉鍫熶紖
		char message[128] = {0};
		rlen = recvfrom(sock, message, 100, 0, (struct sockaddr*)&from,(socklen_t*)&len);
		if(rlen <= 0)
		{
			fprintf(stdout, "read error....(%d)\n", rlen);
			perror("recvfrom");
			continue;
		}
		else
		{	
			char *recip = inet_ntoa(((struct sockaddr_in *)&from)->sin_addr);
			printf("from:%s>>>%s, rlen:%d\n",recip, message, rlen);
			
			char cur_str[RECVSIZE] = {0};
			char *pcur = cur_str;
			struct remote_request remote_req = {0};
			int len = 0, ret;
			
			while(rlen - len > 0) {
				if(!(message + len)) {
					printf("msg is NULL\n");
					len++;
					continue;
				}
				ret = find_next_request(message + len, &pcur);
				if(ret > 0)
					len += ret;
				else 
					break;
				
				//printf("message:%s\n", message + len);
				printf("cur_str:%s\n", cur_str);
				ret = parse_one_message(cur_str, &remote_req);
				printf("remote_seq.seq_str:%s\n", remote_req.seq_str);
				
				if((0 == ret) && 
					(0 == strncmp(localstr, (const char *)(remote_req.seq_str), strlen(localstr)))) {
						char sbuf[SENDSIZE] = {0};
						int slen = encode_send_message(sbuf, remote_req.port, argv[1]);
						if (send2remote(sock, sbuf, slen, remote_req.port, &from) <= 0) {  
							perror("send2remote");
							continue;
						}
						fprintf(stdout, "send ok\n");
				}
			}	
		}
		fprintf(stdout, "hhh\n");
	}

	return 0;
}

