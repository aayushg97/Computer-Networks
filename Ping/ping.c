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

typedef struct {
	double min;
	double avg;
	double max;
	double mdev;
	int n;
}statistic;


void updateStats(statistic *stats, double rtt){
	double temp;

	if(stats->min > rtt)
		stats->min = rtt;
	if(stats->max < rtt)
		stats->max = rtt;

	temp = pow(stats->mdev,2) + stats->n * pow(stats->avg, 2);
	temp += pow(rtt, 2);

	stats->avg = (stats->avg * stats->n + rtt)/(stats->n+1);

	stats->mdev = sqrt(temp - ((stats->n+1) * pow(stats->avg, 2)));

	stats->n += 1;
}

void printStats(statistic *stats, int transmitted, struct timeval start){
	struct timeval end;
	gettimeofday(&end, NULL);
	double time = 1000*(end.tv_sec - start.tv_sec) + ((double)(end.tv_usec - start.tv_usec))/1000;

	double loss = (((double)(transmitted - stats->n))/((double)transmitted)) * 100;
	printf("\n%d packets transmitted, %d received, %0.3f%% packet loss, time %0.3f ms\n", transmitted, stats->n, loss, time);
	if(stats->n!=0)
		printf("rtt min/avg/max/mdev = %0.3f/%0.3f/%0.3f/%0.3f ms\n", stats->min, stats->avg, stats->max, stats->mdev);
}

/*struct in_addr getSourceIp(int sockfd){
	struct ifreq ifr;
	ifr.ifr_addr.sa_family = AF_INET;

	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
	ioctl(sockfd, SIOCGIFADDR, &ifr);

	//printf("%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

	return ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr ;
}*/

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
	ip_header->saddr = INADDR_ANY;//inet_addr(inet_ntoa(getSourceIp(sockfd)));
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

double ping(int sockfd, char *dest, struct sockaddr_in hostaddr, int seq_no){
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

	d = sendto(sockfd, snd_packet, pack_len, 0, (struct sockaddr *)(&hostaddr), sizeof(struct sockaddr));
	if(d<0){
		return -2.0;
	}
	//printf("sendto %s\n",strerror(errno));

	hostlen = sizeof(hostaddr);
	d = recvfrom(sockfd, recv_packet, pack_len, 0, (struct sockaddr *)(&hostaddr), &hostlen);

	//printf("recvfrom %s\n",strerror(errno));

	if(d<0){
		//printf("error in recvfrom \n");
		return -1.0;
	}
	else{
		//printf("Reply received\n");
		ip_reply = (struct iphdr *)recv_packet;
		icmp_reply = (struct icmphdr *)(recv_packet + sizeof(struct iphdr));
		message_reply = (payload *)(recv_packet + sizeof(struct iphdr) + sizeof(struct icmphdr));
        
        printf("%ld bytes received ",ntohs(ip_reply->tot_len) - sizeof(struct iphdr));
		
		temp = inet_ntoa(*((struct in_addr *)(&ip_reply->saddr)));
		printf("from %s ",temp);

		gettimeofday(&recvTime, NULL);
		rtt = 1000*(recvTime.tv_sec - message_reply->sendTime.tv_sec) + ((double)(recvTime.tv_usec - message_reply->sendTime.tv_usec))/1000;
		printf("icmp_sequence=%d ttl=%d time=%0.3fms\n",ntohs(icmp_reply->un.echo.sequence),ip_reply->ttl,rtt);
	}

	return rtt;
}

int main(int argc, char **argv){
	struct sockaddr_in hostaddr;
	int sockfd, optval = 1, i;
	statistic stats;
	stats.max = -1.0;
	stats.min = 10000.0;
	stats.avg = 0.0;
	stats.mdev = 0.0;
	stats.n = 0;
	double rtt;
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
		rtt = ping(sockfd, argv[1], hostaddr, i);
	
		if(rtt > 0.0){
			updateStats(&stats, rtt);
		}
		else{
			if(rtt < -1.5){
				printf("Host unreachable\n");
				return 0;
			}
		}
	}

	printStats(&stats, atoi(argv[2]), start);

	close(sockfd);
	return 0;
}