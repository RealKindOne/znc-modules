// Initial version created by "KindOne @ irc.libera.chat".
//
// Module to support MONITOR.
// http://ircv3.org/specification/monitor-3.2
//
// TODO ...
//
//
//	Input:
//		- Adding:
//			- Sanitizing checking.
//				- Make the checking case insensitive.
//			- Prevent nicks from starting with numbers/invalid characters.
//			- Allow adding via Webadmin.
//			- Don't go over "MONITOR=" limit.
//		- Deleting:
//			- Sanitizing checking.
//				- Make the checking case insensitive.
//			- Allow deleting via Webadmin.
//
//	Output:
//		- Allow people to customize the output of the module, without modifying/recompiling the code.
//			- PutModule() from raw/numeric 730
//
//	IRCd:
//		- Only load/start on networks that support MONITOR.
//			- Maybe: <*monitor> Error. This %network% does not support MONITOR.
//		- When connecting/loading, add multiple nicks per "MONITOR +", instead of one per line. Excess floods.
//		- Honor MONITOR=, from the 005, warn if we have more entries than the IRCd can allow.
//			- Maybe only "MONITOR + ..." the first however many the IRCd supports.
//
// OnLoad/OnIRCConnected/OnModUnload:
//		- Add option to enable/disable the "MONITOR C". Some people might already have nicks added, before loading.
//		- OnLoad/OnIRCConnected send the same commands, combine them?
//
//	Other:
//		- Other things that I might of missed and/or can't think of.
//

#include <znc/IRCNetwork.h>
#include <znc/Modules.h>

using std::vector;

class CMonitor : public CModule {
  public:
    MODCONSTRUCTOR(CMonitor) {
        AddHelpCommand();
        AddCommand("Add", static_cast<CModCommand::ModCmdFunc>(&CMonitor::Add),
                   "<nick1>[,nick2[,...]]", "Add one or more nicks to monitor");
        AddCommand("Del", static_cast<CModCommand::ModCmdFunc>(&CMonitor::Del),
                   "<id | nick1>[,nick2[,...]]", "Delete by ID or by nick list");
        AddCommand("List", static_cast<CModCommand::ModCmdFunc>(&CMonitor::List),
                   "", "List all monitored nicks");
        AddCommand("Reload", static_cast<CModCommand::ModCmdFunc>(&CMonitor::Reload),
                   "", "Reload nicks from storage and update server");
        AddCommand("Swap", static_cast<CModCommand::ModCmdFunc>(&CMonitor::Swap),
                   "<id1> <id2>", "Swap two nicks by ID");
        AddCommand("Sort", static_cast<CModCommand::ModCmdFunc>(&CMonitor::Sort),
                   "", "Sort nicks alphabetically and update server");
        AddCommand("Clear", static_cast<CModCommand::ModCmdFunc>(&CMonitor::Clear),
                   "", "Remove all nicks from the list and server");
    }

    virtual ~CMonitor() {
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        GetNV("Monitor").Split("\n", m_vMonitor, false);
        if (m_pNetwork->IsIRCConnected()) {
            SendMonitorClear();
            SendMonitorAdd(m_vMonitor);
        }
        return true;
    }

    void OnModuleUnloading() {
        if (m_pNetwork->IsIRCConnected()) {
            SendMonitorClear();
        }
    }

    void OnIRCConnected() override {
        SendMonitorAdd(m_vMonitor);
    }

    void OnISupport(CMessage& msg) {
        for (const auto& param : msg.GetParams()) {
            if (param.Token(0, false, "=").Equals("NICKLEN")) {
                m_uNickLen = param.Token(1, true, "=").ToUInt();
                if (m_uNickLen == 0) m_uNickLen = 30;
            } else if (param.Token(0, false, "=").Equals("MONITOR")) {
                m_uMonitorLimit = param.Token(1, true, "=").ToUInt();
            }
        }
    }

    EModRet OnNumericMessage(CNumericMessage& numeric) override {
        switch (numeric.GetCode()) {
            case 730: {  // RPL_MONONLINE
                // :irc.libera.chat 730 KindOne :EvilOne!KindOne@1.2.3.4

                // Uncomment / comment the other one if you want to switch the output.
                //  Online: EvilOne!KindOne@1.2.3.4
                //  PutModule("Online: " + sLine.Token(3).TrimPrefix_n() + "");

                //  Online: EvilOne KindOne@1.2.3.4
                PutModule("Online: " + numeric.GetParam(1).TrimPrefix_n().Replace_n("!", " "));

                return HALT;
            }
            case 731: {  // RPL_MONOFFLINE
                // :irc.libera.chat 731 KindOne :EvilOne
                // Offine: EvilOne
                PutModule("Offline: " + numeric.GetParam(1).TrimPrefix_n());
                return HALT;
            }
            case 732: {  // RPL_MONLIST
                // :irc.libera.chat 732 KindOne :EvilOne,EpicOne,KindTwo
                VCString vsNicks;
                numeric.GetParam(1).TrimPrefix_n().Split(",", vsNicks, false);
                for (const CString& sNick : vsNicks) {
                    PutModule("List: " + sNick);
                }
                return HALT;
            }
            case 733: {  // RPL_ENDOFMONLIST
                // :irc.libera.chat 733 KindOne :End of MONITOR list
                PutModule("End of MONITOR list.");
                return HALT;
            }
            case 734: {  // ERR_MONLISTFULL
                // :irc.libera.chat 734 KindOne 100 Nick101,Nick102 :Monitor list is full
                CString sMsg = "Error: Monitor list full. Cannot add: " +
                               numeric.GetParam(2).Replace_n(",", " ");
                PutModule(sMsg);
                return HALT;
            }

                return CONTINUE;
        }
        return CONTINUE;
    }

  private:
    void Add(const CString& sCommand) {
        CString sArgs = sCommand.Token(1, true);
        if (sArgs.empty()) {
            PutModule("Usage: add <nick>[,nick2[,...]]");
            return;
        }

        VCString vsNewNicks;
        sArgs.Split(",", vsNewNicks, false);

        VCString vsValid;
        VCString vsErrors;

        for (CString& sNick : vsNewNicks) {
            sNick.Trim();
            if (std::find_if(m_vMonitor.begin(), m_vMonitor.end(),
                             [&](const CString& a) { return a.Equals(sNick); }) != m_vMonitor.end()) {
                vsErrors.push_back(sNick + " (already in list)");
                continue;
            }
            if (m_uMonitorLimit > 0 && m_vMonitor.size() + vsValid.size() >= m_uMonitorLimit) {
                vsErrors.push_back(sNick + " (would exceed server limit)");
                continue;
            }
            vsValid.push_back(sNick);
        }

        for (const CString& sNick : vsValid) {
            m_vMonitor.push_back(sNick);
            PutModule("Added " + sNick);
        }
        for (const CString& sErr : vsErrors) {
            PutModule("Failed: " + sErr);
        }

        if (m_pNetwork->IsIRCConnected() && !vsValid.empty()) {
            SendMonitorAdd(vsValid);
        }

        Save();
    }

    void Del(const CString& sCommand) {
        CString sArgs = sCommand.Token(1, true);
        if (sArgs.empty()) {
            PutModule("Usage: del <id | nick1>[,nick2[,...]]");
            return;
        }

        VCString vsNicks;
        sArgs.Split(",", vsNicks, false);

        // Trim all nicks and remove empty ones
        VCString vsTrimmed;
        for (CString& nick : vsNicks) {
            nick.Trim();
            if (!nick.empty())
                vsTrimmed.push_back(nick);
        }

        if (vsTrimmed.empty()) {
            PutModule("No valid nicks/IDs provided.");
            return;
        }

        // If multiple items or the single item is not all digits, treat as nick list
        bool bIsMulti = (vsTrimmed.size() > 1);
        if (!bIsMulti && vsTrimmed.size() == 1) {
            // Single item: could be ID (all digits) or a nick
            const CString& sItem = vsTrimmed[0];
            if (sItem.find_first_not_of("0123456789") == CString::npos) {
                // All digits -> treat as ID
                u_int iNum = sItem.ToUInt();
                if (iNum == 0 || iNum > m_vMonitor.size()) {
                    PutModule("Invalid # requested");
                    return;
                }
                CString sRemoved = m_vMonitor[iNum - 1];
                m_vMonitor.erase(m_vMonitor.begin() + iNum - 1);
                if (m_pNetwork->IsIRCConnected())
                    SendMonitorDel({sRemoved});
                PutModule("Removed " + sRemoved);
                Save();
                return;
            }
            // else fall through to nick deletion
        }

        // Delete by nick list (multi or single nick)
        VCString vsRemoved;
        VCString vsNotFound;
        for (const CString& sNick : vsTrimmed) {
            auto it = std::find_if(m_vMonitor.begin(), m_vMonitor.end(),
                                   [&](const CString& a) { return a.Equals(sNick); });
            if (it != m_vMonitor.end()) {
                vsRemoved.push_back(*it);
                m_vMonitor.erase(it);
            } else {
                vsNotFound.push_back(sNick);
            }
        }
        if (!vsRemoved.empty()) {
            if (m_pNetwork->IsIRCConnected())
                SendMonitorDel(vsRemoved);
            for (const CString& s : vsRemoved)
                PutModule("Removed " + s);
        }
        for (const CString& s : vsNotFound)
            PutModule("Not found: " + s);

        Save();
    }

    void List(const CString& sCommand) {
        CTable Table;
        Table.AddColumn("Id");
        Table.AddColumn("Nick");
        unsigned int idx = 1;
        for (const CString& sNick : m_vMonitor) {
            Table.AddRow();
            Table.SetCell("Id", CString(idx++));
            Table.SetCell("Nick", sNick);
        }
        if (!PutModule(Table))
            PutModule("No nicks found.");
    }

    void Reload(const CString& sCommand) {
        m_vMonitor.clear();
        GetNV("Monitor").Split("\n", m_vMonitor, false);
        if (m_pNetwork->IsIRCConnected()) {
            SendMonitorClear();
            SendMonitorAdd(m_vMonitor);
        }
        PutModule("Reloaded " + CString(m_vMonitor.size()) + " nicks.");
    }

    void Swap(const CString& sCommand) {
        u_int iA = sCommand.Token(1).ToUInt();
        u_int iB = sCommand.Token(2).ToUInt();
        if (iA == 0 || iA > m_vMonitor.size() || iB == 0 || iB > m_vMonitor.size()) {
            PutModule("Illegal # requested");
            return;
        }
        std::iter_swap(m_vMonitor.begin() + iA - 1, m_vMonitor.begin() + iB - 1);
        PutModule("Swapped " + m_vMonitor[iA - 1] + " and " + m_vMonitor[iB - 1]);
        Save();
    }

    void Sort(const CString& sCommand) {
        std::sort(m_vMonitor.begin(), m_vMonitor.end(),
                  [](const CString& a, const CString& b) {
                      return a.AsLower() < b.AsLower();
                  });
        PutModule("Nicks sorted.");
        Save();
    }

    void Clear(const CString& sCommand) {
        m_vMonitor.clear();
        if (m_pNetwork->IsIRCConnected())
            SendMonitorClear();
        Save();
        PutModule("Monitor list cleared.");
    }

    static const size_t MAX_LIST_LEN = 500;  // 510 - len("MONITOR + ")

    void SendMonitorAdd(const VCString& nicks) {
        if (nicks.empty()) return;
        CString current;
        for (const CString& nick : nicks) {
            size_t additional = nick.size() + (current.empty() ? 0 : 1);  // comma if needed
            if (current.size() + additional <= MAX_LIST_LEN) {
                if (!current.empty()) current += ",";
                current += nick;
            } else {
                // current batch is full, send it
                PutIRC("MONITOR + " + current);
                current = nick;  // start new batch
            }
        }
        if (!current.empty()) {
            PutIRC("MONITOR + " + current);
        }
    }

    void SendMonitorDel(const VCString& nicks) {
        if (nicks.empty()) return;
        CString current;
        for (const CString& nick : nicks) {
            size_t additional = nick.size() + (current.empty() ? 0 : 1);
            if (current.size() + additional <= MAX_LIST_LEN) {
                if (!current.empty()) current += ",";
                current += nick;
            } else {
                PutIRC("MONITOR - " + current);
                current = nick;
            }
        }
        if (!current.empty()) {
            PutIRC("MONITOR - " + current);
        }
    }

    void SendMonitorClear() {
        PutIRC("MONITOR C");
    }

    void Save() {
        CString sBuffer;
        for (const CString& sNick : m_vMonitor) {
            sBuffer += sNick + "\n";
        }
        SetNV("Monitor", sBuffer);
    }

    VCString m_vMonitor;
    unsigned int m_uNickLen = 30;
    unsigned int m_uMonitorLimit = 0;
};

template <>
void TModInfo<CMonitor>(CModInfo& Info) {
    //	Info.SetWikiPage("monitor");
    Info.SetHasArgs(false);
    Info.AddType(CModInfo::NetworkModule);
}

NETWORKMODULEDEFS(CMonitor, "IRCv3 MONITOR support.")
