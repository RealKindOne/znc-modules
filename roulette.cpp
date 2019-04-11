// Fun game of roulette.
// Must be OP for this to work.

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
			// Channel might be -n. Block outside users from playing.
			if (Channel.FindNick(Nick.GetNick())  ==  nullptr) { return HALT; }
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
