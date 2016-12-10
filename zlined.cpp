// Detect /zlines.

#include <znc/IRCNetwork.h>
#include <znc/Chan.h>
#include <znc/Modules.h>

using std::vector;
class Czlined : public CModule {
public:
	MODCONSTRUCTOR(Czlined) {
	}

	virtual ~Czlined() {
	}

	void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) override {
		if (sMessage.Token(0) == "Z-Lined") {
			PutModule("" + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost()+ ")");
		}
	}
};
template<> void TModInfo<Czlined>(CModInfo& Info) {
//	Info.SetWikiPage("zlined");
	Info.SetHasArgs(false);
	Info.AddType(CModInfo::NetworkModule);
}
NETWORKMODULEDEFS(Czlined, "Watch for Z-Lines.")
