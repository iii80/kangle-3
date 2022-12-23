#include "KCompressStream.h"
#include "KHttpRequest.h"
#include "KHttpObject.h"
#include "KGzip.h"
#ifdef ENABLE_BROTLI
#include "KBrotli.h"
#endif

KCompressStream *create_compress_stream(KHttpRequest *rq, KHttpObject *obj, int64_t content_len)
{
	if (content_len >= 0 && content_len < conf.min_compress_length) {
		//̫��
		return NULL;
	}
	if (KBIT_TEST(rq->sink->data.flags,RQ_CONNECTION_UPGRADE)) {
		//͸��ģʽ��ѹ��
		return NULL;
	}
	//���obj���Ϊ�Ѿ�ѹ���������߱���˲���ѹ������ѹ������
	if (obj->IsContentEncoding()) {
		return NULL;
	}
	//obj�ж������,��ѹ��
	if (obj->refs > 1) {
		return NULL;
	}
	//status_code��206����ʾ�ǲ�������ʱҲ��ѹ��,������200��Ӧ��������url ranged����
	//ע���������û�о�����ϸ��֤
	if (obj->data->i.status_code == STATUS_CONTENT_PARTIAL
		|| KBIT_TEST(rq->sink->data.raw_url->flags, KGL_URL_RANGED)) {
		return NULL;
	}
	if (KBIT_TEST(obj->index.flags, FLAG_DEAD) && conf.only_compress_cache == 1) {
		return NULL;
	}
#ifdef ENABLE_BROTLI
	if (obj->need_compress && conf.br_level>0 && KBIT_TEST(rq->sink->data.raw_url->accept_encoding, KGL_ENCODING_BR)) {
		obj->AddContentEncoding(KGL_ENCODING_BR, kgl_expand_string("br"));
		return new KBrotliCompress();
	}
#endif
	//�ͻ���֧��gzipѹ����ʽ
	if (obj->need_compress && conf.gzip_level>0 && KBIT_TEST(rq->sink->data.raw_url->accept_encoding, KGL_ENCODING_GZIP)) {
		obj->AddContentEncoding(KGL_ENCODING_GZIP, kgl_expand_string("gzip"));
		return new KGzipCompress(conf.gzip_level);
	}
	return NULL;
}