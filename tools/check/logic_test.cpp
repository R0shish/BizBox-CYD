// Unit tests for the pure logic in textutil.h (amounts, words, MIME, UTF-8).
// Built and run by tools/check.sh.
#include "Arduino.h"
#include "../../textutil.h"

static int fails = 0, passes = 0;
#define EXPECT(cond, ...) do { if (cond) passes++; else { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void eqStr(const String& got, const char* want, const char* what) {
  EXPECT(got == want, "%s: got \"%s\" want \"%s\"", what, got.c_str(), want);
}

// amount in paisa, or -1 for "not a payment"
static void amt(const char* subject, long want, const char* cur = "Rs") {
  uint32_t p = 0;
  bool ok = parseAmount(subject, p, cur);
  long got = ok ? (long)p : -1;
  EXPECT(got == want, "parseAmount(\"%s\") = %ld, want %ld", subject, got, want);
}

static void words_tests() {
  eqStr(words(0), "zero", "0");
  eqStr(words(7), "seven", "7");
  eqStr(words(10), "ten", "10");
  eqStr(words(13), "thirteen", "13");
  eqStr(words(19), "nineteen", "19");
  eqStr(words(20), "twenty", "20");
  eqStr(words(21), "twenty one", "21");
  eqStr(words(99), "ninety nine", "99");
  eqStr(words(100), "one hundred", "100");
  eqStr(words(101), "one hundred one", "101");
  eqStr(words(110), "one hundred ten", "110");
  eqStr(words(999), "nine hundred ninety nine", "999");
  eqStr(words(1000), "one thousand", "1000");
  eqStr(words(1001), "one thousand one", "1001");
  eqStr(words(1500), "one thousand five hundred", "1500");
  eqStr(words(12345), "twelve thousand three hundred forty five", "12345");
  eqStr(words(99999), "ninety nine thousand nine hundred ninety nine", "99999");
  eqStr(words(100000), "one lakh", "1 lakh");
  eqStr(words(125000), "one lakh twenty five thousand", "1.25 lakh");
  eqStr(words(1000005), "ten lakh five", "10 lakh 5");
  eqStr(words(9999999), "ninety nine lakh ninety nine thousand nine hundred ninety nine", "99 lakh");
  eqStr(words(10000000), "one crore", "1 crore");
  eqStr(words(123456789), "twelve crore thirty four lakh fifty six thousand seven hundred eighty nine", "12 crore");
  eqStr(words(4294967295u), "four hundred twenty nine crore forty nine lakh sixty seven thousand two hundred ninety five", "uint32 max");

  eqStr(sayAmount(50000), "five hundred", "say 500");
  eqStr(sayAmount(50050), "five hundred and fifty paisa", "say 500.50");
  eqStr(sayAmount(50), "fifty paisa", "say 0.50");
  eqStr(sayAmount(5), "five paisa", "say 0.05");
  eqStr(sayAmount(100), "one", "say 1");
}

static void amount_tests() {
  // Wallet / bank style subjects
  amt("eSewa: You have received Rs. 1,500.00 from Ram Bahadur (98XXXXXX12)", 150000);
  amt("Fonepay: NPR 500 received from 9801234567 on 2024-05-12 13:42", 50000);
  amt("Khalti | Payment of Rs 250 received", 25000);
  amt("Your A/C XXXX1234 has been credited by NPR 5,000.00 on 12/05/2024. Avl Bal NPR 25,431.20", 500000);
  amt("Txn 7731: Rs 499.5 credited", 49950);
  amt("Payment received: 1,25,000", 12500000);
  amt("Rs 500/- received", 50000);
  amt("500/- received via QR", 50000);
  amt("Fund transfer of NPR 2,000 to your account 0123456789012 successful", 200000);
  amt("12/05 Rs 500", 50000);
  amt("You received \xE2\x82\xB9" "750 via UPI", 75000);                      // ₹750
  amt("Rs.500 received", 50000);
  amt("NPR5000 received", 500000);
  amt("Received 300 rupees from Hari", 30000);
  amt("eSewa Transaction Successful - Amount: 1200, Transaction ID: 0004ABC12", 120000);
  amt("Bank alert: 1,500.50 credited", 150050);
  amt("Fonepay QR payment of NRs 1,050 from SITA SHARMA", 105000);
  amt("Dear Customer, NPR 750.00 has been deposited to A/C 01234567890123", 75000);
  amt("Order #1042 paid: Rs 1,250", 125000);
  amt("Balance NPR 12,000. Received NPR 300", 30000);
  amt("Rs 500 received, balance Rs 12,000", 50000);
  amt("[eSewa] Rs 20 received from 98XXXXXX45 for Ref 778812", 2000);
  amt("Payment of 2500 received for invoice 1043", 250000);
  amt("FW: Rs 80 received", 8000);
  // Devanagari: "रु ५०० प्राप्त भयो"
  amt("\xE0\xA4\xB0\xE0\xA5\x81 \xE0\xA5\xAB\xE0\xA5\xA6\xE0\xA5\xA6 \xE0\xA4\xAA\xE0\xA5\x8D\xE0\xA4\xB0\xE0\xA4\xBE\xE0\xA4\xAA\xE0\xA5\x8D\xE0\xA4\xA4", 50000);
  // Configured currency symbol counts as a currency
  amt("Sale 450 NRP", 45000, "NRP");
  amt("Rs: 640 received", 64000);

  // Not payments
  amt("Get 50% off today!", -1);
  amt("Your OTP is 482913", -1);
  amt("Order #1042 has shipped", -1);
  amt("Payment Received", -1);
  amt("Rs. 10,00,00,000 received", -1);         // 10 crore: implausible for the counter
  amt("Statement for May 2024", -1);
  amt("Meeting at 13:42 on 12/05/2024", -1);
  amt("Call 9801234567", -1);
  amt("Your available balance is NPR 25,000", -1);
  amt("Invoice 1043 from PrintLab", -1);
  amt("Rs 0 received", -1);
  amt("", -1);
  amt("Version 1.500 released", -1);
}

static void money_tests() {
  uint32_t p = 0;
  EXPECT(parseMoney("500", p) && p == 50000, "500");
  EXPECT(parseMoney(" 1,500.5 ", p) && p == 150050, "1,500.5");
  EXPECT(parseMoney("0.05", p) && p == 5, "0.05");
  EXPECT(!parseMoney("-5", p), "-5");
  EXPECT(!parseMoney("0", p), "0");
  EXPECT(!parseMoney("12abc", p), "12abc");
  EXPECT(!parseMoney("1.234", p), "1.234");
  EXPECT(!parseMoney("99999999999", p), "overflow");
  EXPECT(!parseMoney("", p), "empty");
  EXPECT(moneyFromDouble(500.5, p) && p == 50050, "500.5");
  EXPECT(moneyFromDouble(0.1 + 0.2, p) && p == 30, "0.3");
  EXPECT(!moneyFromDouble(-5, p), "-5.0");
  EXPECT(!moneyFromDouble(1e9, p), "1e9");
  EXPECT(!moneyFromDouble(0.001, p), "0.001");
}

static void mime_tests() {
  eqStr(mimeDecodeHeader("=?UTF-8?B?UnMgNTAwIHJlY2VpdmVk?="), "Rs 500 received", "B");
  eqStr(mimeDecodeHeader("=?utf-8?Q?Rs_1=2C500_received?="), "Rs 1,500 received", "Q");
  eqStr(mimeDecodeHeader("Payment: =?UTF-8?B?UnMg?= =?UTF-8?B?NTAw?= done"), "Payment: Rs 500 done", "adjacent words join");
  eqStr(mimeDecodeHeader("=?ISO-8859-1?Q?Caf=E9_Rs_80?="), "Caf\xC3\xA9 Rs 80", "latin1 -> utf8");
  eqStr(mimeDecodeHeader("=?UTF-8?B?4KSw4KWBIOClq+ClpuClpg==?="), "\xE0\xA4\xB0\xE0\xA5\x81 \xE0\xA5\xAB\xE0\xA5\xA6\xE0\xA5\xA6", "devanagari");
  eqStr(mimeDecodeHeader("plain Rs 50"), "plain Rs 50", "plain");
  eqStr(mimeDecodeHeader("=?UTF-8?X?bad?= Rs 5"), "=?UTF-8?X?bad?= Rs 5", "unknown encoding kept");
  eqStr(mimeDecodeHeader("=?UTF-8?Q?a=?="), "=?UTF-8?Q?a=?=", "truncated escape kept");
  eqStr(mimeDecodeHeader("tab\there"), "tab here", "tabs");
  uint32_t p = 0;
  EXPECT(parseAmount(mimeDecodeHeader("=?UTF-8?B?4KSw4KWBIOClq+ClpuClpg==?= =?UTF-8?Q?_received?="), p) && p == 50000,
         "decoded devanagari amount");
}

static void utf8_tests() {
  String s = "Caf\xC3\xA9 Rs";                 // "Café Rs"
  eqStr(utf8Cut(s, 4), "Caf", "cut before é");
  eqStr(utf8Cut(s, 5), "Caf\xC3\xA9", "cut after é");
  eqStr(utf8Cut(s, 100), "Caf\xC3\xA9 Rs", "no cut");
  String d = "\xE0\xA4\xB0\xE0\xA5\x81";      // रु (6 bytes)
  eqStr(utf8Cut(d, 5), "\xE0\xA4\xB0", "cut inside 3-byte char");
  String e = "ab\xC3\xA9";
  utf8PopBack(e); eqStr(e, "ab", "pop é");
  utf8PopBack(e); eqStr(e, "a", "pop b");
}

static void escape_tests() {
  eqStr(jsonEsc("a\"b\\c\nd\x01"), "a\\\"b\\\\c\\nd\\u0001", "jsonEsc");
  eqStr(esc("<a href='x'>&\""), "&lt;a href=&#39;x&#39;&gt;&amp;&quot;", "esc");
  eqStr(urlEnc("a b&c=d/é"), "a%20b%26c%3Dd%2F%C3%A9", "urlEnc");
  eqStr(imapQuote("pa\"ss\\word"), "\"pa\\\"ss\\\\word\"", "imapQuote");
}

static void date_tests() {
  uint32_t t = 0;
  EXPECT(parseImapDate("01-Jan-1970 00:00:00 +0000", t) && t == 0, "epoch");
  EXPECT(parseImapDate("12-May-2024 13:42:05 +0000", t) && t == 1715521325u, "2024 UTC (%u)", t);
  EXPECT(parseImapDate(" 2-Mar-2024 05:45:00 +0545", t) && t == 1709337600u, "padded day + NPT (%u)", t);
  EXPECT(parseImapDate("29-Feb-2024 00:00:00 -0700", t) && t == 1709190000u, "leap day, negative zone (%u)", t);
  EXPECT(!parseImapDate("12-Foo-2024 13:42:05 +0000", t), "bad month");
  EXPECT(!parseImapDate("garbage", t), "garbage");
}

static void imap_tests() {
  eqStr(fromName("\"eSewa\" <no-reply@esewa.com.np>"), "eSewa", "from quoted");
  eqStr(fromName("Fonepay Alerts <alerts@fonepay.com>"), "Fonepay Alerts", "from bare name");
  eqStr(fromName("alerts@fonepay.com"), "fonepay.com", "from address only");
  eqStr(fromName("<x@khalti.com>"), "khalti.com", "from angle only");
  eqStr(fromName("=?UTF-8?B?S2hhbHRp?= <a@khalti.com>"), "Khalti", "from encoded");

  uint32_t u[3];
  int n = imapSearchUids("* SEARCH 9 4 12 7 15 3", 4, u, 3);
  EXPECT(n == 4 && u[0] == 7 && u[1] == 9 && u[2] == 12, "search: n=%d %u %u %u", n, u[0], u[1], u[2]);
  n = imapSearchUids("* SEARCH", 0, u, 3);
  EXPECT(n == 0, "empty search");
  n = imapSearchUids("* SEARCH 5", 5, u, 3);           // "n:*" always returns the last UID
  EXPECT(n == 0, "uid range quirk");

  uint32_t v = 0;
  EXPECT(imapRespCode("* OK [UIDVALIDITY 594] UIDs valid.", "UIDVALIDITY", v) && v == 594, "uidvalidity");
  EXPECT(imapRespCode("* OK [UIDNEXT 3410] Predicted next UID.", "UIDNEXT", v) && v == 3410, "uidnext");
  EXPECT(!imapRespCode("* 12 EXISTS", "UIDNEXT", v), "no code");

  ImapMsg m[3];
  ImapFetchParser p(m, 3);
  const char* lines[] = {
    "* 3 FETCH (UID 101 INTERNALDATE \"12-May-2024 13:42:05 +0000\" BODY[HEADER.FIELDS (SUBJECT FROM)] {120}\r\n",
    "Subject: =?UTF-8?B?UnMgNTAwIHJl?=\r\n",
    " =?UTF-8?B?Y2VpdmVk?=\r\n",
    "From: \"eSewa\" <no-reply@esewa.com.np>\r\n",
    "\r\n",
    ")\r\n",
    "* 4 FETCH (INTERNALDATE \"12-May-2024 13:50:00 +0000\" BODY[HEADER.FIELDS (SUBJECT FROM)] {60}\r\n",
    "FROM: alerts@fonepay.com\r\n",
    "SUBJECT: NPR 1,200 received\r\n",
    "\r\n",
    " UID 102)\r\n",
    "a4 OK Success\r\n",
  };
  for (const char* l : lines) p.line(l);
  EXPECT(p.count == 2, "fetch count %d", p.count);
  EXPECT(m[0].uid == 101 && m[1].uid == 102, "uids %u %u", m[0].uid, m[1].uid);
  eqStr(m[0].date, "12-May-2024 13:42:05 +0000", "date");
  eqStr(mimeDecodeHeader(m[0].subject), " Rs 500 received", "folded encoded subject");
  eqStr(fromName(m[0].from), "eSewa", "from 1");
  eqStr(fromName(m[1].from), "fonepay.com", "from 2");
  uint32_t a = 0;
  EXPECT(parseAmount(mimeDecodeHeader(m[1].subject), a) && a == 120000, "subject 2 amount");
}

int main() {
  imap_tests();
  words_tests();
  amount_tests();
  money_tests();
  mime_tests();
  utf8_tests();
  escape_tests();
  date_tests();
  printf("logic tests: %d passed, %d failed\n", passes, fails);
  if (fails) return 1;
  puts("LOGIC_OK");
  return 0;
}
