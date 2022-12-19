#include "KChildApiService.h"
#include "export.h"
#include "api_child.h"

KChildApiService::KChildApiService(KApiDso* dso) : KApiService(dso)
{
	st = NULL;
}
KChildApiService::~KChildApiService()
{
}
KGL_RESULT KChildApiService::start(KFastcgiStream<KSocketStream>* st)
{
	this->st = st;
	if (!st->readParams(&env)) {
		debug("child cann't readParams\n");
		return KGL_ESOCKET_BROKEN;
	}
	KGL_RESULT result = KApiService::start();
	assert(st);
	if (!headSended) {
		st->write_data(_KS("Status: 200\r\n\r\n"));
	}
	if (!st->has_data_empty_arrived()) {
		//still have data to read
		//debug("now wait stdin empty body package\n");
		char buf[512];
		while (st->read(buf, sizeof(buf)) > 0);
	}
	if (!st->write_end()) {
		debug("child cann't write_end\n");
		return KGL_ESOCKET_BROKEN;
	}
	return result;
}
int KChildApiService::writeClient(const char* str, int len)
{
	headSended = true;
	if (!headSended) {
		headSended = true;
		const char* defaultHeaders = "Status: 500 Server Error\r\n\r\n";
		st->write_data(defaultHeaders, (int)strlen(defaultHeaders));
	}
	if (st->write_data(str, len)) {
		return len;
	}
	return -1;
}
KGL_RESULT KChildApiService::map_url_path(const char* url, LPVOID file, LPDWORD file_len)
{
	int len = (int)strlen(url);
	if (!st->write_data(API_CHILD_MAP_PATH, url, len)) {
		return KGL_ESOCKET_BROKEN;
	}
	FCGI_Header header;
	char* buf = NULL;
	for (;;) {
		if (!st->read_package(&header, &buf, len)) {
			return KGL_ESOCKET_BROKEN;
		}
		//debug("package type=[%d] len=[%d]\n", header.type, len);
		if (header.type == API_CHILD_MAP_PATH_RESULT) {
			break;
		}
		if (buf) {
			xfree(buf);
		}
	}
	KGL_RESULT result = set_variable(file, file_len, buf);
	//printf("buf=[%s] file=[%s]\n", buf, (char *)file);
	if (buf) {
		xfree(buf);
	}
	return result;
}
int KChildApiService::readClient(char* buf, int len)
{
	return st->read(buf, len);
}
bool KChildApiService::setStatusCode(const char* status, int len)
{
	if (headSended) {
		return true;
	}
	if (len == 0) {
		len = (int)strlen(status);
	}
	headSended = true;
	if (!st->write_data("Status: ", 8)) {
		return false;
	}
	if (!st->write_data(status, len)) {
		return false;
	}
	return st->write_data("\r\n", 2);
}
KGL_RESULT KChildApiService::addHeader(const char* attr, int len)
{
	if (len == 0) {
		len = (int)strlen(attr);
	}
	headSended = true;
	return st->write_data(attr, len) ? KGL_OK : KGL_ESOCKET_BROKEN;
}
bool KChildApiService::execUrl(HSE_EXEC_URL_INFO* urlInfo)
{
	return false;
}
bool KChildApiService::initECB(EXTENSION_CONTROL_BLOCK* ecb)
{
	memset(ecb, 0, sizeof(EXTENSION_CONTROL_BLOCK));
	ecb->cbSize = sizeof(EXTENSION_CONTROL_BLOCK);
	ecb->ConnID = (HCONN) static_cast<KApiService*>(this);
	ecb->dwVersion = MAKELONG(0, 6);
	ecb->lpszMethod = (LPSTR)env.getEnv("REQUEST_METHOD");
	ecb->lpszLogData[0] = '\0';
	ecb->lpszPathInfo = (char*)env.getEnv("PATH_INFO");
	ecb->lpszPathTranslated = (char*)env.getEnv("PATH_TRANSLATED");
	ecb->cbTotalBytes = env.contentLength;
	ecb->cbLeft = ecb->cbTotalBytes;
	ecb->lpszContentType = (env.contentType ? env.contentType : (char*)"");
	ecb->dwHttpStatusCode = STATUS_OK;
	ecb->lpszQueryString = (char*)env.getEnv("QUERY_STRING");
	if (ecb->lpszQueryString == NULL) {
		ecb->lpszQueryString = (char*)"";
	}
	//ecb->lpszQueryString = xstrdup(ecb->lpszQueryString);
	//ecb->lpszPathTranslated = xstrdup(ecb->lpszPathTranslated);
	//ecb->lpszMethod = xstrdup(ecb->lpszMethod);


	ecb->ServerSupportFunction = ServerSupportFunction;
	ecb->GetServerVariable = GetServerVariable;
	ecb->WriteClient = WriteClient;
	ecb->ReadClient = ReadClient;
	return true;
}

