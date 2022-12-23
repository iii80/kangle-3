#include <string.h>
#include <stdlib.h>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#endif
#include <errno.h>
#include "KDiskCache.h"
#include "do_config.h"
#include "utils.h"
#include "cache.h"
#include "KHttpObject.h"
#include "KHttpObjectHash.h"
#include "KCache.h"
#ifdef ENABLE_DB_DISK_INDEX
#include "KDiskCacheIndex.h"
#endif
#include "kforwin32.h"
#include "kmalloc.h"
KMutex obj_lock[HASH_SIZE+2];
static volatile uint32_t disk_file_index = (rand() & CACHE_DIR_MASK2);
volatile uint32_t disk_file_base = (uint32_t)time(NULL);
static KMutex indexLock;
KHttpObjectBody::KHttpObjectBody(KHttpObjectBody *data)
{
	memset(this, 0, sizeof(KHttpObjectBody));
	kgl_safe_copy_body_data(&i, &data->i);
	KHttpHeader *hot = headers;
	KHttpHeader *tmp = data->headers;
	while (tmp) {
		KHttpHeader *new_t = (KHttpHeader *)xmalloc(sizeof(KHttpHeader));
		new_t->attr = xstrdup(tmp->attr);
		if(tmp->val){
			new_t->val = xstrdup(tmp->val);
		}else{
			new_t->val = NULL;
		}
		new_t->attr_len = tmp->attr_len;
		new_t->val_len = tmp->val_len;
		new_t->next = NULL;
		if(hot==NULL){
			headers = new_t;
		}else{
			hot->next = new_t;
		}
		hot = new_t;
		tmp = tmp->next;
	}
}
void KHttpObject::ResponseVaryHeader(KHttpRequest *rq)
{
	if (uk.vary == NULL) {
		return;
	}
	KMutex *lock = getLock();
	lock->Lock();
	if (uk.vary->key == NULL) {
		lock->Unlock();
		return;
	}
	rq->ResponseVary(uk.vary->key);
#if 0
	int key_len = 0;
	char *hot = uk.vary->key;
	while (*hot && *hot != KGL_VARY_EXTEND_CHAR) {
		key_len++;
		hot++;
	}
	if (key_len > 0) {
		rq->responseHeader(kgl_expand_string("Vary"), uk.vary->key, (hlen_t)key_len);
	}
#endif
	lock->Unlock();
}
bool KHttpObject::AddVary(KHttpRequest *rq,const char *val,int val_len)
{
	KBIT_SET(index.flags, OBJ_HAS_VARY);
	if (uk.vary == NULL) {
		char *vary_val = rq->BuildVary(val);
		if (vary_val != NULL) {
			uk.vary = new KVary;
			uk.vary->val = vary_val;
			uk.vary->key = strdup(val);
			return true;
		}
	}
	return false;
}
char *KHttpObject::BuildVary(KHttpRequest *rq)
{
	if (uk.vary == NULL) {
		return NULL;
	}
	KMutex *lock = getLock();
	lock->Lock();
	if (uk.vary->key == NULL) {
		lock->Unlock();
		return NULL;
	}
	char *vary = rq->BuildVary(uk.vary->key);
	lock->Unlock();
	return vary;
}
KHttpObject::KHttpObject(KHttpRequest *rq,KHttpObject *obj)
{
	init(rq->sink->data.url);
	uk.url->encoding = rq->sink->data.raw_url->encoding;
	index.flags = obj->index.flags;
	KBIT_CLR(index.flags,FLAG_IN_DISK|OBJ_IS_READY|OBJ_IS_STATIC2|FLAG_NO_BODY|ANSW_HAS_CONTENT_LENGTH|ANSW_HAS_CONTENT_RANGE);
	KBIT_SET(index.flags,FLAG_IN_MEM);
	index.last_verified = obj->index.last_verified;
	data = new KHttpObjectBody(obj->data);
}
KHttpObject::~KHttpObject() {
#ifdef ENABLE_DISK_CACHE
	unlinkDiskFile();
#endif
	if (data) {
		delete data;
	}
	if (uk.url) {
		uk.url->relase();
	}
	if (uk.vary) {
		delete uk.vary;
	}
}
void KHttpObject::Dead()
{
	KBIT_SET(index.flags, FLAG_DEAD);
	dc_index_update = 1;
}
void KHttpObject::UpdateCache(KHttpObject *obj)
{
	kassert(!obj->in_cache);
	kassert(in_cache);
	if (cmp_vary_key(uk.vary, obj->uk.vary)!=0) {
		cache.UpdateVary(this, obj->uk.vary);
	}
	//TODO:����vary,��max_age�Ȼ������	
	return;
}
int KHttpObject::GetHeaderSize(int url_len)
{
	if (index.head_size > 0) {
		return index.head_size;
	}
	int len = sizeof(KHttpObjectFileHeader);
	if (url_len == 0) {
		char *u = get_obj_url_key(this,&url_len);
		if (u == NULL) {
			return 0;
		}
		free(u);
	}
	len += url_len + sizeof(int);
	KHttpHeader *header = data->headers;
	while (header) {
		len += header->attr_len + header->val_len + 2 * sizeof(int);
		header = header->next;
	}
	len += sizeof(int);
	index.head_size = kgl_align(len, kgl_aio_align_size);
	return index.head_size;
}
#ifdef ENABLE_DISK_CACHE
void KHttpObjectBody::create_type(HttpObjectIndex *index)
{
	//{{ent
#ifdef ENABLE_BIG_OBJECT_206
	if (KBIT_TEST(index->flags, FLAG_BIG_OBJECT_PROGRESS)) {
		this->i.type = BIG_OBJECT_PROGRESS;
		this->sbo = new KSharedBigObject;
		//this->sbo->setBodyStart(index->head_size);
		return;
	}
#endif//}}
	if (index->content_length >= conf.max_cache_size) {
		this->i.type = BIG_OBJECT;
		return;
	}
	this->i.type = MEMORY_OBJECT;
}
bool KHttpObjectBody::restore_header(KHttpObject *obj, char *buffer, int len)
{
	KHttpObjectFileHeader *fileHeader = (KHttpObjectFileHeader *)buffer;
	if (!is_valide_dc_sign(fileHeader)) {
		return false;
	}
	if (len != (int)fileHeader->dbi.index.head_size) {
		//head_size����
		return false;
	}
	int hotlen = len - sizeof(KHttpObjectFileHeader);
	char *hot = (char *)(fileHeader + 1);
	//skip url
	skipString(&hot, hotlen);
	kgl_safe_copy_body_data(&this->i, &fileHeader->body);	
	if (this->i.status_code == 0) {
		this->i.status_code = STATUS_OK;
	}
	return read_obj_head(this, &hot, hotlen);
}
void KHttpObject::unlinkDiskFile()
{
	if (KBIT_TEST(index.flags,FLAG_IN_DISK)) {
#ifdef ENABLE_DB_DISK_INDEX
		if (dci) {
			dci->start(ci_del,this);
		}
#endif
		char *name = getFileName();
		int ret = unlink(name);
		char *url = this->uk.url->getUrl();
		assert(url);
		if (url) {
			klog(KLOG_INFO, "unlink disk cache obj=[%p %x] url=[%s] file=[%s] ret=[%d] errno=[%d]\n",
				this,
				this->index.flags,
				url,
				name,
				ret,
				errno);
			free(url);
		}
#ifdef ENABLE_BIG_OBJECT_206
		if (KBIT_TEST(index.flags,FLAG_BIG_OBJECT_PROGRESS)) {
			KStringBuf partfile;
			partfile << name << ".part";
			if (0!=unlink(partfile.getString())) {
				klog(KLOG_ERR,"cann't unlink file[%s] errno=%d\n",partfile.getString(),errno);
			}
		}
#endif
		free(name);
	}
}
char *KHttpObject::getFileName(bool part)
{	
	KStringBuf s;
	get_disk_base_dir(s);
	if (dk.filename1==0) {
		dk.filename1 = katom_get((void *)&disk_file_base);
		dk.filename2 = katom_inc((void *)&disk_file_index);
	}
	s.addHex((dk.filename1 & CACHE_DIR_MASK1));
	s << PATH_SPLIT_CHAR;
	s.addHex((dk.filename2 & CACHE_DIR_MASK2));
	s << PATH_SPLIT_CHAR;
	s.addHex(dk.filename1);
	s << "_";
	s.addHex(dk.filename2);
	if (part) {
		s << ".part";
	}
	return s.stealString();
}
void KHttpObject::write_file_header(KHttpObjectFileHeader *dci_header)
{
	memset(dci_header, 0, sizeof(KHttpObjectFileHeader));
	kgl_memcpy(dci_header->fix_str, CACHE_FIX_STR, sizeof(CACHE_FIX_STR));

	kgl_memcpy(&dci_header->dbi.index, &index, sizeof(HttpObjectIndex));
	dci_header->dbi.url_flag_encoding = uk.url->flag_encoding;	
	
	kgl_safe_copy_body_data(&dci_header->body, &data->i);

	dci_header->version = CACHE_DISK_VERSION;
	dci_header->body_complete = 1;
#ifdef ENABLE_BIG_OBJECT_206
	if (KBIT_TEST(index.flags, FLAG_BIG_OBJECT_PROGRESS)) {
		dci_header->body_complete = 0;
	}
#endif
}
bool KHttpObject::save_dci_header(KBufferFile *fp)
{
	KHttpObjectFileHeader dci_header;
	write_file_header(&dci_header);
	return fp->write((char *)&dci_header, sizeof(KHttpObjectFileHeader)) == sizeof(KHttpObjectFileHeader);
}
char *KHttpObject::build_aio_header(int &len)
{
	int url_len;
	char *u = get_obj_url_key(this, &url_len);
	if (u == NULL) {
		return NULL;
	}
	len = GetHeaderSize(url_len);
	char *buf = (char *)aio_alloc_buffer(len);
	char *hot = buf;
	write_file_header((KHttpObjectFileHeader *)hot);
	hot += sizeof(KHttpObjectFileHeader);
	hot += write_string(hot, u, url_len);
	free(u);
	KHttpHeader *header = data->headers;
	while (header) {
		hot += write_string(hot, header->attr, header->attr_len);
		hot += write_string(hot, header->val, header->val_len);
		header = header->next;
	}
	hot += write_string(hot, NULL, 0);
	kassert(hot - buf <= (INT64)index.head_size);
	return buf;
}
bool KHttpObject::save_header(KBufferFile *fp,const char *url,int url_len)
{
	save_dci_header(fp);
	writeString(fp,url,url_len);
	KHttpHeader *header = data->headers;
	while (header) {
		writeString(fp,header->attr,header->attr_len);
		writeString(fp,header->val,header->val_len);
		header = header->next;
	}
	writeString(fp,NULL);
	int pad_len = index.head_size - (int)fp->get_total_write();
	kassert(pad_len >= 0);
	if (pad_len > 0) {
		fp->write(NULL, pad_len);
	}
	return true;
}

bool KHttpObject::swapout(KBufferFile *file,bool fast_model)
{
	if (fast_model && !KBIT_TEST(index.flags, FLAG_IN_DISK)) {
		return false;
	}
	kbuf *tmp;
	char *filename = NULL;
	assert(data);
	if (KBIT_TEST(index.flags,FLAG_IN_DISK)) {
		if (dc_index_update == 0) {
			return true;
		}
#ifdef ENABLE_DB_DISK_INDEX
		if (dci) {
			dci->start(ci_update,this);
		}
#endif
	}
	dc_index_update = 0;
	if (conf.disk_cache <= 0 || cache.is_disk_shutdown()) {
		return false;
	}
	int url_len = 0;
	char *url = get_obj_url_key(this, &url_len);
	if (url == NULL) {
		return false;
	}
	GetHeaderSize(url_len);
	INT64 buffer_size = index.content_length + index.head_size;
	if (buffer_size > KGL_MAX_BUFFER_FILE_SIZE) {
		buffer_size = KGL_MAX_BUFFER_FILE_SIZE;
	}
	filename = getFileName();
	klog(KLOG_INFO, "swap out obj=[%p %x] url=[%s] to file [%s]\n",
		this,
		index.flags,
		url,
		filename);
	kassert(!file->opened());
	file->init();
	if (!file->open(filename,fileModify, KFILE_DSYNC)) {
		int err = errno;
		klog(KLOG_WARNING,"cann't open file [%s] to write. errno=[%d %s]\n",filename,err,strerror(err));
		goto swap_out_failed;
	}
	if (!save_header(file, url, url_len)) {
		goto swap_out_failed;
	}
#ifdef ENABLE_BIG_OBJECT_206
	if (data->i.type == BIG_OBJECT_PROGRESS) {
		/*
		if (!data->sbo->saveProgress(this)) {
			klog(KLOG_ERR, "save obj progress failed file=[%s].\n", filename);
			goto swap_out_failed;
		}
		goto swap_out_success;
		*/
	}
#endif
	if (KBIT_TEST(index.flags, FLAG_IN_DISK)) {
		//�����Ѿ��У�����
		goto swap_out_success;
	}
	if (data->i.type != MEMORY_OBJECT) {
		klog(KLOG_ERR, "swapout failed obj type=[%d],file=[%s].\n", data->i.type,filename);
		goto swap_out_failed;
	}
	tmp = data->bodys;
	while (tmp) {
		if (file->write(tmp->data, tmp->used)<(int)tmp->used) {
			klog(KLOG_ERR,"cann't write cache to disk file=[%s].\n",filename);
			goto swap_out_failed;
		}
		tmp=tmp->next;
	}
	cache.getHash(h)->IncDiskObjectSize(this);
#ifdef ENABLE_DB_DISK_INDEX
	if (dci) {		
		dci->start(ci_add,this);
	}
#endif
swap_out_success:
	if (url) {
		free(url);
	}
	if (filename) {
		free(filename);
	}
	file->close();
	return true;
swap_out_failed:
	if (url) {
		free(url);
	}
	if (file->opened()) {
		file->close();
		unlink(filename);
	}
	if (filename) {
		free(filename);
	}
	return false;
}
#endif
