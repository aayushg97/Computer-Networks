/* 
 * tcpclient.c - A simple TCP client
 * usage: tcpclient <host> <port>
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
#include <fcntl.h>

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n,i,j;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    char fil[100];
    int fsize,psize;
    char temp[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
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

    /* connect: create a connection with the server */
    if (connect(sockfd, &serveraddr, sizeof(serveraddr)) < 0) 
      error("ERROR connecting");

	/* Sending user selected file to the server*/
	struct stat ast;				// ast will store status of some file
	int tfd;
	printf("Enter file name: ");	// ask user for a filename
	bzero(fil,100);
    scanf("%s",fil);				// take filename as input from the user
    
	stat(fil,&ast);
	fsize = ast.st_size;    		// set size of the file
	
	psize = fsize;					// initialise number of bytes to be sent to the server
    
    n = write(sockfd,fil,100);		// send filename to the server 
    n = write(sockfd,&fsize,sizeof(fsize));		// send filesize to the server
    
    if(!fork()){
    	/*
    		Compute checksum of the file
    	*/
    	n = remove("checksum.txt");
    	n = open("checksum.txt",O_WRONLY|O_CREAT,0666);
    	close(1);
    	j = dup(n);
    	execlp("md5sum","md5sum","-t",fil,(char *)0);
    }
    
    wait(NULL);
    tfd = open(fil,O_RDONLY);		// open the file in read-only mode
    
    while(psize>0){
		bzero(buf,BUFSIZE);
		
		/*
    		if remaining number of bytes >= BUFSIZE then read BUFSIZE bytes from the file
    		and write the bytes on the socket.
    	*/
		if(psize>=BUFSIZE){
			n = read(tfd,buf,BUFSIZE);
			n = write(sockfd,buf,BUFSIZE);		
		}
		
		/*
    		if remaining number of bytes < BUFSIZE then read remaining bytes from the file
    		and write the bytes on the socket.
    	*/
		else{
			n = read(tfd,buf,psize);
			n = write(sockfd,buf,psize);
		}
		
		psize -= BUFSIZE;		// update the number of bytes to be sent to the server
	}
	
	close(tfd);
	
	bzero(temp,BUFSIZE);
	i = open("checksum.txt",O_RDONLY);
	n = read(i,temp,BUFSIZE);
	close(i);
	
	bzero(buf,BUFSIZE);
	n = read(sockfd,buf,BUFSIZE);		// Receive a checksum value from the server
	
	/*
		Compare checksum of the file and the checksum value received from the server
	*/
	if(strcmp(buf,temp)==0)
		printf("MD5 matched \n");		// If checksums match then print MD5 matched
	else 
		printf("MD5 not matched \n");	// If checksums don't match then print MD5 not matched
	close(sockfd);
	return 0;
}
