// Original antiidle module would message "\xAE" to ourself.
// Modified to send a message into a channel.

// Tip:
// If you don't want to idle in the channel you can do something like this to allow
// external messages.

//    /msg chanserv register ##CHANGE_THIS_CHANNEL_NAME_INTO_SOMETHING_ELSE
//    /msg chanserv set ##CHANGE_THIS_CHANNEL_NAME_INTO_SOMETHING_ELSE guard on
//    /msg chanserv set ##CHANGE_THIS_CHANNEL_NAME_INTO_SOMETHING_ELSE mlock -n
//    /part ##CHANGE_THIS_CHANNEL_NAME_INTO_SOMETHING_ELSE

// TODO:
//	Recreate channel if cannot send.
// :irc.foobar.net 404 KindOne ##CHANGE_THIS_CHANNEL_NAME_INTO_SOMETHING_ELSE :Cannot send to nick/channel
// 	OnNumericMessage() { if 404  && match first chraracter as #, { join TARGET ; }}
// 	OnJoin() if (chan TARGET) && (nick ChanServ) { part TARGET }

/*
 * Copyright (C) 2004-2012  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/IRCNetwork.h>
#include <znc/Nick.h>

#define TARGET "##CHANGE_THIS_CHANNEL_NAME_INTO_SOMETHING_ELSE"
#define MESSAGE "anti-idle script..."

class CAntiIdle;

class CAntiIdleJob : public CTimer {
  public:
    CAntiIdleJob(CModule* pModule, unsigned int uInterval, unsigned int uCycles, const CString& sLabel, const CString& sDescription)
        : CTimer(pModule, uInterval, uCycles, sLabel, sDescription) {}
    virtual ~CAntiIdleJob() {}

  protected:
    virtual void RunJob();
};

class CAntiIdle : public CModule {
  public:
    MODCONSTRUCTOR(CAntiIdle) {
        SetInterval(30);
    }

    virtual ~CAntiIdle() {}

    virtual bool OnLoad(const CString& sArgs, CString& sErrorMsg) {
        if (!sArgs.Trim_n().empty())
            SetInterval(sArgs.ToInt());
        return true;
    }

    virtual void OnModCommand(const CString& sCommand) {
        CString sCmdName = sCommand.Token(0).AsLower();
        if (sCmdName == "set") {
            CString sInterval = sCommand.Token(1, true);
            SetInterval(sInterval.ToInt());

            if (m_uiInterval == 0)
                PutModule("AntiIdle is now turned off.");
            else
                PutModule("AntiIdle is now set to " + CString(m_uiInterval) + " seconds.");
        } else if (sCmdName == "off") {
            SetInterval(0);
            PutModule("AntiIdle is now turned off");
        } else if (sCmdName == "show") {
            if (m_uiInterval == 0)
                PutModule("AntiIdle is turned off.");
            else
                PutModule("AntiIdle is set to " + CString(m_uiInterval) + " seconds.");
        } else {
            PutModule("Commands: set, off, show");
        }
    }

  private:
    void SetInterval(int i) {
        if (i < 0)
            return;

        m_uiInterval = i;

        RemTimer("AntiIdle");

        if (m_uiInterval == 0) {
            return;
        }

        AddTimer(new CAntiIdleJob(this, m_uiInterval, 0, "AntiIdle", "Periodically sends a msg to the user"));
    }

    unsigned int m_uiInterval;
};

void CAntiIdleJob::RunJob() {
    GetModule()->PutIRC("PRIVMSG " TARGET " :" MESSAGE);
}

NETWORKMODULEDEFS(CAntiIdle, "Hides your real idle time")
