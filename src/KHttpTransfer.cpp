/*
 * KHttpTransfer.cpp
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

#include "http.h"
#include "KHttpTransfer.h"
#include "kmalloc.h"
#include "KCacheStream.h"
#include "KGzip.h"
#include "KContentTransfer.h"
#include "kselector.h"
#include "KFilterContext.h"
#include "KHttpFilterManage.h"
#include "KDefaultHttpStream.h"
//{{ent
#include "KBigObjectContext.h"
#include "KBigObjectStream.h"
//}}
#include "cache.h"


KHttpTransfer::KHttpTransfer(KHttpRequest *rq, KHttpObject *obj) {
	init(rq, obj);
}
KHttpTransfer::KHttpTransfer() {
	init(NULL, NULL);
}
void KHttpTransfer::init(KHttpRequest *rq, KHttpObject *obj) {
	this->obj = obj;
}
KHttpTransfer::~KHttpTransfer() {
	
}

StreamState KHttpTransfer::write_all(void *arg, const char *str, int len) {
	auto rq = (KHttpRequest*)arg;
	if (len<=0) {
		return STREAM_WRITE_SUCCESS;
	}
	if (st==NULL) {
		if (sendHead(rq, false) != STREAM_WRITE_SUCCESS) {
			KBIT_SET(rq->sink->data.flags,RQ_CONNECTION_CLOSE);
			return STREAM_WRITE_FAILED;
		}
	}
	if (st == NULL) {
		return STREAM_WRITE_FAILED;
	}
	return st->write_all(rq, str, len);
}
StreamState KHttpTransfer::write_end(void *arg, KGL_RESULT result) {
	auto rq = (KHttpRequest*)arg;
	if (st == NULL) {
		if (sendHead(rq, true) != STREAM_WRITE_SUCCESS) {
			KBIT_SET(rq->sink->data.flags,RQ_CONNECTION_CLOSE);
			return KHttpStream::write_end(rq, STREAM_WRITE_FAILED);
		}
	}
	return KHttpStream::write_end(rq, result);
}
bool KHttpTransfer::support_sendfile()
{
	return false;
}
StreamState KHttpTransfer::sendHead(KHttpRequest *rq, bool isEnd) {
	INT64 start = 0;
	INT64 send_len = 0;
	StreamState result = STREAM_WRITE_SUCCESS;
	INT64 content_len = (isEnd?0:-1);
	cache_model cache_layer = cache_memory;
	kassert(!KBIT_TEST(rq->sink->data.flags, RQ_HAVE_RANGE) || rq->ctx->obj->data->i.status_code==STATUS_CONTENT_PARTIAL);
	if (KBIT_TEST(obj->index.flags,ANSW_HAS_CONTENT_LENGTH)) {
		content_len = obj->index.content_length;
#ifdef ENABLE_DISK_CACHE
		if (content_len > conf.max_cache_size) {
			if (content_len > conf.max_bigobj_size
				|| !obj_can_disk_cache(rq, obj)) {
				cache_layer = cache_none;
			} else {
				cache_layer = cache_disk;
			}
		}
#else
		cache_layer = cache_none;
#endif
	}
	if (rq->needFilter()) {
		/*
		 ����Ƿ���Ҫ�������ݱ任�㣬���Ҫ���򳤶�δ֪
		 */
		content_len = -1;
		cache_layer = cache_memory;
	}
	KCompressStream *compress_st = NULL;
	//bool gzip_layer = false;
	if (unlikely(rq->ctx->internal && !rq->ctx->replace)) {
		/* ������,�ڲ��������滻ģʽ */
	} else {
		compress_st = create_compress_stream(rq, obj, content_len);
		if (compress_st) {
			KBIT_SET(rq->sink->data.flags, RQ_TE_COMPRESS);
			content_len = -1;
			cache_layer = cache_memory;
			//obj->insertHttpHeader(kgl_expand_string("Content-Encoding"), kgl_expand_string("gzip"));
			//obj->uk.url->set_content_encoding(compress_st->GetEncoding());
		}
#ifdef WORK_MODEL_TCP
		//�˿�ӳ�䲻����httpͷ
		if (!KBIT_TEST(rq->GetWorkModel(), WORK_MODEL_TCP))
#endif
		build_obj_header(rq, obj, content_len, start, send_len);
#ifndef NDEBUG
		if (KBIT_TEST(rq->sink->data.flags, RQ_TE_COMPRESS)) {
			assert(compress_st);
		} else {
			assert(compress_st == NULL);
		}
#endif
	}
	if (obj->in_cache) {
		cache_layer = cache_none;
	}
#if 0
	else if (KBIT_TEST(rq->filter_flags,RF_ALWAYS_ONLINE) && status_code_can_cache(obj->data->status_code)) {
		//��������ģʽ
#if 0
		if (KBIT_TEST(obj->index.flags, ANSW_NO_CACHE)) {
			//����Ƕ�̬��ҳ��������Cookie(�ο�ģʽ)
			if (rq->parser.findHttpHeader(kgl_expand_string("Cookie")) == NULL
				&& rq->parser.findHttpHeader(kgl_expand_string("Cookie2")) == NULL
				&& obj->findHeader(kgl_expand_string("Set-Cookie")) == NULL
				&& obj->findHeader(kgl_expand_string("Set-Cookie2")) == NULL) {
				KBIT_SET(obj->index.flags, OBJ_MUST_REVALIDATE);
				KBIT_CLR(obj->index.flags, ANSW_NO_CACHE | FLAG_DEAD);
			}
		}
#endif
	}
#endif
	loadStream(rq, compress_st,cache_layer);
	return result;
}
bool KHttpTransfer::loadStream(KHttpRequest *rq, KCompressStream *compress_st, cache_model cache_layer) {

	/*
	 ���µ��Ͽ�ʼ����
	 �����������ٷ���.
	 */
	assert(st==NULL && rq);
#if 0
#ifdef ENABLE_TF_EXCHANGE
	if (rq->tf) {
		st = rq->tf;
		autoDelete = false;
	} else {
#endif		

#ifdef ENABLE_TF_EXCHANGE
	}
#endif
#endif
	st = new KDefaultHttpStream();
	autoDelete = true;
#if 0
	/*
	 ����Ƿ���chunk��
	 */
	if ((!rq->ctx->internal || rq->ctx->replace) && KBIT_TEST(rq->sink->data.flags, RQ_TE_CHUNKED)) {
	//if (!(KBIT_TEST(workModel,WORK_MODEL_INTERNAL|WORK_MODEL_REPLACE)==WORK_MODEL_INTERNAL) && KBIT_TEST(rq->sink->data.flags,RQ_TE_CHUNKED)) {
		KWriteStream *st2 = new KChunked(st, autoDelete);
		if (st2) {
			st = st2;
			autoDelete = true;
		} else {
			return false;
		}
	}
#endif
	//����Ƿ����cache��
	
	if (KBIT_TEST(obj->index.flags,ANSW_NO_CACHE)==0) {
		if (cache_layer != cache_none) {
			KCacheStream *st_cache = new KCacheStream(st, autoDelete);
			if (st_cache) {
				st_cache->init(rq,obj,cache_layer);
				st = st_cache;
				autoDelete = true;
			}
		}
	}
	//����Ƿ�Ҫ����gzipѹ����
	if (compress_st) {
		compress_st->connect(st, autoDelete);
		//KGzipCompress *st_gzip = new KGzipCompress(st, autoDelete);
		//debug("����gzipѹ����=%p,up=%p\n",st_gzip,st);
		//if (st_gzip) {
		st = compress_st;
		autoDelete = true;
		//} else {
		//	return false;
		//}
	}
	//���ݱ任��
	if ((!rq->ctx->internal || rq->ctx->replace) && rq->needFilter()) {
	//if (!(KBIT_TEST(workModel,WORK_MODEL_INTERNAL|WORK_MODEL_REPLACE)==WORK_MODEL_INTERNAL) && rq->needFilter()) {
		//debug("�������ݱ任��\n");
		if (rq->of_ctx->head) {
			KContentTransfer *st_content = new KContentTransfer(st, autoDelete);
			if (st_content) {
				st = st_content;
				autoDelete = true;
			} else {
				return false;
			}
		}
		KWriteStream *filter_st_head = rq->of_ctx->getFilterStreamHead();
		if (filter_st_head) {				
			KHttpStream *filter_st_end = rq->of_ctx->getFilterStreamEnd();
			assert(filter_st_end);
			if (filter_st_end) {
				filter_st_end->connect(st,autoDelete);
				//st ��rq->of_ctx��������autoDeleteΪfalse
				autoDelete = false;
				st = filter_st_head;
			}
		}		
	}
	//�������
	return true;
}
