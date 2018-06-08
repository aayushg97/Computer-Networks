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
#include <time.h>

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

/*
    Function to determine whether to drop a packet or not
*/
int drop_packet(float drop_prob){
    srand(time(NULL));
    int a = rand()%10;
    float b = ((float)a)/10.0;
    if(b<drop_prob)
        return 1;
    else
        return 0;
}

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
  float drop_prob;
  /* 
   * check command line arguments 
   */
  if (argc < 2) {
    fprintf(stderr, "usage: %s <port_for_server> <drop_probability>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  if (argc < 3) {
    fprintf(stderr, "usage: %s <port_for_server> <drop_probability>\n", argv[0]);
    exit(1);
  }
  drop_prob = atof(argv[2]);

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
  
  char fil[100];					// fil stores name of the file
  struct packet pack;				// structure to store a packet
  int cseq,tfd,i,n1,j,fsize,psize,chunks,pchunks,flag,itr,var;
									// fsize stores size of the file
									// psize stores the number of bytes to be received from client
  
  while (1) {
  	cseq = 1;

    /*
      Receive filename from client
    */
  	itr = 0;	
  	flag = 0;						
  	while(flag==0){
  		if(itr>10)
  			printf("Client not responding\n");		// if no packet is received from client after several timeouts then exit
  				
  		n = recvfrom(sockfd,&pack,BUFSIZE+8,0,(struct sockaddr *) &clientaddr, &clientlen);
  													// receive a packet from client
		
		
  		if(pack.seq_no==1 && drop_packet(drop_prob)==0){
  			n1 = sendto(sockfd,&cseq,sizeof(int),0,(struct sockaddr *) &clientaddr, clientlen);
  													// send acknowledgement to client
            printf("Received packet with sequence number 1\n");

			strcpy(fil,pack.chunk);					// retrieve filename
			flag = 1;
  		}
  		itr++;
  	}
    ////////////////////////////
    
    cseq++;
    
    /*
        Receive file size fom client
    */  	
  	itr = 0;
  	flag = 0;
  	while(flag==0){
  		if(itr>10)
  			printf("Client not responding\n");		// if no packet is received from client after several timeouts then exit
  			
  		n = recvfrom(sockfd,&pack,BUFSIZE+8,0,(struct sockaddr *) &clientaddr, &clientlen);
  													// receive a packet from client
  		if(pack.seq_no==2 && drop_packet(drop_prob)==0){
  			n1 = sendto(sockfd,&cseq,sizeof(int),0,(struct sockaddr *) &clientaddr, clientlen);
  													// send acknowledgement to client
            printf("Received packet with sequence number 2\n");

  			fsize = atoi(pack.chunk);				// retrieve filesize	
			flag = 1;
  		}
  		itr++;
  	}
  	/////////////////////////

    cseq++;
    
    /*
        Receive no of chunks from client
    */
  	itr = 0;
  	flag = 0;
  	while(flag==0){
  		if(itr>10)
  			printf("Client not responding\n");		// if no packet is received from client after several timeouts then exit
  			
  		n = recvfrom(sockfd,&pack,BUFSIZE+8,0,(struct sockaddr *) &clientaddr, &clientlen);
  													// receive a packet from client
  		if(pack.seq_no==3 && drop_packet(drop_prob)==0){
  			n1 = sendto(sockfd,&cseq,sizeof(int),0,(struct sockaddr *) &clientaddr, clientlen);
  													// send acknowledgement to client
            printf("Received packet with sequence number 3\n");

  			chunks = atoi(pack.chunk);				// retrieve filesize	
			pchunks = chunks;
			flag = 1;
  		}
  		itr++;
  	}
    /////////////////////

  	cseq++;
  	printf("Size of file is %d bytes\n",fsize);
  	printf("No of chunks is %d\n",chunks);
  	
  	tfd = open(fil,O_WRONLY|O_CREAT,0666);			// create a file with name same as filename sent by the client
  	
    /*
        Receive packets and send ack
    */
  	while(pchunks>0){
  		n = recvfrom(sockfd,&pack,BUFSIZE+8,0,(struct sockaddr *) &clientaddr, &clientlen);
  													         // receive a packet from client

        if(drop_packet(drop_prob)==0){
            printf("Received packet with sequence no %d, expected packet with sequence no %d ",pack.seq_no,cseq);
      		if(cseq == pack.seq_no){
      			n = sendto(sockfd,&cseq,sizeof(int),0,(struct sockaddr *) &clientaddr, clientlen);
      													         // send acknowledgement to client
                printf("Sending ack %d\n",cseq);
      			
      			n = write(tfd,pack.chunk,pack.length);	         // write data from client on the file	
      			
      				
    			pchunks--; 							         // update remaining bytes to be read
      			cseq++;									         // update current next sequence number
      		}
      		else{
                var = cseq-1;
      			n = sendto(sockfd,&var,sizeof(int),0,(struct sockaddr *) &clientaddr, clientlen);		
                                                                 // send ack of last acknowledged packet to client
                printf("Sending ack %d\n",var);
      		}
        }
  	}
  	//////////////////////////

  	close(tfd);
  
  	if(!fork()){											//
  		n = remove("checksum.txt");							//
    	n = open("checksum.txt",O_WRONLY|O_CREAT,0666);		//
    	close(1);											// generate checksum of the file
    	j = dup(n);		                                    //
    	execlp("md5sum","md5sum","-t",fil,(char *)0);		//
    }
    wait(NULL);		
    i = open("checksum.txt",O_RDONLY);	
    bzero(buf,BUFSIZE);		
    n = read(i,buf,BUFSIZE);	
    close(i);		
    
    n = sendto(sockfd,buf,BUFSIZE,0,(struct sockaddr *) &clientaddr, clientlen);	// send file's checksum to client
    
    //close(sockfd);
  	
  	printf("File received\n");
  	//
  }
}
