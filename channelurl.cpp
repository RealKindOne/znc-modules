// Channel URL blocker.
// So services on freenode will spam this if they set a URL.
// See "/msg ChanServ help set URL"
// :services. 328 KindOne #channel :http://channel.url.goes.here

#include <znc/IRCNetwork.h>
#include <znc/Modules.h>

using std::vector;

class CChannelURL : public CModule {
  public:
    MODCONSTRUCTOR(CChannelURL) {}

    virtual ~CChannelURL() {}

    EModRet OnNumericMessage(CNumericMessage& numeric) {
        if (numeric.GetCode() == 328) {
            return HALT;
        }
        return CONTINUE;
    }
};

template <>
void TModInfo<CChannelURL>(CModInfo& Info) {
    //	Info.SetWikiPage("ChannelURL");
    Info.SetHasArgs(false);
}

NETWORKMODULEDEFS(CChannelURL, "Hide Channel URL (Numeric 328) from IRC clients.")
