// WARNING -- THIS CAN CAUSE YOU TO HAVE SendQ DISCONNECTS.

// Libera.Chat supports "userhost-in-names", but it is hidden in
// the "CAP LS". Someone might be in a large amount of channels and get
// disconnected by SendQ'd.

// solanum/doc/reference.conf ; #hidden_caps = "userhost-in-names";

// WARNING -- THIS CAN CAUSE YOU TO HAVE SendQ DISCONNECTS.

#include <znc/IRCNetwork.h>
#include <znc/Modules.h>

class CForceUserHostNames : public CModule {
  public:
    MODCONSTRUCTOR(CForceUserHostNames) {}

    EModRet OnIRCConnecting(CIRCSock* pIRCSock) override {
        // Request the unadvertised capability
        PutIRC("CAP REQ :userhost-in-names");
        return CONTINUE;
    }
};

NETWORKMODULEDEFS(CForceUserHostNames, "Forces userhost-in-names capability request")