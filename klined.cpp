// Detect /klines.
// <*klined> idiot (ident@example.com) from channels: #foobar, #xyz

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>

using std::vector;
class Cklined : public CModule {
  public:
    MODCONSTRUCTOR(Cklined) {}

    virtual ~Cklined() {}

    void OnQuitMessage(CQuitMessage& Message,
                       const vector<CChan*>& vChans) override {
        const CNick& Nick = Message.GetNick();
        const CString sMessage = Message.GetReason();
        if (sMessage.Token(0) == "K-Lined") {
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
void TModInfo<Cklined>(CModInfo& Info) {
    //	Info.SetWikiPage("klined");
    Info.SetHasArgs(false);
}
NETWORKMODULEDEFS(Cklined, "Watch for K-Lines.")
