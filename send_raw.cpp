/*
 * Copyright (C) 2004-2025 ZNC, see the NOTICE file for details.
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

#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <znc/IRCSock.h>

using std::vector;
using std::map;

class CSendRaw_Mod : public CModule {
    void SendClient(const CString& sLine) {
        CUser* pUser =
            FindUser(sLine.Token(1));

        if (pUser) {
            CIRCNetwork* pNetwork =
                FindNetwork(pUser, sLine.Token(2));

            if (pNetwork) {
                pNetwork->PutUser(sLine.Token(3, true));
                PutModule(t_f("Sent [{1}] to {2}/{3}")(sLine.Token(3, true),
                                                       pUser->GetUsername(),
                                                       pNetwork->GetName()));
            }
        }
    }

    void SendServer(const CString& sLine) {
        CUser* pUser =
            FindUser(sLine.Token(1));

        if (pUser) {
            CIRCNetwork* pNetwork =
                FindNetwork(pUser, sLine.Token(2));

            if (pNetwork) {
                pNetwork->PutIRC(sLine.Token(3, true));
                PutModule(t_f("Sent [{1}] to IRC server of {2}/{3}")(
                    sLine.Token(3, true), pUser->GetUsername(),
                    pNetwork->GetName()));
            }
        }
    }

    void SendZNC(const CString& sLine) {
        if (!GetUser()->IsAdmin()) {
            PutModule("Access denied!");
            return;
        }
        CUser* pUser = CZNC::Get().FindUser(sLine.Token(1));

        if (pUser) {
            CIRCNetwork* pNetwork = pUser->FindNetwork(sLine.Token(2));

            if (pNetwork) {
            // This processes the message as if it came FROM the IRC server
            pNetwork->GetIRCSock()->ReadLine(sLine.Token(3, true));
                PutModule(t_f("Injected [{1}] from IRC server to {2}/{3}")(
                    sLine.Token(3, true), pUser->GetUsername(),
                    pNetwork->GetName()));
            } else {
                PutModule(t_f("Network {1} not found for user {2}")(
                    sLine.Token(2), sLine.Token(1)));
            }
        } else {
            PutModule(t_f("User {1} not found")(sLine.Token(1)));
        }
    }

    void CurrentClient(const CString& sLine) {
        CString sData = sLine.Token(1, true);
        GetClient()->PutClient(sData);
    }

    CUser* FindUser(const CString& sUsername) {
        if (sUsername.Equals("$me") || sUsername.Equals("$user"))
            return GetUser();

        CUser* pUser = CZNC::Get().FindUser(sUsername);
        if (!pUser) {
            PutModule(t_f("User [{1}] not found")(sUsername));
            return nullptr;
        }

        // For non-admin users, only allow access to themselves
        if (pUser != GetUser() && !GetUser()->IsAdmin()) {
            PutModule(t_s(
                "You need to have admin rights to modify other users"));
            return nullptr;
        }
        return pUser;
    }

    CIRCNetwork* FindNetwork(CUser* pUser, const CString& sNetwork) {
        if (sNetwork.Equals("$net") || sNetwork.Equals("$network")) {
            if (pUser != GetUser()) {
                PutModule(t_s(
                    "You cannot use $network to modify other users!"));
                return nullptr;
            }
            return CModule::GetNetwork();
        }

        CIRCNetwork* pNetwork = pUser->FindNetwork(sNetwork);
        if (!pNetwork) {
            PutModule(
                t_f("User {1} does not have a network named [{2}].")(
                    pUser->GetUsername(), sNetwork));
        }
        return pNetwork;
    }

  public:
    ~CSendRaw_Mod() override {}

    bool OnLoad(const CString& sArgs, CString& sErrorMsg) override {
        return true;
    }

    CString GetWebMenuTitle() override { return t_s("Send Raw"); }

    bool OnWebRequest(CWebSock& WebSock, const CString& sPageName,
                      CTemplate& Tmpl) override {
        if (sPageName == "index") {
            if (WebSock.IsPost()) {
                CUser* pCurrentUser = GetUser();

                // For non-admin users, validate they're only accessing their
                // own data
                CString sRequestedUser =
                    WebSock.GetParam("network").Token(0, false, "/");
                if (!pCurrentUser->IsAdmin() &&
                    sRequestedUser != pCurrentUser->GetUsername()) {
                    WebSock.GetSession()->AddError(
                        t_s("You can only send to your own user"));
                    return true;
                }

                CUser* pUser = CZNC::Get().FindUser(
                    WebSock.GetParam("network").Token(0, false, "/"));
                if (!pUser) {
                    WebSock.GetSession()->AddError(t_s("User not found"));
                    return true;
                }

                CIRCNetwork* pNetwork = pUser->FindNetwork(
                    WebSock.GetParam("network").Token(1, false, "/"));
                if (!pNetwork) {
                    WebSock.GetSession()->AddError(t_s("Network not found"));
                    return true;
                }

                bool bToServer = WebSock.GetParam("send_to") == "server";
                const CString sLine = WebSock.GetParam("line");

                Tmpl["user"] = pUser->GetUsername();
                Tmpl[bToServer ? "to_server" : "to_client"] = "true";
                Tmpl["line"] = sLine;

                if (bToServer) {
                    pNetwork->PutIRC(sLine);
                } else {
                    pNetwork->PutUser(sLine);
                }

                WebSock.GetSession()->AddSuccess(t_s("Line sent"));
            }

            CUser* pCurrentUser = GetUser();

            if (pCurrentUser->IsAdmin()) {
                // Admin users can see all users
                const map<CString, CUser*>& msUsers = CZNC::Get().GetUserMap();
                for (const auto& it : msUsers) {
                    CTemplate& l = Tmpl.AddRow("UserLoop");
                    l["Username"] = it.second->GetUsername();

                    vector<CIRCNetwork*> vNetworks = it.second->GetNetworks();
                    for (const CIRCNetwork* pNetwork : vNetworks) {
                        CTemplate& NetworkLoop = l.AddRow("NetworkLoop");
                        NetworkLoop["Username"] = it.second->GetUsername();
                        NetworkLoop["Network"] = pNetwork->GetName();
                    }
                }
            } else {
                // Non-admin users can only see their own networks
                CTemplate& l = Tmpl.AddRow("UserLoop");
                l["Username"] = pCurrentUser->GetUsername();

                vector<CIRCNetwork*> vNetworks = pCurrentUser->GetNetworks();
                for (const CIRCNetwork* pNetwork : vNetworks) {
                    CTemplate& NetworkLoop = l.AddRow("NetworkLoop");
                    NetworkLoop["Username"] = pCurrentUser->GetUsername();
                    NetworkLoop["Network"] = pNetwork->GetName();
                }
            }

            return true;
        }

        return false;
    }

    MODCONSTRUCTOR(CSendRaw_Mod) {
        AddHelpCommand();

        CString sUserRef =
            GetUser()->IsAdmin() ? t_s("the user's") : t_s("your");
        CString sYouRef =
            GetUser()->IsAdmin() ? t_s("the user is") : t_s("you are");

        AddCommand(
            "Client", t_d("[user] [network] [data to send]"),
            t_d("The data will be sent to " + sUserRef + " IRC client(s)"),
            [=](const CString& sLine) { SendClient(sLine); });

        AddCommand("Server", t_d("[user] [network] [data to send]"),
                   t_d("The data will be sent to the IRC server " + sYouRef +
                       " connected to"),
                   [=](const CString& sLine) { SendServer(sLine); });
        AddCommand("Current", t_d("[data to send]"),
                   t_d("The data will be sent to your current client"),
                   [=](const CString& sLine) { CurrentClient(sLine); });
        AddCommand("ZNC", t_d("[user] [network] [data to send]"),
                   t_d("The data will be sent as if sent from the IRC server"),
                   [=](const CString& sLine) { SendZNC(sLine); });
    }
};

template <>
void TModInfo<CSendRaw_Mod>(CModInfo& Info) {
    Info.SetWikiPage("send_raw");
}

USERMODULEDEFS(CSendRaw_Mod, t_s("Lets you send raw IRC lines as yourself (and "
                                 "others if you are admin)"))
