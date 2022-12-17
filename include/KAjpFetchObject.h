/*
 * KAjpFetchObject.h
 * implement ajp/1.3 protocol
 *  Created on: 2010-7-31
 *      Author: keengo
 */

#ifndef KAJPFETCHOBJECT_H_
#define KAJPFETCHOBJECT_H_
#include "global.h"
#include "KAsyncFetchObject.h"
#include "KAcserver.h"
#include "KFileName.h"
#include "KAjpMessage.h"

class KAjpFetchObject : public KAsyncFetchObject
{
public:
	KAjpFetchObject();
	virtual ~KAjpFetchObject();
protected:
	//��������ͷ��buffer�С�
	KGL_RESULT buildHead(KHttpRequest* rq) override;
	//����head
	kgl_parse_result ParseHeader(KHttpRequest* rq, char** data, char* end) override;
	//����post���ݵ�buffer�С�
	void buildPost(KHttpRequest* rq) override;
	//����body
	KGL_RESULT ParseBody(KHttpRequest* rq, char** data, char* end) override;
	void BuildPostEnd();
	bool checkContinueReadBody(KHttpRequest* rq) override
	{
		return !bodyEnd;
	}
private:
	void ReadBodyEnd(KHttpRequest* rq) {
		bodyEnd = true;
		expectDone(rq);
	}
	bool parse(char** str, char *end, KAjpMessageParser* msg);
	unsigned char parseMessage(KHttpRequest* rq, KHttpObject* obj, KAjpMessageParser* msg);
	int body_len;
	bool bodyEnd;
};

#endif /* KAJPFETCHOBJECT_H_ */
