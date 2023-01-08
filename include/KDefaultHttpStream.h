#ifndef KDEFAULTHTTPSTREAM_H
#define KDEFAULTHTTPSTREAM_H
#include "KHttpStream.h"
#include "KHttpRequest.h"

class KDefaultHttpStream : public KWriteStream {
public:
	bool support_sendfile(void *arg) override {
		return ((KHttpRequest*)arg)->sink->support_sendfile();
	}
	KGL_RESULT sendfile(void* rq, kfiber_file* fp, int64_t *len) override {
		return ((KHttpRequest*)rq)->sendfile(fp, len);
	}
	KGL_RESULT write_end(void* arg, KGL_RESULT result) override
	{
		KHttpRequest* rq = ((KHttpRequest*)arg);		
		if (result != KGL_OK) {
			KHttpObject* obj = rq->ctx->obj;
			KBIT_SET(rq->sink->data.flags, RQ_CONNECTION_CLOSE | RQ_BODY_NOT_COMPLETE);
			//�����http2��http3������£��˴�Ҫ�����δ��ݴ��󣬵���terminal stream
			//�������λ��󻺴�
		}
		return result;
	}
protected:
	int write(void *rq, const char *buf, int len) override
	{
		return ((KHttpRequest *)rq)->Write(buf, len);
	}
};
#endif
