/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef request_h_include
#define request_h_include
#include <string.h>
#include <map>
#include <list>
#include <string>
#include "global.h"
#include "ksocket.h"
#include "KMutex.h"
#include "KAcserver.h"
#include "KHttpHeaderManager.h"
#include "KBuffer.h"
#include "KAutoBuffer.h"
#include "KReadWriteBuffer.h"
#include "KDomainUser.h"
#include "KSendable.h"
#include "KStringBuf.h"
#include "KHttpAuth.h"
#include "do_config.h"
#include "KContext.h"
#include "KResponseContext.h"
#include "KUrl.h"
#include "KFileName.h"
#include "KSpeedLimit.h"
#include "KTempFile.h"
#include "KInputFilter.h"
#include "kserver.h"
#include "KFlowInfo.h"
#include "KHttp2.h"
#include "kmalloc.h"
#include "kconnection.h"
#include "KHttpParser.h"
#include "KFetchObject.h"
#include "KSink.h"
#define KGL_REQUEST_POOL_SIZE 4096
#ifdef ENABLE_STAT_STUB
extern volatile uint64_t kgl_total_requests ;
extern volatile uint64_t kgl_total_accepts;
extern volatile uint64_t kgl_total_servers;
extern volatile uint32_t kgl_reading;
extern volatile uint32_t kgl_writing;
extern volatile uint32_t kgl_waiting;
#endif
#define READ_BUFF_SZ	8192

class KFetchObject;
class KSubVirtualHost;
class KFilterHelper;
class KHttpObject;
class KAccess;
class KFilterKey;
class KRequestQueue;
class KBigObjectContext;
class KSink;
class KHttpTransfer;
#define		REQUEST_EMPTY	0
#define		REQUEST_READY	1
#define 	MIN_SLEEP_TIME	4

//{{ent
class KAntiRefreshContext;
//}}

class KManageIP {
public:
	KMutex ip_lock;
	std::map<ip_addr, unsigned> ip_map;
};

class KContext;
class KOutputFilterContext;
class KHttpFilterContext;
class KHttpRequest;
typedef kev_result(*KHttpRequestWriteHook)(KHttpRequest *rq);
typedef kev_result(*request_callback)(KHttpRequest *rq, void *arg, int got);

struct kgl_write_hook {
	void *arg;
	KHttpRequestWriteHook call;
	kgl_write_hook *next;
};

typedef struct {
	void *arg;
	int got;
	result_callback result;
	kgl_write_hook *hook_head;
	kgl_write_hook *hook_last;
} kgl_request_stack;

kev_result kgl_call_write_hook(KOPAQUE data, void *arg, int got);

class KHttpRequest {
public:
	inline KHttpRequest(KSink *sink)
	{
		memset(this, 0, sizeof(*this));
		klist_init(&fo);
		ctx = new KContext;
		this->sink = sink;
	}
	~KHttpRequest();
	bool isBad();
	void set_url_param(char *param);
	//�ж��Ƿ���post���ݿɶ�
	bool has_post_data(kgl_input_stream* in);

	std::string getInfo();
	char *getUrl();
	void beginRequest();
	int EndRequest();
	uint32_t get_upstream_flags();
	int getFollowLink()
	{
		int follow_link = 0;
		if (conf.path_info) {
			follow_link|=FOLLOW_PATH_INFO;
		}
		if (KBIT_TEST(filter_flags,RF_FOLLOWLINK_OWN)) {
			follow_link|=FOLLOW_LINK_OWN;
			return follow_link;
		}
		if (KBIT_TEST(filter_flags,RF_FOLLOWLINK_ALL)) {
			follow_link|=FOLLOW_LINK_ALL;
		}
		return follow_link;
	}
	void CloseFetchObject();
	KGL_RESULT HandleResult(KGL_RESULT result);
	bool rewriteUrl(const char *newUrl, int errorCode = 0,const char *prefix = NULL);
	void EnterRequestQueue();
	void LeaveRequestQueue();
	bool NeedTempFile(bool upload);
	char *map_url_path(const char* url, KBaseRedirect *caller);
	uint32_t filter_flags;
	KSink *sink;
	//����Դ
	bool IsFetchObjectEmpty()
	{
		return klist_empty(&fo);
	}
	KFetchObject *GetFetchObject()
	{
		if (klist_empty(&fo)) {
			return NULL;
		}
		return kgl_list_data(fo.next, KFetchObject, queue);
	}
	KFetchObject *GetNextFetchObject(KFetchObject *fo)
	{
		if (fo->queue.next == &this->fo) {
			return NULL;
		}
		return kgl_list_data(fo->queue.next, KFetchObject, queue);
	}
	KSubVirtualHost* get_virtual_host();
	kgl_list fo;
	//�����ļ�ӳ��
	KFileName *file;
	KHttpTransfer *tr;
	//http��֤
	KHttpAuth *auth;
	//�й�object������������
	KContext *ctx;	
	//����������
#ifdef ENABLE_INPUT_FILTER
	bool hasInputFilter()
	{
		if (if_ctx==NULL) {
			return false;
		}
		return !if_ctx->isEmpty();
	}
	/************
	* �������
	*************/
	KInputFilterContext *if_ctx;
	KInputFilterContext *getInputFilterContext()
	{
		if (if_ctx == NULL && (sink->data.content_length>0 || sink->data.url->param)) {
			if_ctx = new KInputFilterContext(this);
		}
		return if_ctx;
	}
#endif
	/****************
	* �������
	*****************/
	KOutputFilterContext *of_ctx;
	KOutputFilterContext *getOutputFilterContext();
	void addFilter(KFilterHelper *chain);
	inline bool responseStatus(uint16_t status_code)
	{
		return sink->response_status(status_code);
	}
	inline bool responseHeader(kgl_header_type name,const char *val,hlen_t val_len)
	{
		return this->responseHeader(kgl_header_type_string[name].data, (hlen_t)kgl_header_type_string[name].len,val,val_len);
	}
	inline bool responseHeader(KHttpHeader *header)
	{
		return responseHeader(header->attr,header->attr_len,header->val,header->val_len);
	}
	inline bool responseHeader(kgl_str_t *name,kgl_str_t *val)
	{
		return responseHeader(name->data, hlen_t(name->len),val->data,hlen_t(val->len));
	}
	inline bool responseHeader(kgl_str_t *name,const char *val,hlen_t val_len)
	{
		return responseHeader(name->data, hlen_t(name->len),val, hlen_t(val_len));
	}
	inline bool responseHeader(const char *name,hlen_t name_len,int val)
	{
		char buf[16];
		int len = snprintf(buf,sizeof(buf)-1,"%d",val);
		return responseHeader(name,name_len,buf,len);
	}
	bool responseContentLength(int64_t content_length);
	//����true��һ����Ҫ��Ӧcontent-length��chunk
	inline bool responseConnection() {
		return sink->response_connection();
	}
	bool responseHeader(const char *name,hlen_t name_len,const char *val,hlen_t val_len);
	//������header��ʼ����bodyʱ����
	bool startResponseBody(INT64 body_len);
	inline bool needFilter() {
		return of_ctx!=NULL;
	}
	void ResponseVary(const char *vary);
	char *BuildVary(const char *vary);
	const char *getMethod();
	int Write(WSABUF *buf, int bc);
	int Write(const char *buf, int len);
	bool WriteAll(const char *buf, int len);
	bool WriteBuff(kbuf *buf);
	int checkFilter(KHttpObject *obj);
	//����(����)
	KSpeedLimitHelper *slh;
	void pushSpeedLimit(KSpeedLimit *sl)
	{
		KSpeedLimitHelper *helper = new KSpeedLimitHelper(sl);
		helper->next = slh;
		slh = helper;
	}
	int getSleepTime(int len)
	{
		int sleepTime = 0;
		KSpeedLimitHelper *helper = slh;
		while (helper) {
			int t = helper->sl->getSleepTime(len);
			if (t>sleepTime) {
				sleepTime = t;
			}
			helper = helper->next;
		}
		return sleepTime;
	}
	uint16_t GetSelfPort()
	{
		return sink->get_self_port();
	}
	void SetSelfPort(uint16_t port, bool ssl);
	//�ͻ���ʵip(�п��ܱ��滻)
	const char *getClientIp()
	{
		return sink->get_client_ip();
	}
	void registerRequestCleanHook(kgl_cleanup_f callBack,void *data)
	{
		assert(sink->pool);
		kgl_pool_cleanup_t *cn = kgl_pool_cleanup_add(sink->pool, 0);
		cn->data = data;
		cn->handler = callBack;
	}
	void registerConnectCleanHook(kgl_cleanup_f callBack,void *data)
	{
		kgl_pool_cleanup_t *cn = kgl_pool_cleanup_add(sink->get_connection_pool(), 0);
		cn->data = data;
		cn->handler = callBack;
	}	
	void pushFlowInfo(KFlowInfo *fi)
	{
		return sink->push_flow_info(fi);
	}
	uint8_t GetWorkModel()
	{
		return sink->get_server_model();
	}
	void InsertFetchObject(KFetchObject *fo);
	void AppendFetchObject(KFetchObject *fo);
	bool HasFinalFetchObject();
	bool NeedCheck();
#ifdef ENABLE_BIG_OBJECT_206
	//�����������
	KBigObjectContext *bo_ctx;
#endif

#ifdef ENABLE_REQUEST_QUEUE
	KRequestQueue *queue;
#endif
	//�Ӷ��Ϸ����ڴ棬��rqɾ��ʱ���Զ��ͷš�
	void *alloc_connect_memory(int size)
	{
		return kgl_pnalloc(sink->get_connection_pool(), size);
	}
	void *alloc_request_memory(int size)
	{
		return kgl_pnalloc(sink->pool, size);
	}
	const char *GetHttpValue(const char *attr)
	{
		KHttpHeader *next = sink->data.header;
		while (next) {
			if (!strcasecmp(attr, next->attr)) {
				return next->val;
			}
			next = next->next;
		}
		return NULL;
	}
	bool IsSync()
	{
		return KBIT_TEST(sink->data.flags, RQ_SYNC);
	}
	void add_sync()
	{
		sink->add_sync();
		KBIT_SET(sink->data.flags, RQ_SYNC);
	}
	void remove_sync()
	{
		sink->remove_sync();
		KBIT_CLR(sink->data.flags, RQ_SYNC);
	}
	int Read(char *buf, int len);
#ifdef ENABLE_REQUEST_QUEUE
	bool NeedQueue();
	void ReleaseQueue();
#endif
	KGL_RESULT OpenNextFetchObject(KFetchObject* fo, kgl_input_stream* in, kgl_output_stream* out, const char* queue);
};
struct RequestError
{
	int code;
	const char *msg;
	void set(int code,const char *msg)
	{
		this->code = code;
		this->msg = msg;
	}
};
inline u_short string_hash(const char *p,int len, u_short res) {
    while(*p && len>0){
        --len;
        res = res*3 + (*p);
        p++;
    }
    return res;
}
inline u_short string_hash(const char *p, u_short res) {
        int i = 8;
        while(*p && i){
                --i;
                res *= *p;
                p++;
        }
        return res;
        /*
        if (p && *p) {
                //p = p + strlen(p) - 1;
                i = 8;
                while ((p >= str) && i) {
                        i--;
                        res += *p * *p;
                        p--;
                }
        }
        return res;
        */
}
//kev_result handleStartRequest(KHttpRequest *rq, int header_length);
#endif
