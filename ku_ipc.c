#include <linux/init.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/rculist.h>
#include "ku_ipc.h"



MODULE_LICENSE("GPL");

struct msgData
{
  	struct list_head list;
  	int data_size;
  	long type;
	void *data;
};

struct wait_msg_info
{
	struct list_head list;
	int size;
	int type;
};

typedef struct queue_list
{
  	struct list_head list;
  	int key;
	int size;
	int sndtmp;
	int rcvtmp;
	int *delete_flag;
	struct msgData msgQueue;
	struct wait_msg_info snd_info_head;
	struct wait_msg_info rcv_info_head;
	wait_queue_head_t sndQueue;
	wait_queue_head_t rcvQueue;
	wait_queue_head_t delete_wait_queue;
}QUEUE;

QUEUE queueEntry;
int key_amount;
dev_t dev_num;
static struct cdev *cd_cdev;
spinlock_t queue_lock,data_lock,sndWait_lock,sndWait_lock;

static int open_interface(struct inode *inode , struct file *file)
{
	printk("KU_SYS_V OPEN!!\n");		
	return 0;
}

static long sys_ioctl(struct file *file,unsigned int cmd,unsigned long arg)
{
  	QUEUE *queue_tmp ;
  	struct list_head *pos,*q,*pos1,*q1;
  	struct msgData *tmp_msgData;
	int key,i,*delete_tmp,old;
	switch(cmd)
	{
		case SIMPLE_IOCTL1:
			if (key_amount==0)
			{ 	
				printk("hi\n");
				return 1;
			}
			else
			{
				printk("hellow\n");
				spin_lock(&queue_lock);
				list_for_each_entry(queue_tmp,&(queueEntry.list),list)
				{
				  	if(queue_tmp->key == arg)
					{
						spin_unlock(&queue_lock);
						return 0;
					}
				}
				spin_unlock(&queue_lock);
				return 1;
			}
		break;	
		case SIMPLE_IOCTL2:
			
				spin_lock(&queue_lock);
			  	queue_tmp=NULL;
				queue_tmp=(QUEUE *)kmalloc(sizeof(QUEUE),GFP_KERNEL);
			  	queue_tmp->key=(int)arg;
				queue_tmp->size=0;
				queue_tmp->sndtmp=0;
				queue_tmp->rcvtmp=0;
				queue_tmp->delete_flag=(int *)kmalloc(sizeof(int),GFP_KERNEL);
				*(queue_tmp->delete_flag)=0;
				spin_lock(&data_lock);
				INIT_LIST_HEAD(&((queue_tmp->msgQueue).list));
				INIT_LIST_HEAD(&(queue_tmp->snd_info_head));
				INIT_LIST_HEAD(&(queue_tmp->rcv_info_head.list));
				init_waitqueue_head(&(queue_tmp->sndQueue));
				init_waitqueue_head(&(queue_tmp->rcvQueue));
				init_waitqueue_head(&(queue_tmp->delete_wait_queue));
				spin_unlock(&data_lock);
				queue_tmp->msgQueue.data=kmalloc(KUIPC_MAXMSG,GFP_KERNEL);
			  	list_add(&(queue_tmp->list),&(queueEntry.list));
			  	spin_unlock(&queue_lock);
				key_amount++;
				printk("key_amount 1\n");
				return 1;
		
		break;
		case SIMPLE_IOCTL3:
			  	key = arg;

			  	spin_lock(&queue_lock);
			  	printk("11\n");
			  	list_for_each_safe(pos1,q1,&(queueEntry.list))
				{
					printk("222\n");
					queue_tmp=list_entry(pos1,QUEUE,list);
					printk("33\n");
					if(queue_tmp->key == arg)
					{

						delete_tmp=(int*)kmalloc(sizeof(int),GFP_KERNEL);
						*delete_tmp=1;
						printk("rcu_rock\n");
						old=queue_tmp->delete_flag;
						printk("rcu_unrock\n");
						rcu_assign_pointer((queue_tmp->delete_flag),delete_tmp);
						printk("rcu_assign_pointer\n");
						synchronize_rcu();
						printk("synchronize_rcu\n");
						kfree(old);
						printk("kfree old\n");
						
						spin_lock(&sndWait_lock);
						printk("my god\n");
							if(queue_tmp->sndtmp>0)
							{
								printk("my god = %d\n",queue_tmp->sndtmp);
								for(i=0;i<queue_tmp->sndtmp;i++)
								{
									wake_up_interruptible(&(queue_tmp->sndQueue));
									printk("snd wake up event\n");
								}
								spin_unlock(&sndWait_lock);
								wait_event(queue_tmp->delete_wait_queue,1==1);
							}
							else
							{
								spin_unlock(&sndWait_lock);
							}

						spin_lock(&sndWait_lock);
							if(queue_tmp->rcvtmp>0)
							{
								for(i=0;i<queue_tmp->rcvtmp;i++)
								{
									wake_up_interruptible(&(queue_tmp->rcvQueue));
									printk("rcv wake up event\n");
								}
								spin_unlock(&sndWait_lock);
								wait_event(queue_tmp->delete_wait_queue,1==1);
							}
							else
							{
								spin_unlock(&sndWait_lock);
							}
					
						printk("aaa\n");
						spin_lock(&data_lock);


						list_for_each_safe(pos,q,&(queue_tmp->msgQueue.list))
						{
							tmp_msgData=list_entry(pos,struct msgData,list);
							kfree(tmp_msgData->data);
							list_del(pos);
							kfree(tmp_msgData);
						}
						printk("bbb\n");
						spin_unlock(&data_lock);
						list_del(pos1);
						kfree(queue_tmp);
						key_amount--;
						spin_unlock(&queue_lock);
						return 0;
					}
				}
				spin_unlock(&queue_lock);
				return -1;
				
		break;

	}
	return -1;
}

static int read_msg(struct file *file,char *buf,size_t len,loff_t *lot )
{
	RCV_MSG *rcv_data;
	int key,structSize,i,roop_flag=0,flag,tmp;
	long type;
	int wake_up_size=0;
	struct msgData *tmp_msgData;
	struct list_head *pos,*q,*pos2,*q2;
	struct wait_msg_info *wt_msg_info;
	struct wait_msg_info *wt_msg_info2;
	struct wait_msg_info *wt_msg_info_tmp;
	rcv_data = (RCV_MSG*)buf;
	key=rcv_data->key;
	type=rcv_data->type;
	flag=rcv_data->flag;
	QUEUE *queue_tmp ;
	printk("rcvkey=%d\n",key);
	printk("rcvtype=%d\n",type);
	printk("rcvflag = %d\n",flag);
	if(!(copy_from_user((void *)rcv_data,(void *)buf,len)))
	{

	}
	else
	{
		return -3;
	}
	
	printk("rcv_data %d\n",rcv_data->size);

	structSize=rcv_data->size;

	spin_lock(&queue_lock);
	list_for_each_entry(queue_tmp,&(queueEntry.list),list)
	{
		printk("loop\n");
		if(queue_tmp->key==key)// same key
		{
			while(1)
			{
				printk("loop2\n");
				//spin_lock(&sndWait_lock);

				if(roop_flag>0)
				{
					spin_lock(&sndWait_lock);
					queue_tmp->rcvtmp--;
					printk("wait_queue list_for_each_entry\n");

					list_for_each_safe(pos2,q2,&(queue_tmp->rcv_info_head.list))
					{
						wt_msg_info_tmp=list_entry(pos2,struct wait_msg_info,list);
						if(wt_msg_info_tmp==wt_msg_info2)
						{
							list_del(pos2);
							kfree(wt_msg_info);
							break;
						}
					}
					spin_unlock(&sndWait_lock);
					printk("snd tmp --!!\n");
					printk("test_wakeup:%ld\n", queue_tmp->size);
					printk("sss queue_tmp_address=%p\n",queue_tmp);
				}
				printk("end check roop_flag condition\n");
				if(queue_tmp->size>0)//size bigger than 0
				{	
				//	spin_unlock(&sndWait_lock);
					printk("find type flag=%d\n",rcv_data->flag);
					printk("queue_tmp -> size=%d\n",queue_tmp -> size);
					spin_lock(&data_lock);
					list_for_each_safe(pos,q,&(queue_tmp->msgQueue.list))
					{
						printk("loop2\n");
						tmp_msgData=list_entry(pos,struct msgData,list);
						if(tmp_msgData->type == type)//same type
						{
							if((flag & MSG_NOERROR))
							{
								printk("write\n");
								rcv_data->msg =  kmalloc(structSize,GFP_KERNEL);
								*((long *)(rcv_data->msg))=type;
								for(i=4;i<structSize;i++)
								{  
									*((char *)(rcv_data->msg)+i)=*((char *)(tmp_msgData->data)+(i-4));
								}
								printk("data %d\n",(int)(*((int *)(rcv_data->msg))));
								printk("data %d\n",(int)(*((int *)(rcv_data->msg)+1)));
								if(copy_to_user((void*)(rcv_data->msg_add),(void*)rcv_data->msg ,structSize))
								{
									printk("cp fail\n");
									spin_unlock(&data_lock);
									spin_unlock(&queue_lock);
									return -4;
								}
								list_del(pos);
								kfree(tmp_msgData);
								queue_tmp->size-=structSize;
								spin_unlock(&data_lock);
								spin_unlock(&queue_lock);
								
								spin_lock(&sndWait_lock);
								printk("wake_up_interruptible\n");
								list_for_each_entry(wt_msg_info,&(queue_tmp->snd_info_head.list),list)
								{
									printk("Jeong kuk1\n");
									if((wt_msg_info->size+queue_tmp->size+wake_up_size)<=KUIPC_MAXVOL)
									{
										wake_up_size+=wt_msg_info->size;
										wake_up_interruptible(&(queue_tmp->sndQueue));
										printk("Jeong kuk2\n");
									}
									else
									{
										break;
									}
								}
								printk("end_wake_up_interruptible\n");
								spin_unlock(&sndWait_lock);

								return i+4;
							}//snd
							else if(!(rcv_data->flag & MSG_NOERROR) && tmp_msgData->data_size>structSize)
							{
								printk("no write\n");
								spin_unlock(&data_lock);
								spin_unlock(&queue_lock);
								return -3;
							}//not cut message
						}//fine message
					}
					//find not message
					spin_unlock(&data_lock);
					spin_unlock(&queue_lock);	
					return -1;
				}//size bigger than 0
				else if(!(flag & KU_IPC_NOWAIT))
				{
					printk("enter wait process\n");
					//spin_unlock(&sndWait_lock);
					spin_unlock(&queue_lock);
					while(1)
					{
						printk("roop\n");
						roop_flag++;
						if(roop_flag==1)
		             	{ 
		             		spin_lock(&sndWait_lock);
		             		queue_tmp->rcvtmp++;
		             		wt_msg_info2=(struct wait_msg_info *)kmalloc(sizeof(struct wait_msg_info),GFP_KERNEL);
		             		wt_msg_info2->type = type;
		             		list_add_tail(&(wt_msg_info2->list),&(queue_tmp->rcv_info_head.list));
		             		printk("rcv_add_tail\n");
		             		spin_unlock(&sndWait_lock);
		             		//spin_unlock(&queue_lock);
		             	}
		             	rcu_read_lock();
		             	tmp=*(queue_tmp->delete_flag);
		             	rcu_read_unlock();
						if(!(wait_event_interruptible_exclusive(queue_tmp->rcvQueue,(queue_tmp->size>0)||(tmp==1))))
						{
							
							printk("recive wake_up signal1!!\n");
							break;
						}
						
						roop_flag++;
					}//while(1)

					spin_lock(&queue_lock);
					rcu_read_lock();
					if(*(queue_tmp->delete_flag)==1)
					{
						rcu_read_unlock();
						spin_lock(&sndWait_lock);
						queue_tmp->rcvtmp--;
						if(queue_tmp->rcvtmp==0)
						{
							wake_up(&(queue_tmp->delete_wait_queue));
							spin_unlock(&sndWait_lock);
							rcu_read_unlock();
							spin_unlock(&queue_lock);
							return -1;
						}
						spin_unlock(&sndWait_lock);
					}
					rcu_read_unlock();
				}//else if(!(rcv_data->flag && MSG_NOWAIT))
				else if((flag & KU_IPC_NOWAIT))
				{
					spin_unlock(&queue_lock);
					return -1;
				}
			}//while(1)
		}
	}
	spin_unlock(&queue_lock);
	return -2;
}


static int write_msg(struct file *file,const char *buf,size_t len,loff_t *lof)
{
  	int i,roop_flag=0,list_flag=0,tmp;
  	SND_MSG *sndMSG;
	QUEUE *queue_tmp = 0;
	struct wait_msg_info *wt_msg_info;
	struct wait_msg_info *wt_msg_info2;
	struct wait_msg_info *wt_msg_info_tmp;
	struct list_head *pos,*q;
	

	sndMSG = (SND_MSG *)kmalloc(sizeof(SND_MSG),GFP_KERNEL);
	if(len>=KUIPC_MAXMSG)
	{
		return -3;
	}
  	if(!(copy_from_user((void *)sndMSG,(void *)buf,len)))
	{		
		printk("copy write_msg = %d\n",sndMSG->key);
		spin_lock(&queue_lock);
		list_for_each_entry(queue_tmp,&(queueEntry.list),list)
		{
			printk(" write loop\n");
		 	if(queue_tmp->key==sndMSG->key)
			{
				printk(" write if in\n");
				while(1)
				{

				  	if((queue_tmp->size+sndMSG->size)<=KUIPC_MAXVOL)
				  	{
		
						struct msgData *msg_tmp =(struct msgData*) kmalloc(sizeof(struct msgData),GFP_KERNEL);
						msg_tmp->data=kmalloc(sndMSG->size,GFP_KERNEL);
						msg_tmp->type= *((long*)(sndMSG->msg));
						for(i=4;i<sndMSG->size;i++)
						{  
							*((char *)(msg_tmp->data)+(i-4))=*((char *)(sndMSG->msg)+i);
						}
						msg_tmp->data_size=sndMSG->size;
						spin_lock(&data_lock);
						list_add_tail(&(msg_tmp->list),&(queue_tmp->msgQueue.list));
						spin_unlock(&data_lock);
						queue_tmp->size+=sndMSG->size;
						printk("test2:%ld\n", queue_tmp->size);
						printk("queue_tmp_address=%p\n",queue_tmp);
						//kfree(sndMSG->msg);
						kfree(sndMSG);
						if(roop_flag>0)
						{
							spin_lock(&sndWait_lock);
							queue_tmp->sndtmp--;
							printk("wait_queue list_for_each_entry\n");

							list_for_each_safe(pos,q,&(queue_tmp->snd_info_head.list))
							{
								wt_msg_info_tmp=list_entry(pos,struct wait_msg_info,list);
								if(wt_msg_info==wt_msg_info_tmp)
								{
									list_del(pos);
									kfree(wt_msg_info);
									break;
								}
								
							}
							spin_unlock(&sndWait_lock);
							printk("snd tmp --!!\n");
							printk("test_wakeup:%ld\n", queue_tmp->size);
							printk("sss queue_tmp_address=%p\n",queue_tmp);
						}
						spin_lock(&sndWait_lock);
						list_for_each_entry(wt_msg_info2,&(queue_tmp->rcv_info_head.list),list)
						{
							printk("rcv Jeong kuk1\n");
							if((wt_msg_info2->type)==msg_tmp->type)
							{
								printk("rcv Jeong kuk2\n");
								wake_up_interruptible(&(queue_tmp->rcvQueue));
								list_flag=1;
								break;
							}
							else if(list_flag==0)
							{
								wake_up_interruptible(&(queue_tmp->rcvQueue));
							}
						}
						spin_unlock(&sndWait_lock);
						spin_unlock(&queue_lock);
						return 1;
				  	}
	             	else if(sndMSG->flag == KU_IPC_NOWAIT)
	             	{
	             		spin_unlock(&queue_lock);
	             		return -1;
	             	}

	             	spin_unlock(&queue_lock);
	             	
	             	while(1)
	             	{
	             		roop_flag++;
	             		if(roop_flag==1)
	             		{ 
	             			spin_lock(&sndWait_lock);
	             			queue_tmp->sndtmp++;
	             			wt_msg_info=(struct wait_msg_info *)kmalloc(sizeof(struct wait_msg_info),GFP_KERNEL);
	             			wt_msg_info->size = sndMSG->size;
	             			list_add_tail(&(wt_msg_info->list),&(queue_tmp->snd_info_head));
	             			printk("add_tail\n");
	             			spin_unlock(&sndWait_lock);
	             		}
	             		printk("wait_event_interruptible_exclusive\n");
	             		rcu_read_lock();
	             		tmp=(*(queue_tmp->delete_flag));
	             		rcu_read_unlock();
						if(!(wait_event_interruptible_exclusive(queue_tmp->sndQueue,(KUIPC_MAXVOL>=(queue_tmp->size+sndMSG->size)||(tmp==1)))))
						{
						 	break;
						 	printk(" wake_up signal1!!\n");
						 }
						printk("wake_up signal2!!\n");
						roop_flag++;
	             	}
	             	spin_lock(&queue_lock);
	             	rcu_read_lock();
					printk(" delete_flag =%d!!\n",*(queue_tmp->delete_flag));

					if(*(queue_tmp->delete_flag)==1)
					{
						printk(" zzz\n");
						rcu_read_unlock();
						spin_lock(&sndWait_lock);
							queue_tmp->sndtmp--;
							if(queue_tmp->sndtmp==0)
							{
								wake_up(&(queue_tmp->delete_wait_queue));
							}
						spin_unlock(&sndWait_lock);
						spin_unlock(&queue_lock);
						printk(" zzzzz\n");
						return -1;
					}
					printk(" zzzzz\n");
					rcu_read_unlock();
					printk(" kkkk\n");
				}
			  	return -1;
			}
		}
		spin_unlock(&queue_lock);
		return -2;
	}
	else
	{
		return -3;
	}
}

int release_interface(struct inode *inode,struct file *file)
{
  printk("release\n");
  return 0;
}

struct file_operations simple_char_fops = 
{
	.write=write_msg,
	.open=open_interface,
	.unlocked_ioctl=sys_ioctl,
	.release=release_interface,
	.read=read_msg,
};

static int __init ku_sys_v_init(void)
{
	INIT_LIST_HEAD(&(queueEntry.list));
	printk("1\n");
	queueEntry.msgQueue.data=kmalloc(KUIPC_MAXMSG,GFP_KERNEL);
	printk("2\n");
	key_amount=0;
	alloc_chrdev_region(&dev_num,0,1,DEV_NAME);
	cd_cdev=cdev_alloc();
	cdev_init(cd_cdev,&simple_char_fops);
	cdev_add(cd_cdev,dev_num,1);

	return 0;
}

static void __exit ku_sys_v_exit(void)
{

	printk("Exit Module\n");
	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num,1);
}

module_init(ku_sys_v_init);
module_exit(ku_sys_v_exit);