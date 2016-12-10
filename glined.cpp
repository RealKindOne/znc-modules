// Detect /glines.

#include <znc/IRCNetwork.h>
#include <znc/Chan.h>
#include <znc/Modules.h>

using std::vector;
class Cglined : public CModule {
public:
	MODCONSTRUCTOR(Cglined) {
	}

	virtual ~Cglined() {
	}

	void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) override {
		if (sMessage.Token(0) == "G-Lined") {
			PutModule("" + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost()+ ")");
		}
	}
};
template<> void TModInfo<Cglined>(CModInfo& Info) {
//	Info.SetWikiPage("glined");
	Info.SetHasArgs(false);
	Info.AddType(CModInfo::NetworkModule);
}
NETWORKMODULEDEFS(Cglined, "Watch for G-Lines.")