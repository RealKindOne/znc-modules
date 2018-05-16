// notice.cpp

// Module rejects all notices unless the sender is added.
// Module also converts the notices into PRIVMSG's because notices annoy me.

/*
 * Copyright (C) 2004-2016 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/User.h>

using std::map;
using std::set;
using std::vector;

class CnoticeMod;

class CnoticeUser {
  public:
    CnoticeUser() {}

    CnoticeUser(const CString& sLine) { FromString(sLine); }

    CnoticeUser(const CString& sUsername, const CString& sHostmasks)
        : m_sUsername(sUsername) {
        AddHostmasks(sHostmasks);
    }

    virtual ~CnoticeUser() {}

    const CString& GetUsername() const { return m_sUsername; }

    bool HostMatches(const CString& sHostmask) {
        for (const CString& s : m_ssHostmasks) {
            if (sHostmask.WildCmp(s, CString::CaseInsensitive)) {
                return true;
            }
        }
        return false;
    }

    CString GetHostmasks() const {
        return CString(",").Join(m_ssHostmasks.begin(), m_ssHostmasks.end());
    }

    bool DelHostmasks(const CString& sHostmasks) {
        VCString vsHostmasks;
        sHostmasks.Split(",", vsHostmasks);

        for (const CString& s : vsHostmasks) {
            m_ssHostmasks.erase(s);
        }

        return m_ssHostmasks.empty();
    }

    void AddHostmasks(const CString& sHostmasks) {
        VCString vsHostmasks;
        sHostmasks.Split(",", vsHostmasks);

        for (const CString& s : vsHostmasks) {
            m_ssHostmasks.insert(s);
        }
    }

    CString ToString() const {
          return m_sUsername + "\t" + GetHostmasks();
    }

    bool FromString(const CString& sLine) {
        m_sUsername = sLine.Token(0, false, "\t");
        sLine.Token(1, false, "\t").Split(",", m_ssHostmasks);
	return true;
    }


  private:
  protected:
    CString m_sUsername;
    set<CString> m_ssHostmasks;
};

class CnoticeMod : public CModule {
  public:
    MODCONSTRUCTOR(CnoticeMod) {
        AddHelpCommand();
        AddCommand("ListUsers", static_cast<CModCommand::ModCmdFunc>( &CnoticeMod::OnListUsersCommand), "", "List all users");
        AddCommand("AddMasks", static_cast<CModCommand::ModCmdFunc>( &CnoticeMod::OnAddMasksCommand), "<user> <mask>,[mask] ...", "Adds masks to a user");
        AddCommand("DelMasks", static_cast<CModCommand::ModCmdFunc>( &CnoticeMod::OnDelMasksCommand), "<user> <mask>,[mask] ...", "Removes masks from a user");
        AddCommand("AddUser", static_cast<CModCommand::ModCmdFunc>( &CnoticeMod::OnAddUserCommand), "<user> <hostmask>[,<hostmasks>...]", "Adds a user");
        AddCommand("DelUser", static_cast<CModCommand::ModCmdFunc>( &CnoticeMod::OnDelUserCommand), "<user>", "Removes a user");
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {

        // Load the users
        for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
            const CString& sLine = it->second;
            CnoticeUser* pUser = new CnoticeUser;

            if (!pUser->FromString(sLine) ||
                FindUser(pUser->GetUsername().AsLower())) {
                delete pUser;
            } else {
                m_msUsers[pUser->GetUsername().AsLower()] = pUser;
            }
        }

        return true;
    }

    ~CnoticeMod() override {
        for (const auto& it : m_msUsers) {
            delete it.second;
        }
        m_msUsers.clear();
    }

    virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage) {
        for (const auto& it : m_msUsers) {
            if (it.second->HostMatches(Nick.GetNickMask())) {
                PutUser(":" + Nick.GetNickMask() + " PRIVMSG " + GetUser()->GetNick() + " :" + sMessage);
                break;
            }
        }
        return HALTCORE;
    }

    void OnModCommand(const CString& sLine) override {
        CString sCommand = sLine.Token(0).AsUpper();
            HandleCommand(sLine);
    }

    void OnAddUserCommand(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CString sHost = sLine.Token(2);

        if (sHost.empty()) {
            PutModule("Usage: AddUser <user> <hostmask>[,<hostmasks>...]");
        } else {
              CnoticeUser* pUser = AddUser(sUser, sHost);

            if (pUser) {
                SetNV(sUser, pUser->ToString());
            }
        }
    }

    void OnDelUserCommand(const CString& sLine) {
        CString sUser = sLine.Token(1);

        if (sUser.empty()) {
            PutModule("Usage: DelUser <user>");
        } else {
            DelUser(sUser);
            DelNV(sUser);
        }
    }

    void OnListUsersCommand(const CString& sLine) {
        if (m_msUsers.empty()) {
            PutModule("There are no users defined");
            return;
        }

        CTable Table;

        Table.AddColumn("User");
        Table.AddColumn("Hostmasks");

        for (const auto& it : m_msUsers) {
            VCString vsHostmasks;
            it.second->GetHostmasks().Split(",", vsHostmasks);
            for (unsigned int a = 0; a < vsHostmasks.size(); a++) {
                Table.AddRow();
                if (a == 0) {
                    Table.SetCell("User", it.second->GetUsername());
                } else if (a == vsHostmasks.size() - 1) {
                    Table.SetCell("User", "`-");
                } else {
                    Table.SetCell("User", "|-");
                }
                Table.SetCell("Hostmasks", vsHostmasks[a]);
            }
        }

        PutModule(Table);
    }

    void OnAddMasksCommand(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CString sHostmasks = sLine.Token(2, true);

        if (sHostmasks.empty()) {
            PutModule("Usage: AddMasks <user> <mask>,[mask] ...");
            return;
        }

        CnoticeUser* pUser = FindUser(sUser);

        if (!pUser) {
            PutModule("No such user");
            return;
        }

        pUser->AddHostmasks(sHostmasks);
        PutModule("Hostmasks(s) added to user [" + pUser->GetUsername() + "]");
        SetNV(pUser->GetUsername(), pUser->ToString());
    }

    void OnDelMasksCommand(const CString& sLine) {
        CString sUser = sLine.Token(1);
        CString sHostmasks = sLine.Token(2, true);

        if (sHostmasks.empty()) {
            PutModule("Usage: DelMasks <user> <mask>,[mask] ...");
            return;
        }

        CnoticeUser* pUser = FindUser(sUser);

        if (!pUser) {
            PutModule("No such user");
            return;
        }

        if (pUser->DelHostmasks(sHostmasks)) {
            PutModule("Removed user [" + pUser->GetUsername() + "]");
            DelUser(sUser);
            DelNV(sUser);
        } else {
            PutModule("Hostmasks(s) Removed from user [" + pUser->GetUsername() + "]");
            SetNV(pUser->GetUsername(), pUser->ToString());
        }
    }

    CnoticeUser* FindUser(const CString& sUser) {
        map<CString, CnoticeUser*>::iterator it =
            m_msUsers.find(sUser.AsLower());

        return (it != m_msUsers.end()) ? it->second : nullptr;
    }

	CnoticeUser* FindUserByHost(const CString& sHostmask) {
        for (const auto& it : m_msUsers) {
            CnoticeUser* pUser = it.second;
            if (pUser->HostMatches(sHostmask)) {
                return pUser;
            }
        }

        return nullptr;
    }

	bool Checknotice(const CNick& Nick) {
		CnoticeUser* pUser = FindUserByHost(Nick.GetHostMask());

        if (!pUser) {
            return false;
        }

        return true;
    }

    void DelUser(const CString& sUser) {
        map<CString, CnoticeUser*>::iterator it =
            m_msUsers.find(sUser.AsLower());

        if (it == m_msUsers.end()) {
            PutModule("That user does not exist");
            return;
        }

        delete it->second;
        m_msUsers.erase(it);
        PutModule("User [" + sUser + "] removed");
    }

      CnoticeUser* AddUser(const CString& sUser, const CString& sHosts) {
        if (m_msUsers.find(sUser) != m_msUsers.end()) {
            PutModule("That user already exists");
            return nullptr;
        }

        CnoticeUser* pUser = new CnoticeUser(sUser, sHosts);
        m_msUsers[sUser.AsLower()] = pUser;
        PutModule("User [" + sUser + "] added with hostmask(s) [" + sHosts + "]");
        return pUser;
    }


  private:
    map<CString, CnoticeUser*> m_msUsers;
};


template <>
void TModInfo<CnoticeMod>(CModInfo& Info) {
//    Info.SetWikiPage("notice");
}

NETWORKMODULEDEFS(CnoticeMod, "Convert NOTICE's into PRIVMSG's.")
