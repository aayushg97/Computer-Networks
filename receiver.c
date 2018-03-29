#include "wrapper.h"

int main(int argc, char **argv){
	int sockfd; /* socket file descriptor - an ID to uniquely identify a socket by the application program */
  	int portno; /* port to listen on */
	int clientlen; /* byte size of client's address */
  	struct sockaddr_in serveraddr; /* server's addr */
	struct sockaddr_in clientaddr; /* client addr */
	struct hostent *hostp; /* client host info */
	char buf[PAYLOAD_SIZE]; /* message buf */
	char *hostaddrp; /* dotted decimal host addr string */
	int optval; /* flag value for setsockopt */
	int n; /* message byte size */
	
	/* 
	* check command line arguments 
	*/
	if (argc < 2) {
	fprintf(stderr, "usage: %s <port_for_server>\n", argv[0]);
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

	/*struct timeval tv;
	tv.tv_sec = IDLE_TIME;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);*/

	/* 
	* main loop: wait for a datagram, then echo it
	*/
	clientlen = sizeof(clientaddr);
	int itr = 0;

	pthread_create(&t1b, NULL, thread1b, (void*)sockfd);
    pthread_create(&t2, NULL, thread2, (void*)sockfd);
    main_thread = pthread_self();
	// app starts here

	int tfd, fsize, chunks, bytes_written;
	char fil[50], size[20], chunk[20];

	//fsize = 7938673;//;
	appRecv(fil, 100, (struct sockaddr*)(&clientaddr), &clientlen);
	appRecv(size, 20, (struct sockaddr*)(&clientaddr), &clientlen);
	appRecv(chunk, 20, (struct sockaddr*)(&clientaddr), &clientlen);


	fsize = atoi(size);
	chunks = atoi(chunk);

	printf("filename = _%s_, size = _%d_, chunks = _%d_\n",fil,fsize,chunks);

	tfd = open(fil, O_WRONLY | O_CREAT, 0777);

	while(chunks>0){
		if(fsize>=PAYLOAD_SIZE){
			appRecv(buf, PAYLOAD_SIZE, (struct sockaddr*)(&clientaddr), &clientlen);
			fsize -= PAYLOAD_SIZE;
			bytes_written = PAYLOAD_SIZE;
		}
		else{
			appRecv(buf, fsize, (struct sockaddr*)(&clientaddr), &clientlen);
			bytes_written = fsize;
			fsize = 0;
		}

		chunks--;
		itr++;
		n = write(tfd, buf, bytes_written);
		printf("Writing %d bytes on file with itr = %d\n",n,itr);
	}

	close(tfd);
	printf("Main thread exiting\n");
	pthread_exit(NULL);
	
}
