// Detect /zlines.

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>

using std::vector;
class Czlined : public CModule {
  public:
    MODCONSTRUCTOR(Czlined) {}

    virtual ~Czlined() {}

    void OnQuitMessage(CQuitMessage& Message,
                       const vector<CChan*>& vChans) override {
        const CNick& Nick = Message.GetNick();
        const CString sMessage = Message.GetReason();
        if (sMessage.Token(0) == "Z-Lined") {
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
void TModInfo<Czlined>(CModInfo& Info) {
    //	Info.SetWikiPage("zlined");
    Info.SetHasArgs(false);
}
NETWORKMODULEDEFS(Czlined, "Watch for Z-Lines.")
