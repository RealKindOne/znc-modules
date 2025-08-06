// Detect /glines.

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>

using std::vector;
class Cglined : public CModule {
  public:
    MODCONSTRUCTOR(Cglined) {}

    virtual ~Cglined() {}

    void OnQuitMessage(CQuitMessage& Message,
                       const vector<CChan*>& vChans) override {
        const CNick& Nick = Message.GetNick();
        const CString sMessage = Message.GetReason();
        if (sMessage.Token(0) == "G-Lined") {
            VCString vsChans;
            for (CChan* pChan : vChans) {
                vsChans.push_back(pChan->GetName());
            }
            PutModule("" + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") from channels: " +
                      CString(", ").Join(vsChans.begin(), vsChans.end()));
        }
    }
};
template <>
void TModInfo<Cglined>(CModInfo& Info) {
    //	Info.SetWikiPage("glined");
    Info.SetHasArgs(false);
}
NETWORKMODULEDEFS(Cglined, "Watch for G-Lines.")