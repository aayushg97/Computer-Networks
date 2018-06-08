/* TODO List */
// try custom ip header
// max and min values at 100% packet loss
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#include <netdb.h> 
#include <time.h>
#include <errno.h>

typedef struct {
	struct timeval sendTime;
}payload;

struct in_addr getSourceIp(int sockfd){
	struct ifreq ifr;
	ifr.ifr_addr.sa_family = AF_INET;

	strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ-1);
	ioctl(sockfd, SIOCGIFADDR, &ifr);

	//printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

	return ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr ;
}

unsigned short csum (uint8_t *pBuffer, int nLen){
    unsigned short nWord;
    unsigned int nSum = 0;
    int i;

    for (i = 0; i < nLen; i = i + 2)
    {
        nWord = ((pBuffer [i] << 8) & 0xFF00) + (pBuffer [i + 1] & 0xFF);
        nSum = nSum + (unsigned int)nWord;
    }

    while (nSum >> 16)
    {
        nSum = (nSum & 0xFFFF) + (nSum >> 16);
    }

    nSum = ~nSum;

    return ((unsigned short) nSum);
}

void setIPHeader(struct iphdr *ip_header, char *dest, int sockfd, int seq_no){
	ip_header->ihl = 5;					
	ip_header->version = 4;
	ip_header->tos = 0;
	ip_header->tot_len = sizeof(struct iphdr) + sizeof(struct icmphdr) + sizeof(payload);
	ip_header->frag_off = 0;
	ip_header->id = htons(seq_no);
	ip_header->ttl = 61;
	ip_header->protocol = IPPROTO_ICMP;
	ip_header->saddr = inet_addr("10.145.163.128");
	ip_header->daddr = inet_addr(dest);
	ip_header->check = 0;
	ip_header->check = htons(csum((uint8_t *)ip_header, sizeof(struct iphdr)));
}

void setICMPHeader(struct icmphdr *icmp_header, int seq_no){
	icmp_header->type = 8;
	icmp_header->code = 0;
	icmp_header->un.echo.id = htons(seq_no);
	icmp_header->un.echo.sequence = htons(seq_no);
	icmp_header->checksum = 0;
}

void ping(int sockfd, char *dest, struct sockaddr_in hostaddr, int seq_no){
	struct iphdr *ip_header, *ip_reply;
	struct icmphdr *icmp_header, *icmp_reply;
	payload *message, *message_reply;
	struct timeval recvTime;
	char *snd_packet, *recv_packet, *temp;
	int hostlen, d, errno, pack_len;
	double rtt;

	pack_len = sizeof(struct iphdr) + sizeof(struct icmphdr) + sizeof(payload);
	snd_packet = malloc(pack_len);
	recv_packet = malloc(pack_len);
	memset(snd_packet, 0, pack_len);
	memset(recv_packet, 0, pack_len);

	ip_header = (struct iphdr *)snd_packet;
	icmp_header = (struct icmphdr *)(snd_packet + sizeof(struct iphdr));
	message = (payload *)(snd_packet + sizeof(struct iphdr) + sizeof(struct icmphdr));

	setIPHeader(ip_header, dest, sockfd, seq_no);
	setICMPHeader(icmp_header, seq_no);

	setsockopt(sockfd, IPPROTO_IP, IP_TTL, &(ip_header->ttl), sizeof(ip_header->ttl));

	gettimeofday(&(message->sendTime), NULL);
	
	icmp_header->checksum = htons(csum((uint8_t *)icmp_header, sizeof(struct icmphdr) + sizeof(payload)));

	sendto(sockfd, snd_packet, sizeof(struct iphdr) + sizeof(struct icmphdr) + sizeof(payload), 0, (struct sockaddr *)(&hostaddr), sizeof(struct sockaddr));

	d = recvfrom(sockfd, recv_packet, sizeof(struct iphdr) + sizeof(struct icmphdr) + sizeof(payload), 0, (struct sockaddr *)(&hostaddr), sizeof(struct sockaddr));
	if(d<0){
		printf("Timeout\n");
	}
}

int main(int argc, char **argv){
	struct sockaddr_in hostaddr;
	int sockfd, optval = 1, i;
	struct timeval start;
	char *temp;

	gettimeofday(&start, NULL);
	
    if(argc < 3){
		printf("usage : %s <destination address><number of pings>\n",argv[0]);
	}

	if((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP))==-1)
		printf("Failed to create socket %s\n",strerror(errno));

	//printf("socket %s\n",strerror(errno));

	setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(int));
	
	struct timeval timeout;      
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
	setsockopt (sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

	hostaddr.sin_family = AF_INET;
	hostaddr.sin_addr.s_addr = inet_addr(argv[1]);

	temp = inet_ntoa(*((struct in_addr *)&hostaddr.sin_addr.s_addr));

	printf("PING %s (%s) %ld(%ld) bytes of data.\n", argv[1], temp, sizeof(payload), sizeof(struct iphdr) + sizeof(struct icmphdr) + sizeof(payload));

	for(i=0;i<atoi(argv[2]);i++){
		ping(sockfd, argv[1], hostaddr, i);
	}

	close(sockfd);
	return 0;
}