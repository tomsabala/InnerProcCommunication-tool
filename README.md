# InnerProcCommunication-tool
This project is taken as part of Operating System class assignment.

The project purpose is to make an easy communication tool for inner processes.
The tool is called Message Slot Character Device, 
and it is allowing processes to communicate by reading and sending messages throw several channels.

## Instalation

### Creating the module
The communication mechanism is basicly a new Device Driver that we need to load to our kernel.
So inorder to load our new module type:
`make` <br />

`sudo insmod message_slot.ko` <br />

### Creating a new CharDev
`sudo mknod /dev/"char_dev_name" c 235 [minor_number]`

In "char_dev_name" place your Char-Dev wanted name

In [minor_number] insert a minoer number between 0 and 255 included.

`sudo chmod o+rw /dev/"char_dev_name"

### Compile Reader and Sender

`gcc -O3 -Wall -std=c11 message_sender.c -o send.o`

`gcc -O3 -Wall -std=c11 message_reader.c -o read.o`


### Write and read messages
Using the send/read.o files you can now read and send messages between inter processes.
The message_sender.c file needs to get the char_dev filepath, the channel id, and the message itself

For example:

`./send.o "/dev/my_chardev" 1 "Hello World!!"`

writes the message "Hello World!!" to channel 1 in my_chardev

The message_reader.c file needs to get the char_dev filepath, and the channel id

For example:

`./read.o "/dev/my_chardev" 1`

reads the message "Hello World!!" from channel 1 in my_chardev


