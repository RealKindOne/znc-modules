// Detect /klines.

#include <znc/IRCNetwork.h>
#include <znc/Chan.h>
#include <znc/Modules.h>

using std::vector;
class Cklined : public CModule {
public:
	MODCONSTRUCTOR(Cklined) {
	}

	virtual ~Cklined() {
	}

	void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) override {
		if (sMessage.Token(0) == "K-Lined") {
			PutModule("" + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost()+ ")");
		}
	}
};
template<> void TModInfo<Cklined>(CModInfo& Info) {
//	Info.SetWikiPage("klined");
	Info.SetHasArgs(false);
	Info.AddType(CModInfo::NetworkModule);
}
NETWORKMODULEDEFS(Cklined, "Watch for K-Lines.")
