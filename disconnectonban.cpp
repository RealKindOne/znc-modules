// Disconnect On Ban module.
// Prevent your ZNC from re-connecting to a network if you are network banned.

#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/User.h>  // For GetUser()->GetStatusPrefix()

class Cdisconnectonban : public CModule {
  public:
    MODCONSTRUCTOR(Cdisconnectonban) {}

    virtual ~Cdisconnectonban() {}

    EModRet OnNumericMessage(CNumericMessage& msg) override {
        if (msg.GetCode() == 465) {
            CString sPrefix = GetUser()->GetStatusPrefix();
            // :irc.server.com 465 mynick :You are banned from this server- Temporary K-line 1 min. - Example (2024/12/29 15.39)
            m_pNetwork->SetIRCConnectEnabled(false);

            // Send message to client by *status.
            // TODO: Figure out how to remove extra spaces around module name?
            m_pNetwork->PutStatus("You are banned from the network. ZNC has disabled the network by the \x002 disconnectonban \x002 module. Use ` /msg " + sPrefix + "status jump ` to reconnect.");

            // Send message to client via *disconnectonban
            //PutModule("You are banned from the network. ZNC has disabled the network because this module is loaded.");
        }
        return CONTINUE;
    }
};

template <>
void TModInfo<Cdisconnectonban>(CModInfo& Info) {
    //Info.SetWikiPage("disconnectonban");
}
// Network only module.
NETWORKMODULEDEFS(Cdisconnectonban, "Disconnect ZNC from the network when you are K/G/Z-lined.")