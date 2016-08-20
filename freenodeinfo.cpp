// Initial version created by "KindOne @ chat.freenode.net".
//
// EXTREMELY CRUDE ZNC module to hide the "[freenode-info]" lines.
//
// TODO ...
//
//      - Maybe only allow loading on freenode?
//

#include <znc/Modules.h>

class Cfreenodeinfo : public CModule {
public:

	MODCONSTRUCTOR(Cfreenodeinfo) {
	}

	virtual ~Cfreenodeinfo() {}

	virtual EModRet OnRaw(CString& sLine) {
		// :card.freenode.net NOTICE #channel :[freenode-info] channel trolls ...
		if (sLine.Token(3) == ":[freenode-info]") {
			return HALT;
		}
		return CONTINUE;
	}

};

template<> void TModInfo<Cfreenodeinfo>(CModInfo& Info) {
	Info.AddType(CModInfo::NetworkModule);
}

NETWORKMODULEDEFS(Cfreenodeinfo, "Hides [freenode-info] notices.")