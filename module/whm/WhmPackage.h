/*
 * WhmPackage.h
 *
 *  Created on: 2010-8-31
 *      Author: keengo
 */

#ifndef WHMPACKAGE_H_
#define WHMPACKAGE_H_
#include <map>
#include "KServiceProvider.h"
#include "KXmlEvent.h"
#include "KCountable.h"
#include "WhmCallMap.h"
/*
 * whm��,�������ļ����ɵ�whm���ü�event��Ϣ.
 */
class WhmPackage: public KXmlEvent, public KCountable {
public:
	WhmPackage();
	virtual ~WhmPackage();
	int process(const char *callName,WhmContext *context);
	int getInfo(WhmContext *context);
	void flush();
	int query(WhmContext *context);
	int terminate(WhmContext *context);
	void setFile(const char *file)
	{
		this->file = file;
	}
	bool startElement(KXmlContext *context) override;
	bool startCharacter(KXmlContext *context, char *character, int len) override;
	bool endElement(KXmlContext *context) override;
private:
//	WhmContext *whmContext;
	WhmCallMap *newCallMap(const std::string &name, const std::string &callName);
	WhmExtend *findExtend(const std::string &name);
	WhmCallMap *findCallMap(const std::string &name);
	/*
	 * ����ӳ��
	 */
	std::map<std::string, WhmCallMap *> callmap;
	std::map<std::string, WhmExtend *> extends;
	WhmCallMap *curCallable;
	WhmExtend *curExtend;
	std::string file;
	std::string version;
};

#endif /* WHMPACKAGE_H_ */
