#include "ku_ipc.h"
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <malloc.h>


int ku_msgget(int key, int msgflg)
{
	int rtn,dev=-1;
	long key_var=key;
	dev = open("/dev/sys_V_dev",O_RDWR);
	rtn=ioctl(dev,SIMPLE_IOCTL1,key_var);

	
	if(rtn==1)
	{
		rtn=ioctl(dev,SIMPLE_IOCTL2,key_var);//new msg queue create
		if(rtn == 1)
		{
			printf("space \n");
			return key;
		}
		else
		{
		  	return -1;
		}
	}
	else if(rtn==0)
	{
		if(msgflg == KU_IPC_EXCL)
			return -1;
		else
		  	return key;
	}
	else
	{
		return -1;
	}
	close(dev);
}


int ku_msgsnd(int msqid,void *msqp,int msgsz,int msgflg)
{//return 1: snd success ,-1:full queue,-2:delete queue
  	int dev,rtn=0;
	SND_MSG sndMsg;
	SLEEP_IF slp_info;
	dev = open("/dev/sys_V_dev",O_RDWR);
	sndMsg.key=msqid;
	sndMsg.size=msgsz;
	sndMsg.msg=msqp;
	sndMsg.flag=msgflg;
	
		printf("write function\n");
		rtn=write(dev,(char *)(&sndMsg),sizeof(sndMsg));
		printf("rtbn =%d\n",rtn);
	  	if(msgflg&KU_KU_IPC_NOWAIT)
		{		
			 close(dev);	
			 if(rtn==1) 
			 {
			 	
			 	return 1;
			 }
			 else	
			 {
			 	return -1;
			 }
		}
		else
		{
			close(dev);
			if(rtn==1) return 1;
			else if(rtn==-2)return -1;
			else if(rtn==-1)
			{
				printf("write Error\n");
			}
		}
	
	if(rtn==1)
	{
		close(dev);
		return 1;
	}
	else
	{
	  	return -1;
	}
}

int ku_msgrcv(int msqid,void *msgp,int msgsz,long msgtyp,int msgflg)
{
	int dev,rtn,i=0;
	RCV_MSG rcv_data;
	void *copy_space;

	copy_space =(void *)malloc(msgsz);
	dev = open("/dev/sys_V_dev",O_RDWR);

	rcv_data.key = msqid;
	rcv_data.type = msgtyp;
	rcv_data.size = msgsz;
	rcv_data.msg_add = copy_space;
	rcv_data.flag =msgflg;


		printf("msg flag =%d\n",rcv_data.flag);
		printf("read function\n");
		rtn=read(dev,(char *)(&rcv_data),sizeof(RCV_MSG));
		printf("msg flag =%d\n",msgflg);
		// 1: success -1:No message -2:delete queue -3:cut situ
		//flag1:must 0:fail
		if(msgflg & KU_IPC_NOWAIT) 
		{
			printf("no wait\n");
			if(rtn >=0)
			{
				
				printf("copy = %d\n",*((int*)copy_space+1));
				for(i=0;i<msgsz;i++)
				{
					*((char*)msgp+i)=*((char*)copy_space+i);
				}
				close(dev);
				return msgsz;
			}
			else
			{
				close(dev);
				return -1;
			}
		}
		else//wait
		{
			if(rtn >=0)
			{
				for(i=0;i<msgsz;i++)
				{
					*((char*)msgp+i)=*((char*)copy_space+i);
				}
				close(dev);
				return msgsz;
			}
		}

		printf("rtbn =%d\n",rtn);
	
	return -1;
}

int ku_msgclose(int msqid)
{
	int dev,rtn;
	dev = open("/dev/sys_V_dev",O_RDWR);

	rtn=ioctl(dev,SIMPLE_IOCTL3,msqid);

	if(rtn)
	{
		return -1;
	}
	else
	{
		return 0;
	}

}





























