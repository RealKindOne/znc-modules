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

#include <znc/Chan.h>
#include <znc/IRCNetwork.h>
#include <znc/Query.h>

using std::list;
using std::set;
using std::vector;

class CWatchSource {
  public:
    CWatchSource(const CString& sSource, bool bNegated) {
        m_sSource = sSource;
        m_bNegated = bNegated;
    }
    virtual ~CWatchSource() {}

    // Getters
    const CString& GetSource() const { return m_sSource; }
    bool IsNegated() const { return m_bNegated; }
    // !Getters

    // Setters
    // !Setters
  private:
  protected:
    bool m_bNegated;
    CString m_sSource;
};

class CWatchEntry {
  public:
    CWatchEntry(const CString& sHostMask, const CString& sTarget,
                const CString& sPattern) {
        m_bDisabled = false;
        m_bDetachedClientOnly = false;
        m_bDetachedChannelOnly = false;
        m_sPattern = (sPattern.size()) ? sPattern : "*";

        CNick Nick;
        Nick.Parse(sHostMask);

        m_sHostMask = (Nick.GetNick().size()) ? Nick.GetNick() : "*";
        m_sHostMask += "!";
        m_sHostMask += (Nick.GetIdent().size()) ? Nick.GetIdent() : "*";
        m_sHostMask += "@";
        m_sHostMask += (Nick.GetHost().size()) ? Nick.GetHost() : "*";

        if (sTarget.size()) {
            m_sTarget = sTarget;
        } else {
            m_sTarget = "$";
            m_sTarget += Nick.GetNick();
        }
    }
    virtual ~CWatchEntry() {}

    bool IsMatch(const CNick& Nick, const CString& sText,
                 const CString& sSource, const CIRCNetwork* pNetwork) {
        if (IsDisabled()) {
            return false;
        }

        bool bGoodSource = true;

        if (!sSource.empty() && !m_vsSources.empty()) {
            bGoodSource = false;

            for (unsigned int a = 0; a < m_vsSources.size(); a++) {
                const CWatchSource& WatchSource = m_vsSources[a];

                if (sSource.WildCmp(WatchSource.GetSource(),
                                    CString::CaseInsensitive)) {
                    if (WatchSource.IsNegated()) {
                        return false;
                    } else {
                        bGoodSource = true;
                    }
                }
            }
        }

        if (!bGoodSource) return false;
        if (!Nick.GetHostMask().WildCmp(m_sHostMask, CString::CaseInsensitive))
            return false;
        return (sText.WildCmp(pNetwork->ExpandString(m_sPattern),
                              CString::CaseInsensitive));
    }

    bool operator==(const CWatchEntry& WatchEntry) {
        return (GetHostMask().Equals(WatchEntry.GetHostMask()) &&
                GetTarget().Equals(WatchEntry.GetTarget()) &&
                GetPattern().Equals(WatchEntry.GetPattern()));
    }

    // Getters
    const CString& GetHostMask() const { return m_sHostMask; }
    const CString& GetTarget() const { return m_sTarget; }
    const CString& GetPattern() const { return m_sPattern; }
    bool IsDisabled() const { return m_bDisabled; }
    bool IsDetachedClientOnly() const { return m_bDetachedClientOnly; }
    bool IsDetachedChannelOnly() const { return m_bDetachedChannelOnly; }
    const vector<CWatchSource>& GetSources() const { return m_vsSources; }
    CString GetSourcesStr() const {
        CString sRet;

        for (unsigned int a = 0; a < m_vsSources.size(); a++) {
            const CWatchSource& WatchSource = m_vsSources[a];

            if (a) {
                sRet += " ";
            }

            if (WatchSource.IsNegated()) {
                sRet += "!";
            }

            sRet += WatchSource.GetSource();
        }

        return sRet;
    }
    // !Getters

    // Setters
    void SetHostMask(const CString& s) { m_sHostMask = s; }
    void SetTarget(const CString& s) { m_sTarget = s; }
    void SetPattern(const CString& s) { m_sPattern = s; }
    void SetDisabled(bool b = true) { m_bDisabled = b; }
    void SetDetachedClientOnly(bool b = true) { m_bDetachedClientOnly = b; }
    void SetDetachedChannelOnly(bool b = true) { m_bDetachedChannelOnly = b; }
    void SetSources(const CString& sSources) {
        VCString vsSources;
        VCString::iterator it;
        sSources.Split(" ", vsSources, false);

        m_vsSources.clear();

        for (it = vsSources.begin(); it != vsSources.end(); ++it) {
            if (it->at(0) == '!' && it->size() > 1) {
                m_vsSources.push_back(CWatchSource(it->substr(1), true));
            } else {
                m_vsSources.push_back(CWatchSource(*it, false));
            }
        }
    }
    // !Setters
  private:
  protected:
    CString m_sHostMask;
    CString m_sTarget;
    CString m_sPattern;
    bool m_bDisabled;
    bool m_bDetachedClientOnly;
    bool m_bDetachedChannelOnly;
    vector<CWatchSource> m_vsSources;
};

class CWatcherMod : public CModule {
  public:
    MODCONSTRUCTOR(CWatcherMod) {
        AddHelpCommand();
        AddCommand("Add", t_d("<HostMask> [Target] [Pattern]"), t_d("Used to add an entry to watch for."),
                   [=](const CString& sLine) { Watch(sLine); });
        AddCommand("List", "", t_d("List all entries being watched."),
                   [=](const CString& sLine) { List(); });
        AddCommand("Dump", "", t_d("Dump a list of all current entries to be used later."),
                   [=](const CString& sLine) { Dump(); });
        AddCommand("Del", t_d("<Id>"), t_d("Deletes Id from the list of watched entries."),
                   [=](const CString& sLine) { Remove(sLine); });
        AddCommand("Clear", "", t_d("Delete all entries."),
                   [=](const CString& sLine) { Clear(); });
        AddCommand("Enable", t_d("<Id | *>"), t_d("Enable a disabled entry."),
                   [=](const CString& sLine) { Enable(sLine); });
        AddCommand("Disable", t_d("<Id | *>"), t_d("Disable (but don't delete) an entry."),
                   [=](const CString& sLine) { Disable(sLine); });
        AddCommand("SetDetachedClientOnly", t_d("<Id | *> <True | False>"), t_d("Enable or disable detached client only for an entry."),
                   [=](const CString& sLine) { SetDetachedClientOnly(sLine); });
        AddCommand("SetDetachedChannelOnly", t_d("<Id | *> <True | False>"), t_d("Enable or disable detached channel only for an entry."),
                   [=](const CString& sLine) { SetDetachedChannelOnly(sLine); });
        AddCommand("SetSources", t_d("<Id> [#chan priv #foo* !#bar]"), t_d("Set the source channels that you care about."),
                   [=](const CString& sLine) { SetSources(sLine); });
        AddCommand("ExemptAdd", t_d("<HostMask> [Pattern]"),
                   t_d("Add an entry to exempt list."),
                   [=](const CString& sLine) { ExemptAdd(sLine); });
        AddCommand("ExemptList", "", t_d("List all exempt entries."),
                   [=](const CString& sLine) { ExemptList(); });
        AddCommand("ExemptDel", t_d("<Id>"), t_d("Delete exempt entry by Id."),
                   [=](const CString& sLine) { ExemptRemove(sLine); });
        AddCommand("ExemptEnable", t_d("<Id | *>"),
                   t_d("Enable a disabled exempt entry."),
                   [=](const CString& sLine) { ExemptEnable(sLine); });
        AddCommand("ExemptDisable", t_d("<Id | *>"),
                   t_d("Disable (but don't delete) an exempt entry."),
                   [=](const CString& sLine) { ExemptDisable(sLine); });
        AddCommand("ExemptSetSources", t_d("<Id> [#chan priv #foo* !#bar]"),
                   t_d("Set the source channels for an exempt entry."),
                   [=](const CString& sLine) { ExemptSetSources(sLine); });
        AddCommand(
            "ExemptDump", "",
            t_d("Dump a list of all current exempt entries to be used later."),
            [=](const CString& sLine) { ExemptDump(); });
    }

    ~CWatcherMod() override {}

    bool OnLoad(const CString& sArgs, CString& sMessage) override {
        m_lsWatchers.clear();
        m_lsExempts.clear();

        bool bWarn = false;

        for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
            CString sKey = it->first;
            VCString vList;

            bool bIsExempt = false;
            if (sKey.StartsWith("EXEMPT:")) {
                bIsExempt = true;
                sKey = sKey.substr(7);  // Remove "EXEMPT:" prefix
            } else if (sKey.StartsWith("WATCH:")) {
                sKey = sKey.substr(6);  // Remove "WATCH:" prefix
            }

            sKey.Split("\n", vList);

            if (bIsExempt) {
                // Exempt entries: hostmask, pattern, disabled, sources
                if (vList.size() != 4) {
                    bWarn = true;
                    continue;
                }

                CWatchEntry ExemptEntry(vList[0], "",
                                        vList[1]);  // Empty target
                ExemptEntry.SetDisabled(vList[2].Equals("disabled"));
                ExemptEntry.SetSources(vList[3]);
                // Don't set detached settings for exempt entries

                m_lsExempts.push_back(ExemptEntry);
            } else {
                // Watch entries: full format with backwards compatibility
                if (vList.size() != 5 && vList.size() != 7) {
                    bWarn = true;
                    continue;
                }

                CWatchEntry WatchEntry(vList[0], vList[1], vList[2]);
                WatchEntry.SetDisabled(vList[3].Equals("disabled"));

                if (vList.size() == 5) {
                    WatchEntry.SetSources(vList[4]);
                } else {
                    WatchEntry.SetDetachedClientOnly(vList[4].ToBool());
                    WatchEntry.SetDetachedChannelOnly(vList[5].ToBool());
                    WatchEntry.SetSources(vList[6]);
                }

                m_lsWatchers.push_back(WatchEntry);
            }
        }

        if (bWarn)
            sMessage = t_s("WARNING: malformed entry found while loading");

        return true;
    }
    void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes,
                   const CString& sArgs) override {
        Process(OpNick, "* " + OpNick.GetNick() + " sets mode: " + sModes + " " + sArgs + " on " + Channel.GetName(),
                Channel.GetName());
    }

    void OnKickMessage(CKickMessage& Message) override {
        const CNick& OpNick = Message.GetNick();
        const CString sKickedNick = Message.GetKickedNick();
        CChan& Channel = *Message.GetChan();
        const CString sMessage = Message.GetReason();
        Process(OpNick,
                "* " + OpNick.GetNick() + " kicked " + sKickedNick + " from " +
                    Channel.GetName() + " because [" + sMessage + "]",
                Channel.GetName());
    }

    void OnQuitMessage(CQuitMessage& Message,
                       const vector<CChan*>& vChans) override {
        const CNick& Nick = Message.GetNick();
        const CString& sMessage = Message.GetReason();

        // Collect all channel names, except ignored channel
        VCString vsAllChans;
        for (CChan* pChan : vChans) {
            vsAllChans.push_back(pChan->GetName());
        }
        // Fix for #451 - Skip ignored channels.
        // Note:
        // If you only share a ignored channel then you will not get the quit message.
        // If you share the ignored channel and a non-ignored channel then you get
        // both channels in the message.
        CString sQuitMessage = "* Quits: " + Nick.GetNick() + " (" +
                               Nick.GetIdent() + "@" + Nick.GetHost() + ") (" +
                               sMessage + ")";

        // Append channel names if there are any
        if (!vsAllChans.empty()) {
            sQuitMessage += " (" + CString(", ").Join(vsAllChans.begin(), vsAllChans.end()) + ")";
        }

        CIRCNetwork* pNetwork = GetNetwork();
        set<CString> sHandledTargets;

        // Process for each channel AND for global in one pass
        // Start with empty source for global entries
        vector<CString> sources;
        sources.push_back("");  // Global entries

        // Add each channel as a source
        for (CChan* pChan : vChans) {
            sources.push_back(pChan->GetName());
        }

        // Process all sources in one pass
        for (const CString& sSource : sources) {
            ProcessForQuit(Nick, sQuitMessage, sSource, sHandledTargets);
        }
    }

    void OnJoinMessage(CJoinMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CChan& Channel = *Message.GetChan();
        Process(Nick,
                "* " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" +
                    Nick.GetHost() + ") joins " + Channel.GetName(),
                Channel.GetName());
    }

    void OnPartMessage(CPartMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CChan& Channel = *Message.GetChan();
        const CString sMessage = Message.GetReason();
        Process(Nick,
                "* " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" +
                    Nick.GetHost() + ") parts " + Channel.GetName() + "(" +
                    sMessage + ")",
                Channel.GetName());
    }

    void OnNickMessage(CNickMessage& Message,
                       const vector<CChan*>& vChans) override {
        const CNick& OldNick = Message.GetNick();
        const CString sNewNick = Message.GetNewNick();
        Process(OldNick,
                "* " + OldNick.GetNick() + " is now known as " + sNewNick, "");
    }

    EModRet OnCTCPReplyMessage(CCTCPMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        Process(Nick, "* CTCP: " + Nick.GetNick() + " reply [" + sMessage + "]",
                "priv");
        return CONTINUE;
    }

    EModRet OnPrivCTCPMessage(CCTCPMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        Process(Nick, "* CTCP: " + Nick.GetNick() + " [" + sMessage + "]",
                "priv");
        return CONTINUE;
    }

    EModRet OnChanCTCPMessage(CCTCPMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        CChan& Channel = *Message.GetChan();
        Process(Nick,
                "* CTCP: " + Nick.GetNick() + " [" + sMessage + "] to [" +
                    Channel.GetName() + "]",
                Channel.GetName());
        return CONTINUE;
    }

    EModRet OnPrivNoticeMessage(CNoticeMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        Process(Nick, "-" + Nick.GetNick() + "- " + sMessage, "priv");
        return CONTINUE;
    }

    EModRet OnChanNoticeMessage(CNoticeMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        CChan& Channel = *Message.GetChan();
        Process(
            Nick,
            "-" + Nick.GetNick() + ":" + Channel.GetName() + "- " + sMessage,
            Channel.GetName());
        return CONTINUE;
    }

    EModRet OnPrivTextMessage(CTextMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        Process(Nick, "<" + Nick.GetNick() + "> " + sMessage, "priv");
        return CONTINUE;
    }

    EModRet OnChanTextMessage(CTextMessage& Message) override {
        const CNick& Nick = Message.GetNick();
        CString sMessage = Message.GetText();
        CChan& Channel = *Message.GetChan();
        Process(
            Nick,
            "<" + Nick.GetNick() + ":" + Channel.GetName() + "> " + sMessage,
            Channel.GetName());
        return CONTINUE;
    }

  private:
    void ProcessForQuit(const CNick& Nick, const CString& sMessage,
                        const CString& sSource, set<CString>& sHandledTargets) {
        CIRCNetwork* pNetwork = GetNetwork();
        CChan* pChannel = pNetwork->FindChan(sSource);

        // Consolidated exempt checking - check both global and source-specific
        // exempts
        for (list<CWatchEntry>::iterator it = m_lsExempts.begin();
             it != m_lsExempts.end(); ++it) {
            CWatchEntry& ExemptEntry = *it;

            // Check if this exempt entry applies to our current source
            bool exemptApplies = false;

            if (ExemptEntry.GetSourcesStr().empty()) {
                // Global exempt - applies to all sources
                exemptApplies =
                    ExemptEntry.IsMatch(Nick, sMessage, "", pNetwork);
            } else {
                // Source-specific exempt - only applies if source matches
                exemptApplies =
                    ExemptEntry.IsMatch(Nick, sMessage, sSource, pNetwork);
            }

            if (exemptApplies) {
                return;  // Skip this source
            }
        }

        // Process watch entries for this source
        for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
             it != m_lsWatchers.end(); ++it) {
            CWatchEntry& WatchEntry = *it;

            if (pNetwork->IsUserAttached() &&
                WatchEntry.IsDetachedClientOnly()) {
                continue;
            }

            if (pChannel && !pChannel->IsDetached() &&
                WatchEntry.IsDetachedChannelOnly()) {
                continue;
            }

            if (WatchEntry.IsMatch(Nick, sMessage, sSource, pNetwork) &&
                sHandledTargets.count(WatchEntry.GetTarget()) < 1) {
                if (pNetwork->IsUserAttached()) {
                    pNetwork->PutUser(":" + WatchEntry.GetTarget() +
                                      "!watch@znc.in PRIVMSG " +
                                      pNetwork->GetCurNick() + " :" + sMessage);
                } else {
                    CQuery* pQuery = pNetwork->AddQuery(WatchEntry.GetTarget());
                    if (pQuery) {
                        pQuery->AddBuffer(
                            ":" + WatchEntry.GetTarget() +
                                "!watch@znc.in PRIVMSG {target} :{text}",
                            sMessage);
                    }
                }
                sHandledTargets.insert(WatchEntry.GetTarget());
            }
        }
    }
    void Process(const CNick& Nick, const CString& sMessage,
                 const CString& sSource) {
        set<CString> sHandledTargets;
        CIRCNetwork* pNetwork = GetNetwork();
        CChan* pChannel = pNetwork->FindChan(sSource);

        for (list<CWatchEntry>::iterator it = m_lsExempts.begin();
             it != m_lsExempts.end(); ++it) {
            CWatchEntry& ExemptEntry = *it;
            if (ExemptEntry.IsMatch(Nick, sMessage, sSource, pNetwork)) {
                return;  // Skip processing if exempt
            }
        }

        for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
             it != m_lsWatchers.end(); ++it) {
            CWatchEntry& WatchEntry = *it;

            if (pNetwork->IsUserAttached() &&
                WatchEntry.IsDetachedClientOnly()) {
                continue;
            }

            if (pChannel && !pChannel->IsDetached() &&
                WatchEntry.IsDetachedChannelOnly()) {
                continue;
            }

            if (WatchEntry.IsMatch(Nick, sMessage, sSource, pNetwork) &&
                sHandledTargets.count(WatchEntry.GetTarget()) < 1) {
                if (pNetwork->IsUserAttached()) {
                    pNetwork->PutUser(":" + WatchEntry.GetTarget() +
                                      "!watch@znc.in PRIVMSG " +
                                      pNetwork->GetCurNick() + " :" + sMessage);
                } else {
                    CQuery* pQuery = pNetwork->AddQuery(WatchEntry.GetTarget());
                    if (pQuery) {
                        pQuery->AddBuffer(":" + _NAMEDFMT(WatchEntry.GetTarget()) +
                                              "!watch@znc.in PRIVMSG {target} :{text}",
                                          sMessage);
                    }
                }
                sHandledTargets.insert(WatchEntry.GetTarget());
            }
        }
    }

    void SetDisabled(unsigned int uIdx, bool bDisabled) {
        if (uIdx == (unsigned int)~0) {
            for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
                 it != m_lsWatchers.end(); ++it) {
                (*it).SetDisabled(bDisabled);
            }

            PutModule(bDisabled ? t_s("Disabled all entries.")
                                : t_s("Enabled all entries."));
            Save();
            return;
        }

        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsWatchers.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsWatchers.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        (*it).SetDisabled(bDisabled);
        if (bDisabled)
            PutModule(t_f("Id {1} disabled")(uIdx + 1));
        else
            PutModule(t_f("Id {1} enabled")(uIdx + 1));
        Save();
    }

    void SetDetachedClientOnly(const CString& sLine) {
        bool bDetachedClientOnly = sLine.Token(2).ToBool();
        CString sTok = sLine.Token(1);
        unsigned int uIdx;

        if (sTok == "*") {
            uIdx = ~0;
        } else {
            uIdx = sTok.ToUInt();
        }

        if (uIdx == (unsigned int)~0) {
            for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
                 it != m_lsWatchers.end(); ++it) {
                (*it).SetDetachedClientOnly(bDetachedClientOnly);
            }

            if (bDetachedClientOnly)
                PutModule(t_s("Set DetachedClientOnly for all entries to Yes"));
            else
                PutModule(t_s("Set DetachedClientOnly for all entries to No"));
            Save();
            return;
        }

        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsWatchers.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsWatchers.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        (*it).SetDetachedClientOnly(bDetachedClientOnly);
        if (bDetachedClientOnly)
            PutModule(t_f("Id {1} set to Yes")(uIdx + 1));
        else
            PutModule(t_f("Id {1} set to No")(uIdx + 1));
        Save();
    }

    void SetDetachedChannelOnly(const CString& sLine) {
        bool bDetachedChannelOnly = sLine.Token(2).ToBool();
        CString sTok = sLine.Token(1);
        unsigned int uIdx;

        if (sTok == "*") {
            uIdx = ~0;
        } else {
            uIdx = sTok.ToUInt();
        }

        if (uIdx == (unsigned int)~0) {
            for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
                 it != m_lsWatchers.end(); ++it) {
                (*it).SetDetachedChannelOnly(bDetachedChannelOnly);
            }

            if (bDetachedChannelOnly)
                PutModule(t_s("Set DetachedChannelOnly for all entries to Yes"));
            else
                PutModule(t_s("Set DetachedChannelOnly for all entries to No"));
            Save();
            return;
        }

        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsWatchers.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsWatchers.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        (*it).SetDetachedChannelOnly(bDetachedChannelOnly);
        if (bDetachedChannelOnly)
            PutModule(t_f("Id {1} set to Yes")(uIdx + 1));
        else
            PutModule(t_f("Id {1} set to No")(uIdx + 1));
        Save();
    }

    void List() {
        CTable Table;
        Table.AddColumn(t_s("Id"));
        Table.AddColumn(t_s("HostMask"));
        Table.AddColumn(t_s("Target"));
        Table.AddColumn(t_s("Pattern"));
        Table.AddColumn(t_s("Sources"));
        Table.AddColumn(t_s("Off"));
        Table.AddColumn(t_s("DetachedClientOnly"));
        Table.AddColumn(t_s("DetachedChannelOnly"));

        unsigned int uIdx = 1;

        for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
             it != m_lsWatchers.end(); ++it, uIdx++) {
            CWatchEntry& WatchEntry = *it;

            Table.AddRow();
            Table.SetCell(t_s("Id"), CString(uIdx));
            Table.SetCell(t_s("HostMask"), WatchEntry.GetHostMask());
            Table.SetCell(t_s("Target"), WatchEntry.GetTarget());
            Table.SetCell(t_s("Pattern"), WatchEntry.GetPattern());
            Table.SetCell(t_s("Sources"), WatchEntry.GetSourcesStr());
            Table.SetCell(t_s("Off"),
                          (WatchEntry.IsDisabled()) ? t_s("Off") : "");
            Table.SetCell(
                t_s("DetachedClientOnly"),
                WatchEntry.IsDetachedClientOnly() ? t_s("Yes") : t_s("No"));
            Table.SetCell(
                t_s("DetachedChannelOnly"),
                WatchEntry.IsDetachedChannelOnly() ? t_s("Yes") : t_s("No"));
        }

        if (Table.size()) {
            PutModule(Table);
        } else {
            PutModule(t_s("You have no entries."));
        }
    }

    void ExemptAdd(const CString& sLine) {
        CString sHostMask = sLine.Token(1);
        CString sPattern = sLine.Token(2, true);

        CString sMessage;

        if (sHostMask.size()) {
            CWatchEntry ExemptEntry(sHostMask, "",
                                    sPattern);  // Empty string for target

            bool bExists = false;
            for (list<CWatchEntry>::iterator it = m_lsExempts.begin();
                 it != m_lsExempts.end(); ++it) {
                if (*it == ExemptEntry) {
                    sMessage = t_f("Exempt entry for {1} already exists.")(
                        ExemptEntry.GetHostMask());
                    bExists = true;
                    break;
                }
            }

            if (!bExists) {
                sMessage = t_f("Adding exempt entry: {1} watching for [{2}]")(
                    ExemptEntry.GetHostMask(), ExemptEntry.GetPattern());
                m_lsExempts.push_back(ExemptEntry);
            }
        } else {
            sMessage = t_s("ExemptAdd: Not enough arguments.  Try Help");
        }

        PutModule(sMessage);
        Save();
    }

    void ExemptRemove(const CString& sLine) {
        unsigned int uIdx = sLine.Token(1).ToUInt();

        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsExempts.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsExempts.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        m_lsExempts.erase(it);
        PutModule(t_f("Exempt Id {1} removed.")(uIdx + 1));
        Save();
    }

    void ExemptList() {
        CTable Table;
        Table.AddColumn(t_s("Id"));
        Table.AddColumn(t_s("HostMask"));
        Table.AddColumn(t_s("Pattern"));
        Table.AddColumn(t_s("Sources"));
        Table.AddColumn(t_s("Off"));

        unsigned int uIdx = 1;
        for (list<CWatchEntry>::iterator it = m_lsExempts.begin();
             it != m_lsExempts.end(); ++it, uIdx++) {
            CWatchEntry& ExemptEntry = *it;

            Table.AddRow();
            Table.SetCell(t_s("Id"), CString(uIdx));
            Table.SetCell(t_s("HostMask"), ExemptEntry.GetHostMask());
            Table.SetCell(t_s("Pattern"), ExemptEntry.GetPattern());
            Table.SetCell(t_s("Sources"), ExemptEntry.GetSourcesStr());
            Table.SetCell(t_s("Off"),
                          (ExemptEntry.IsDisabled()) ? t_s("Off") : "");
        }

        if (Table.size()) {
            PutModule(Table);
        } else {
            PutModule(t_s("You have no exempt entries."));
        }
    }

    void ExemptEnable(const CString& sLine) {
        CString sTok = sLine.Token(1);
        if (sTok == "*") {
            SetExemptDisabled(~0, false);
        } else {
            SetExemptDisabled(sTok.ToUInt(), false);
        }
    }

    void ExemptDisable(const CString& sLine) {
        CString sTok = sLine.Token(1);
        if (sTok == "*") {
            SetExemptDisabled(~0, true);
        } else {
            SetExemptDisabled(sTok.ToUInt(), true);
        }
    }

    void SetExemptDisabled(unsigned int uIdx, bool bDisabled) {
        if (uIdx == (unsigned int)~0) {
            for (list<CWatchEntry>::iterator it = m_lsExempts.begin();
                 it != m_lsExempts.end(); ++it) {
                (*it).SetDisabled(bDisabled);
            }

            PutModule(bDisabled ? t_s("Disabled all exempt entries.")
                                : t_s("Enabled all exempt entries."));
            Save();
            return;
        }

        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsExempts.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsExempts.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        (*it).SetDisabled(bDisabled);
        if (bDisabled)
            PutModule(t_f("Exempt Id {1} disabled")(uIdx + 1));
        else
            PutModule(t_f("Exempt Id {1} enabled")(uIdx + 1));
        Save();
    }
    void ExemptSetSources(const CString& sLine) {
        unsigned int uIdx = sLine.Token(1).ToUInt();
        CString sSources = sLine.Token(2, true);

        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsExempts.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsExempts.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        (*it).SetSources(sSources);
        PutModule(t_f("Sources set for exempt Id {1}.")(uIdx + 1));
        Save();
    }

    void ExemptDump() {
        if (m_lsExempts.empty()) {
            PutModule(t_s("You have no exempt entries."));
            return;
        }

        PutModule("---------------");
        PutModule("/msg " + GetModNick() + " EXEMPTCLEAR");

        unsigned int uIdx = 1;

        for (list<CWatchEntry>::iterator it = m_lsExempts.begin();
             it != m_lsExempts.end(); ++it, uIdx++) {
            CWatchEntry& ExemptEntry = *it;
            PutModule("/msg " + GetModNick() + " EXEMPTADD " +
                      ExemptEntry.GetHostMask() + " " +
                      ExemptEntry.GetPattern());

            if (ExemptEntry.GetSourcesStr().size()) {
                PutModule("/msg " + GetModNick() + " EXEMPTSETSOURCES " +
                          CString(uIdx) + " " + ExemptEntry.GetSourcesStr());
            }

            if (ExemptEntry.IsDisabled()) {
                PutModule("/msg " + GetModNick() + " EXEMPTDISABLE " +
                          CString(uIdx));
            }
        }

        PutModule("---------------");
    }

    void Dump() {
        if (m_lsWatchers.empty()) {
            PutModule(t_s("You have no entries."));
            return;
        }

        PutModule("---------------");
        PutModule("/msg " + GetModNick() + " CLEAR");

        unsigned int uIdx = 1;

        for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
             it != m_lsWatchers.end(); ++it, uIdx++) {
            CWatchEntry& WatchEntry = *it;

            PutModule("/msg " + GetModNick() + " ADD " +
                      WatchEntry.GetHostMask() + " " + WatchEntry.GetTarget() +
                      " " + WatchEntry.GetPattern());

            if (WatchEntry.GetSourcesStr().size()) {
                PutModule("/msg " + GetModNick() + " SETSOURCES " +
                          CString(uIdx) + " " + WatchEntry.GetSourcesStr());
            }

            if (WatchEntry.IsDisabled()) {
                PutModule("/msg " + GetModNick() + " DISABLE " + CString(uIdx));
            }

            if (WatchEntry.IsDetachedClientOnly()) {
                PutModule("/msg " + GetModNick() + " SETDETACHEDCLIENTONLY " +
                          CString(uIdx) + " TRUE");
            }

            if (WatchEntry.IsDetachedChannelOnly()) {
                PutModule("/msg " + GetModNick() + " SETDETACHEDCHANNELONLY " +
                          CString(uIdx) + " TRUE");
            }
        }

        PutModule("---------------");
    }

    void SetSources(const CString& sLine) {
        unsigned int uIdx = sLine.Token(1).ToUInt();
        CString sSources = sLine.Token(2, true);

        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsWatchers.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsWatchers.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        (*it).SetSources(sSources);
        PutModule(t_f("Sources set for Id {1}.")(uIdx + 1));
        Save();
    }

    void Enable(const CString& sLine) {
        CString sTok = sLine.Token(1);
        if (sTok == "*") {
            SetDisabled(~0, false);
        } else {
            SetDisabled(sTok.ToUInt(), false);
        }
    }

    void Disable(const CString& sLine) {
        CString sTok = sLine.Token(1);
        if (sTok == "*") {
            SetDisabled(~0, true);
        } else {
            SetDisabled(sTok.ToUInt(), true);
        }
    }

    void Clear() {
        m_lsWatchers.clear();
        PutModule(t_s("All entries cleared."));
        Save();
    }

    void Remove(const CString& sLine) {
        unsigned int uIdx = sLine.Token(1).ToUInt();

        uIdx--;  // "convert" index to zero based
        if (uIdx >= m_lsWatchers.size()) {
            PutModule(t_s("Invalid Id"));
            return;
        }

        list<CWatchEntry>::iterator it = m_lsWatchers.begin();
        for (unsigned int a = 0; a < uIdx; a++) ++it;

        m_lsWatchers.erase(it);
        PutModule(t_f("Id {1} removed.")(uIdx + 1));
        Save();
    }

    void Watch(const CString& sLine) {
        CString sHostMask = sLine.Token(1);
        CString sTarget = sLine.Token(2);
        CString sPattern = sLine.Token(3, true);

        CString sMessage;

        if (sHostMask.size()) {
            CWatchEntry WatchEntry(sHostMask, sTarget, sPattern);

            bool bExists = false;
            for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
                 it != m_lsWatchers.end(); ++it) {
                if (*it == WatchEntry) {
                    sMessage = t_f("Entry for {1} already exists.")(
                        WatchEntry.GetHostMask());
                    bExists = true;
                    break;
                }
            }

            if (!bExists) {
                sMessage = t_f("Adding entry: {1} watching for [{2}] -> {3}")(
                    WatchEntry.GetHostMask(), WatchEntry.GetPattern(),
                    WatchEntry.GetTarget());
                m_lsWatchers.push_back(WatchEntry);
            }
        } else {
            sMessage = t_s("Watch: Not enough arguments.  Try Help");
        }

        PutModule(sMessage);

        Save();
    }
    void Save() {
        ClearNV(false);

        // Save watch entries with full format
        for (list<CWatchEntry>::iterator it = m_lsWatchers.begin();
             it != m_lsWatchers.end(); ++it) {
            CWatchEntry& WatchEntry = *it;
            CString sSave = "WATCH:";

            sSave += WatchEntry.GetHostMask() + "\n";
            sSave += WatchEntry.GetTarget() + "\n";
            sSave += WatchEntry.GetPattern() + "\n";
            sSave += (WatchEntry.IsDisabled() ? "disabled\n" : "enabled\n");
            sSave += CString(WatchEntry.IsDetachedClientOnly()) + "\n";
            sSave += CString(WatchEntry.IsDetachedChannelOnly()) + "\n";
            sSave += WatchEntry.GetSourcesStr();
            sSave += " ";

            SetNV(sSave, "", false);
        }

        // Save exempt entries with simplified format (no target, no detached
        // settings)
        for (list<CWatchEntry>::iterator it = m_lsExempts.begin();
             it != m_lsExempts.end(); ++it) {
            CWatchEntry& ExemptEntry = *it;
            CString sSave = "EXEMPT:";

            sSave += ExemptEntry.GetHostMask() + "\n";
            sSave += ExemptEntry.GetPattern() + "\n";
            sSave += (ExemptEntry.IsDisabled() ? "disabled\n" : "enabled\n");
            sSave += ExemptEntry.GetSourcesStr();
            sSave += " ";

            SetNV(sSave, "", false);
        }

        SaveRegistry();
    }

    list<CWatchEntry> m_lsWatchers;
    list<CWatchEntry> m_lsExempts;
};

template <>
void TModInfo<CWatcherMod>(CModInfo& Info) {
    Info.SetWikiPage("watch");
}

NETWORKMODULEDEFS(
    CWatcherMod,
    t_s("Copy activity from a specific user into a separate window"))
