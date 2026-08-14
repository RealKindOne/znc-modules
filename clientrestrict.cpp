// This module allows you to restrict commands from running
// on specific IRC clients using the 'Client ID' feature.

// Client ID login as 'admin' - For normal IRC client
// This client is exempt - No commands are blocked from the client.
// /server 127.0.0.1 9999 KindOne@admin/foobar:password

// Client ID login as 'phone' - For phone IRC client
// Commands from this client are blocked.
// /server 127.0.0.1 9999 KindOne@phone/foobar:password

// /msg *clientrestrict add KILL

#include <znc/Client.h>
#include <znc/Modules.h>

class CClientRestrictMod : public CModule {
    VCString m_vCmds;

    void Save() {
        SetNV("Commands", CString("\n").Join(m_vCmds.begin(), m_vCmds.end()));
    }

    void Add(const CString& sLine) {
        CString sCmd = sLine.Token(1, true);
        if (sCmd.empty()) {
            PutModule("Usage: add <command>");
            return;
        }
        m_vCmds.push_back(sCmd);
        PutModule("Added!");
        Save();
    }

    void Del(const CString& sLine) {
        u_int iNum = sLine.Token(1, true).ToUInt();
        if (iNum > m_vCmds.size() || iNum <= 0) {
            PutModule("Illegal # Requested");
            return;
        }
        m_vCmds.erase(m_vCmds.begin() + iNum - 1);
        PutModule("Command Erased.");
        Save();
    }

    void List(const CString& sLine) {
        CTable Table;
        unsigned int index = 1;
        Table.AddColumn("Id");
        Table.AddColumn("Command");

        for (const CString& sCmd : m_vCmds) {
            Table.AddRow();
            Table.SetCell("Id", CString(index++));
            Table.SetCell("Command", sCmd);
        }

        if (PutModule(Table) == 0) PutModule("No commands in list.");
    }

  public:
    MODCONSTRUCTOR(CClientRestrictMod) {
        AddHelpCommand();
        AddCommand("Add", "<command>", "Add a command",
                   [=](const CString& sLine) { Add(sLine); });
        AddCommand("Del", "<id>", "Delete a command by ID",
                   [=](const CString& sLine) { Del(sLine); });
        AddCommand("List", "", "List commands",
                   [=](const CString& sLine) { List(sLine); });
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        GetNV("Commands").Split("\n", m_vCmds, false);
        return true;
    }

    EModRet OnUserRawMessage(CMessage& Message) override {
        CString sCmd = Message.GetCommand();

        bool bRestricted = false;
        for (const CString& sStored : m_vCmds) {
            if (sCmd.Equals(sStored)) {
                bRestricted = true;
                break;
            }
        }

        if (bRestricted && GetClient() &&
            GetClient()->GetIdentifier() != "admin") {
            PutModule("Command '" + sCmd + "' blocked for this client");
            return HALT;
        }

        return CONTINUE;
    }
};
template <>
void TModInfo<CClientRestrictMod>(CModInfo& Info) {
    Info.AddType(CModInfo::NetworkModule);
}

NETWORKMODULEDEFS(CClientRestrictMod, "Restrict commands per client identifier")
