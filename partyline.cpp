// Modified partyline.cpp
//
// Original partyline would put the partyline channels into all your networks.
// This modified version lets you pick use one (or more) networks you want to use.
// The /msg ?NICK ... is still sent to all network connections... I'll work on
//    that later... maybe.

/*
 * Copyright (C) 2004-2026 ZNC, see the NOTICE file for details.
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
#include <znc/User.h>

using std::map;
using std::set;
using std::vector;

// If you change these and it breaks, you get to keep the pieces
#define CHAN_PREFIX_1 "~"
#define CHAN_PREFIX_1C '~'
#define CHAN_PREFIX CHAN_PREFIX_1 "#"

#define NICK_PREFIX CString("?")
#define NICK_PREFIX_C '?'

struct SPartylineUser {
    CString sUsername;
    CIRCNetwork* pNetwork;

    bool operator<(const SPartylineUser& other) const {
        if (sUsername != other.sUsername) return sUsername < other.sUsername;
        return pNetwork < other.pNetwork;
    }
};

class CPartylineChannel {
  public:
    CPartylineChannel(const CString& sName) { m_sName = sName.AsLower(); }
    ~CPartylineChannel() {}

    const CString& GetTopic() const { return m_sTopic; }
    const CString& GetName() const { return m_sName; }

    const set<SPartylineUser>& GetUsers() const { return m_ssUsers; }

    void SetTopic(const CString& s) { m_sTopic = s; }

    void AddUser(const CString& sUsername, CIRCNetwork* pNetwork) {
        m_ssUsers.insert({sUsername, pNetwork});
    }

    void DelUser(const CString& sUsername, CIRCNetwork* pNetwork) {
        m_ssUsers.erase({sUsername, pNetwork});
    }

    bool IsInChannel(const CString& sUsername, CIRCNetwork* pNetwork) {
        return m_ssUsers.find({sUsername, pNetwork}) != m_ssUsers.end();
    }

    // Helper to get all networks for a user
    set<CIRCNetwork*> GetUserNetworks(const CString& sUsername) {
        set<CIRCNetwork*> networks;
        for (const auto& user : m_ssUsers) {
            if (user.sUsername == sUsername) {
                networks.insert(user.pNetwork);
            }
        }
        return networks;
    }

  protected:
    CString m_sTopic;
    CString m_sName;
    set<SPartylineUser> m_ssUsers;
};

class CPartylineMod : public CModule {
  public:
    void ListChannelsCommand(const CString& sLine) {
        if (m_ssChannels.empty()) {
            PutModule(t_s("There are no open channels."));
            return;
        }

        CTable Table;

        Table.AddColumn(t_s("Channel"));
        Table.AddColumn(t_s("Users"));

        for (set<CPartylineChannel*>::const_iterator a = m_ssChannels.begin();
             a != m_ssChannels.end(); ++a) {
            Table.AddRow();

            Table.SetCell(t_s("Channel"), (*a)->GetName());
            Table.SetCell(t_s("Users"), CString((*a)->GetUsers().size()));
        }

        PutModule(Table);
    }

    MODCONSTRUCTOR(CPartylineMod) {
        AddHelpCommand();
        AddCommand("List", "", t_d("List all open channels"),
                   [=](const CString& sLine) { ListChannelsCommand(sLine); });
    }

    ~CPartylineMod() override {
        // Kick all clients who are in partyline channels
        for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin();
             it != m_ssChannels.end(); ++it) {
            const set<SPartylineUser>& ssUsers = (*it)->GetUsers();

            for (set<SPartylineUser>::const_iterator it2 = ssUsers.begin();
                 it2 != ssUsers.end(); ++it2) {
                CUser* pUser = CZNC::Get().FindUser(it2->sUsername);
                if (!pUser) continue;

                vector<CClient*> vClients = pUser->GetAllClients();

                for (vector<CClient*>::const_iterator it3 = vClients.begin();
                     it3 != vClients.end(); ++it3) {
                    CClient* pClient = *it3;
                    // Only kick clients on the network where the user joined the channel
                    if (pClient->GetNetwork() == it2->pNetwork) {
                        pClient->PutClient(":*" + GetModName() +
                                           "!znc@znc.in KICK " + (*it)->GetName() +
                                           " " + pClient->GetNick() + " :" +
                                           GetModName() + " unloaded");
                    }
                }
            }
        }

        while (!m_ssChannels.empty()) {
            delete *m_ssChannels.begin();
            m_ssChannels.erase(m_ssChannels.begin());
        }
    }

    bool OnBoot() override {
        // The config is now read completely, so all Users are set up
        Load();

        return true;
    }

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();

        for (map<CString, CUser*>::const_iterator it = msUsers.begin();
             it != msUsers.end(); ++it) {
            CUser* pUser = it->second;
            for (CClient* pClient : pUser->GetAllClients()) {
                CIRCNetwork* pNetwork = pClient->GetNetwork();
                if (!pNetwork || !pNetwork->IsIRCConnected() ||
                    !pNetwork->GetChanPrefixes().Contains(CHAN_PREFIX_1)) {
                    pClient->PutClient(
                        ":" + GetIRCServer(pNetwork) + " 005 " +
                        pClient->GetNick() + " CHANTYPES=" +
                        (pNetwork ? pNetwork->GetChanPrefixes() : "") +
                        CHAN_PREFIX_1 " :are supported by this server.");
                }
            }
        }

        VCString vsChans;
        VCString::const_iterator it;
        sArgs.Split(" ", vsChans, false);

        for (it = vsChans.begin(); it != vsChans.end(); ++it) {
            if (it->Left(2) == CHAN_PREFIX) {
                m_ssDefaultChans.insert(it->Left(32));
            }
        }

        Load();

        return true;
    }

    void Load() {
        CString sAction, sKey;
        CPartylineChannel* pChannel;
        for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
            if (it->first.find(":") != CString::npos) {
                sAction = it->first.Token(0, false, ":");
                sKey = it->first.Token(1, true, ":");
            } else {
                // backwards compatibility for older NV data
                sAction = "fixedchan";
                sKey = it->first;
            }

            if (sAction == "fixedchan") {
                // Sorry, this was removed
            }

            if (sAction == "topic") {
                pChannel = FindChannel(sKey);
                if (pChannel && !(it->second).empty()) {

                    PutChan(pChannel->GetUsers(), ":irc.znc.in TOPIC " +
                                                      pChannel->GetName() +
                                                      " :" + it->second);
                    pChannel->SetTopic(it->second);
                }
            }
        }

        return;
    }

    void SaveTopic(CPartylineChannel* pChannel) {
        if (!pChannel->GetTopic().empty())
            SetNV("topic:" + pChannel->GetName(), pChannel->GetTopic());
        else
            DelNV("topic:" + pChannel->GetName());
    }

    EModRet OnDeleteUser(CUser& User) override {
        // Loop through each chan
        for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin();
             it != m_ssChannels.end();) {
            CPartylineChannel* pChan = *it;
            // RemoveUser() might delete channels, so make sure our
            // iterator doesn't break.
            ++it;

            // Remove user from all networks they're in this channel
            set<CIRCNetwork*> networks = pChan->GetUserNetworks(User.GetUserName());
            for (CIRCNetwork* pNetwork : networks) {
                RemoveUser(&User, pNetwork, pChan, "KICK", "User deleted", true);
            }
        }
        return CONTINUE;
    }

    EModRet OnNumericMessage(CNumericMessage& Msg) override {
        if (Msg.GetCode() == 5) {
            for (int i = 0; i < Msg.GetParams().size(); ++i) {
                if (Msg.GetParams()[i].StartsWith("CHANTYPES=")) {
                    Msg.SetParam(i, Msg.GetParam(i) + CHAN_PREFIX_1);
                    m_spInjectedPrefixes.insert(GetNetwork());
                    break;
                }
            }
        }

        return CONTINUE;
    }

    void OnIRCDisconnected() override {
        m_spInjectedPrefixes.erase(GetNetwork());
    }

    void OnClientLogin() override {
        CUser* pUser = GetUser();
        CClient* pClient = GetClient();
        CIRCNetwork* pNetwork = GetNetwork();
        if (!pNetwork || !pNetwork->IsIRCConnected()) {
            pClient->PutClient(":" + GetIRCServer(pNetwork) + " 005 " +
                               pClient->GetNick() + " CHANTYPES=" +
                               CHAN_PREFIX_1 " :are supported by this server.");
        }

        // Only join default channels on this specific network
        for (const CString& sChan : m_ssDefaultChans) {
            CPartylineChannel* pChannel = GetChannel(sChan);

            if (!pChannel->IsInChannel(pUser->GetUserName(), pNetwork)) {
                JoinUser(pUser, pNetwork, pChannel);
            }
        }

        // Show existing channels on this network
        CString sNickMask = pClient->GetNickMask();
        for (CPartylineChannel* pChannel : m_ssChannels) {
            if (pChannel->IsInChannel(pUser->GetUserName(), pNetwork)) {
                pClient->PutClient(":" + sNickMask + " JOIN " + pChannel->GetName());

                if (!pChannel->GetTopic().empty()) {
                    pClient->PutClient(":" + GetIRCServer(pNetwork) + " 332 " +
                                       pClient->GetNickMask() + " " +
                                       pChannel->GetName() + " :" +
                                       pChannel->GetTopic());
                }

                SendNickList(pUser, pNetwork, pChannel->GetUsers(), pChannel->GetName());
            }
        }
    }

    void OnClientDisconnect() override {
        CUser* pUser = GetUser();
        CIRCNetwork* pNetwork = GetNetwork();

        if (!pUser->IsUserAttached() && !pUser->IsBeingDeleted()) {
            for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin();
                 it != m_ssChannels.end(); ++it) {
                const set<SPartylineUser>& ssUsers = (*it)->GetUsers();

                if (ssUsers.find({pUser->GetUserName(), pNetwork}) != ssUsers.end()) {
                    PutChan(ssUsers,
                            ":*" + GetModName() + "!znc@znc.in MODE " +
                                (*it)->GetName() + " -ov " + NICK_PREFIX +
                                pUser->GetUserName() + " " + NICK_PREFIX +
                                pUser->GetUserName(),
                            false, true, pUser, nullptr, pNetwork);
                }
            }
        }
    }

    EModRet OnUserRawMessage(CMessage& Msg) override {
        if ((Msg.GetCommand().Equals("WHO") ||
             Msg.GetCommand().Equals("MODE")) &&
            Msg.GetParam(0).StartsWith(CHAN_PREFIX_1)) {
            return HALT;
        } else if (Msg.GetCommand().Equals("TOPIC") &&
                   Msg.GetParam(0).StartsWith(CHAN_PREFIX)) {
            const CString sChannel = Msg.As<CTopicMessage>().GetTarget();
            CString sTopic = Msg.As<CTopicMessage>().GetText();

            sTopic.TrimPrefix(":");

            CUser* pUser = GetUser();
            CClient* pClient = GetClient();
            CIRCNetwork* pNetwork = GetNetwork();
            CPartylineChannel* pChannel = FindChannel(sChannel);

            if (pChannel && pChannel->IsInChannel(pUser->GetUserName(), pNetwork)) {
                const set<SPartylineUser>& ssUsers = pChannel->GetUsers();
                if (!sTopic.empty()) {
                    if (pUser->IsAdmin()) {
                        PutChan(ssUsers, ":" + pClient->GetNickMask() + " TOPIC " + sChannel + " :" + sTopic, true, false, pUser, nullptr, pNetwork);
                        pChannel->SetTopic(sTopic);
                        SaveTopic(pChannel);
                    } else {
                        pUser->PutUser(":irc.znc.in 482 " + pClient->GetNick() +
                                       " " + sChannel +
                                       " :You're not channel operator");
                    }
                } else {
                    sTopic = pChannel->GetTopic();

                    if (sTopic.empty()) {
                        pUser->PutUser(":irc.znc.in 331 " + pClient->GetNick() +
                                       " " + sChannel + " :No topic is set.");
                    } else {
                        pUser->PutUser(":irc.znc.in 332 " + pClient->GetNick() +
                                       " " + sChannel + " :" + sTopic);
                    }
                }
            } else {
                pUser->PutUser(":irc.znc.in 442 " + pClient->GetNick() + " " +
                               sChannel + " :You're not on that channel");
            }
            return HALT;
        }

        return CONTINUE;
    }

    EModRet OnUserPart(CString& sChannel, CString& sMessage) override {
        if (sChannel.Left(1) != CHAN_PREFIX_1) {
            return CONTINUE;
        }

        if (sChannel.Left(2) != CHAN_PREFIX) {
            GetClient()->PutClient(":" + GetIRCServer(GetNetwork()) + " 401 " +
                                   GetClient()->GetNick() + " " + sChannel +
                                   " :No such channel");
            return HALT;
        }

        CPartylineChannel* pChannel = FindChannel(sChannel);
        CIRCNetwork* pNetwork = GetNetwork();

        PartUser(GetUser(), pNetwork, pChannel, sMessage);

        return HALT;
    }

    void PartUser(CUser* pUser, CIRCNetwork* pNetwork, CPartylineChannel* pChannel,
                  const CString& sMessage = "") {
        RemoveUser(pUser, pNetwork, pChannel, "PART", sMessage);
    }

    void RemoveUser(CUser* pUser, CIRCNetwork* pNetwork, CPartylineChannel* pChannel,
                    const CString& sCommand, const CString& sMessage = "",
                    bool bNickAsTarget = false) {
        if (!pChannel || !pChannel->IsInChannel(pUser->GetUserName(), pNetwork)) {
            return;
        }

        // Remove user from specific network
        pChannel->DelUser(pUser->GetUserName(), pNetwork);

        // Only send PART to clients on this network
        CString sCmd = " " + sCommand + " ";
        CString sMsg = sMessage.empty() ? "" : " :" + sMessage;

        for (CClient* pClient : pUser->GetAllClients()) {
            if (pClient->GetNetwork() == pNetwork) {
                if (bNickAsTarget) {
                    pClient->PutClient(":" + pClient->GetNickMask() + sCmd +
                                       pChannel->GetName() + " " +
                                       pClient->GetNick() + sMsg);
                } else {
                    pClient->PutClient(":" + pClient->GetNickMask() + sCmd +
                                       pChannel->GetName() + sMsg);
                }
            }
        }

        // Notify others on the same network
        CString sHost = pUser->GetBindHost();
        if (sHost.empty()) {
            sHost = "znc.in";
        }

        CString sPartMsg = ":" + NICK_PREFIX + pUser->GetUserName() + "!" +
                           pUser->GetIdent() + "@" + sHost + sCmd +
                           pChannel->GetName();

        if (bNickAsTarget) {
            sPartMsg += " " + NICK_PREFIX + pUser->GetUserName();
        }
        sPartMsg += sMsg;

        PutChan(pChannel->GetUsers(), sPartMsg, false, true, pUser, nullptr, pNetwork);

        // Rejoin if default channel and not being deleted
        if (!pUser->IsBeingDeleted() &&
            m_ssDefaultChans.find(pChannel->GetName()) != m_ssDefaultChans.end()) {
            JoinUser(pUser, pNetwork, pChannel);
        }

        // Delete channel if empty across all networks
        if (pChannel->GetUsers().empty()) {
            delete pChannel;
            m_ssChannels.erase(pChannel);
        }
    }

    EModRet OnUserJoin(CString& sChannel, CString& sKey) override {
        if (sChannel.Left(1) != CHAN_PREFIX_1) {
            return CONTINUE;
        }

        if (sChannel.Left(2) != CHAN_PREFIX) {
            GetClient()->PutClient(":" + GetIRCServer(GetNetwork()) + " 403 " +
                                   GetClient()->GetNick() + " " + sChannel +
                                   " :Channels look like " CHAN_PREFIX "znc");
            return HALT;
        }

        sChannel = sChannel.Left(32);
        CPartylineChannel* pChannel = GetChannel(sChannel);
        CIRCNetwork* pNetwork = GetNetwork();

        JoinUser(GetUser(), pNetwork, pChannel);

        return HALT;
    }

    void JoinUser(CUser* pUser, CIRCNetwork* pNetwork, CPartylineChannel* pChannel) {
        if (!pChannel || pChannel->IsInChannel(pUser->GetUserName(), pNetwork)) {
            return;
        }

        // Add user with specific network
        pChannel->AddUser(pUser->GetUserName(), pNetwork);

        // Only send JOIN to clients on this network
        for (CClient* pClient : pUser->GetAllClients()) {
            if (pClient->GetNetwork() == pNetwork) {
                pClient->PutClient(":" + pClient->GetNickMask() + " JOIN " +
                                   pChannel->GetName());
            }
        }

        // Notify other users on the same network
        CString sHost = pUser->GetBindHost();
        if (sHost.empty()) {
            sHost = "znc.in";
        }

        CString sJoinMsg = ":" + NICK_PREFIX + pUser->GetUserName() + "!" +
                           pUser->GetIdent() + "@" + sHost + " JOIN " +
                           pChannel->GetName();

        PutChan(pChannel->GetUsers(), sJoinMsg, false, true, pUser, nullptr, pNetwork);

        // Send topic and names list only to clients on this network
        if (!pChannel->GetTopic().empty()) {
            for (CClient* pClient : pUser->GetAllClients()) {
                if (pClient->GetNetwork() == pNetwork) {
                    pClient->PutClient(":" + GetIRCServer(pNetwork) + " 332 " +
                                       pClient->GetNickMask() + " " +
                                       pChannel->GetName() + " :" +
                                       pChannel->GetTopic());
                }
            }
        }

        SendNickList(pUser, pNetwork, pChannel->GetUsers(), pChannel->GetName());

        // Set modes only on this network
        if (pUser->IsAdmin()) {
            PutChan(pChannel->GetUsers(), ":*" + GetModName() + "!znc@znc.in MODE " + pChannel->GetName() + " +o " + NICK_PREFIX + pUser->GetUserName(),
                    false, false, pUser, nullptr, pNetwork);
        }
        PutChan(pChannel->GetUsers(), ":*" + GetModName() + "!znc@znc.in MODE " + pChannel->GetName() + " +v " + NICK_PREFIX + pUser->GetUserName(),
                false, false, pUser, nullptr, pNetwork);
    }

    EModRet HandleMessage(const CString& sCmd, const CString& sTarget,
                          const CString& sMessage) {
        if (sTarget.empty()) {
            return CONTINUE;
        }

        char cPrefix = sTarget[0];
        if (cPrefix != CHAN_PREFIX_1C && cPrefix != NICK_PREFIX_C) {
            return CONTINUE;
        }

        CUser* pUser = GetUser();
        CClient* pClient = GetClient();
        CIRCNetwork* pNetwork = GetNetwork();
        CString sHost = pUser->GetBindHost();
        if (sHost.empty()) {
            sHost = "znc.in";
        }

        if (cPrefix == CHAN_PREFIX_1C) {
            CPartylineChannel* pChannel = FindChannel(sTarget);
            if (!pChannel) {
                pClient->PutClient(":" + GetIRCServer(pNetwork) + " 401 " +
                                   pClient->GetNick() + " " + sTarget +
                                   " :No such channel");
                return HALT;
            }

            // Check if user is in channel on this network
            if (!pChannel->IsInChannel(pUser->GetUserName(), pNetwork)) {
                pClient->PutClient(":" + GetIRCServer(pNetwork) + " 442 " +
                                   pClient->GetNick() + " " + sTarget +
                                   " :You're not on that channel");
                return HALT;
            }

            CString sMsg = ":" + NICK_PREFIX + pUser->GetUserName() + "!" +
                           pUser->GetIdent() + "@" + sHost + " " + sCmd +
                           " " + sTarget + " :" + sMessage;

            // Send to all users in channel (cross-network)
            PutChan(pChannel->GetUsers(), sMsg, true, false, pUser, nullptr, pNetwork);
        } else {
            // Private message handling remains similar
            CString sNick = sTarget.LeftChomp_n(1);
            CUser* pTargetUser = CZNC::Get().FindUser(sNick);

            if (pTargetUser) {
                vector<CClient*> vClients = pTargetUser->GetAllClients();
                if (vClients.empty()) {
                    pClient->PutClient(":" + GetIRCServer(pNetwork) + " 401 " +
                                       pClient->GetNick() + " " + sTarget +
                                       " :User is not attached: " + sNick);
                    return HALT;
                }

                for (CClient* pTarget : vClients) {
                    pTarget->PutClient(":" + NICK_PREFIX + pUser->GetUserName() + "!" +
                                       pUser->GetIdent() + "@" + sHost + " " + sCmd +
                                       " " + pTarget->GetNick() + " :" + sMessage);
                }
            } else {
                pClient->PutClient(":" + GetIRCServer(pNetwork) + " 401 " +
                                   pClient->GetNick() + " " + sTarget +
                                   " :No such znc user: " + sNick);
            }
        }

        return HALT;
    }

    EModRet OnUserMsg(CString& sTarget, CString& sMessage) override {
        return HandleMessage("PRIVMSG", sTarget, sMessage);
    }

    EModRet OnUserNotice(CString& sTarget, CString& sMessage) override {
        return HandleMessage("NOTICE", sTarget, sMessage);
    }

    EModRet OnUserCTCP(CString& sTarget, CString& sMessage) override {
        return HandleMessage("PRIVMSG", sTarget, "\001" + sMessage + "\001");
    }

    EModRet OnUserCTCPReply(CString& sTarget, CString& sMessage) override {
        return HandleMessage("NOTICE", sTarget, "\001" + sMessage + "\001");
    }

    const CString GetIRCServer(CIRCNetwork* pNetwork) {
        if (!pNetwork) {
            return "irc.znc.in";
        }

        const CString& sServer = pNetwork->GetIRCServer();
        if (!sServer.empty()) return sServer;
        return "irc.znc.in";
    }

    bool PutChan(const CString& sChan, const CString& sLine,
                 bool bIncludeCurUser = true, bool bIncludeClient = true,
                 CUser* pUser = nullptr, CClient* pClient = nullptr) {
        CPartylineChannel* pChannel = FindChannel(sChan);

        if (pChannel != nullptr) {
            PutChan(pChannel->GetUsers(), sLine, bIncludeCurUser,
                    bIncludeClient, pUser, pClient, GetNetwork());
            return true;
        }

        return false;
    }

    void PutChan(const set<SPartylineUser>& ssUsers, const CString& sLine,
                 bool bIncludeCurUser = true, bool bIncludeClient = true,
                 CUser* pUser = nullptr, CClient* pClient = nullptr,
                 CIRCNetwork* pSenderNetwork = nullptr) {
        const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();

        if (!pUser) pUser = GetUser();
        if (!pClient) pClient = GetClient();

        for (const SPartylineUser& partylineUser : ssUsers) {
            CUser* pTargetUser = CZNC::Get().FindUser(partylineUser.sUsername);
            if (!pTargetUser) continue;

            // Check if this is the sender
            bool isSender = (pTargetUser == pUser);

            if (isSender && !bIncludeCurUser) {
                continue;
            }

            // Get all clients for this user
            vector<CClient*> vClients = pTargetUser->GetAllClients();

            for (CClient* pTargetClient : vClients) {
                // For the sender, check if we should exclude this specific client
                if (isSender && !bIncludeClient && pTargetClient == pClient) {
                    continue;
                }

                // Send the message to this client
                pTargetClient->PutClient(sLine);
            }
        }
    }

    void PutUserIRCNick(CUser* pUser, const CString& sPre,
                        const CString& sPost) {
        const vector<CClient*>& vClients = pUser->GetAllClients();
        vector<CClient*>::const_iterator it;
        for (it = vClients.begin(); it != vClients.end(); ++it) {
            (*it)->PutClient(sPre + (*it)->GetNick() + sPost);
        }
    }

    void SendNickList(CUser* pUser, CIRCNetwork* pNetwork,
                      const set<SPartylineUser>& ssUsers, const CString& sChan) {
        CString sNickList;

        for (const SPartylineUser& partylineUser : ssUsers) {
            CUser* pChanUser = CZNC::Get().FindUser(partylineUser.sUsername);

            if (pChanUser == pUser) {
                continue;
            }

            if (pChanUser && pChanUser->IsUserAttached()) {
                sNickList += (pChanUser->IsAdmin()) ? "@" : "+";
            }

            sNickList += NICK_PREFIX + partylineUser.sUsername + " ";

            if (sNickList.size() >= 500) {
                PutUserIRCNick(pUser, ":" + GetIRCServer(pNetwork) + " 353 ",
                               " @ " + sChan + " :" + sNickList);
                sNickList.clear();
            }
        }

        if (sNickList.size()) {
            PutUserIRCNick(pUser, ":" + GetIRCServer(pNetwork) + " 353 ",
                           " @ " + sChan + " :" + sNickList);
        }

        vector<CClient*> vClients = pUser->GetAllClients();
        for (vector<CClient*>::const_iterator it = vClients.begin();
             it != vClients.end(); ++it) {
            CClient* pClient = *it;
            pClient->PutClient(":" + GetIRCServer(pNetwork) + " 353 " +
                               pClient->GetNick() + " @ " + sChan + " :" +
                               ((pUser->IsAdmin()) ? "@" : "+") +
                               pClient->GetNick());
        }

        PutUserIRCNick(pUser, ":" + GetIRCServer(pNetwork) + " 366 ",
                       " " + sChan + " :End of /NAMES list.");
    }

    CPartylineChannel* FindChannel(const CString& sChan) {
        CString sChannel = sChan.AsLower();

        for (set<CPartylineChannel*>::iterator it = m_ssChannels.begin();
             it != m_ssChannels.end(); ++it) {
            if ((*it)->GetName().AsLower() == sChannel) return *it;
        }

        return nullptr;
    }

    CPartylineChannel* GetChannel(const CString& sChannel) {
        CPartylineChannel* pChannel = FindChannel(sChannel);

        if (!pChannel) {
            pChannel = new CPartylineChannel(sChannel.AsLower());
            m_ssChannels.insert(pChannel);
        }

        return pChannel;
    }

  private:
    set<CPartylineChannel*> m_ssChannels;
    set<CIRCNetwork*> m_spInjectedPrefixes;
    set<CString> m_ssDefaultChans;
};

template <>
void TModInfo<CPartylineMod>(CModInfo& Info) {
    Info.SetWikiPage("partyline");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(
        Info.t_s("You may enter a list of channels the user joins, when "
                 "entering the internal partyline."));
}

GLOBALMODULEDEFS(
    CPartylineMod,
    t_s("Internal channels and queries for users connected to ZNC"))
