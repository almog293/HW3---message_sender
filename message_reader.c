#include "message_slot.h"

#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int fd;
    int read_message_len;
    unsigned long ch_id;
    char message_buffer[BUFFER_LEN];

    if(argc != 3)
    {
        perror("Error - Invalid number of arguments\n");
        exit(1);
    }
    fd = open(argv[1], O_RDWR);
    if( fd < 0)
    {
        perror("Error - opening file descriptor\n");
        exit(1);
    }
    ch_id = atoi(argv[2]);
    if(ioctl(fd,MSG_SLOT_CHANNEL,ch_id)<0)
    {
        perror("Error - ioctl\n");
        close(fd);
        exit(1);
    }
    if((read_message_len = read(fd,message_buffer,BUFFER_LEN))<0)
    {
        perror("Error - reading from fd\n");
        close(fd);
        exit(1);
    }
    if(write(STDOUT_FILENO,message_buffer,read_message_len) != read_message_len)
    {
        perror("Error - writing message to stdout\n");
        close(fd);
        exit(1);
    }


    close(fd);
    exit(0);
}
