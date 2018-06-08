/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <fcntl.h>

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

struct packet{
	int seq_no;
	int length;
	char chunk[BUFSIZE];
};

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    // Sending user selected file to the server
    serverlen = sizeof(serveraddr);
    
    struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    
    char fil[100],temp[BUFSIZE];				// fil stores filename
    int fsize,psize,pchunks,chunks,baseptr,currptr,tfd,i,j,ack,itr,repack=0;
    											// fsize stores filesize
    int timeout,x,window=3;
    struct stat ast;							// ast will store status of some file
    struct packet pack,pack_arr[3];				// structure to store a packet
    
	printf("Enter file name: ");
	bzero(pack.chunk,BUFSIZE);
    scanf("%s",fil);							// take filename as input from the user
    
	stat(fil,&ast);
	fsize = ast.st_size;    					// set size of the file
	printf("Size of file is %d bytes\n",fsize);
	psize = fsize;								// initialise number of bytes to be sent to the server
	chunks = (fsize%BUFSIZE) ? (fsize/BUFSIZE)+1 : (fsize/BUFSIZE);
	pchunks = chunks;
	
	ack = 0;
	itr = 0;	
	while(ack<=0){
		if(itr>10){
			printf("Server not responding\n");	// if acknowledgement is not received even after many timeouts then exit
			return 0;
		}
		
		if(itr>0){
			printf("Retransmitted Packet with sequence number %d\n",1);
												// print packet retransmission info
			repack++;	
		}
		
		strcpy(pack.chunk,fil);
		pack.length = strlen(pack.chunk);
		pack.seq_no = 1;
		n = sendto(sockfd,&pack,BUFSIZE+8, 0, (struct sockaddr*)(&serveraddr), serverlen);
												// send filename to the server 
		ack = recvfrom(sockfd,buf,BUFSIZE,0, (struct sockaddr*)(&serveraddr), &serverlen);	
												// receive acknowledgement from server
		itr++;
	}
	
	ack = 0;
	itr = 0;	
	while(ack<=0){
		if(itr>10){
			printf("Server not responding\n");	// if acknowledgement is not received even after many timeouts then exit
			return 0;
		}
		
		if(itr>0){
			printf("Retransmitted Packet with sequence number %d\n",2);
												// print packet retransmission info
			repack++;	
		}
		
		sprintf(pack.chunk,"%d",fsize);
		pack.length = strlen(pack.chunk);
		pack.seq_no = 2;
		n = sendto(sockfd,&pack,BUFSIZE+8,0,(struct sockaddr*)(&serveraddr),serverlen);
												// send filesize to the server 
		ack = recvfrom(sockfd,buf,BUFSIZE,0, (struct sockaddr*)(&serveraddr), &serverlen);
												// receive acknowledgement from server
		itr++;
	}

	ack = 0;
	itr = 0;
	while(ack<=0){
		if(itr>10){
			printf("Server not responding\n");	// if acknowledgement is not received even after many timeouts then exit
			return 0;
		}
		
		if(itr>0){
			printf("Retransmitted Packet with sequence number %d\n",3);
												// print packet retransmission info
			repack++;	
		}
		
		sprintf(pack.chunk,"%d",chunks);
		pack.length = strlen(pack.chunk);
		pack.seq_no = 3;
		n = sendto(sockfd,&pack,BUFSIZE+8,0,(struct sockaddr*)(&serveraddr),serverlen);
												// send number of chunks to the server 
		ack = recvfrom(sockfd,buf,BUFSIZE,0, (struct sockaddr*)(&serveraddr), &serverlen);
												// receive acknowledgement from server
		itr++;
	}
	
	if(!fork()){
		n = remove("checksum.txt");							//
    	n = open("checksum.txt",O_WRONLY|O_CREAT,0666);		//
    	close(1);											// generate checksum of the file
    	j = dup(n);											//	
    	execlp("md5sum","md5sum","-t",fil,(char *)0);		// 
    }
    
    wait(NULL);
	
	tfd = open(fil,O_RDONLY);

	bzero(pack.chunk,BUFSIZE);
	
	baseptr = 3;
	currptr = 3;
	timeout = 0;
	ack = -1;
	itr = 0;
	while(1){
		while(baseptr-currptr<window && pchunks>0){
			n = read(tfd,pack_arr[baseptr-currptr].chunk,BUFSIZE);
			pack_arr[baseptr-currptr].length = n;
			pack_arr[baseptr-currptr].seq_no = baseptr+1;
			
			n = sendto(sockfd, &(pack_arr[baseptr-currptr]),BUFSIZE+8, 0,(struct sockaddr*)(&serveraddr), serverlen);
			baseptr++;
			pchunks--;
		}
		
		if(timeout==1){
			for(i=0;i<window;i++){
				n = sendto(sockfd, &(pack_arr[i]),BUFSIZE+8, 0,(struct sockaddr*)(&serveraddr), serverlen);
			}
			timeout = 0;
		}
		
		while(1){
			n = recvfrom(sockfd, &ack, sizeof(int), 0, (struct sockaddr*)(&serveraddr), &serverlen);
													// wait for server's acknowledgement
			
			if(n>=0){
				if(ack>currptr && ack<=baseptr){
					/*if(ack==currptr+1){
						currptr++;
						strcpy(pack_arr[0].chunk,pack_arr[1].chunk);
						pack_arr[0].length = pack_arr[1].length;
						pack_arr[0].seq_no = pack_arr[1].seq_no;
						
						strcpy(pack_arr[1].chunk,pack_arr[2].chunk);
						pack_arr[1].length = pack_arr[2].length;
						pack_arr[1].seq_no = pack_arr[2].seq_no;
					}	
					else{
						if(ack==currptr+2){
							currptr += 2;
							strcpy(pack_arr[0].chunk,pack_arr[2].chunk);
							pack_arr[0].length = pack_arr[2].length;
							pack_arr[0].seq_no = pack_arr[2].seq_no;
						}
						else{
							currptr += 3;
						}
					}*/
					
					//
					for(x=ack+1;x<=baseptr;x++){
						strcpy(pack_arr[x-ack-1].chunk,pack_arr[x-currptr-1].chunk);
						pack_arr[x-ack-1].length = pack_arr[x-currptr-1].length; 
						pack_arr[x-ack-1].seq_no = pack_arr[x-currptr-1].seq_no; 
					}
					currptr = ack;
					//
					
					break;
				}	
												
			}
			else{
				timeout = 1;
				break;
			}
		}
		
		if(timeout==1){
			if(itr>10){
				printf("Server not responding\n");	// Server's acknowledgement is not received
				return 0;
			}
			
		
			printf("Retransmitted Packet with sequence numbers %d to %d\n",pack_arr[0].seq_no,pack_arr[0].seq_no+2);
												// print retransmission info
			repack++;	
			
			itr++;
		}
		else{
			itr = 0;
		}
		
		if(pchunks<=0 && currptr==baseptr && timeout==0){
			break;
		}
	}
	
    close(tfd);
	
	bzero(temp,BUFSIZE);
	i = open("checksum.txt",O_RDONLY);
	n = read(i,temp,BUFSIZE);						// read checksum from checksum.txt
	close(i);
	
	bzero(buf,BUFSIZE);
	
	n = recvfrom(sockfd,buf,BUFSIZE,0, (struct sockaddr*)(&serveraddr), &serverlen);
													// receive checksum from server
																
	
	if(strcmp(buf,temp)==0)
		printf("MD5 matched \n");
	else 
		printf("MD5 not matched \n");
		
	printf("%d packets were retransmitted\n",repack);
		
	close(sockfd);
   
    return 0;
}
