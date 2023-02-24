/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#ifndef KVIRTUALHOST_H
#define KVIRTUALHOST_H
/**
 * virtual host
 */
#include <time.h>
#include <string>
#include <map>
#include "global.h"
#include "KLogElement.h"
//#include "KLocalFetchObject.h"
#include "KFileName.h"
#include "KCountable.h"
#include "KVirtualHost.h"
#include "KSubVirtualHost.h"
#include "KPathRedirect.h"
#include "KAccess.h"
#include "KUserManageInterface.h"
#include "utils.h"
#include "KBaseVirtualHost.h"
#include "KRequestQueue.h"
#include "extern.h"
#include "KFlowInfo.h"
#include "KConnectionLimit.h"
#include "KXmlAttribute.h"
//��vh�����ã������������Ĳ���
#define VH_REFS_CONNECT_DELTA 1
class KTempleteVirtualHost;
/**
* ���Ը�����
*/
class KAttributeHelper
{
public:
	KAttributeHelper(KXmlAttribute &attr): attribute(attr)
	{		
	}
	KXmlAttribute &getAttribute()
	{
		return attribute;
	}
	bool getValue(const char *name,std::string &value) const 
	{
		auto it = attribute.find(name);
		if(it==attribute.end()){
			return false;
		}
		value = (*it).second;
		return true;
	}
	void setValue(const char *name,const char *value)
	{
		attribute.emplace(name, value);
	}
private:
	KXmlAttribute &attribute;
};
/**
* ����������
*/
class KVirtualHost: public KBaseVirtualHost,public KSslConfig, public KCountable {
public:
	KVirtualHost();
	virtual ~KVirtualHost();
	bool setDocRoot(const std::string &docRoot);
	KSubVirtualHost *getFirstSubVirtualHost() {
		if (hosts.size() == 0) {
			return NULL;
		}
		return *(hosts.begin());
	}
	virtual bool isTemplete()
	{
		return false;
	}
	friend class KHttpRequest;
	friend class KNsVirtualHost;
	friend class KVirtualHostManage;
	union {
		struct {
			uint16_t closed : 1;
			uint16_t browse : 1;
			//ͳ������
			uint16_t fflow : 1;
			uint16_t ip_hash:1;
			uint16_t inherit:1;
			uint16_t ext:1;
			uint16_t db:1;
#ifdef ENABLE_VH_RUN_AS
			uint16_t need_kill_process : 1;
#ifdef _WIN32
			uint16_t logoned:1;
			uint16_t logonresult:1;
#endif
#endif
#ifndef _WIN32
			uint16_t chroot:1;
#endif
#ifdef ENABLE_USER_ACCESS
			uint16_t access_file_loaded:1;
#endif
			uint16_t app_share:1;
			/* Ӧ�ó�������� */
			uint8_t app;
		};
		uint32_t flags;
	};
	/*
	 * ģ��
	 */
	KTempleteVirtualHost *tvh;
#ifdef ENABLE_USER_ACCESS	
	int checkRequest(KHttpRequest *rq, KFetchObject **fo);
	int checkResponse(KHttpRequest *rq, KFetchObject** fo);
	int checkPostMap(KHttpRequest *rq, KFetchObject** fo);
	std::string user_access;
#endif
	std::string doc_root;
	std::string GetDocumentRoot()
	{
		std::string orig_doc_root;
		if (strncasecmp(doc_root.c_str(), conf.path.c_str(), conf.path.size()) == 0) {
			orig_doc_root =  doc_root.substr(conf.path.size());
		} else {
			orig_doc_root = doc_root;
		}
		strip_path_end(orig_doc_root);
		return orig_doc_root;
	}
#ifdef ENABLE_VH_LOG_FILE
	KLogElement *logger;
	std::string logFile;
	void setLogFile(KAttributeHelper *ah,KVirtualHost *tm=NULL);
#endif
#ifdef ENABLE_VH_RUN_AS
	/*
	 * �����Ƿ���Ҫɱ����Ӧ�Ľ���,����true��Ҫɱ�����̣�����ɱ������
	 */
	bool caculateNeedKillProcess(KVirtualHost *ov);
	void KillAllProcess();
#endif
#ifdef ENABLE_VH_QUEUE
	void initQueue(KVirtualHost *ov)
	{
		if(queue){
			return;
		}
		if(max_worker>0){
			if(ov){
				queue = ov->queue;
			}
			if(queue){
				queue->addRef();
			}else{
				queue = new KRequestQueue;
			}
			queue->set(max_worker,max_queue);
		}
	}
	unsigned getWorkerCount();
	unsigned getQueueSize();
	KRequestQueue *queue;
	unsigned max_worker;
	unsigned max_queue;
#endif
#ifdef ENABLE_VH_RS_LIMIT
	KSpeedLimit *refsSpeedLimit()
	{
		KSpeedLimit *sl = NULL;
		lock.Lock();
		sl = this->sl;
		if (sl) {
			sl->addRef();
		}
		lock.Unlock();
		return sl;
	}
	void SetStatus(int status)
	{
		closed = (status != 0);
	}
	void setSpeedLimit(const char *speed_limit_str,KVirtualHost *ov);
	void setSpeedLimit(int speed_limit,KVirtualHost *ov);
	int GetConnectionCount();
	bool addConnection(KHttpRequest *rq) {
		if(cur_connect==NULL || max_connect==0){
			return true;
		}
		return cur_connect->addConnection(rq,max_connect);
	}
	void initConnect(KVirtualHost *ov)
	{
		if(cur_connect){
			return;
		}
		if(max_connect>0){
			if(ov){
				cur_connect = ov->cur_connect;
			}
			if(cur_connect){
				cur_connect->addRef();
			}else{
				cur_connect = new KConnectionLimit;
			}
		}
	}
	//��ǰ��������Ϣ
	KConnectionLimit *cur_connect;
	//����������
	int max_connect;
	//��������
	int speed_limit;
#endif
#ifdef ENABLE_VH_FLOW
	KFlowInfo *refsFlowInfo()
	{
		KFlowInfo *flow = NULL;
		lock.Lock();
		flow = this->flow;
		if (flow) {
			flow->addRef();
		}
		lock.Unlock();
		return flow;
	}
	void setFlow(bool fflow,KVirtualHost *ov)
	{
		lock.Lock();
		if (flow) {
			flow->release();
			flow = NULL;
		}
		this->fflow = fflow;
		if (fflow) {
			if (ov) {
				flow = ov->flow;
			}
			if (flow) {
				flow->addRef();
			} else {
				flow = new KFlowInfo;
			}
		}
		lock.Unlock();
	}
	INT64 get_speed(bool reset)
	{
		if (flow) {
			return flow->get_speed(reset);
		}
		return 0;
	}

#endif
	//����webdav��Ӧ��У��
	KBaseRedirect* refsPathRedirect(const char* path, int path_len);
	KFetchObject *findPathRedirect(KHttpRequest *rq, KFileName *file,const char *path,
			bool fileExsit,bool &result);
	/*
	 check if a file will map to the rd
	 */
	bool isPathRedirect(KHttpRequest *rq, KFileName *file, bool fileExsit,
			KRedirect *rd);
	KFetchObject *findFileExtRedirect(KHttpRequest *rq, KFileName *file,
			bool fileExsit,bool &result);
	KFetchObject *findDefaultRedirect(KHttpRequest *rq,KFileName *file,bool fileExsit);
	std::string name;

#ifdef ENABLE_BASED_PORT_VH
	std::list<std::string> binds;
#endif
	bool empty()
	{
		if (!hosts.empty()) {
			return false;
		}
		return true;
	}
	std::list<KSubVirtualHost *> hosts;
#ifdef ENABLE_VH_RUN_AS
	//std::string add_dir;
	bool setRunAs(const std::string &user, const std::string &group);


	int id[2];
	/*
	 * run user
	 */
	std::string user;
	/*
	 * run group
	 */
	std::string group;
	std::string getUser()
	{
		return user;
	}
#ifdef _WIN32
	HANDLE logon(bool &result);
	Token_t getLogonedToken(bool &result)
	{
		result = logoned;
		return token;
	}
#endif
	Token_t createToken(bool &result);
	Token_t getProcessToken(bool &result);
	static void createToken(Token_t token);	
#else
	/*
	���ڲ�֧���������������û�ʱ����һ��ȫ���û���
	*/
	std::string getUser() {
		return "-";
	}
#endif
	std::vector<std::string> apps;
	void setApp(int app);
	std::string getApp(KHttpRequest *rq);
	static void closeToken(Token_t token);
	virtual void buildXML(std::stringstream &s);	
	bool loadApiRedirect(KApiPipeStream *st,int workType);
	bool saveAccess();
	void setAccess(const std::string &access_file);
	std::string htaccess;
	KAccess access[2];
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	kgl_ssl_ctx *ssl_ctx;
	bool setSSLInfo(const std::string &certfile, const std::string &keyfile, const std::string &cipher,const std::string &protocols);
	std::string get_cert_file() override;
	std::string get_key_file() override;
	kgl_ssl_ctx* refs_ssl_ctx() override {
		if (ssl_ctx == nullptr) {
			this->ssl_ctx = KSslConfig::refs_ssl_ctx();
		}
		if (ssl_ctx == nullptr) {
			return nullptr;
		}
		kgl_add_ref_ssl_ctx(ssl_ctx);
		return ssl_ctx;
	}
#endif
private:
#ifdef ENABLE_VH_RS_LIMIT
	//��ǰ������Ϣ
	KSpeedLimit *sl;
#endif
#ifdef ENABLE_VH_FLOW
	//������
	KFlowInfo *flow;
#endif
	bool loadApiRedirect(KRedirect *rd,KApiPipeStream *st,int workType);
#ifdef ENABLE_VH_RUN_AS
#ifdef _WIN32
	HANDLE token;
#endif
#endif
public:
#ifdef ENABLE_USER_ACCESS
	bool loadAccess(KVirtualHost *vh=NULL);
#endif
};


#endif /*KVIRTUALHOST_H_*/
