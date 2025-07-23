/*
 *
 * pretendclient.cpp
 *
 *  Connect to networks when your IRC client connects to znc.
 *  Disconnect from network when your IRC client disconnects from znc.
 *
 *  Known issues:
 *     Servers can have rate limiting and other measures. This can cause a 
 *     delay in connecting to the network.
 *
 *
 */

#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/User.h>
#include <znc/znc.h>

class Cpretendclient : public CModule {
  public:
    MODCONSTRUCTOR(Cpretendclient) {}

    virtual ~Cpretendclient() {}

    void OnClientLogin() override {
        CIRCNetwork* pNetwork = GetNetwork();
        if (!pNetwork) {
            PutModule("No network provided. Not connecting.");
            return;
        }

        if (!pNetwork->GetIRCConnectEnabled()) {
            pNetwork->SetIRCConnectEnabled(true);
            PutModule("Enabled IRC connection for this network");
        }

        if (!pNetwork->IsIRCConnected()) {
            if (pNetwork->Connect()) {
                PutModule("Connection initiated successfully");
            } else {
                PutModule(
                    "Failed to initiate connection - likely due to server "
                    "throttling. Connection will be retried automatically.");
            }
        }
    }

    void OnClientDisconnect() override {
        CIRCNetwork* pNetwork = GetNetwork();
        if (pNetwork && pNetwork->IsIRCConnected()) {
            // Check if this was the last client
            if (GetUser()->GetAllClients().size() <= 1) {
                pNetwork->SetIRCConnectEnabled(false);
            }
        }
    }

    // Disable the network if banned.
    EModRet OnNumericMessage(CNumericMessage& msg) override {
        if (msg.GetCode() == 465) {
            CString sPrefix = GetUser()->GetStatusPrefix();
            // :irc.server.com 465 mynick :You are banned from this server-
            // Temporary K-line 1 min. - Example (2024/12/29 15.39)
            m_pNetwork->SetIRCConnectEnabled(false);

            m_pNetwork->PutStatus(
                "You are banned from the network. ZNC has disabled the "
                "network. Use ` /msg " +
                sPrefix +
                "status jump ` to reconnect. (Sent from pretendclient module)");
        }
        return CONTINUE;
    }
};
template <>
void TModInfo<Cpretendclient>(CModInfo& Info) {
    Info.SetHasArgs(false);
}
NETWORKMODULEDEFS(Cpretendclient, "Make ZNC act like a IRC client.")