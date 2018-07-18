// stripinput.cpp
// Copy/paste of stripcontrols.cpp

// You will need to set DenyLoadMod on the user so they do not unload this module.

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

#include <znc/Modules.h>

class CstripinputMod : public CModule {
  public:
    MODCONSTRUCTOR(CstripinputMod) {}

    EModRet OnUserMsg(CString& sTarget, CString& sMessage) override {
        sMessage.StripControls();
        return CONTINUE;
    }

    EModRet OnUserAction(CString& sTarget, CString& sMessage) override {
        sMessage.StripControls();
        return CONTINUE;
    }

    EModRet OnUserNotice(CString& sTarget, CString& sMessage) override {
        sMessage.StripControls();
        return CONTINUE;
    }

    EModRet OnUserTopic(CString& sChannel, CString& sTopic) override {
        sTopic.StripControls();
        return CONTINUE;
    }

    EModRet OnUserPart(CString& sChannel, CString& sMessage) override {
        sMessage.StripControls();
        return CONTINUE;
    }
};

template <>
void TModInfo<CstripinputMod>(CModInfo& Info) {
    Info.SetWikiPage("stripinput");
    Info.AddType(CModInfo::UserModule);
}

NETWORKMODULEDEFS(CstripinputMod,  "Strip color/bold/italics/etc.. from all user input.")
