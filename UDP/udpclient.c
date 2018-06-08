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
    int fsize,psize,tfd,i,j,ack,tsize,name_ack,size_ack,name_itr,size_itr,pack_itr,repack=0;
    											// fsize stores filesize
    struct stat ast;							// ast will store status of some file
    struct packet pack;							// structure to store a packet
    
	printf("Enter file name: ");
	bzero(pack.chunk,BUFSIZE);
    scanf("%s",fil);							// take filename as input from the user
    
	stat(fil,&ast);
	fsize = ast.st_size;    					// set size of the file
	printf("Size of file is %d bytes\n",fsize);
	psize = fsize;								// initialise number of bytes to be sent to the server
	
	name_ack = 0;
	size_ack = 0;
	name_itr = 0;
	size_itr = 0;
	pack_itr = 0;
	
	while(name_ack==0){
		if(name_itr>10){
			printf("Server not responding\n");	// if acknowledgement is not received even after many timeouts then exit
			return 0;
		}
		
		if(name_itr>0){
			printf("Retransmitted Packet with sequence number %d\n",1);
												// print packet retransmission info
			repack++;	
		}
		
		strcpy(pack.chunk,fil);
		pack.length = strlen(pack.chunk);
		pack.seq_no = 1;
		n = sendto(sockfd,&pack,BUFSIZE+8, 0, (struct sockaddr*)(&serveraddr), serverlen);
												// send filename to the server 
		name_ack = recvfrom(sockfd,buf,BUFSIZE,0, (struct sockaddr*)(&serveraddr), &serverlen);	
												// receive acknowledgement from server
		name_itr++;
	}
	
	while(size_ack==0){
		if(size_itr>10){
			printf("Server not responding\n");	// if acknowledgement is not received even after many timeouts then exit
			return 0;
		}
		
		if(size_itr>0){
			printf("Retransmitted Packet with sequence number %d\n",2);
												// print packet retransmission info
			repack++;	
		}
		
		sprintf(pack.chunk,"%d",fsize);
		pack.length = strlen(pack.chunk);
		pack.seq_no = 2;
		n = sendto(sockfd,&pack,BUFSIZE+8,0,(struct sockaddr*)(&serveraddr),serverlen);
												// send filesize to the server 
		size_ack = recvfrom(sockfd,buf,BUFSIZE,0, (struct sockaddr*)(&serveraddr), &serverlen);
												// receive acknowledgement from server
		size_itr++;
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
	
	n = 1;
    for(i=3;;i++){
		if(n>=0){
		
			/*
				if remaining number of bytes >= BUFSIZE then read BUFSIZE bytes from the file
				and write the bytes on the socket.
    		*/
			if(psize>=BUFSIZE)						//
				n = read(tfd,pack.chunk,BUFSIZE);	// read from the file
				
			/*
				if remaining number of bytes < BUFSIZE then read remaining bytes from the file
				and write the bytes on the socket.
			*/	
			else									//
				n = read(tfd,pack.chunk,psize);		//
				
			pack.length = strlen(pack.chunk);
			psize -= BUFSIZE;						// update remaining file size
			pack.seq_no = i;
		}
		
		n = sendto(sockfd, &pack,BUFSIZE+8, 0,(struct sockaddr*)(&serveraddr), serverlen);
													// send this packet to the server
		
		while(1){
			n = recvfrom(sockfd, &ack, sizeof(int), 0, (struct sockaddr*)(&serveraddr), &serverlen);
													// wait for server's acknowledgement
			if(ack==i || n<0){
				break;								
			}
		}
		
		if(n<0){
			if(pack_itr>10){
				printf("Server not responding\n");	// Server's acknowledgement is not received
				return 0;
			}
			
			if(pack_itr>0){
				printf("Retransmitted Packet with sequence number %d\n",i);
													// print retransmission info
				repack++;	
			}
			
			i--;
			pack_itr++;
		}
		else 
			pack_itr=0;
		
		if(psize<=0 && n>=0)
			break;
	}
	close(tfd);
	
	bzero(temp,BUFSIZE);
	i = open("checksum.txt",O_RDONLY);
	n = read(i,temp,BUFSIZE);						// read checksum from checksum.txt
	close(i);
	
	bzero(buf,BUFSIZE);
	n = recvfrom(sockfd,buf,BUFSIZE,0, (struct sockaddr*)(&serveraddr), &serverlen);
													// receive checksum from server
																
	
	printf("buf = %s %d %d and temp = %s %d %d\n",buf,(int)strlen(buf),(int)buf[40],temp,(int)strlen(temp),(int)temp[39]);
	
	if(strcmp(buf,temp)==0)
		printf("MD5 matched \n");
	else 
		printf("MD5 not matched \n");
		
	printf("%d packets were retransmitted\n",repack);
		
	close(sockfd);
    //
    
    /* get a message from the user */
    /*bzero(buf, BUFSIZE);
    printf("Please enter msg: ");
    fgets(buf, BUFSIZE, stdin);*/

    /* send the message to the server */
    /*serverlen = sizeof(serveraddr);
    n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");*/
    
    /* print the server's reply */
    /*n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);
    if (n < 0) 
      error("ERROR in recvfrom");
    printf("Echo from server: %s", buf);*/
    return 0;
}
