#include "wrapper.h"

int main(int argc, char **argv){
	int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[PAYLOAD_SIZE];

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

    serverlen = sizeof(serveraddr);

    struct timeval tv;
	tv.tv_sec = IDLE_TIME;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    

    pthread_create(&t1a, NULL, thread1a, (void*)sockfd);
    pthread_create(&t2, NULL, thread2, (void*)sockfd);
    main_thread = pthread_self();

    // application starts here

    int fd, fsize, chunks;
    char fil[100], size[20], chunk[20];
    struct stat ast;

    printf("Enter the filename\n");
    scanf("%s",fil);

    fd = open(fil,O_RDONLY);

    stat(fil,&ast);
	fsize = ast.st_size; 

	chunks = (fsize%PAYLOAD_SIZE==0) ? fsize/PAYLOAD_SIZE : (fsize/PAYLOAD_SIZE)+1 ;

	sprintf(size,"%d",fsize);
	sprintf(chunk,"%d",chunks);

	appSend(fil, 100, (struct sockaddr*)(&serveraddr), serverlen);
	appSend(size, 20, (struct sockaddr*)(&serveraddr), serverlen);
	appSend(chunk, 20, (struct sockaddr*)(&serveraddr), serverlen);

	while(fsize>0){
		n = read(fd, buf, PAYLOAD_SIZE);
		fsize -= n;
		appSend(buf, n, (struct sockaddr*)(&serveraddr), serverlen);
	}

	close(fd);

	printf("main Thread exiting\n");
	pthread_exit(NULL);
}
