#ifndef KWORKMODELACL_H
#define KWORKMODELACL_H
#include "KAcl.h"
class KWorkModelAcl : public KAcl
{
public:
	KWorkModelAcl()
	{
		flag = 0;
	}
	~KWorkModelAcl()
	{
	}
	KAcl *new_instance() override {
		return new KWorkModelAcl();
	}
	const char *getName() override {
		return "work_model";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		return KBIT_TEST(rq->sink->get_server_model(),flag)>0;
	}
	std::string getDisplay() override {
		std::stringstream s;
		getFlagString(s);
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) override {
		flag = 0;
#ifdef WORK_MODEL_TCP
		if (attribute["tcp"] == "1") {
			flag |= WORK_MODEL_TCP;
		}
#endif
		if (attribute["ssl"] == "1") {
			flag |= WORK_MODEL_SSL;
		}
		
	}
	void buildXML(std::stringstream &s) override {
		getFlagString(s);
		s << ">";
	}
	std::string getHtml(KModel *model) override {
		std::stringstream s;
		KWorkModelAcl *m_chain = (KWorkModelAcl *)model;
#ifdef WORK_MODEL_TCP
		s << "<input type=checkbox name='tcp' value='1' ";
		if (m_chain && KBIT_TEST(m_chain->flag,WORK_MODEL_TCP)) {
			s << "checked";
		}
		s << ">tcp";
#endif
		s << "<input type=checkbox name='ssl' value='1' ";
		if (m_chain && KBIT_TEST(m_chain->flag, WORK_MODEL_SSL)) {
			s << "checked";
		}
		s << ">ssl";
		return s.str();
	}
private:
	int flag;
	void getFlagString(std::stringstream &s)
	{
#ifdef WORK_MODEL_TCP
		if (KBIT_TEST(flag, WORK_MODEL_TCP)) {
			s << "tcp='1' ";
		}
#endif
		if (KBIT_TEST(flag, WORK_MODEL_SSL)) {
			s << "ssl='1' ";
		}

	}
};
#endif
