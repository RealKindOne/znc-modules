// myinsanity.cpp

// Syntax:
//    /msg *myinsanity showme

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/User.h>
#include <set>

class CMyInsanity : public CModule {
  public:
    MODCONSTRUCTOR(CMyInsanity) {}

    ~CMyInsanity() override {}

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        return true;
    }

    void OnModCommand(const CString& sCommand) override {
        if (sCommand.Equals("showme", CString::CaseInsensitive)) {

            size_t totalChannels = 0;
            size_t totalUniqueNicks = 0;
            size_t totalNetworks = 0;
            size_t powerChannels = 0;
            size_t powerUsers = 0;

            CUser* pUser = GetUser();
            if (!pUser) {
                PutModule("Error: No user found!");
                return;
            }

            const std::vector<CIRCNetwork*>& networks = pUser->GetNetworks();
            totalNetworks = networks.size();

            for (CIRCNetwork* pNetwork : networks) {
                if (!pNetwork) continue;

                std::set<CString> uniqueNicks;

                const std::vector<CChan*>& channels = pNetwork->GetChans();

                for (CChan* pChannel : channels) {
                    if (!pChannel) continue;

                    totalChannels++;

                    const std::map<CString, CNick>& nicks = pChannel->GetNicks();

                    for (const auto& nickPair : nicks) {
                        uniqueNicks.insert(nickPair.second.GetNick());
                    }

                    if (pChannel->HasPerm('~') || pChannel->HasPerm('&') ||
                        pChannel->HasPerm('@') || pChannel->HasPerm('%')) {
                        powerChannels++;
                        powerUsers += pChannel->GetNickCount();
                    }
                }

                totalUniqueNicks += uniqueNicks.size();
            }

            PutModule("I see " + CString(totalUniqueNicks) +
                      " unique nicks in " + CString(totalChannels) +
                      " channels on " + CString(totalNetworks) +
                      " networks and have power over " + CString(powerUsers) +
                      " users in " + CString(powerChannels) + " channels.");
        } else {
            PutModule("Unknown command. Use 'showme'");
        }
    }
};

USERMODULEDEFS(CMyInsanity, "Counts users, channels, and networks")