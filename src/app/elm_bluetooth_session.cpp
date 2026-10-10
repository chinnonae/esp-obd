#include "app/elm_bluetooth_session.h"

#include "elm/elm_errors.h"
#include "elm/elm_formatter.h"

namespace esp_obd::app {

TransportOutput ElmBluetoothSession::finishDiagnosticIfReady() {
  TransportOutput out;
  elm::ElmReply reply = app_.takeDiagnosticReply();
  out += reply.text.c_str();
  if (reply.appendPrompt) {
    out += '>';
  }
  return out;
}

TransportOutput ElmBluetoothSession::handleLineEvent(can::Milliseconds now,
                                                      const LineEvent& event) {
  TransportOutput out;
  switch (event.kind) {
    case LineEventKind::Line: {
      elm::ElmReply reply = app_.execute(now, event.text.c_str());
      if (reply.resetPerformed) {
        discardUntilPoll_ = true;
      }
      if (reply.kind == elm::ElmReplyKind::DiagnosticRequest) {
        if (app_.takeSearchingNotice()) {
          out += elm::kSearchingText;
          out += elm::lineBreak(app_.engine().session());
        }
        if (!app_.diagnosticPending()) {
          out += finishDiagnosticIfReady().c_str();
        }
        // Otherwise deferred: poll() will report it once ready.
      } else if (reply.kind == elm::ElmReplyKind::Text) {
        out += reply.text.c_str();
        if (reply.appendPrompt) {
          out += '>';
        }
      }
      // NoReply: nothing is written.
      break;
    }
    case LineEventKind::Overflow: {
      elm::ElmReply reply = elm::textReply(app_.engine().session(), elm::kUnknownCommandText);
      out += reply.text.c_str();
      out += '>';
      break;
    }
    case LineEventKind::TimedOut:
    case LineEventKind::Disconnected:
    case LineEventKind::None:
    default:
      break;
  }
  return out;
}

TransportOutput ElmBluetoothSession::onByte(can::Milliseconds now, uint8_t byte) {
  TransportOutput out;

  if (discardUntilPoll_) {
    return out;
  }
  if (discardUntilCr_) {
    if (byte == '\r') {
      discardUntilCr_ = false;
    }
    return out;
  }

  if (app_.monitorActive()) {
    app_.stopMonitor();
    out += elm::kStoppedText;
    out += elm::responseEnding(app_.engine().session());
    out += '>';
    return out;
  }

  if (app_.diagnosticPending()) {
    if (app_.poll(now)) {
      // The reply was already in: deliver it, then treat the byte normally.
      out += finishDiagnosticIfReady().c_str();
    } else {
      app_.abortDiagnostic();
      discardUntilCr_ = byte != '\r';
      out += elm::kStoppedText;
      out += elm::responseEnding(app_.engine().session());
      out += '>';
      return out;
    }
  }

  if (app_.engine().session().echoEnabled) {
    out += static_cast<char>(byte);
  }

  LineEvent event = lineReader_.onByte(now, byte);
  out += handleLineEvent(now, event).c_str();
  return out;
}

TransportOutput ElmBluetoothSession::poll(can::Milliseconds now) {
  TransportOutput out;
  discardUntilPoll_ = false;

  if (app_.monitorActive()) {
    out += app_.pollMonitor(now).c_str();
    return out;
  }

  if (app_.diagnosticPending()) {
    if (app_.poll(now)) {
      out += finishDiagnosticIfReady().c_str();
    }
    return out;
  }

  LineEvent event = lineReader_.poll(now);
  out += handleLineEvent(now, event).c_str();
  return out;
}

}  // namespace esp_obd::app
