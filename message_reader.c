#include "message_slot.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

/*prototypes declerations */
void validate_input(int);

/**
	function is responsible for checking that the number of arguments
	given is 3.
	@param argc: number of arguments
*/
void validate_input(int argc)
{
	if (argc < 3) 
	{
		perror("one or more arguments is missing\n");
		exit(1);	
	}
	if (argc > 3)
	{
		perror("too many arguments\n");
		exit(1);
	}
	return;
}


int main(int argc, char **argv)
{
	char *file_path, msg[BUF_LEN];
	int channel_id, fd, bytes_read, err_num;

	/* check input */
	validate_input(argc);
	file_path = argv[1];
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
	if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) < 0)
	{
		err_num = errno;
		fprintf(stderr, "coule not set channel id\n%s\n", strerror(err_num));
		exit(1);
	}
	
	/* read message */
	bytes_read = read(fd, msg, BUF_LEN);
	if(bytes_read < 0)
	{
		err_num = errno;
		fprintf(stderr, "could not read message\n%s\n", strerror(err_num));
		exit(1);
	}
	
	/* close file and write message */
	close(fd);
	write(STDOUT_FILENO, msg, bytes_read);
	
	return 0;
}
