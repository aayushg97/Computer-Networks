#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFSIZE 255
#define TIMEOUT 20		// time-out value 20 seconds

/*structure to store peer details*/
typedef struct{
	char name[20];
	char ip[20];
	int port;
	int socket_fd;		// socket used to communicate with a particular peer
	long latest_commn;	// time of last communication with this client
}user;

// global variables
user *user_list;
int no_users;

void error(char *msg) {
    perror(msg);
    exit(0);
}

// returns index of queried user in the user_list 
int findUser(char *query, int query_type){
	int i;
	for(i=0;i<no_users;i++){
		if(query_type == 0){					// type 0 : query on username
			if(!strcmp(user_list[i].name,query))
				return i;
		}
		if(query_type == 1){					// type 1 : query on ip
			if(!strcmp(user_list[i].ip,query) /*&& port==htons(user_list[i].port)*/)
				return i;
		}
	}
	return -1;
}

// populates the userlist with user-details from file
void readUsers(){
	FILE *fp;
	int i;

	fp = fopen("peerlist.txt","r");
	fscanf(fp,"%d\n",&no_users);
	user_list = (user *)malloc(no_users*sizeof(user));

	for(i=0;i<no_users;i++){
		fscanf(fp,"%s\t%s\t%d\n",user_list[i].name,user_list[i].ip,&(user_list[i].port));
		user_list[i].socket_fd = -1;
	}

	fclose(fp);
}

// thread-function to implement client timeout 
void *timeout(void *param){
	int i;
	while(1){
		for(i=0;i<no_users;i++){
			if(user_list[i].socket_fd != -1){
				if((time(NULL)-user_list[i].latest_commn)>TIMEOUT){
					printf("%s timed-out. Closing connection ...\n", user_list[i].name);
					close(user_list[i].socket_fd);
					user_list[i].socket_fd = -1;
				}
			}
		}
	}
	pthread_exit(0);
}

int main(int argc, char **argv){
	int parentfd,clientfd,optval,portno,clientlen,nfd,i,index,len,b_read;
	char in[BUFSIZE],*ptr;
	fd_set readfds;
	pthread_t tid;
	
	// port to run on is taken as input
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
    	exit(1);
  	}
  	portno = atoi(argv[1]);

	struct sockaddr_in serveraddr,clientaddr;
	
	// parent socket which listens for incoming connections
	parentfd = socket(AF_INET, SOCK_STREAM, 0);
	nfd = parentfd;

	optval = 1;
  	setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR,(const void *)&optval , sizeof(int));
	
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)portno);

	if (bind(parentfd, (struct sockaddr *) &serveraddr,sizeof(serveraddr)) < 0) 
    		error("ERROR on binding");

  	if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */ 
	    	error("ERROR on listen");
  	printf("Chat Application Running ....\nmessage format: friendname/<msg>\n");

  	// add stdin and listening socket to read-file descriptors set
	FD_ZERO(&readfds);
	FD_SET(0,&readfds);
	FD_SET(parentfd,&readfds);

	clientlen = sizeof(clientaddr);
	readUsers();

	// start timeout thread
	pthread_create(&tid,NULL,timeout,NULL);

	while(1){

		select(nfd+1,&readfds,NULL,NULL,NULL);		//no timeout for select
		
		if(FD_ISSET(parentfd,&readfds)){			// listening socket changes when client tries to connect with it
			clientfd = accept(parentfd,(struct sockaddr *) &clientaddr, &clientlen);	// accept connection request
			index = findUser(inet_ntoa(clientaddr.sin_addr),1);				// find user based on client's ip
			if(index==-1){
				printf("Unknown user attempting to connect...denied\n");
				close(clientfd);
			}
			else{
				user_list[index].latest_commn = time(NULL);					// initialise commn. time
				user_list[index].socket_fd = clientfd;						// add client's file descriptor to user-list 
				nfd = clientfd;
				printf("%s established connection\n", user_list[index].name);
			}
		}
		else if(FD_ISSET(0,&readfds)){				// stdin fd changes when sending a message
			len=-1;
			do{
				read(0,&in[++len],1);				// read msg as in
			}while(in[len]!='\n');
			in[len+1]='\0';
			if(strcmp(in,"quit\n")==0){				// if user enters quit then exit
				printf("bye!\n");
				close(parentfd);
				exit(0);
			}
			ptr = strchr(in,'/');					// find username-message delimiter
			if(!ptr)
				printf("receiver not specified\n");
			else{
				*ptr='\0';
				index = findUser(in,0);	// find user based on username
				
				if(index==-1){
					printf("<%s> user not in peer list.\n",in);
				}
				else{
					/*create client socket if doesn't already exist*/
					if(user_list[index].socket_fd == -1){
    					user_list[index].latest_commn = time(NULL);			// update latest comm with client	
						user_list[index].socket_fd = socket(AF_INET, SOCK_STREAM, 0);
						clientaddr.sin_family = AF_INET;
						inet_aton(user_list[index].ip,&clientaddr.sin_addr);    			
						clientaddr.sin_port = htons((unsigned short)user_list[index].port);
						if(connect(user_list[index].socket_fd, &clientaddr, sizeof(clientaddr)) < 0) 
    						error("ERROR connecting");
    					nfd = user_list[index].socket_fd;
					}
					ptr++;
					len=-1;
							do{
								write(user_list[index].socket_fd,&ptr[++len],1);	// send msg to client
							}while(ptr[len]!='\0');
				}
			}
		}
		else{												// incoming msg
			for(i=0;i<no_users;i++){
				// find user who sent the msg
				if(FD_ISSET(user_list[i].socket_fd,&readfds)){	
					user_list[i].latest_commn = time(NULL);			// update latest comm with client
					len=-1;
					do{
						b_read = read(user_list[i].socket_fd,&in[++len],1);	 // read the msg
					}while(in[len]!='\0');
					if(b_read==0){									// 0 bytes read when writing fd closes
						printf("%s closed connection\n",user_list[i].name);
						user_list[i].socket_fd = -1;
					}
					else	
						printf("%s: %s",user_list[i].name,in);					// display sender and msg
					break;
				}
			}
			if(i>=no_users)
				printf("Error!\n");
		}

		// reset all read file descriptors
		FD_ZERO(&readfds);
		FD_SET(0,&readfds);
		FD_SET(parentfd,&readfds);
		for(i=0;i<no_users;i++){
				if(user_list[i].socket_fd != -1)
					FD_SET(user_list[i].socket_fd,&readfds);
		}		
	}
	pthread_join(tid,NULL);
	return 0;
}