#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/ip_icmp.h>
#include <time.h>
#include <float.h>

//credit to Beej's Guide to Network Programming by Brian Hall.

int time_to_live = 54;

char *lookup_DNS(char *hostname)
{

    struct addrinfo hints, *infoptr;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; // supporting both IPv4 and IPv6
    hints.ai_socktype = SOCK_RAW;

    int result = getaddrinfo(hostname, NULL, &hints, &infoptr);
    if (result)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
        exit(1);
    }

    char *host = malloc(sizeof(char) * 256);

    getnameinfo(infoptr->ai_addr, infoptr->ai_addrlen, host, sizeof(char) * 256, NULL, 0, NI_NUMERICHOST);

    freeaddrinfo(infoptr);

    return host;
}

struct ping_packet
{
    struct icmphdr icmp_header;
    char msg[64 - sizeof(struct icmphdr)];
};


//checksum

unsigned short checksum(void *ptr, int length)
{
    //everthing is unsigned to make bit manipulation more intuitive
    unsigned int sum = 0;
    unsigned short result;
    unsigned short *buffer = ptr;

    while (length > 1)
    {
        sum += *buffer++;
        length -= 2;
    }

    if (length == 1){
        sum += *(unsigned char *)buffer;
    }
        
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int loop = 1;

void ping(char *destination_ip)
{
    struct addrinfo hints, *res;
    int sockfd;
    int ping_counter = 0;
    struct sockaddr_storage their_addr;
    struct ping_packet payload;
    struct ping_packet recv_payload;

    //payload packet is zeroed out
    memset(&payload, 0, sizeof(payload));
    //the payload packet type is an ICMP ECHO request
    payload.icmp_header.type = ICMP_ECHO;
    //the id of the ECHO request is the process id
    payload.icmp_header.un.echo.id = getpid();
    //calculate the checksum of the payload
    payload.icmp_header.checksum = checksum(&payload, sizeof(payload));

    //use getaddrinfo() to pull up all the structs relating to the address

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;

    getaddrinfo(destination_ip, NULL, &hints, &res);

    char *host = malloc(sizeof(char) * 256);

    getnameinfo(res->ai_addr, res->ai_addrlen, host, sizeof(char) * 256, NULL, 0, NI_NUMERICHOST);

    // make a socket:

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (sockfd < 0)
    {
        printf("socket failed\n");
    }

    // set the time to live option

    if (setsockopt(sockfd, SOL_IP, IP_TTL, &time_to_live, sizeof(time_to_live)) != 0)
    {
        printf("TTL operation failed\n");
        return;
    }
    else
    {
        printf("TTL : %d hops\n", time_to_live);
    }

    struct timespec start, end;
    int packet_sent = 0;
    int packet_recv = 0;
    double packet_loss = 0;
    double max_rrt = -DBL_MAX;
    double min_rrt = DBL_MAX;
    double rrt_sum = 0;

    while (loop)
    {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);

        // sendto!
        if (sendto(sockfd, &payload, sizeof(payload), 0, res->ai_addr, res->ai_addrlen) <= 0)
        {
            printf("packet send operation failed\n");
        }
        else
        {
            packet_sent++;
        }
        int length_address = sizeof(their_addr);
        if (recvfrom(sockfd, &recv_payload, sizeof(recv_payload), 0, (struct sockaddr *)&their_addr, &length_address) <= 0)
        {
            printf("packet receive operation failed\n");
        }
        else
        {
            clock_gettime(CLOCK_MONOTONIC_RAW, &end);
            packet_recv++;
            ping_counter++;
            double rtt = (end.tv_sec - start.tv_sec) * 1000.0 + (((end.tv_nsec - start.tv_nsec)) / 1000000.0);
            if(rtt > max_rrt){
                max_rrt = rtt;
            }
            if(rtt < min_rrt){
                min_rrt = rtt;
            }
            rrt_sum += rtt;
            packet_loss = ((double)((packet_sent - packet_recv) / packet_sent) * 100);
            printf("RTT : %.2f ms ---- packet loss is %.2f%% --- 64 bytes from %s\n", rtt, packet_loss, host);
        };
        usleep(1000000);
    }

    printf("\n---- %s statistics ----\n", destination_ip);
    printf("packets sent :%d\n", packet_sent);
    printf("packets received :%d\n", packet_recv);
    printf("packet loss: %.2f%%\n", packet_loss);
    printf("max RRT: %.2f\n", max_rrt);
    printf("min RRT: %.2f\n", min_rrt);
    printf("average RRT: %.2f\n", rrt_sum/ping_counter);



}

//signal handler, sets the global variable loop to zero, stopping the pinging loop.
void handler(int signal_number)
{
    loop = 0;
}

int main(int argc, char *argv[])
{

    if (argc < 2 || argc > 3)
    {
        printf("Usage : sudo ./ping <domain-name> [optional] <time-to-live>\n");
        return 0;
    }
    if (argc == 3)
    {
        if (atoi(argv[2]) == 0)
        {
            printf("<time-to-live> is a positive integer\n");
            return 0;
        }
        time_to_live = atoi(argv[2]);
    }

    printf("The ip address for the domain %s is %s \n", argv[1], lookup_DNS(argv[1]));

    signal(SIGINT, handler); //interrupt handler

    ping(argv[1]);

    return 0;
}
