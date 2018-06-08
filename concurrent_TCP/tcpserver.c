/* 
 * tcpserver.c - A simple TCP echo server 
 * usage: tcpserver <port>
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

#if 0
/* 
 * Structs exported from in.h
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int parentfd; /* parent socket */
  int childfd; /* child socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buffer */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  int i,j,cid=0;

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  parentfd = socket(AF_INET, SOCK_STREAM, 0);
  if (parentfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));

  /* this is an Internet address */
  serveraddr.sin_family = AF_INET;

  /* let the system figure out our IP address */
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

  /* this is the port we will listen on */
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(parentfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * listen: make this socket ready to accept connection requests 
   */
  if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */ 
    error("ERROR on listen");
  printf("Server Running ....\n");
  /* 
   * main loop: wait for a connection request, echo input line, 
   * then close connection.
   */
  clientlen = sizeof(clientaddr);
  while (1) {
	/* 
	 * accept: wait for a connection request 
	 */
	childfd = accept(parentfd, (struct sockaddr *) &clientaddr, &clientlen);
	cid++;
	if (childfd < 0) 
	  error("ERROR on accept");
	
	/* 
	 * gethostbyaddr: determine who sent the message 
	 */
	/*
	hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	if (hostp == NULL)
	  error("ERROR on gethostbyaddr");
	hostaddrp = inet_ntoa(clientaddr.sin_addr);
	if (hostaddrp == NULL)
	  error("ERROR on inet_ntoa\n");
	printf("server established connection with %s (%s)\n", 
	   hostp->h_name, hostaddrp);
	 */
	 
	char fil[100],csum[20];			// fil stores name of the file
	int fsize,psize,tfd;	// fsize stores size of the file
							// psize stores the number of bytes to be received from client
	sprintf(csum,"csum%d.txt",cid);
		
	if(!fork()){
		
		close(parentfd);
		// Receiving file from client
							
		bzero(fil,100);			
		n = read(childfd,fil,100);					// read filename from the socket
		n = read(childfd,&fsize,sizeof(fsize));		// read filesize from the socket
		
		tfd = open(fil,O_WRONLY|O_CREAT,0666);		// create and open a file with name 'fil' in write only mode
	  	
		psize = fsize;								
		while(psize>0){
			bzero(buf,BUFSIZE);						
			
			/*
				if remaining number of bytes >= BUFSIZE then read BUFSIZE bytes from client
				and write the bytes on the newly created file.
			*/
			if(psize>=BUFSIZE){
				n = read(childfd,buf,BUFSIZE);
				n = write(tfd,buf,n);
			}
			
			/*
				if remaining number of bytes < BUFSIZE then read the remaining bytes only from client
				and write the bytes on the newly created file.
			*/
			else {
				n = read(childfd,buf,psize);
				n = write(tfd,buf,n);
			}
			
			psize -= n;		// update the number of bytes to be received from the client
		}
		
		close(tfd);			// close the file
		
		if(!fork()){
			/*
				Compute checksum of the transferred file
			*/
			n = remove(csum);
			n = open(csum,O_WRONLY|O_CREAT,0666);
			close(1);
			j = dup(n);
			execlp("md5sum","md5sum","-t",fil,(char *)0);
		}
		wait(NULL);
		i = open(csum,O_RDONLY);
		bzero(buf,BUFSIZE);
		n = read(i,buf,BUFSIZE);
		n = write(childfd,buf,BUFSIZE);			// send checksum of the transferred file to the client
		close(i);
		
		
		close(childfd);
		return 0;
	}
  }
}
