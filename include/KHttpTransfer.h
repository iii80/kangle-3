/*
 * KHttpTransfer.h
 *
 *  Created on: 2010-5-4
 *      Author: keengo
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

#ifndef KHTTPTRANSFER_H_
#define KHTTPTRANSFER_H_
#include "KHttpRequest.h"
#include "KHttpObject.h"
#include "KCompressStream.h"
#include "KSendable.h"
#include "KChunked.h"
#include "KCacheStream.h"
#ifdef ENABLE_DELTA_ENCODE
#include "KDeltaEncode.h"
#endif
/*
* @deprecated
 * This class use to transfer data to client
 * It support compress(use gzip) and chunk transfer encoding.
 
 ��������:
 ����Դ(localfetch) -->   <--����Դ(pushģʽ)
 chunked����(�л���û��)-->
 degzip(���ݽ�ѹ��,��������ݹ��˱任)-->
 (KHttpTransfer::write_all������Ҳ����httpͷ)
 ���ݹ���/�任 KHttpRequest��checkFilter -->
 gzipѹ��-->
 ��������-->
 chunked����-->
 socket-->
 write hook
 */
class KHttpTransfer: public KHttpStream {
public:
	KHttpTransfer(KHttpRequest *rq, KHttpObject *obj);
	KHttpTransfer();
	~KHttpTransfer();
	void init(KHttpRequest *rq, KHttpObject *obj);
	bool support_sendfile(void* rq) override;
	KGL_RESULT sendfile(void* rq, kfiber_file* fp, int64_t *len) override;
	/*
	 * send actual data to client. cann't head data or chunked head
	 */
	StreamState write_all(void*rq, const char *str, int len) override;
	/*
	 * д���������������û�з���(���ܴ���buffer��)����Ҫ�����������ݡ�
	 */
	StreamState write_end(void*rq, KGL_RESULT result) override;
	StreamState sendHead(KHttpRequest *rq, bool isEnd);
	friend class KDeChunked;
	friend class KGzip;
public:
	KHttpObject *obj;
private:
	bool loadStream(KHttpRequest *rq, KCompressStream *compress_st, cache_model cache_layer);
	//bool isHeadSend;

};

#endif /* KHTTPTRANSFER_H_ */
