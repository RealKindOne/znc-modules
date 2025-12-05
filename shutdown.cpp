// shutdown.cpp

// Remotely shutdown your znc via NOTICE, PRIVMSG, or TOPIC.
// This module does not check who sent the command. Anyone can execute it if they know the COMMAND/PASSWORD.

// Network module, it will only work on the network(s) its loaded on.

// The normal shutdown method checks if the znc.conf file is writable. If the znc.conf file is
//      not writable, znc will warn you and give you an option that allows znc to be forcefully
//      shutdown. This module does not have that check. It will shutdown if znc cannot write
//      the znc.conf file. This can possibly cause data loss.          You have been warned :)

//      /msg your-znc-nick SHUTDOWN hunter2
//      /notice your-znc-nick SHUTDOWN hunter2
//      /topic #channel SHUTDOWN hunter2

// If you want it to work on another command.
#define COMMAND "SHUTDOWN"
// Message sent too everyone connected to the znc.
#define MESSAGE "Remote shutdown!"
// You should seriously change this. Maybe change it every time you use it.
#define PASSWORD "hunter2"

#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/User.h>

class Cshutdown : public CModule {
  public:
    MODCONSTRUCTOR(Cshutdown) {}

    virtual ~Cshutdown() {}

    bool OnLoad(const CString& sArgs, CString& sErrorMsg) override {
        if (!GetUser()->IsAdmin()) {
            sErrorMsg = ("You must have admin privileges to load this module");
            return false;
        }

        return true;
    }

    EModRet virtual OnPrivMsg(CNick& Nick, CString& sMessage) override {
        if ((sMessage.Token(0).StripControls() == COMMAND) && (sMessage.Token(1).Equals(PASSWORD))) {
            CZNC::Get().Broadcast(MESSAGE);
            throw CException(CException::EX_Shutdown);
        }
        return CONTINUE;
    }

    EModRet virtual OnPrivNotice(CNick& Nick, CString& sMessage) override {
        if ((sMessage.Token(0).StripControls() == COMMAND) && (sMessage.Token(1).Equals(PASSWORD))) {
            CZNC::Get().Broadcast(MESSAGE);
            throw CException(CException::EX_Shutdown);
        }
        return CONTINUE;
    }

    // In the event the znc user blocks query.
    EModRet virtual OnTopic(CNick& Nick, CChan& Channel, CString& sMessage) override {
        if ((sMessage.Token(0).StripControls() == COMMAND) && (sMessage.Token(1).Equals(PASSWORD))) {
            CZNC::Get().Broadcast(MESSAGE);
            throw CException(CException::EX_Shutdown);
        }
        return CONTINUE;
    }
};

template <>
void TModInfo<Cshutdown>(CModInfo& Info) {
}

NETWORKMODULEDEFS(Cshutdown, "Forcefully shutdown your znc remotely.")
