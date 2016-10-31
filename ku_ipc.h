
#define KUIPC_MAXMSG 20
#define KUIPC_MAXVOL 40
#define KU_IPC_CREAT 0
#define KU_IPC_EXCL 1
#define KU_IPC_NOWAIT 1
#define MSG_NOERROR 2
#define DEV_NAME "sys_V_dev"
#define IOCTL_START_NUM 0x80
#define IOCTL_NUM1 IOCTL_START_NUM+1
#define IOCTL_NUM2 IOCTL_START_NUM+2
#define IOCTL_NUM3 IOCTL_START_NUM+3
#define SIMPLE_IOCTL_NUM 'z'
#define SIMPLE_IOCTL1 _IOWR(SIMPLE_IOCTL_NUM,IOCTL_NUM1,unsigned long*)
#define SIMPLE_IOCTL2 _IOWR(SIMPLE_IOCTL_NUM,IOCTL_NUM2,unsigned long*)
#define SIMPLE_IOCTL3 _IOWR(SIMPLE_IOCTL_NUM,IOCTL_NUM3,unsigned long*)


typedef struct sleep_info
{
	int key;
	int id;
	int size;
}SLEEP_IF;



typedef struct snd_msg
{
	int key;
	int size;
	int flag;
	void *msg;
}SND_MSG;

typedef struct rcv_msg
{
	int key;
	int size;
	int flag;
	long type;
	void *msg_add;
	void *msg;
}RCV_MSG;
