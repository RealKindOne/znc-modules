// Initial version created by "KindOne @ chat.freenode.net".
//
// EXTREMELY CRUDE ZNC module to support MONITOR.
// http://ircv3.org/specification/monitor-3.2
//
// Most likely can be written better.
//
// Tested/compiled against this version of ZNC, should in theory work with 1.2/1.4.
// <*status> ZNC 1.5-git-211-154bf9f - http://znc.in
// <*status> IPv6: yes, SSL: yes, DNS: threads, charset: yes
//
// TODO ...
//
//
//	Input:
//		- Adding:
//			- Sanitizing checking.
//				- Make the checking case insensitive.
//			- Check NICKLEN= from 005 before adding.
//			- Prevent nicks from starting with numbers/invalid characters.
//			- Allow adding via Webadmin.
//			- Multi nicks per line.
//				- /msg *monitor add EvilOne,EpicOne,MeanOne
//			- Don't go over "MONITOR=" limit.
//		- Deleting:
//			- Sanitizing checking.
//				- Make the checking case insensitive.
//			- Check to see if we are on IRC.
//				- If not on IRC, just delete the nick, don't "MONITOR - NICK"
//			- Execute "MONITOR - NICK" when they are deleted, if on IRC.
//			- Allow deleting via Webadmin.
//			- Multi nicks per line.
//				- /msg *monitor del EvilOne,EpicOne,MeanOne
//		- Allow sorting nicks into A-B-C order.
//
//
//	Output:
//		- Allow people to customize the output of the module, without modifying/recompiling the code.
//			- PutModule() from raw/numeric 730
//		- Make "MONITOR L" output one nick per line. Multiple nicks per line looks horrid in my opinion.
//			- Maybe allow this to be customizable.
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
//		- Figure out how to make it send "MONITOR C" on unload.
//		- Check to see if we are on IRC before sending the onload commands.
//
//	Other:
//		- Other things that I might of missed and/or can't think of.
//


#include <znc/IRCNetwork.h>
#include <znc/Modules.h>


using std::vector;

class CMonitor : public CModule {

	// Add Nicks.
	void Add(const CString& sCommand) {
		CString sMon = sCommand.Token(1, true);

		if (sMon.empty()) {
			PutModule("Usage: add <nick>");
			return;
		}

		m_vMonitor.push_back(ParseMonitor(sMon));
		// Add the nick to the MONITOR list immediately.
		// The IRCd has to send it back, so the "Added" line will be seen first.
		if (m_pNetwork->IsIRCConnected()) {
			PutIRC("MONITOR + " + sMon);
			PutModule("Added " + sMon);
			Save();
		}
		else {
			PutModule("Added " + sMon);
			Save();
		}
}

	// Delete Nicks.
	void Del(const CString& sCommand) {
		u_int iNum = sCommand.Token(1, true).ToUInt();

		if (iNum > m_vMonitor.size() || iNum <= 0) {
			PutModule("Invalid # Requested");
			return;
		}
		else {
			m_vMonitor.erase(m_vMonitor.begin() + iNum - 1);
			PutModule("Nick Erased.");
		}
		Save();
	}

	// List Nicks.
	void List(const CString& sCommand) {
		CTable Table;
		unsigned int index = 1;

		Table.AddColumn("Id");
		Table.AddColumn("Nick");

		for (VCString::const_iterator it = m_vMonitor.begin(); it != m_vMonitor.end(); ++it, index++) {
			Table.AddRow();
			Table.SetCell("Id", CString(index));
			Table.SetCell("Nick", *it);

			CString sExpanded = ExpandString(*it);
		}

		if (PutModule(Table) == 0) {
			PutModule("No nicks found.");
		}
	}

	// Instead of reloading the module, let them do this.
	void Reload(const CString& sCommand) {
		GetNV("Monitor").Split("\n", m_vMonitor, false);
		// Clear the list on load.
		PutIRC("MONITOR C");

		for (VCString::const_iterator it = m_vMonitor.begin(); it != m_vMonitor.end(); ++it) {
			// Add the nicks from the list
			PutIRC("MONITOR + " + *it);
		}
	}

	// Move the nicks around.
	void Swap(const CString& sCommand) {
		u_int iNumA = sCommand.Token(1).ToUInt();
		u_int iNumB = sCommand.Token(2).ToUInt();

		if (iNumA > m_vMonitor.size() || iNumA <= 0 || iNumB > m_vMonitor.size() || iNumB <= 0) {
			PutModule("Illegal # Requested");
		}
		else {
			std::iter_swap(m_vMonitor.begin() + (iNumA - 1), m_vMonitor.begin() + (iNumB - 1));
			PutModule("Nicks Swapped.");
			Save();
		}
	}

public:
	MODCONSTRUCTOR(CMonitor) {
		AddHelpCommand();
		AddCommand("Add",     static_cast<CModCommand::ModCmdFunc>(&CMonitor::Add), "<nick>");
		AddCommand("Del",     static_cast<CModCommand::ModCmdFunc>(&CMonitor::Del), "<id number");
		AddCommand("List",    static_cast<CModCommand::ModCmdFunc>(&CMonitor::List));
		AddCommand("Reload",  static_cast<CModCommand::ModCmdFunc>(&CMonitor::Reload), "Reload the module");
		AddCommand("Swap",    static_cast<CModCommand::ModCmdFunc>(&CMonitor::Swap), "<number> <number>");
	}

	virtual ~CMonitor() {
	}

	CString ParseMonitor(const CString& sArg) const {
		CString sMon = sArg;
		return sMon;
	}


	bool OnLoad(const CString& sArgs, CString& sMessage) {
		GetNV("Monitor").Split("\n", m_vMonitor, false);
		// Clear the list on load.
		PutIRC("MONITOR C");

		for (VCString::const_iterator it = m_vMonitor.begin(); it != m_vMonitor.end(); ++it) {
			// Add the nicks from the list
			PutIRC("MONITOR + " + *it);
		}
		return TRUE;
	}

	EModRet OnNumericMessage(CNumericMessage& numeric) {
		// RPL_MONONLINE
		// :irc.freenode.net 730 KindOne :EvilOne!KindOne@1.2.3.4
		if (numeric.GetCode() == 730) {

			// Uncomment / comment the other one if you want to switch the output.

			//  Online: EvilOne!KindOne@1.2.3.4
			//PutModule("Online: " + sLine.Token(3).TrimPrefix_n() + "");

			//  Online: EvilOne KindOne@1.2.3.4
			PutModule("Online: " + numeric.GetParam(1).TrimPrefix_n().Replace_n("!", " ") + "");

			return HALT;
		}

		// RPL_MONOFFLINE
		// :irc.freenode.net 731 KindOne :EvilOne
		if (numeric.GetCode() == 731) {
			// Offine: EvilOne
			PutModule("Offline: " + numeric.GetParam(1).TrimPrefix_n() + "");
			return HALT;
		}

		// RPL_MONLIST
		// :irc.freenode.net 732 KindOne :EvilOne,EpicOne,KindTwo
		if (numeric.GetCode() == 732) {
			// Nicks: EvilOne EpicOne KindTwo
			PutModule("Nicks: " + numeric.GetParam(1).TrimPrefix_n().Replace_n(",", " ") + "");
			return HALT;
		}

		// RPL_ENDOFMONLIST
		// :irc.freenode.net 733 KindOne :End of MONITOR list
		if (numeric.GetCode() == 733) {
			PutModule("End of MONITOR list.");
			return HALT;
		}

		// ERR_MONLISTFULL
		// :irc.freenode.net 734 KindOne 100 Nick101,Nick102 :Monitor list is full
		if (numeric.GetCode() == 734) {
			// Error: Monitor List full. Cannot add Nick101 Nick102
			PutModule("Error: Monitor List full. Cannot add " + numeric.GetParam(2).Replace_n(",", " ") + "");
			return HALT;
		}

		return CONTINUE;
	}

	void OnIRCConnected() {

		for (VCString::const_iterator it = m_vMonitor.begin(); it != m_vMonitor.end(); ++it) {
			// Add the nicks from the list
			PutIRC("MONITOR + " + *it);
		}
	}


private:
	void Save() {
		CString sBuffer = "";

		for (VCString::const_iterator it = m_vMonitor.begin(); it != m_vMonitor.end(); ++it) {
			sBuffer += *it + "\n";
		}
		SetNV("Monitor", sBuffer);
	}

	VCString m_vMonitor;
};

template<> void TModInfo<CMonitor>(CModInfo& Info) {
//	Info.SetWikiPage("monitor");
	Info.SetHasArgs(false);
	Info.AddType(CModInfo::NetworkModule);
}

NETWORKMODULEDEFS(CMonitor, "IRCv3 MONITOR support.")
