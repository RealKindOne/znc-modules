// lowecasechan.cpp

// This module lowercases only the channel names to your IRC client.

// Note: This module is set as a per network only module.
//       If you just want to work on ALL your networks just change the
//       "NETWORKMODULEDEF" into "USERMODULEDEFS" at the bottom of this file.
//       That way you only have to load it once.

#include <znc/Chan.h>
#include <znc/Client.h>
#include <znc/Message.h>
#include <znc/Modules.h>

class CLowercaseChanMod : public CModule {
  public:
    MODCONSTRUCTOR(CLowercaseChanMod) {}

    EModRet OnSendToClientMessage(CMessage& Message) override {
        // Transform channel names in various message types
        switch (Message.GetType()) {
            case CMessage::Type::Join:
                TransformJoinMessage(Message.As<CJoinMessage>());
                break;
            case CMessage::Type::Part:
                TransformPartMessage(Message.As<CPartMessage>());
                break;
            case CMessage::Type::Text:
                TransformTextMessage(Message.As<CTextMessage>());
                break;
            case CMessage::Type::Notice:
                TransformNoticeMessage(Message.As<CNoticeMessage>());
                break;
            case CMessage::Type::Topic:
                TransformTopicMessage(Message.As<CTopicMessage>());
                break;
            case CMessage::Type::Kick:
                TransformKickMessage(Message.As<CKickMessage>());
                break;
            case CMessage::Type::Mode:
                TransformModeMessage(Message.As<CModeMessage>());
                break;
            case CMessage::Type::Numeric:
                TransformNumericMessage(Message.As<CNumericMessage>());
                break;
            default:
                break;
        }

        return CONTINUE;
    }

  private:
    void TransformJoinMessage(CJoinMessage& Message) {
        CString sChannel = Message.GetTarget();
        if (IsChannel(sChannel)) {
            Message.SetTarget(sChannel.AsLower());
        }
    }

    void TransformPartMessage(CPartMessage& Message) {
        CString sChannel = Message.GetTarget();
        if (IsChannel(sChannel)) {
            Message.SetTarget(sChannel.AsLower());
        }
    }

    void TransformTextMessage(CTextMessage& Message) {
        CString sTarget = Message.GetTarget();
        if (IsChannel(sTarget)) {
            Message.SetTarget(sTarget.AsLower());
        }
    }

    void TransformNoticeMessage(CNoticeMessage& Message) {
        CString sTarget = Message.GetTarget();
        if (IsChannel(sTarget)) {
            Message.SetTarget(sTarget.AsLower());
        }
    }

    void TransformTopicMessage(CTopicMessage& Message) {
        CString sChannel = Message.GetTarget();
        if (IsChannel(sChannel)) {
            Message.SetTarget(sChannel.AsLower());
        }
    }

    void TransformKickMessage(CKickMessage& Message) {
        CString sChannel = Message.GetTarget();
        if (IsChannel(sChannel)) {
            Message.SetTarget(sChannel.AsLower());
        }
    }

    void TransformModeMessage(CModeMessage& Message) {
        CString sTarget = Message.GetTarget();
        if (IsChannel(sTarget)) {
            Message.SetTarget(sTarget.AsLower());
        }
    }

    void TransformNumericMessage(CNumericMessage& Message) {
        unsigned int uCode = Message.GetCode();

        switch (uCode) {
            case 353:  // RPL_NAMES
                TransformNamesReply(Message);
                break;
            case 332:  // RPL_TOPIC
            case 333:  // RPL_TOPICWHOTIME
            case 366:  // RPL_ENDOFNAMES
                TransformChannelParam(Message, 1);
                break;
            default:
                // Check if first parameter is a channel
                if (Message.GetParams().size() > 1) {
                    TransformChannelParam(Message, 1);
                }
                break;
        }
    }

    void TransformNamesReply(CNumericMessage& Message) {
        // RPL_NAMES: :server 353 nick = #channel :name1 name2 name3
        if (Message.GetParams().size() >= 3) {
            CString sChannel = Message.GetParam(2);
            if (IsChannel(sChannel)) {
                Message.SetParam(2, sChannel.AsLower());
            }
        }
    }

    void TransformChannelParam(CNumericMessage& Message, size_t uParamIndex) {
        if (Message.GetParams().size() > uParamIndex) {
            CString sChannel = Message.GetParam(uParamIndex);
            if (IsChannel(sChannel)) {
                Message.SetParam(uParamIndex, sChannel.AsLower());
            }
        }
    }

    bool IsChannel(const CString& sName) {
        return !sName.empty() && (sName[0] == '#' || sName[0] == '&' ||
                                  sName[0] == '!' || sName[0] == '+');
    }
};

NETWORKMODULEDEFS(CLowercaseChanMod, "Lowercase channel names for all IRC events")