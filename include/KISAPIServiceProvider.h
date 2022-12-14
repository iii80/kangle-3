/*
 * KISAPIServiceProvider.h
 *
 *  Created on: 2010-8-7
 *      Author: keengo
 */

#ifndef KISAPISERVICEPROVIDER_H_
#define KISAPISERVICEPROVIDER_H_
#include <sstream>
#include <vector>
//#include "KApiFetchObject.h"
#include "KServiceProvider.h"
#include "khttpext.h"
#include "KHttpKeyValue.h"

#include "KStream.h"
#include "KStringBuf.h"

class KISAPIServiceProvider: public KServiceProvider, public KStream {
public:
	KISAPIServiceProvider();
	virtual ~KISAPIServiceProvider();
	Token_t getToken() override {
		Token_t token;
		if (pECB->ServerSupportFunction(pECB->ConnID,
				HSE_REQ_GET_IMPERSONATION_TOKEN, (LPVOID) &token, NULL, NULL)
				== TRUE) {
			return token;
		}
		return NULL;
	}
	KWStream *getOutputStream() override{
		return this;
	}
	KRStream *getInputStream() override{
		return this;
	}
	bool execUrl(const char *url) override;
	void log(const char *str) override;
	int64_t get_left() override
	{
		return pECB->cbLeft;
	}
	int read(char *buf, int len) override;
	int write(const char *buf, int len) override;
	StreamState write_end(KGL_RESULT result) override;
	char getMethod() override {
		if (meth != METH_UNSET) {
			return meth;
		}
		meth = KHttpKeyValue::get_method(pECB->lpszMethod, (int)strlen(pECB->lpszMethod));
		return meth;
	}
	const char *getFileName() override{
		return pECB->lpszPathTranslated;
	}
	const char *getQueryString() override{
		return pECB->lpszQueryString;
	}
	const char *getRequestUri() override{
		return pECB->lpszPathInfo;
	}
	INT64 getContentLength() override {
		return pECB->cbTotalBytes;
	}
	const char *getContentType() override{
		return pECB->lpszContentType;
	}	
	char *getHttpHeader(const char *attr) override{
		std::stringstream s;
		s << "HTTP_";
		while (*attr) {
			if (*attr == '-') {
				s << "_";
			} else {
				s << (char) toupper(*attr);
			}
			attr++;
		}
		int len = sizeof(header_val);
		if (getEnv(s.str().c_str(), header_val, &len)) {
			return header_val;
		}
		return NULL;
	}
	bool isHeadSend() override{
		return headSended;
	}
	bool sendKnowHeader(int attr, const char *val) override{
		return true;
	}
	bool sendUnknowHeader(const char *attr, const char *val) override;
	bool sendStatus(int statusCode, const char *statusLine) override;
	bool getEnv(const char *attr, char *val, int *len) override;

	//bool getServerVar(const char *name, char *value, int len);
	void setECB(EXTENSION_CONTROL_BLOCK *pECB) {
		this->pECB = pECB;
	}
	EXTENSION_CONTROL_BLOCK* pECB;
private:
	char header_val[512];
	void checkHeaderSend();
	char meth;
	char *docRoot;
	bool headSended;	
	KStringBuf s;
	KStringBuf status;
};

#endif /* KISAPISERVICEPROVIDER_H_ */
