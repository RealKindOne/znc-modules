// Fun game of roulette.
// Must be OP for this to work.

// RUN AT OWN RISK.

// First attempt at rand(), I have no idea if thats the correct way, it seems to work.


#include <znc/Chan.h>
#include <znc/Modules.h>
#include <znc/IRCNetwork.h>

using std::vector;

class Croulette : public CModule {

public:
	MODCONSTRUCTOR(Croulette) {}

	virtual ~Croulette() {}

	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) override {
		if ((sMessage.Token(0).StripControls() == "!roulette") && (Channel.HasPerm(CChan::Op))) {

			int x = rand()%6+1;

			if (x == 4) {
				PutIRC("KICK " + Channel.GetName() + " " + Nick.GetNick() + " :*BANG*");
			}
			else {
				PutIRC("PRIVMSG " + Channel.GetName() + " :" + Nick.GetNick() + " *CLICK*");
			}
		}
		return CONTINUE;
	}
};

template <>
void TModInfo<Croulette>(CModInfo& Info) {

}

NETWORKMODULEDEFS(Croulette, "Russian Roulette.")
