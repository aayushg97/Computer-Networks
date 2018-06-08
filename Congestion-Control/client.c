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

/*
	structure to store a packet
*/
struct packet{					
	int seq_no;
	int length;
	char chunk[BUFSIZE];
	struct packet *next;
};

/*
	structure to represent baseptr and currptr to a packet 
*/
struct point{
	int index;
	struct packet *ptr;
};

int min(int a,int b){
	if(a<b)
		return a;
	else
		return b;
}

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
    
    /*
		Set timeout 
    */
    struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    //////////////////

    char fil[100],temp1[BUFSIZE];								// fil stores filename
    int fsize,pchunks,chunks,tfd,i,j,ack,itr,repack=0;			// fsize stores filesize
    int timeout,x,window,var,flag;
    struct stat ast;											// ast will store status of some file
    struct packet pack;											// structure to store a packet
    struct point baseptr,currptr;								// pointers to last sent packet and last acknowledged packet
    struct packet *pack_arr;									// list of packets
    struct packet *temp, *last;
    
	printf("Enter file name: ");
	bzero(pack.chunk,BUFSIZE);
    scanf("%s",fil);							// take filename as input from the user
    
	stat(fil,&ast);
	fsize = ast.st_size;    					// set size of the file
	printf("Size of file is %d bytes\n",fsize);							// initialise number of bytes to be sent to the server
	chunks = (fsize%BUFSIZE) ? (fsize/BUFSIZE)+1 : (fsize/BUFSIZE);		// compute no of chunks
	pchunks = chunks;
	printf("Number of chunks is %d\n",pchunks);

	/*
		Constructing the buffer
	*/
	pack_arr = (struct packet *)malloc(sizeof(struct packet));			// first packet; contains filename		
	pack_arr->next = NULL;
	pack_arr->seq_no = 1;
	strcpy(pack_arr->chunk,fil);
	pack_arr->length = strlen(pack_arr->chunk);

	temp = (struct packet *)malloc(sizeof(struct packet));				// second packet; conatins filesize
	temp->next = NULL;
	temp->seq_no = 2;
	sprintf(temp->chunk,"%d",fsize);
	temp->length = strlen(temp->chunk);
	pack_arr->next = temp;

	temp = (struct packet *)malloc(sizeof(struct packet));				// third packet; contains no of chunks
	temp->next = NULL;
	temp->seq_no = 3;
	sprintf(temp->chunk,"%d",chunks);
	temp->length = strlen(temp->chunk);
	pack_arr->next->next = temp;

	last = pack_arr->next->next;
	tfd = open(fil,O_RDONLY);
	while(pchunks--){													// Populate the buffer with file contents
		temp = (struct packet *)malloc(sizeof(struct packet));
		temp->next = NULL;
		temp->seq_no = last->seq_no+1;
		n = read(tfd,temp->chunk,BUFSIZE);
		temp->length = n;
		last->next = temp;
		last = last->next;
	}

	close(tfd);
	pchunks = chunks;

	temp = (struct packet *)malloc(sizeof(struct packet));
	temp->next = pack_arr;
	pack_arr = temp;
	
	
	if(!fork()){
		n = remove("checksum.txt");							//
    	n = open("checksum.txt",O_WRONLY|O_CREAT,0666);		//
    	close(1);											// generate checksum of the file
    	j = dup(n);											//	
    	execlp("md5sum","md5sum","-t",fil,(char *)0);		// 
    }
    
    wait(NULL);

	bzero(pack.chunk,BUFSIZE);
	
	window = 3;					// initial window size = 3
	baseptr.index = 0;			// baseptr points to the latest packet sent by client
	baseptr.ptr = pack_arr;
	currptr.index = 0;			// currptr points to the last packet acknowledged 
	currptr.ptr = pack_arr;
	timeout = 0;
	ack = 0;
	itr = 0;
	flag = 0;

	while(1){
		/*
			Limiting no of outstanding packets to window size
		*/
		while((baseptr.index-currptr.index)<window && (baseptr.ptr)->next!=NULL){
			baseptr.ptr = (baseptr.ptr)->next;
			baseptr.index = baseptr.index + 1;

			n = sendto(sockfd, baseptr.ptr, sizeof(struct packet), 0,(struct sockaddr*)(&serveraddr), serverlen);
			flag = 1;	
		}

		if(flag){
			flag = 0;
			printf("\nTransmitted packets with sequence numbers %d to %d with window size %d\n",(currptr.ptr)->next->seq_no,min(baseptr.index,last->seq_no),window);
		}
		/////////////////////////////////////
		
		timeout = 0;

		/*
			Ack implementation
		*/
		while(1){
			n = recvfrom(sockfd, &ack, sizeof(int), 0, (struct sockaddr*)(&serveraddr), &serverlen);
													// wait for server's acknowledgement
			
			if(n>0){
				/* Something is received */
				if(ack>currptr.index && ack<=baseptr.index){
					/* Expected ack is received */
					
					for(x=currptr.index;x<ack;x++){
						//temp = currptr.ptr;
						currptr.ptr = (currptr.ptr)->next;
						currptr.index = currptr.index + 1;
						//free(temp);
						window++;
					}
					
					break;
				}							
			}
			else{
				/* timeout occurs */
				timeout = 1;
				window /= 2; 

				baseptr.ptr = currptr.ptr;
				baseptr.index = currptr.index;
				break;
			}
		}
		//////////////////////////////
		
		/* In case of timeout print retransmission info */
		if(timeout==1){
			if(itr>10){
				printf("Server not responding\n");	// Server's acknowledgement is not received
				return 0;
			}
			
			printf("\nTimeout occured.....\n");
			printf("Retransmitted Packet with sequence numbers %d to %d with window size %d\n",(currptr.ptr)->next->seq_no,min(currptr.index + window,last->seq_no),window);
												// print retransmission info
			repack+=min(window,last->seq_no - currptr.index);		// counts total number of retransmissions	
			
			itr++;
		}
		else{
			itr = 0;
		}
		////////////////////////////////
		
		/* exit loop if all packets have been sent and acknowledged successfully */
		if((baseptr.ptr)->next==NULL && currptr.index==baseptr.index && timeout==0){
			break;
		}
	}
	
	bzero(temp1,BUFSIZE);
	i = open("checksum.txt",O_RDONLY);
	n = read(i,temp1,BUFSIZE);						// read checksum from checksum.txt
	close(i);
	
	bzero(buf,BUFSIZE);
	
	n = recvfrom(sockfd,buf,BUFSIZE,0, (struct sockaddr*)(&serveraddr), &serverlen);
													// receive checksum from server
																
	
	if(strcmp(buf,temp1)==0)
		printf("MD5 matched \n");
	else 
		printf("MD5 not matched \n");
		
	printf("%d packets were retransmitted\n",repack);
		
	close(sockfd);
   
    return 0;
}
