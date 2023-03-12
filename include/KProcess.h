#ifndef KPROCESS_H
#define KPROCESS_H
#include "global.h"
#include "kforwin32.h"
#include "KStream.h"
#ifndef _WIN32
//{{ent
#ifdef ENABLE_ADPP
#define NORMAL_PRIORITY_CLASS 0
#define IDLE_PRIORITY_CLASS   20
#endif
//}}
#define ULONG64	              unsigned long long
#endif
/*
�ӽ�����
*/
class KProcess
{
public:
	KProcess();
	~KProcess();
	bool bind(pid_t pid)
	{
		if(this->pid){
			return false;
		}
		this->pid = pid;
		return true;
	}
	pid_t stealPid()
	{
		pid_t val = pid;
#ifndef _WIN32
		pid = 0;
#else
		pid = NULL;
#endif
		return val;
	}
	pid_t getPid()
	{
		return pid;
	}
	time_t getPowerOnTime()
	{
		return lastPoweron;
	}
#ifdef _WIN32
	bool bindProcessId(DWORD id);
#endif
	//�Ѹý��̱��浽�ļ��������������˳�ʱ�ɰ�ȫ����ɱ��
	bool saveFile(const char *dir,const char *unix_file=NULL);
//{{ent
#ifdef ENABLE_ADPP
	/*
	���� -1,downPriority
	���� 1 upPriority
	*/
	int flushCpuUsage(const KString&user, const KString&name,ULONG64 cpuTime,int cpu_limit);

	static ULONG64 getCpuTime();
	/*
		�õ��ý��̵�cpuʹ����.
	*/
	int getCpuUsage(ULONG64 cpuTime);
	int getLastCpuUsage()
	{
		return cpu_usage;
	}
	int getPriority()
	{
		return priority;
	}
	/*
	�µ����ȼ�
	*/
	bool downPriority();
	/*
	�ϵ����ȼ�
	*/
	bool upPriority();
	static ULONG64     lastQueryTime;
	ULONG64     lastUsedTime;
#endif
//}}
	bool isKilled()
	{
		return killed;
	}
	bool isActive();
	int getProcessId();
	bool kill();
	void detach()
	{
		killed = true;
	}
	int sig;
private:
	void cleanFile();
//{{ent
#ifdef ENABLE_ADPP
	int priority;
	int cpu_usage;
	bool setPriority(int priority);
#endif
//}}
private:
	pid_t pid;
	bool killed;
	time_t lastPoweron;
	char *file;
};
#endif
