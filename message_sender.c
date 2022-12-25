#include "message_slot.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/*prototypes declerations */
void validate_input(int);

/**
	function is responsible for checking that the number of arguments
	given is 4.
	@param argc: number of arguments
*/
void validate_input(int argc)
{
	if (argc < 4) 
	{
		perror("one or more arguments is missing\n");
		exit(1);	
	}
	if (argc > 4)
	{
		perror("too many arguments\n");
		exit(1);
	}
	return;
}


int main(int argc, char **argv)
{
	char *file_path, *msg;
	int channel_id, fd, bytes_written, err_num;

	/* check input */
	validate_input(argc);
	
	file_path = argv[1];
	msg = argv[3];
	channel_id = atoi(argv[2]);
	
	/* open file */
	fd = open(file_path, O_RDWR);
	if (fd < 0)
	{
		err_num = errno;
		fprintf(stderr, "could not open file\n%s\n", strerror(err_num));
		exit(1);
	}
	
	/* set channel id */
	if ((err_num = ioctl(fd, MSG_SLOT_CHANNEL, channel_id)) < 0)
	{
		err_num = errno;
		fprintf(stderr, "coule not set channel id\n%s\n", strerror(err_num));
		exit(1);
	}
	
	/* write message */
	bytes_written = write(fd, msg, strlen(msg));
	if(bytes_written < 0)
	{
		err_num = errno;
		fprintf(stderr, "could not write message\n%s\n", strerror(err_num));
		exit(1);
	}
	
	/* clost file */
	close(fd);
	return 0;
}

