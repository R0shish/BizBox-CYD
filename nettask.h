#pragma once
// ==========================================================================
//  The network task's loop (see net.h for the rules it follows):
//  payment polling (Gmail / Apps Script), Connect webhooks and Connect pull.
//  One job at a time, each with its own timeouts.
// ==========================================================================

const uint32_t NET_POLL_MS = 15000;

void netTask(void*) {
  uint32_t lastPoll = 0, lastPull = 0, seenVersion = 0;
  bool first = true, more = false;
  PollCfg c;      // copies of the settings this task uses, refreshed after a Save
  CxCfg x;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(200));
    if (WiFi.status() != WL_CONNECTED) continue;

    if (first || seenVersion != cfgVersion) {
      CfgLock l;
      seenVersion = cfgVersion;
      c.mode = cfg.emailMode;
      c.imap.user = cfg.gmailUser;
      c.imap.pass = cfg.gmailPass;
      c.imap.query = cfg.imapQuery;
      c.imap.currency = cfg.currency;
      c.imap.verifyTls = cfg.verifyGoogle;
      c.url = cfg.url;
      c.key = cfg.key;
      x.hook = cfg.cxHook; x.header = cfg.cxHeader; x.secret = cfg.cxSecret;
      x.pull = cfg.cxPull; x.everyMs = (uint32_t)cfg.cxEvery * 1000UL; x.requireSigned = cfg.cxSigned;
      x.pOrders = cfg.cxPOrders; x.pId = cfg.cxPId; x.pTitle = cfg.cxPTitle; x.pAmount = cfg.cxPAmount;
      x.pStatus = cfg.cxPStatus; x.pCommands = cfg.cxPCommands; x.pCursor = cfg.cxPCursor; x.statusMap = cfg.cxMap;
    }
    uint32_t now = millis();

    // 1. Payments
    if (first || netPollNow || now - lastPoll >= (more ? 1000UL : NET_POLL_MS)) {
      first = false;
      netPollNow = false;
      lastPoll = now;
      if (c.mode == 2) { netDot = NET_OK; more = false; }   // local API only
      else {
        netDot = NET_SYNC;
        bool ok = c.mode == 0 ? netPollGmail(c, more) : netPollAppsScript(c);
        if (c.mode != 0) more = false;
        netDot = ok ? NET_OK : NET_ERR;
      }
    }

    // 2. Connect: deliver due webhooks (a few per round so payments aren't held up)
    for (int i = 0; i < 3 && connectDeliverDue(x); i++) {}

    // 3. Connect: pull orders/commands
    if (x.pull.length() && (cxPullNow || millis() - lastPull >= x.everyMs)) {
      cxPullNow = false;
      lastPull = millis();
      connectPull(x);
    }
  }
}

void netBegin(const ImapState& st, const String& since, const String& cursor) {
  netImapState = st;
  netSince = since.length() ? since : String("0");
  netQueue = xQueueCreate(24, sizeof(NetEvent));
  cfgLock = xSemaphoreCreateMutex();
  connectBegin(cursor);
  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);
}
