/* 
 * udpserver.c - A UDP echo server 
 * usage: udpserver <port_for_server>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

struct packet{
	int seq_no;
	int length;
	char chunk[BUFSIZE];
};

int main(int argc, char **argv) {
  int sockfd; /* socket file descriptor - an ID to uniquely identify a socket by the application program */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port_for_server>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {
  
  	//
  	char fil[100];					// fil stores name of the file
  	struct packet pack;				// structure to store a packet
  	int cseq=3,tfd,i,n1,j,fsize,psize,flag,abc=1,def=2,itr=0;
  									// fsize stores size of the file
									// psize stores the number of bytes to be received from client
  	
  	flag = 0;						
  	while(flag==0){
  		if(itr>10)	
  			printf("Client not responding\n");		// if no packet is received from client after several timeouts then exit
  			
  		n = recvfrom(sockfd,&pack,BUFSIZE+8,0,(struct sockaddr *) &clientaddr, &clientlen);
  													// receive a packet from client
  		if(pack.seq_no==1){
  			n1 = sendto(sockfd,&abc,sizeof(int),0,(struct sockaddr *) &clientaddr, clientlen);
  													// send acknowledgement to client
			strcpy(fil,pack.chunk);					// retrieve filename
			flag = 1;
  		}
  		itr++;
  	}
  	
  	itr = 0;
  	flag = 0;
  	while(flag==0){
  		if(itr>10)
  			printf("Client not responding\n");		// if no packet is received from client after several timeouts then exit
  			
  		n = recvfrom(sockfd,&pack,BUFSIZE+8,0,(struct sockaddr *) &clientaddr, &clientlen);
  													// receive a packet from client
  		if(pack.seq_no==2){
  			n1 = sendto(sockfd,&def,sizeof(int),0,(struct sockaddr *) &clientaddr, clientlen);
  													// send acknowledgement to client
  			fsize = atoi(pack.chunk);				// retrieve filesize	
			flag = 1;
  		}
  		itr++;
  	}
  	
  	printf("Size of file is %d bytes\n",fsize);
  	psize = fsize;
  	
  	tfd = open(fil,O_WRONLY|O_CREAT,0666);			// create a file with name same as filename sent by the client
  	
  	while(psize>0){
  		n = recvfrom(sockfd,&pack,BUFSIZE+8,0,(struct sockaddr *) &clientaddr, &clientlen);
  													// receive a packet from client
  		if(cseq == pack.seq_no){
  			n1 = sendto(sockfd,&cseq,sizeof(int),0,(struct sockaddr *) &clientaddr, clientlen);
  													// send acknowledgement to client
  			if(psize>=BUFSIZE){
  				n = write(tfd,pack.chunk,BUFSIZE);	//
  				//psize -= BUFSIZE;					//	
			}										// write data from client on the file
  			else{ 									//
  				n = write(tfd,pack.chunk,psize);	//
  				//psize = 0;						//
  			}	
  			 
  			psize -= n; 							// update remaining bytes to be read
  			cseq++;									// update current next sequence number
  		}
  		else{
  			n = sendto(sockfd,&(pack.seq_no),sizeof(int),0,(struct sockaddr *) &clientaddr, clientlen);
  													// send acknowledgement to client
  		}
  	}
  	
  	close(tfd);
  	
  	if(!fork()){											//
  		n = remove("checksum.txt");							//
    	n = open("checksum.txt",O_WRONLY|O_CREAT,0666);		//
    	close(1);											// generate checksum of the file
    	j = dup(n);											//
    	execlp("md5sum","md5sum","-t",fil,(char *)0);		//
    }
    wait(NULL);
    i = open("checksum.txt",O_RDONLY);
    bzero(buf,BUFSIZE);
    n = read(i,buf,BUFSIZE);
    close(i);
    
    n = sendto(sockfd,buf,BUFSIZE,0,(struct sockaddr *) &clientaddr, clientlen);	// send file's checksum to client
    
    //close(sockfd);
  	
  	//

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    /*bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    /*hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);
    
    /* 
     * sendto: echo the input back to the client 
     */
    /*n = sendto(sockfd, buf, strlen(buf), 0, 
	       (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");*/
  }
}
