#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <sys/stat.h>
#include <time.h>
#include <assert.h>

#define PORT     8080 
#define MAXLINE 1300

long long diff(struct timespec t1, struct timespec t2)
{
    struct timespec diff;
    if (t2.tv_nsec-t1.tv_nsec < 0) {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec - 1;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec + 1000000000;
    } else {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;
    }
    return (diff.tv_sec * 1000000000 + diff.tv_nsec);
}


struct request_header {
	int seq;
	int len;
	struct timespec time;
};

int main(int argc, char *argv[]) { 
	int sockfd; 
	char buffer[MAXLINE]; 
	struct sockaddr_in     servaddr; 
	struct timespec time1, time2;

	struct timespec time3, time4;
	int ret1, ret2, len; 
	FILE *fptr1;
	FILE *fptr2;

	assert(argc > 2);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	assert(sockfd > 2);

	memset(&servaddr, 0, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_port = htons(PORT); 
	mkdir("results", 0664);

	if (argv[1][0] == '4') {
		servaddr.sin_addr.s_addr = inet_addr("10.0.1.1");
		fptr1 = fopen("results/exp1_4","w");
	}

	if (argv[1][0] == '1') {
		servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
		fptr1 = fopen("results/exp1_1","w");
	}
	/* unikraft nolwip */
	if (argv[1][0] == '2') {
		servaddr.sin_addr.s_addr = inet_addr("10.0.0.2"); 
		fptr1 = fopen("results/exp1_2","w");
	}
	/* linux in vn */
	if (argv[1][0] == '3') {
		servaddr.sin_addr.s_addr = inet_addr("10.0.0.13"); 
		fptr1 = fopen("results/exp1_3","w");
	}

	if (argv[1][0] == '5') {
		servaddr.sin_addr.s_addr = inet_addr("10.0.0.12"); 
		fptr1 = fopen("results/exp1_5","w");

	}

	long long total_time = 0;
	long long nsecs;
	long long mean_latency = 0;

	long long time_mes = 0;
	long num_packets = 0;
	/* We send packets */
	struct request_header req;
	req.seq = 0;
	req.len = 0;

	int count = atoi(argv[2]);
	int expect = 0;
	printf("## Running trial with %d\n", count);

	for (int i = 0; i < count; i++) {
		req.seq = i;
		clock_gettime(CLOCK_MONOTONIC, &time1);
		req.time = time1;
		/* write in packet time snet */
		sendto(sockfd, (const char *)&req, sizeof(struct request_header), 
				0, (const struct sockaddr *) &servaddr,  
				sizeof(servaddr)); 
	}

	

	/* For each packet we recieve now we send another one */
	int i = count;
	int lost_packets = 0;
	int ok = 0;

	while (1) {
		/* CLOCK_PROCESS_CPUTIME_ID ...? CLOCK_PROCESS_CPUTIME_ID */
		clock_gettime(CLOCK_MONOTONIC, &time3);
		/* TODO: write in packet time */
		/* TODO: write time on packet and and 
		 *
		 * print number of lost packets */

		ret2 = recvfrom(sockfd, (char *)&req, sizeof(struct request_header),
				MSG_WAITALL, (struct sockaddr *) &servaddr, 
				&len);

		clock_gettime(CLOCK_MONOTONIC, &time2);
		time1 = req.time;
		lost_packets += expect != req.seq;
		/* Update the request's params */
		req.time = time2;
		req.seq = i;


		ret1 = sendto(sockfd, (const char *)&req, sizeof(struct request_header), 
				0, (const struct sockaddr *) &servaddr,  
				sizeof(servaddr));

		
		/* Sanity checks */
		assert(ret1 >= 0);
		assert(ret2 >= 0);
		
		clock_gettime(CLOCK_MONOTONIC, &time4);

		nsecs = diff(time1, time2);
		total_time += nsecs;

		nsecs = diff(time3, time4);
		time_mes += nsecs;

		num_packets++;


		if (time_mes >= 1000000000) {
			printf("%ld requests / sec \n", num_packets);
			printf("mean latency (last sec) %ld usecs \n", total_time / num_packets * 1 / 1000);
			//printf("Lost %d packets in last sec\n", lost_packets);
			total_time = 0;

			num_packets = 0;
			lost_packets = 0;
			time_mes = 0;
		}

		//i = (i + 1) % (count + 10000);
		//expect = (expect + 1) % (count + 10000);
		
	}

	close(sockfd); 
	return 0; 
} 
