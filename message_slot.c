/**
\	Message Slot Kernel Module Assignment
/
\
/
\	Author: Tom Sabala
/
*/

/* kernel-level access */
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

/* kernel source tree */
#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset */
#include <linux/slab.h> 	/* for kmalloc */

/* module license */
MODULE_LICENSE("GPL");

/* include definitions for IOCTL operations */
#include "message_slot.h"

/* <><><><><><><><> DEVICE DRIVER STRUCTURE <><><><><><><><> */

/**
	Each slot will be saving messages in a binary-search tree,
	a node in the tree will be a message, 
	which will have a key-value of channel-id & message respectivley.
	
	All the slots will be saved in inside a const size array of size 256.
*/
typedef struct message_node {
	unsigned long channel_id; 					/* the channel_id number associated with the message */
	char msg[BUF_LEN]; 							/* message buffer */
	int bytes_written;
	struct message_node *left_msg, *right_msg; 	/* left and right pointers */
}MSG_Node;

typedef struct slot_info {
	int Is_Open;	/* indicated wheather the slot has been opened or not */
	MSG_Node *root; /* the messages tree root-node */
}SLOT_Info;

SLOT_Info *slots[256];

/* prototypes */
static MSG_Node* create_msg_node(unsigned long);
static MSG_Node* insert_msg_node(SLOT_Info *, unsigned long);
static MSG_Node* get_msg_node(SLOT_Info *, unsigned long);
static void free_slots_mem(void);
static void free_binary_tree_mem(MSG_Node *);

/* <><><><><><<><><><>STRUCTURES FUNCTIONS <><><><><><><><><><>*/
/**
	create a new message_node 
	with channel_id set to the argument id
	
	@param id: channel id for the new message_node
	@return: pointer to the new message_node
*/
static MSG_Node* create_msg_node(unsigned long id) 
{
	/* allocate memory */
	MSG_Node *msg = kmalloc(sizeof(MSG_Node), GFP_KERNEL);
	if ( msg == NULL )
	{
		/* failed to allocate memory */
		return NULL;
	}
	
	/* set channel_id */
	msg->channel_id = id;
	/* memset(msg->msg, 0, BUF_LEN); */
	msg->bytes_written = 0;
	
	return msg;
}

/**
	insert a new message_node to the slot binary-tree
	
	@param slot: a pointer to the referred message slot struct
	@param id: the proper channel_id which need to be insert
	@return: the pointer for the new message_node
*/
static MSG_Node* insert_msg_node(SLOT_Info *slot, unsigned long id) 
{
	/* create new message node */
	MSG_Node *msg, *tmp, *prev;
	msg = create_msg_node(id);
	
	if ( msg == NULL )
	{
		/* failed to allocate memory */
		return NULL;
	}
	
	/* binary tree is empty */
	if ( slot->root == NULL ) 
	{
		slot->root = msg;
		return msg;
	}
	
	/* search for the proper place where the msg_node needs to be insert to */
	tmp = slot->root;
	while (tmp != NULL)
	{
		prev = tmp;
		if( tmp->channel_id < id )
		{
			tmp = tmp->right_msg;
		}
		else
		{
			tmp = tmp->left_msg;
		}
	}
	
	/* insert message_node */
	if ( prev->channel_id < id )
	{
		prev->right_msg = msg;
	}
	else 
	{
		prev->left_msg = msg;
	}
	
	return msg;
}

/**
	get the message_node with channel_id equals id
	
	@param slot: the message_slot in which we are looking at
	@param id: the channel_id we are looking for
	@return: the message_node pointer
*/
static MSG_Node* get_msg_node(SLOT_Info *slot, unsigned long id)
{	
	MSG_Node *tmp;
	
	tmp = slot->root;
	while(tmp != NULL && tmp->channel_id != id) 
	{
		if (tmp->channel_id < id)
		{
			tmp = tmp->right_msg;
		}
		else 
		{
			tmp = tmp->left_msg;
		}
	}
	
	return tmp;
}

/**
	free all the used slots
*/
static void free_slots_mem(void)
{
	int i;
	for(i=0; i<256; i++)
	{
		if(slots[i] != NULL) /* for each used slot we wish to free all memory allocate with-in its tree, include itself */
		{
			free_binary_tree_mem(slots[i]->root);
			kfree(slots[i]);
		}
	}
}

/**
	free all the nodes from a given binary tree root
*/
static void free_binary_tree_mem(MSG_Node *root)
{
	if(root == NULL)
		return;
	
	/* free left and right subtrees */
	free_binary_tree_mem(root->left_msg);
	free_binary_tree_mem(root->right_msg);
	
	/* free root */
	kfree(root);
}

/* <><><><><><><><><><><> DEVICE FUNCTIONS <><><><><><><><><><><> */
static int device_open( struct inode* inode,
                        struct file*  file )
{
	int minor_num;
	minor_num = iminor(inode); /* extract minor number */
	
	/* if that is the first time we are asking to use this slot */
	if(slots[minor_num] == NULL) 
	{ 
		slots[minor_num] = kmalloc(sizeof(SLOT_Info), GFP_KERNEL); /* allocate memory first */
		if(slots[minor_num] == NULL)
		{
			printk("could not allocate memory\n");
			return -EINVAL;
		}
		
		slots[minor_num]->Is_Open = 0;
	}

  	slots[minor_num]->Is_Open ++; /* increase number od using users by one */

  	return SUCCESS;
}

static int device_release( struct inode* inode,
                           struct file*  file)
{
  	int minor_num = iminor(inode);
	
	/* if that is the first time we are asking to use this slot, that there is nothing to release */
	/*
	if(slots[minor_num] == NULL) 
	{
		printk("slot has not been used yet\n");
		return -EINVAL;
	}
	*/
  	slots[minor_num]->Is_Open --;
  	
  	return SUCCESS;
}

static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
	/* Number of bytes actually written to the buffer */
   	ssize_t bytes_read = 0;
   	unsigned long channel_id;
   	int minor_num = iminor(file->f_inode);
	MSG_Node *msg_node;
	
	/* check that slot has been opened */	
	if(slots[minor_num] == NULL || slots[minor_num]->Is_Open == 0)
	{
		printk("message slot has not been opened yet\n");
   		return -EINVAL;
	}

	channel_id = (unsigned long)file->private_data;
	msg_node = get_msg_node(slots[minor_num], channel_id);
	
   	/* no message found on the channel */
   	if (msg_node == NULL) 
   	{
   		printk("no channel has been set on the file descriptor\n");
   		return -EINVAL;
   	}
	
	/* message is empty */
	if (msg_node->bytes_written == 0)
	{
		printk("no message exists on the channel\n");
		return -EWOULDBLOCK;
	}
	
	/* buffer is too small for the message */
	if ( length < msg_node->bytes_written)
	{
		printk("provided buffer length is too small to hold the last message written on the channel\n");
		return -ENOSPC;
	}
	
   	/* Actually put the data into the buffer */
   	for( bytes_read = 0; bytes_read<msg_node->bytes_written; ++bytes_read ) 
	{
		if (put_user(msg_node->msg[bytes_read], &buffer[bytes_read]) != 0)
		{
			printk("an error has occurred\n");
			return -EINVAL;
		}
	}
	
   	return bytes_read;
}

static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset)
{
  	ssize_t bytes_written;
	unsigned long channel_id;
	int minor_num = iminor(file->f_inode);
	MSG_Node *msg_node;
	
	/* check that message slot is indeed open */
	if(slots[minor_num] == NULL || 0 == slots[minor_num]->Is_Open)
	{
		printk("message slot has not been opened yet\n");
		return -EINVAL;
	}
	
	if (length < 1 || length > BUF_LEN)
	{
		printk("passed message length is 0 or more than 128\n");
		return -EMSGSIZE;
	}
	
	channel_id = (unsigned long)file->private_data;
	msg_node = get_msg_node(slots[minor_num], channel_id);
	
   	/* no message found on the channel */
   	if (msg_node == NULL) 
   	{
   		printk("no channel has been set on the file descriptor\n");
   		return -EINVAL;
   	}
	
	for( bytes_written = 0; bytes_written<length; ++bytes_written ) 
	{
		if (get_user(msg_node->msg[bytes_written], &buffer[bytes_written]) != 0)
		{
			printk("an error has occurred\n");
			return -EINVAL;
		}
	}
	
	msg_node->bytes_written = bytes_written;
	
	/* return the number of input characters used */
	return bytes_written;
}

static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
	int minor_num;
	SLOT_Info *slot;
	MSG_Node *msg_node;
	
	/* invalid channel id */
	if ( ioctl_param == 0 )
	{
		printk("invalid cahnnel id\n");
		return -EINVAL;
	}
	
  	/* Switch according to the ioctl called */
  	if( MSG_SLOT_CHANNEL == ioctl_command_id ) 
  	{
  		minor_num = iminor(file->f_inode);
  		slot = slots[minor_num];

  		/* slot is not opened */
  		if(slot == NULL || slot->Is_Open == 0)
  		{
  			printk("no message slot found\n");
  			return -EINVAL;
  		}
		
  		msg_node = get_msg_node(slot, ioctl_param);
   		
   		/* no message found on the channel */
   		if (msg_node == NULL) 
   		{
   			/* insert new message node */
   			msg_node = insert_msg_node(slot, ioctl_param);
   			if ( msg_node == NULL )
   			{
   				printk("failed to allocate memory\n");
   				return -EINVAL;
   			}
   		}
  		
  		file->private_data = (void *)ioctl_param;
    }
  	else 
  	{
  		printk("invalid command\n");
  		return -EINVAL;
  	}
	
  	return SUCCESS;
}

/* <><><><><><><><><><><> DEVICE SETUP <><><><><><><><><><><<><> */

/* assigning functions to elements of the device structure */
struct file_operations Fops = {
	.owner	  		= THIS_MODULE, 
	.read           = device_read,
	.write          = device_write,
	.open           = device_open,
	.unlocked_ioctl = device_ioctl,
	.release        = device_release,
};

/* Initialize the module - Register the character device */
static int __init simple_init(void)
{
	int i, rc = -1;
	
	/* init slots array */ 
  	for (i=0; i<256; i++)
  	{
  		slots[i] = NULL;
  	}
  	
  	/* Register driver capabilities. Obtain major num */
  	rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );

  	/* Negative values signify an error */
  	if( rc < 0 ) {
    	printk( KERN_ALERT "%s registraion failed for  %d\n",
                       	DEVICE_FILE_NAME, MAJOR_NUM );
    	return rc;
  	}
  	
  	return 0;
}

/* Unregister the device */
/* need to free memory */
static void __exit simple_cleanup(void)
{
	free_slots_mem();
	unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

module_init(simple_init);
module_exit(simple_cleanup);
