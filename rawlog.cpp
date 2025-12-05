// Updated for 1.x - KindOne @ chat.freenode.net
// Turned into a network module. - Loading as a user would spam all networks
// into a single file.

// Original Source: https://tomaw.net/tmp/rawlog.cpp

/*
 * rawlog module for znc.  Based on CNU's Logging Module
 *
 * Copyright (C) 2006-2007, CNU <bshalm@broadpark.no>
 * (http://cnu.dieplz.net/znc)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <znc/IRCNetwork.h>
#include <znc/Modules.h>
#include <znc/Server.h>

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

class CRawLogMod : public CModule {
  public:
    MODCONSTRUCTOR(CRawLogMod) {}

    void PutLog(const CString& sLine);
    CString GetServer();
    virtual void OnIRCConnected();
    virtual void OnIRCDisconnected();
    virtual EModRet OnRaw(CString& sLine);
};

void CRawLogMod::PutLog(const CString& sLine) {
    std::ofstream ofLog;
    std::stringstream ssPath;
    time_t curtime;
    tm* timeinfo;

    time(&curtime);
    timeinfo = localtime(&curtime);

    ssPath.fill('0');
    ssPath << GetSavePath() << "/" << std::setw(4) << (timeinfo->tm_year + 1900)
           << std::setw(2) << (timeinfo->tm_mon + 1) << std::setw(2)
           << (timeinfo->tm_mday) << ".log";

    ofLog.open(ssPath.str().c_str(), std::ofstream::app);

    if (ofLog.good()) {
        /* Write line: [HH:MM:SS] MSG */
        ofLog.fill('0');
        ofLog << "[" << std::setw(2) << timeinfo->tm_hour << ":" << std::setw(2)
              << timeinfo->tm_min << ":" << std::setw(2) << timeinfo->tm_sec
              << "] " << sLine << std::endl;
    }
}

CString CRawLogMod::GetServer() {
    CServer* pServer = GetNetwork()->GetCurrentServer();

    if (!pServer) return "(no server)";

    return pServer->GetName() + ":" + CString(pServer->GetPort());
}

void CRawLogMod::OnIRCConnected() {
    PutLog("Connected to IRC (" + GetServer() + ")");
}

void CRawLogMod::OnIRCDisconnected() {
    PutLog("Disconnected from IRC (" + GetServer() + ")");
}

CModule::EModRet CRawLogMod::OnRaw(CString& sMessage) {
    PutLog(sMessage);
    return CONTINUE;
}

template <>
void TModInfo<CRawLogMod>(CModInfo& Info) {
    Info.AddType(CModInfo::NetworkModule);
}

NETWORKMODULEDEFS(CRawLogMod, "tomaw's IRC raw-logging Module")
