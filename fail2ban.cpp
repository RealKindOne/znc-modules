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

#include <znc/znc.h>
#include <znc/User.h>

class CFailToBanMod : public CModule {
  public:
    MODCONSTRUCTOR(CFailToBanMod) {
        AddHelpCommand();
        AddCommand(
            "Timeout", t_d("[minutes]"),
            t_d("The number of minutes IPs are blocked after a failed login."),
            [=](const CString& sLine) { OnTimeoutCommand(sLine); });
        AddCommand("Attempts", t_d("[count]"),
                   t_d("The number of allowed failed login attempts."),
                   [=](const CString& sLine) { OnAttemptsCommand(sLine); });
        AddCommand("Ban", t_d("<hosts>"), t_d("Ban the specified hosts."),
                   [=](const CString& sLine) { OnBanCommand(sLine); });
        AddCommand("Unban", t_d("<ID>|<hosts>"),
                   t_d("Unban the specified ID or host."),
                   [=](const CString& sLine) { OnUnbanCommand(sLine); });
        AddCommand("List", "", t_d("List banned hosts."),
                   [=](const CString& sLine) { OnListCommand(sLine); });
    }
    ~CFailToBanMod() override {}

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        CString sTimeout = sArgs.Token(0);
        CString sAttempts = sArgs.Token(1);
        unsigned int timeout = sTimeout.ToUInt();

        if (sAttempts.empty())
            m_uiAllowedFailed = 2;
        else
            m_uiAllowedFailed = sAttempts.ToUInt();

        if (sArgs.empty()) {
            timeout = 1;
        } else if (timeout == 0 || m_uiAllowedFailed == 0 ||
                   !sArgs.Token(2, true).empty()) {
            sMessage =
                t_s("Invalid argument, must be the number of minutes IPs are "
                    "blocked after a failed login and can be followed by "
                    "number of allowed failed login attempts");
            return false;
        }

        // SetTTL() wants milliseconds
        m_Cache.SetTTL(timeout * 60 * 1000);

        return true;
    }

    void OnPostRehash() override { m_Cache.Clear(); }

    void Add(const CString& sHost, unsigned int count) {
        time_t now = time(nullptr);
        BanInfo* pInfo = m_Cache.GetItem(sHost);

        if (pInfo) {
            pInfo->attempts = count;
            pInfo->last_attempt = now;
            // Keep the original first_attempt time
            m_Cache.AddItem(sHost, *pInfo, m_Cache.GetTTL());
        } else {
            BanInfo info = {count, now, now};
            m_Cache.AddItem(sHost, info, m_Cache.GetTTL());
        }
    }

    bool Remove(const CString& sHost) { return m_Cache.RemItem(sHost); }

    void OnTimeoutCommand(const CString& sCommand) {
        if (!GetUser()->IsAdmin()) {
            PutModule(t_s("Access denied"));
            return;
        }

        CString sArg = sCommand.Token(1);

        if (!sArg.empty()) {
            unsigned int uTimeout = sArg.ToUInt();
            if (uTimeout == 0) {
                PutModule(t_s("Usage: Timeout [minutes]"));
            } else {
                m_Cache.SetTTL(uTimeout * 60 * 1000);
                SetArgs(CString(m_Cache.GetTTL() / 60 / 1000) + " " +
                        CString(m_uiAllowedFailed));
                PutModule(t_f("Timeout: {1} min")(uTimeout));
            }
        } else {
            PutModule(t_f("Timeout: {1} min")(m_Cache.GetTTL() / 60 / 1000));
        }
    }

    void OnAttemptsCommand(const CString& sCommand) {
        if (!GetUser()->IsAdmin()) {
            PutModule(t_s("Access denied"));
            return;
        }

        CString sArg = sCommand.Token(1);

        if (!sArg.empty()) {
            unsigned int uiAttempts = sArg.ToUInt();
            if (uiAttempts == 0) {
                PutModule(t_s("Usage: Attempts [count]"));
            } else {
                m_uiAllowedFailed = uiAttempts;
                SetArgs(CString(m_Cache.GetTTL() / 60 / 1000) + " " +
                        CString(m_uiAllowedFailed));
                PutModule(t_f("Attempts: {1}")(uiAttempts));
            }
        } else {
            PutModule(t_f("Attempts: {1}")(m_uiAllowedFailed));
        }
    }

    void OnBanCommand(const CString& sCommand) {
        if (!GetUser()->IsAdmin()) {
            PutModule(t_s("Access denied"));
            return;
        }

        CString sHosts = sCommand.Token(1, true);
        if (sHosts.empty()) {
            PutStatus(t_s("Usage: Ban <hosts>"));
            return;
        }

        VCString vsHosts;
        sHosts.Replace(",", " ");
        sHosts.Split(" ", vsHosts, false, "", "", true, true);

        for (const CString& sHost : vsHosts) {
            // Ban with max attempts to ensure they're blocked
            Add(sHost, m_uiAllowedFailed);
            PutModule(t_f("Banned: {1}")(sHost));
        }
    }

    void OnUnbanCommand(const CString& sCommand) {
        if (!GetUser()->IsAdmin()) {
            PutModule(t_s("Access denied"));
            return;
        }

        CString sTargets = sCommand.Token(1, true);
        if (sTargets.empty()) {
            PutModule(t_s("Usage: Unban <hosts|IDs>"));
            return;
        }

        VCString vsTargets;
        sTargets.Replace(",", " ");
        sTargets.Split(" ", vsTargets, false, "", "", true, true);

        for (const CString& sTarget : vsTargets) {
            bool bUnbanned = false;

            // Check if it's a numeric ID by trying to convert and validating
            unsigned int targetId = sTarget.ToUInt();
            if (targetId > 0 && CString(targetId) == sTarget) {
                // Find host by ID
                unsigned int currentId = 1;
                for (const auto& it : m_Cache.GetItems()) {
                    if (currentId == targetId) {
                        if (Remove(it.first)) {
                            PutModule(t_f("Unbanned ID {1} ({2})")(targetId,
                                                                   it.first));
                            bUnbanned = true;
                        }
                        break;
                    }
                    currentId++;
                }
            } else {
                // Treat as hostname/IP
                if (Remove(sTarget)) {
                    PutModule(t_f("Unbanned: {1}")(sTarget));
                    bUnbanned = true;
                }
            }

            if (!bUnbanned) {
                PutModule(t_f("{1} is not banned.")(sTarget));
            }
        }
    }
    void OnListCommand(const CString& sCommand) {
        if (!GetUser()->IsAdmin()) {
            PutModule(t_s("Access denied"));
            return;
        }

        time_t now = time(nullptr);

        CTable Table;
        Table.AddColumn(t_s("ID", "list"));
        Table.AddColumn(t_s("Username", "list"));
        Table.AddColumn(t_s("Host", "list"));
        Table.AddColumn(t_s("Attempts", "list"));
        Table.AddColumn(t_s("First Seen", "list"));
        Table.AddColumn(t_s("Last Seen", "list"));
        Table.AddColumn(t_s("Expires In", "list"));
        Table.SetStyle(CTable::ListStyle);

        unsigned int id = 1;
        for (const auto& it : m_Cache.GetItems()) {
            Table.AddRow();
            Table.SetCell(t_s("ID", "list"), CString(id++));
            Table.SetCell(t_s("Username", "list"), it.second.username);
            Table.SetCell(t_s("Host", "list"), it.first);
            Table.SetCell(t_s("Attempts", "list"), CString(it.second.attempts));
            // Format timestamps
            CString sFirstSeen =
                CUtils::FormatTime(it.second.first_attempt, "%Y-%m-%d %H:%M:%S",
                                   GetUser()->GetTimezone());
            CString sLastSeen =
                CUtils::FormatTime(it.second.last_attempt, "%Y-%m-%d %H:%M:%S",
                                   GetUser()->GetTimezone());

            Table.SetCell(t_s("First Seen", "list"), sFirstSeen);
            Table.SetCell(t_s("Last Seen", "list"), sLastSeen);
            // Calculate remaining time
            time_t ttl_seconds =
                m_Cache.GetTTL() / 1000;  // Convert ms to seconds
            time_t elapsed = now - it.second.last_attempt;
            time_t remaining = ttl_seconds - elapsed;

            CString sCountdown;
            if (remaining <= 0) {
                sCountdown = "Expired";
            } else {
                sCountdown = FormatDuration(remaining);
            }

            Table.SetCell(t_s("Expires In", "list"), sCountdown);
        }

        if (Table.empty()) {
            PutModule(t_s("No bans", "list"));
        } else {
            PutModule(Table);
        }
    }

    void OnClientConnect(CZNCSock* pClient, const CString& sHost,
                         unsigned short uPort) override {
        BanInfo* pInfo = m_Cache.GetItem(sHost);
        if (sHost.empty() || pInfo == nullptr ||
            pInfo->attempts < m_uiAllowedFailed) {
            return;
        }

        // Refresh their ban with updated timestamp
        time_t now = time(nullptr);
        pInfo->last_attempt = now;
        m_Cache.AddItem(sHost, *pInfo, m_Cache.GetTTL());

        pClient->Write(
            "ERROR :Closing link [Please try again later - reconnecting too "
            "fast]\r\n");
        pClient->Close(Csock::CLT_AFTERWRITE);
    }

    void OnFailedLogin(const CString& sUsername,
                       const CString& sRemoteIP) override {
        time_t now = time(nullptr);
        BanInfo* pInfo = m_Cache.GetItem(sRemoteIP);

        if (pInfo) {
            pInfo->attempts++;
            pInfo->last_attempt = now;
            pInfo->username = sUsername;  // Update username
            m_Cache.AddItem(sRemoteIP, *pInfo, m_Cache.GetTTL());
        } else {
            BanInfo info = {1, now, now, sUsername};  // Include username
            m_Cache.AddItem(sRemoteIP, info, m_Cache.GetTTL());
        }
    }

    void OnClientLogin() override { Remove(GetClient()->GetRemoteIP()); }
    EModRet OnLoginAttempt(std::shared_ptr<CAuthBase> Auth) override {
        const CString& sRemoteIP = Auth->GetRemoteIP();
        if (sRemoteIP.empty()) return CONTINUE;

        BanInfo* pInfo = m_Cache.GetItem(sRemoteIP);
        if (pInfo && pInfo->attempts >= m_uiAllowedFailed) {
            // OnFailedLogin() will refresh their ban
            Auth->RefuseLogin("Please try again later - reconnecting too fast");
            return HALT;
        }

        return CONTINUE;
    }

    CString FormatDuration(time_t seconds) {
        if (seconds <= 0) return "0s";

        time_t hours = seconds / 3600;
        seconds %= 3600;
        time_t minutes = seconds / 60;
        seconds %= 60;

        CString result;
        if (hours > 0) result += CString(hours) + "h";
        if (minutes > 0) result += CString(minutes) + "m";
        if (seconds > 0 || result.empty()) result += CString(seconds) + "s";

        return result;
    }

  private:
    struct BanInfo {
        unsigned int attempts;
        time_t first_attempt;
        time_t last_attempt;
        CString username;
    };

    TCacheMap<CString, BanInfo> m_Cache;
    unsigned int m_uiAllowedFailed{};
};

template <>
void TModInfo<CFailToBanMod>(CModInfo& Info) {
    Info.SetWikiPage("fail2ban");
    Info.SetHasArgs(true);
    Info.SetArgsHelpText(
        Info.t_s("You might enter the time in minutes for the IP banning and "
                 "the number of failed logins before any action is taken."));
}

GLOBALMODULEDEFS(CFailToBanMod,
                 t_s("Block IPs for some time after a failed login."))
