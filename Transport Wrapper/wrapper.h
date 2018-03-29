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
#include <pthread.h>

#define PAYLOAD_SIZE 1024
#define BUFFER_SIZE (1024*1024)
#define INITIAL_WINDOW_SIZE 1024
#define MAX_SEQ (1024*1024)
#define SLOW_START_THRESHOLD (100*1024)
#define IDLE_TIME 60

/*
	structure to store a packet
*/
struct segment{					
	int seq_no;
	int length;
	char payload[PAYLOAD_SIZE];
	int recvbuffer_space;
	int isack;
};

/*
	structure representing a buffer
*/
struct buffer{
	struct sockaddr *serveraddr;
	int serverlen;
	int message_size;
	char message[PAYLOAD_SIZE];
};

pthread_t main_thread, t1a, t1b, t2;
struct buffer *sendbuff;			// Sender Buffer
struct buffer *recvbuff;			// Receiver Buffer
struct segment ack_packet, data_packet;
int isSendBufferCreated = 0, isRecvBufferCreated = 0, ack_flag = 0, data_flag = 0;
int start, end;						// Sender Buffer parameters
int rstart, rend;					// Receiver Buffer parameters
struct sockaddr *dataaddr;
int datalen;
float drop_prob;

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

/*
	signal handler function
*/
void sig_handler(int signum){			
	signal(SIGUSR1,sig_handler);
}

int min(int a,int b){
	if(a<b)
		return a;
	else
		return b;
}

/*
	appSend function adds packets to the buffer
*/
void appSend(char message[], int message_size, struct sockaddr *serveraddr, int serverlen){
	signal(SIGUSR1,sig_handler);

	int buffer_status,segments_required,i,j,bytes_copied;
	segments_required = (message_size%PAYLOAD_SIZE==0) ? (message_size/PAYLOAD_SIZE) : (message_size/PAYLOAD_SIZE)+1;

	buffer_status = sendbuffer_handle(segments_required);			// check if buffer has enough space

	while(buffer_status==0){
		pause();													// block the receiver if space is not available
		buffer_status = sendbuffer_handle(segments_required);		
	}

	/*
		if space is available then add data to sender buffer
	*/

	j = 0;
	bytes_copied = 0;
	for(i=0;i<segments_required;i++){
		(sendbuff+((end+i)%BUFFER_SIZE))->serveraddr = serveraddr;
		(sendbuff+((end+i)%BUFFER_SIZE))->serverlen = serverlen;
		(sendbuff+((end+i)%BUFFER_SIZE))->message_size = min(message_size - bytes_copied,PAYLOAD_SIZE);

		while(j<min(message_size - bytes_copied, PAYLOAD_SIZE)){
			(sendbuff+((end+i)%BUFFER_SIZE))->message[j] = message[bytes_copied+j];	
			j++;
		}

		bytes_copied += min(message_size - bytes_copied, PAYLOAD_SIZE);
		
		j = 0;
	}
	
	end = (end + segments_required)%BUFFER_SIZE;
}

/*
	this function checks whether buffer has enough space or not
*/
int sendbuffer_handle(int segments_required){
	/*
		if sender buffer is not created then create sender buffer
	*/
	if(isSendBufferCreated==0){
		sendbuff = (struct buffer *)malloc(BUFFER_SIZE*sizeof(struct buffer));	
		isSendBufferCreated = 1;
		start = 0;
		end = 0;
	}

	if(end<start){
		if(start-end >= segments_required)
			return 1;
		else
			return 0;
	}
	else{
		if(BUFFER_SIZE-end+start >= segments_required)
			return 1;
		else
			return 0;
	}
}

/*
	this function retrieves data from buffer
*/
void appRecv(char message[], int message_size, struct sockaddr *serveraddr, int *serverlen){
	signal(SIGUSR1,sig_handler);
	char temp[PAYLOAD_SIZE];
	int buffer_status = 0, i, j, k, bytes_copied, total_bytes=0, total_segments = 0;

	/*
		if sender buffer is not created then create sender buffer
	*/
	if(isRecvBufferCreated==0){
		recvbuff = (struct buffer *)malloc(BUFFER_SIZE*sizeof(struct buffer));	
		isRecvBufferCreated = 1;
		rstart = 0;
		rend = 0;
	}

	for(i=rstart;i<rend;i++){
		total_bytes += (recvbuff+i)->message_size;
		total_segments++;
		if(total_bytes>=message_size){
			buffer_status = 1;
			break;
		}
	}

	/*
		check if data is available in buffer 
	*/
	while(buffer_status==0){
		pause();						// block receiver if data is not available
		total_bytes = 0;
		total_segments = 0;
		for(i=rstart;i<rend;i++){
			total_bytes += (recvbuff+i)->message_size;
			total_segments++;
			if(total_bytes>=message_size){
				buffer_status = 1;
				break;
			}
		}		
	}

	bytes_copied = 0;

	/*
		Extract data from buffer if data is available in the buffer 
	*/
	while(bytes_copied<message_size){
		serveraddr = (recvbuff+rstart)->serveraddr;
		*serverlen = (recvbuff+rstart)->serverlen;
		if(message_size-bytes_copied >= (recvbuff+rstart)->message_size){
			for(i=0;i<(recvbuff+rstart)->message_size;i++){
				message[bytes_copied+i] = (recvbuff+rstart)->message[i];
			}

			bytes_copied += (recvbuff+rstart)->message_size;
			rstart = (rstart+1)%BUFFER_SIZE;
		}
		else{
			i = 0;
			while(bytes_copied+i<message_size){
				message[bytes_copied+i] = (recvbuff+rstart)->message[i];
				i++;
			}

			bytes_copied += i;

			j=i;
			k=0;

			while(i<(recvbuff+rstart)->message_size){
				temp[i-j] = (recvbuff+rstart)->message[i];
				k++;
				i++;
			}

			for(i=0;i<k;i++){
				(recvbuff+rstart)->message[i] = temp[i];
			}
			(recvbuff+rstart)->message_size = k;
		}
	}
}

/*
	To send packets to the receiver
*/
int udp_send(int sockfd, struct segment *segm, struct sockaddr *serveraddr, int serverlen){
	int n;
	n = sendto(sockfd, segm, sizeof(struct segment), 0, serveraddr, serverlen);
	return n;
}

/*
	This function creates a segment with given sequence number and destination address
*/
void create_packet(int sockfd, int curr_seq, struct sockaddr *serveraddr, int serverlen, int message_size, char message[], int recvbuffer_space, int ack){
	int n, i;
	struct segment segm;					
	segm.seq_no = curr_seq;
	
	if(ack==0){
		segm.length = message_size;

		for(i=0;i<message_size;i++){
			segm.payload[i] = message[i];
		}
	}
	else{
		segm.recvbuffer_space = recvbuffer_space;	
	}
	
	segm.isack = ack;
	n = udp_send(sockfd, &segm, serveraddr, serverlen);
}


/*
	Function to update window parameters
*/
void update_window(int *window, int *ssthresh, int recvbuffer_space, int flag){
	switch(flag){
		case 0: 
			// regular ack is received
			if((*window) < (*ssthresh)){
				// slow start phase
				(*window) += 1024;
			}
			else{
				// AIMD phase
				if((*window)==0){
					(*window) = 1024;
				}
				(*window) = (*window) + (1024*1024)/(*window);
			}

			
			break;

		case 1:
			// triple dupack
			(*ssthresh) = (*window)/2;
			(*window) = (*ssthresh);
			
			break;

		case 2:
			// timeout
			(*ssthresh) = (*window)/2;
			(*window) = 1024;
			
			break;
	}

	(*window) = min((*window), recvbuffer_space);		// Cross check with receiver buffer (flow control)
}


/*
	Function to handle congestion control and flow control
*/
void rate_control(int sockfd){
	signal(SIGUSR1, sig_handler);
	struct buffer *var;
	int baseptr = start-1, currptr = start-1;
	int window = INITIAL_WINDOW_SIZE;
	int out_bytes = 0;
	
	int curr_seq = 0;
	int prev_seq = -1;

	int timeout, itr = 0, flag, repack = 0;
	time_t start_time;
	struct segment ack;
	int bytes_acknowledged, prev_ack = -1, dupack=0;
	int ssthresh = SLOW_START_THRESHOLD;

	int triple_dupack = 0;

	while(1){
		
		while(start==end);

		timeout = 1;
		
		/*
			Limiting no of outstanding packets to window size
		*/
		while(out_bytes<window && ((baseptr+1)%BUFFER_SIZE)<end){
			baseptr = (baseptr+1)%BUFFER_SIZE;
			var = sendbuff + baseptr;
			create_packet(sockfd,curr_seq,var->serveraddr,var->serverlen,var->message_size,var->message, 0, 0);

			curr_seq = (curr_seq + (sendbuff+baseptr)->message_size)%MAX_SEQ;
			
			out_bytes += (sendbuff+baseptr)->message_size;
			flag = 1;	
		}

		if(flag){
			flag = 0;
			printf("\nTransmitted packets with sequence numbers %d to %d with window size %d\n",(prev_seq+1)%MAX_SEQ,(curr_seq-1+MAX_SEQ)%MAX_SEQ,window);
		}

		/*
			Wait till an ack is received or or triple dupack is received or timeout occurs
		*/
		start_time = time(NULL);
		while(time(NULL)-start_time<3){
			if(ack_flag==1){
				ack.seq_no = ack_packet.seq_no;
				ack.recvbuffer_space = ack_packet.recvbuffer_space;
				ack.isack = 1;
				ack_flag = 0;
				timeout = 0;
				break;
			}				
		}
		/////////////////////////////////////
		
		if(timeout==0){
			/* Something is received */
			if((ack.seq_no>prev_seq && ack.seq_no<curr_seq && prev_seq<curr_seq) || ((ack.seq_no>prev_seq || ack.seq_no<curr_seq) && prev_seq>curr_seq)){
				/* Expected ack is received */
				bytes_acknowledged = (ack.seq_no > prev_seq) ? ack.seq_no-prev_seq : MAX_SEQ-prev_seq+ack.seq_no;

				/*
					update buffer parameters, previous sequence numbers and check if buffer is empty
				*/
				while(bytes_acknowledged >= (sendbuff + currptr + 1)->message_size){
					bytes_acknowledged -= (sendbuff + currptr + 1)->message_size;
					out_bytes -= (sendbuff + currptr + 1)->message_size;
					currptr = (currptr+1)%BUFFER_SIZE;
					
					start = (start+1)%BUFFER_SIZE; 
					
					start_time = time(NULL);
					while(start==end){
						if(time(NULL)-start_time>IDLE_TIME){
							printf("thread1 exiting");
							pthread_exit(NULL);
						}
					}
					
				}

				prev_seq = ack.seq_no;
				prev_ack = ack.seq_no;

				
				if(triple_dupack==1){
					/*
						If ack of lost packet (detected by triple dupack) is received
					*/
					baseptr = currptr;
					out_bytes = 0;
					curr_seq = (prev_seq + 1)%MAX_SEQ;
					dupack = 0;
					triple_dupack = 0;
					update_window(&window, &ssthresh, ack.recvbuffer_space, 1);			// update window parameters
				}
				else{
					/*
						If ack of regular packet is received
					*/
					update_window(&window, &ssthresh, ack.recvbuffer_space, 0);			// update window parameters
				}
				
				pthread_kill(main_thread, SIGUSR1);			// Buffer has some empty space, unblock appSend
			}
			else{
				// If dupack is received
				if(ack.seq_no==prev_ack){
					dupack++;
				}
				else{
					//dupack = 0;
				}

				if(dupack==3 && triple_dupack==0){
					// Triple dupack is received
					triple_dupack = 1;
					var = sendbuff+((currptr+1)%BUFFER_SIZE);
					create_packet(sockfd,(prev_seq+1)%MAX_SEQ,var->serveraddr,var->serverlen,var->message_size,var->message, 0, 0);
				}
			}
		}
		else{
			/* timeout occurs */
			baseptr = currptr;
			out_bytes = 0;
			curr_seq = (prev_seq + 1)%MAX_SEQ;
			dupack = 0;
			triple_dupack = 0;
			update_window(&window, &ssthresh, window, 2);
		}
		
		/* In case of timeout print retransmission info */
		if(timeout==1){
			if(itr>10){
				printf("Server not responding\n");	// Server's acknowledgement is not received
				//break;
			}
			
			printf("\nTimeout occured.....\n");
			printf("Retransmitted Packet with sequence numbers %d to %d with window size %d\n",prev_seq+1,prev_seq+window,window);
												// print retransmission info
			repack+=window;						// counts total number of retransmissions	
			
			itr++;
		}
		else{
			itr = 0;
		}
	}
}

/*
	Function to receive data from network layer
*/
int udp_receive(int sockfd, struct segment *ack, struct sockaddr *serveraddr, int *serverlen){
	int n;
	n = recvfrom(sockfd, ack, sizeof(struct segment), 0, serveraddr, serverlen);
	return n;
}

/*
	Function to send ack
*/
void send_ack(int sockfd, int seq_no, int space, struct sockaddr *serveraddr, int serverlen){
	char temporary[50];
	temporary[0] = '0';
	create_packet(sockfd, seq_no, serveraddr, serverlen, 0, temporary, space, 1);		// check parameters again
}

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

/*
	Function to handle receiver buffer
*/
void recvbuffer_handle(int sockfd){
	struct segment data;
	int i, curr_seq = 0, prev_seq = -1, temp, dup_count=0;
	time_t start_time;

	while(1){
		start_time = time(NULL);

		/*
			Wait till parse_packets() receive's data
		*/
		while(data_flag==0){
			if(time(NULL)-start_time>IDLE_TIME){
				printf("thread1 exiting\n");
				pthread_exit(NULL);
			}
		}

		
		data.seq_no = data_packet.seq_no;
		data.length = data_packet.length;
		
		for(i=0;i<data_packet.length;i++){
			data.payload[i] = data_packet.payload[i];
		}

		data.isack = 0;
		data_flag = 0;

		if(rstart<=rend){
			temp = BUFFER_SIZE-rend+rstart;
		}
		else{
			temp = rstart-rend;
		}

		if(data.seq_no==curr_seq && drop_packet(drop_prob)==0){
			/*
				If expected data is received, send ack and add data to buffer
			*/
			printf("Ack sent with value %d\n",(curr_seq+data.length-1)%MAX_SEQ);
			send_ack(sockfd, (curr_seq+data.length-1)%MAX_SEQ, temp, dataaddr, datalen);
			prev_seq = (curr_seq+data.length-1)%MAX_SEQ;

			while(rstart==(rend+1)%BUFFER_SIZE);

			(recvbuff+rend)->serveraddr = NULL;   			// TODO access server addr
			(recvbuff+rend)->serverlen = 0;
			(recvbuff+rend)->message_size = data.length;

			for(i=0;i<data.length;i++){
				(recvbuff+rend)->message[i] = data.payload[i];
			}

			rend = (rend+1)%BUFFER_SIZE;
			curr_seq = (curr_seq + data.length)%MAX_SEQ;

			pthread_kill(main_thread, SIGUSR1);
		}
		else{
			/*
				If expected data is not received then send ack of last consecutive data packets  
			*/
			dup_count++;
			printf("%dth Dupack sent with value %d\n",dup_count,prev_seq);
			send_ack(sockfd, prev_seq, temp, dataaddr, datalen);
		}
	}
}

/*
	Function to parse received packets
*/
void parse_packets(int sockfd){
	int n, i;
	struct sockaddr_in serveraddr;
	int serverlen = 0;
	struct segment recv_packet;

	while(1){
		n = udp_receive(sockfd, &recv_packet, (struct sockaddr*)(&serveraddr), &serverlen);

		if(n<0){
			printf("thread2 exiting\n");
			pthread_exit(NULL);
		}

		if(recv_packet.isack==1){
			/*
				If an ack segment is received forward it to rate_control()
			*/
			while(ack_flag==1);
			printf("Ack received with sequence number %d\n",ack_packet.seq_no);
			ack_packet.seq_no = recv_packet.seq_no;
			ack_packet.recvbuffer_space = recv_packet.recvbuffer_space;
			ack_packet.isack = 1;
			ack_flag = 1;
		}
		else{
			/*
				If a data segment is received forward it to recvbuffer_handle()
			*/
			while(data_flag==1);
			data_packet.seq_no = recv_packet.seq_no;
			data_packet.length = recv_packet.length;
			
			dataaddr = (struct sockaddr*)(&serveraddr);
			datalen = serverlen;

			for(i=0;i<data_packet.length;i++){
				data_packet.payload[i] = recv_packet.payload[i];
			}

			data_packet.isack = 0;
			data_flag = 1;
		}
	}
}

/*
	Thread for sending packets and handling congestion, flow control
*/
void *thread1a(void *param){
	printf("Thread 1a starts\n");
	int sockfd = (int)param;
	rate_control(sockfd);
}

/*
	Thread for adding received segments to buffer and send ack segments
*/
void *thread1b(void *param){
	printf("Thread 1b starts\n");
	int sockfd = (int)param;
	recvbuffer_handle(sockfd);
}

/*
	Thread for receivng packets
*/
void *thread2(void *param){
	printf("Thread 2 starts\n");
	int sockfd = (int)param;
	parse_packets(sockfd);
}
