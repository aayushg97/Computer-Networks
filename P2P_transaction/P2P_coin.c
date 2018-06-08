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

#define MAX_TRANS 1000
#define NODE_ID 1
#define MAX_PEER 10

/*
	structure to store a transaction message
*/
typedef struct {
	struct timeval tstamp;
	int origin_id;
	int send_id;
	int recv_id;
	int amount;
}transaction; 

/*
	structure to store a vote message
*/
typedef struct {
	int node_id;
	char type[10];
	struct timeval tstamp;
	int send_id;
	int recv_id;
	int amount;
	int origin_id;
	char vote;
}vote_msg;

/*
	structure to store peer info
*/
typedef struct {
	int id;
	char ip[20];
	int port;
}peer;

transaction *ledger;		// public ledger for this process
int no_trans = 0;			// no of transactions in this ledger
peer *peer_list;			// list of peers of this process
int no_peers = 0;			// no of peers on peer list

void sys_init(){
	ledger = (transaction *)malloc(MAX_TRANS * sizeof(transaction));		// initialising ledger

	FILE *fp;
	fp = fopen("topo.conf","r");
	int node;

	peer_list = (peer *)malloc(MAX_PEER * sizeof(peer))

	while(fscanf(fp,"%d",node)){
		if(node==NODE_ID){
			//fscanf(fp, "%s")
		}
	}

	fclose(fp);
}

void initiate_transaction(int sockfd, int recv_id, int amount){
	transaction trans_msg;
	gettimeofday(&trans_msg.tstamp, NULL);
	trans_msg.origin_id = NODE_ID;
	trans_msg.send_id = NODE_ID;				// constructing transaction message
	trans_msg.recv_id = recv_id;
	trans_msg.amount = amount;

	broadcast_transaction(sockfd, &trans_msg, -1); 			// broadcasting transaction message without leaving any peer
}

void receive_transaction(int sockfd){			
	transaction recv_trans;
	struct sockaddr_in clientaddr;
	int clientlen = 0, valid, vote_count;
	time_t con_timer;

	while(1){
		/* receive transaction from peers */
		recvfrom(sockfd, &recv_trans, sizeof(transaction), 0, (struct sockaddr *)(&clientaddr), &clientlen);

		vote_count = 0;
		con_timer = time(NULL);
		while(time(NULL)-con_timer < 100){
			broadcast_transaction(sockfd, &recv_trans, recv_trans.origin_id);
			vote_transaction(&recv_trans);
			//receive_vote(sockfd, vote_count);
		}

		reach_consensus(*vote_count, recv_trans);
	}
}

void broadcast_transaction(int sockfd, transaction *trans_msg, int leave_peer){
	int i;
	struct sockaddr_in peeraddr;
	int peerlen;

	/* Send broadcast to every peer except for the one who sent the transaction (leave_peer) */
	for(i=0;i<no_peers;i++){
		if(peer_list[i].id!=leave_peer){
			peeraddr.sin_family = AF_INET;
			peeraddr.sin_addr.s_addr = inet_addr(peer_list[i].ip);
			peeraddr.sin_port = htons((unsigned short)peer_list[i].port);
			peerlen = sizeof(struct sockaddr_in);

			sendto(sockfd, trans_msg, sizeof(transaction), 0, (struct sockaddr *)(&peeraddr), peerlen);
		}
	}
}

int validate_transaction(transaction *trans_msg){
	int i, init_amount = 100;

	for(i=0;i<no_trans;i++){
		if(ledger[i].send_id==trans_msg->send_id){
			init_amount -= ledger[i].amount;			// if A sent money to someone decrease A's wallet money
		}
		else{
			if(ledger[i].recv_id==trans_msg->send_id){
				init_amount += ledger[i].amount;		// if A sent money to someone increase A's wallet money
			}	
		}
	}

	if(init_amount>=trans_msg->amount)
		return 1;							// if A finally has enough money to send then validate the transaction
	else
		return 0;							// else reject the transaction
}

void vote_transaction(int sockfd, transaction *trans_msg){
	vote_msg my_vote;								

	my_vote.node_id = NODE_ID;
	strcpy(my_vote.type, "VOTE");
	gettimeofday(&my_vote.tstamp, NULL);
	my_vote.send_id = trans_msg.send_id;
	my_vote.recv_id = trans_msg.recv_id;				// Construct a vote message
	my_vote.amount = trans_msg.amount;
	my_vote.origin_id = NODE_ID;

	if(validate_transaction(trans_msg))					// if transaction is valid vote Y (yes)
		my_vote.vote = 'Y';
	else
		my_vote.vote = 'N';								// if transaction is invalid vote N (no)

	broadcast_vote(sockfd, &my_vote, -1);
}

void receive_vote(int sockfd, int *vote_count){
	vote_msg recv_vote;
	struct sockaddr_in clientaddr;
	int clientlen = 0;

	while(1){
		/* receive vote from peers */
		recvfrom(sockfd, &recv_vote, sizeof(vote_msg), 0, (struct sockaddr *)(&clientaddr), &clientlen);

		// Update vote count
		if(recv_vote->vote=='Y')
			(*vote_count) = (*vote_count)+1;

		// broadcast vote
		broadcast_vote(sockfd, &recv_vote, recv_vote.origin_id);
	}
}

void broadcast_vote(int sockfd, vote_msg *my_vote, int leave_peer){
	int i;
	struct sockaddr_in peeraddr;
	int peerlen;

	/* Send broadcast to every peer except for the one who sent the transaction (leave_peer) */
	for(i=0;i<no_peers;i++){
		if(i!=leave_peer){
			peeraddr.sin_family = AF_INET;
			peeraddr.sin_addr.s_addr = inet_addr(peer_list[i].ip);
			peeraddr.sin_port = htons((unsigned short)peer_list[i].port);
			peerlen = sizeof(struct sockaddr_in);

			sendto(sockfd, my_vote, sizeof(vote_msg), 0, (struct sockaddr *)(&peeraddr), peerlen);
		}
	}
}

void reach_consensus(int vote_count, trans_msg){
	if(vote_count>no_peers/2){
		memcpy(ledger[no_trans].tstamp, trans_msg.tstamp, sizeof(struct timeval));
		ledger[no_trans].send_id = trans_msg.send_id;
		ledger[no_trans].recv_id = trans_msg.recv_id;
		ledger[no_trans].amount = trans_msg.amount;
	}
}

int main(int argc, char **argv){
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

}