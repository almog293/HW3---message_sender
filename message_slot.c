#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>   /* for kmalloc*/
//Our custom definitions of IOCTL operations
#include "message_slot.h"
MODULE_LICENSE("GPL");


typedef struct channel {
    unsigned long ch_id;
    int msg_size;
    char message[BUFFER_LEN];
    struct channel *next;
} channel;

typedef struct channel_list {
    struct channel *head;
    int size;
    
} channel_list;

static channel_list *device_slot_list[257]; //array of all slots/channels

channel *getChannel(int minor, int ch_id)
{
    channel *curr_node;
    curr_node = device_slot_list[minor]->head;
    while(curr_node != NULL)
    {
        if(curr_node->ch_id == ch_id)
            return curr_node;
        curr_node = curr_node->next;
    }
    return NULL;
}

channel *createChannelId(unsigned long ch_id)
{
    channel *curr_ch = kmalloc(sizeof(channel),GFP_KERNEL);
    if(curr_ch == NULL)
    {
        return NULL;
    }
    curr_ch->ch_id = ch_id;
    curr_ch->msg_size = 0;
    curr_ch->next = NULL;
    return curr_ch;
}

channel *getOrCreateChannel(int minor, unsigned long ch_id)
{

    channel *curr_ch,*perv_node,*curr_node;
    if(device_slot_list[minor] == NULL)
    	return NULL;
    if(device_slot_list[minor]->size == 0) // minor is empty
    {
        if((curr_ch = createChannelId(ch_id)) == NULL)
            return NULL;
        device_slot_list[minor]->head = curr_ch;
        device_slot_list[minor]->size = 1;
    }
    else // minor is not empty
    {
    	curr_ch = NULL;
        curr_node = device_slot_list[minor]->head;
        //search for the ch_id node or creating new and adding to the list
        do
        {
            if(curr_node->ch_id == ch_id)
                curr_ch = curr_node;
            perv_node = curr_node;
            curr_node = curr_node->next;
        } while (curr_node != NULL);
        
        if(curr_ch == NULL)
        {
            if((curr_ch = createChannelId(ch_id)) == NULL)
                return NULL;
            perv_node->next = curr_ch;
            device_slot_list[minor]->size++;
        }
    }
    return curr_ch;
}




//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode, struct file*  file )
{
    channel_list *device;
    int minor = iminor(inode);
    if(device_slot_list[minor] == NULL)
    {
        if((device = kmalloc(sizeof(channel_list) , GFP_KERNEL))== NULL)
            return -1;
        device->size = 0;
        device_slot_list[minor] = device;
    }
    return SUCCESS;
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
    if(ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_command_id == 0)
    {
        return -EINVAL;
    }
    file->private_data = (void *) ioctl_param;
    return SUCCESS;
}


//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
    channel *curr_ch;
    int minor,count;
    unsigned long ch_id;
    char temp_message_buff[BUFFER_LEN];
    minor = iminor(file->f_inode);
    if(device_slot_list[minor] == NULL || buffer == NULL || file->private_data == NULL)
        return -EINVAL;
    ch_id = (unsigned long)file->private_data;
    if((curr_ch = getOrCreateChannel(minor,ch_id)) == NULL)
    	return -EINVAL;
    if(curr_ch->msg_size == 0)
        return -EWOULDBLOCK;
        
    if (curr_ch->msg_size > length)
        return -ENOSPC;
        
    memcpy(temp_message_buff,curr_ch->message,curr_ch->msg_size);
    
    if((count = copy_to_user(buffer,temp_message_buff,curr_ch->msg_size)) != 0)
    	return -EINVAL;

    return curr_ch->msg_size;
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset)
{
    int count,minor;
    char temp[BUFFER_LEN];
    channel *curr_ch;
    unsigned long ch_id;
    
    if (buffer == NULL || file->private_data == NULL)
        return -EINVAL;
        
    if (length <= 0 || length > BUFFER_LEN)
        return -EMSGSIZE;
        
    ch_id = (unsigned long)file->private_data;
    minor = iminor(file->f_inode);
    if((curr_ch = getOrCreateChannel(minor,ch_id)) == NULL)
    	return -EINVAL;
     
    if((count = copy_from_user(temp,buffer,length))!=0)
        return -ENOSPC;
        
    memcpy(curr_ch->message,temp,length);
    curr_ch->msg_size = length;
    
    return curr_ch->msg_size;
}                 

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops = {
  .owner	  = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void)
{
    int rc = -1;
    // Register driver capabilities. Obtain major num
    rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );
    printk("register %s, major number - %d\n",DEVICE_RANGE_NAME,MAJOR_NUM);
    // Negative values signify an error
    if( rc < 0 ) 
    {
    	printk( KERN_ALERT "%s registraion failed for  %d\n",DEVICE_RANGE_NAME, MAJOR_NUM );
    	return rc;
    }

    printk( "Registeration is successful. \n");
    return SUCCESS;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
    //free all array and linked lists
    int i;
    channel *curr_ch,*temp;
	


    // Unregister the device
    // Should always succeed
    printk("unregister\n");
    for(i=0; i< 257; i++)
    {
    	if(device_slot_list[i] != NULL)
    	{
    		curr_ch = device_slot_list[i]->head;
    		do
    		{
    			temp = curr_ch;
    			curr_ch = curr_ch->next;
    			kfree(temp);
    		}while(curr_ch != NULL);
    		kfree(device_slot_list[i]);
    	}
    }
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
    printk("unregister complete\n");

    
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
