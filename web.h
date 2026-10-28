#pragma once
// ==========================================================================
//  Phone dashboard served at http://bizbox.local (and 192.168.4.1 in setup mode)
//  Same design language as the screen: navy, filament accents, mono labels.
// ==========================================================================

static const char WEB_CSS[] PROGMEM = R"CSS(
:root{--bg:#0D1F33;--surf:#153250;--surf2:#112a45;--line:#24496F;--ink:#F0F0F0;--muted:#8FA6BF;--dim:#5B7896;
--cyan:#2ED3F0;--green:#3BE08A;--yellow:#FFC93C;--red:#FF5B5B;
--sans:'Space Grotesk',ui-sans-serif,system-ui,-apple-system,'Segoe UI',Roboto,sans-serif;
--mono:'JetBrains Mono',ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}
*{box-sizing:border-box;margin:0;padding:0}
html{-webkit-text-size-adjust:100%}
body{background:var(--bg);color:var(--ink);font-family:var(--sans);font-size:15px;line-height:1.45;padding-bottom:64px;
background-image:radial-gradient(var(--surf) 1px,transparent 1px);background-size:16px 16px;background-position:8px 8px}
a{color:var(--cyan)}
.strip{height:4px;background:linear-gradient(90deg,#1A1A1A 0 14.28%,#F0F0F0 0 28.57%,#E63946 0 42.85%,#2F6FE4 0 57.14%,#2ED3F0 0 71.42%,#3BE08A 0 85.71%,#FFC93C 0)}
.wrap{max-width:560px;margin:0 auto;padding:0 16px}
.mono,.eyebrow,label,.tab,.pill,small,.k{font-family:var(--mono)}
.eyebrow{font-size:11px;letter-spacing:.14em;text-transform:uppercase;color:var(--cyan)}
.eyebrow b{color:var(--dim);font-weight:500}
header{padding:22px 0 18px}
h1{font-size:34px;line-height:1.05;font-weight:700;letter-spacing:-.02em;margin:8px 0 12px}
h2{font-size:22px;font-weight:700;letter-spacing:-.01em;margin:6px 0 14px}
.meta{display:flex;flex-wrap:wrap;gap:6px 16px;font-family:var(--mono);font-size:12px;color:var(--muted);align-items:center}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--green);margin-right:6px;vertical-align:1px}
.dot.err{background:var(--red)}.dot.sync{background:var(--yellow)}
.pill{display:inline-flex;align-items:center;gap:6px;font-size:11px;letter-spacing:.12em;text-transform:uppercase;border:1px solid currentColor;border-radius:99px;padding:3px 10px;white-space:nowrap}
.pill.open{color:var(--green)}.pill.closed{color:var(--red)}
.tabs{position:sticky;top:0;z-index:9;background:rgba(13,31,51,.94);backdrop-filter:blur(8px);-webkit-backdrop-filter:blur(8px);border-top:1px solid var(--line);border-bottom:1px solid var(--line);margin-bottom:18px}
.tabs .wrap{display:flex;overflow-x:auto;scrollbar-width:none}
.tab{flex:1;min-width:72px;background:none;border:0;color:var(--muted);padding:10px 4px 9px;font-size:11px;letter-spacing:.12em;text-transform:uppercase;cursor:pointer;border-top:2px solid transparent;line-height:1.5}
.tab i{display:block;font-style:normal;color:var(--dim)}
.tab.on{color:var(--ink);border-top-color:var(--cyan)}.tab.on i{color:var(--cyan)}
.pane{display:none}.pane.on{display:block}
.card{background:var(--surf);border-radius:16px;padding:18px;margin-bottom:14px}
.big{display:flex;align-items:baseline;gap:8px;margin:4px 0 16px}
.big span{font-size:22px;font-weight:700;color:var(--muted)}
.big strong{font-size:52px;line-height:1;font-weight:700;letter-spacing:-.03em}
.tiles{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.tile{background:var(--surf2);border-radius:12px;padding:10px 12px}
.k{display:block;font-size:10px;letter-spacing:.14em;text-transform:uppercase;color:var(--dim)}
.v{font-family:var(--mono);font-size:15px;margin-top:2px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.bars{display:flex;align-items:flex-end;gap:3px;height:56px;margin-top:16px;border-bottom:1px solid var(--line)}
.bars div{flex:1;background:#2A6E99;border-radius:2px 2px 0 0;min-height:2px}
.bars div.now{background:var(--cyan)}.bars div.zero{background:var(--line)}
.axis{display:flex;justify-content:space-between;font-family:var(--mono);font-size:10px;color:var(--dim);margin-top:4px}
.row{display:flex;align-items:center;gap:10px;padding:12px 0;border-top:1px solid var(--line)}
.row:first-child{border-top:0}
.row .id{font-family:var(--mono);font-size:12px;color:var(--dim);width:38px}
.row .t{flex:1;min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.row .a{font-family:var(--mono);font-size:13px}
.s-NEW{color:var(--cyan)}.s-PREP{color:var(--yellow)}.s-READY{color:var(--green)}.s-PAID{color:var(--dim)}
.empty{color:var(--muted);text-align:center;padding:18px 0}
label{display:block;font-size:11px;letter-spacing:.12em;text-transform:uppercase;color:var(--muted);margin:16px 0 6px}
label:first-of-type{margin-top:4px}
input,select,textarea{width:100%;padding:12px 13px;border:1px solid var(--line);border-radius:12px;background:var(--bg);color:var(--ink);font:inherit;font-size:15px;outline:none;-webkit-appearance:none;appearance:none}
textarea{font-family:var(--mono);font-size:13px;resize:vertical}
select{background-image:linear-gradient(45deg,transparent 50%,var(--muted) 50%),linear-gradient(135deg,var(--muted) 50%,transparent 50%);background-position:calc(100% - 18px) 50%,calc(100% - 13px) 50%;background-size:5px 5px;background-repeat:no-repeat;padding-right:34px}
input:focus,select:focus,textarea:focus{border-color:var(--cyan);box-shadow:0 0 0 3px rgba(46,211,240,.15)}
input[type=range]{padding:0;border:0;background:none;accent-color:var(--cyan);height:28px;-webkit-appearance:auto;appearance:auto}
small{display:block;font-size:11px;color:var(--dim);margin-top:6px;line-height:1.5}
.check{display:flex;align-items:center;gap:10px;font-family:var(--sans);font-size:15px;letter-spacing:0;text-transform:none;color:var(--ink);margin:12px 0 0}
.check input{width:20px;height:20px;accent-color:var(--cyan);-webkit-appearance:auto;appearance:auto}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.btn{display:block;width:100%;margin-top:16px;padding:14px;border-radius:12px;border:0;background:var(--ink);color:var(--bg);font:inherit;font-weight:700;font-size:15px;text-align:center;text-decoration:none;cursor:pointer}
.btn.ghost{background:none;color:var(--ink);border:1px solid var(--line)}
.btn.cyan{background:var(--cyan);color:var(--bg)}
.btn.danger{background:none;color:var(--red);border:1px solid rgba(255,91,91,.45)}
.btn:active{transform:translateY(1px)}
.banner{border:1px solid var(--yellow);color:var(--ink);border-radius:14px;padding:14px 16px;margin-bottom:14px}
.banner .eyebrow{color:var(--yellow)}
.stat{font-family:var(--mono);font-size:12px;color:var(--muted);margin-top:12px}
.toast{position:fixed;left:50%;bottom:20px;transform:translate(-50%,120px);background:var(--ink);color:var(--bg);font-family:var(--mono);font-size:12px;letter-spacing:.06em;padding:11px 16px;border-radius:99px;transition:transform .25s ease,opacity .25s;z-index:20;opacity:0;pointer-events:none}
.toast.show{transform:translate(-50%,0);opacity:1}
footer{font-family:var(--mono);font-size:11px;color:var(--dim);text-align:center;padding:26px 0 0;letter-spacing:.08em}
)CSS";

static const char WEB_JS[] PROGMEM = R"JS(
const $=s=>document.querySelector(s);
function tab(id,el){document.querySelectorAll('.pane').forEach(p=>p.classList.remove('on'));
document.querySelectorAll('.tab').forEach(t=>t.classList.remove('on'));$('#'+id).classList.add('on');el.classList.add('on');
try{localStorage.setItem('tab',id)}catch(e){}}
function toast(m){const t=$('#toast');t.textContent=m;t.classList.add('show');clearTimeout(t._h);t._h=setTimeout(()=>t.classList.remove('show'),2200)}
function grp(n){n=String(Math.round(n));if(n.length<=3)return n;let t=n.slice(-3),h=n.slice(0,-3);return h.replace(/\B(?=(\d{2})+(?!\d))/g,',')+','+t}
function esc(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function addOrder(){const t=$('#p_item').value.trim(),a=parseInt($('#p_amt').value)||0;if(!t){toast('ADD AN ITEM NAME');return}
fetch('/api/order',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({title:t,amount:a,status:'NEW'})})
.then(()=>{$('#p_item').value='';$('#p_amt').value='';toast('SENT TO SCREEN');refresh()})}
function act(u,m){fetch(u).then(()=>{toast(m);refresh()})}
function presetQR(v){const f=$('#qr_input');const p={upi:'upi://pay?pa=yourstore@upi&pn=Store&cu=INR',paypal:'https://paypal.me/yourbusiness'};if(p[v])f.value=p[v];}
function render(s){
$('#sales').textContent=grp(s.sales);$('#tx').textContent=s.tx;$('#avg').textContent=s.cur+' '+grp(s.avg);$('#last').textContent=s.last||'--:--';
$('#clock').textContent=s.time;$('#imap').textContent=s.imap;$('#ndot').className='dot'+(s.net==3?' err':s.net==2?' sync':'');
const op=$('#openpill');op.className='pill '+(s.open?'open':'closed');op.textContent=s.open?'Open':'Closed';
const mx=Math.max(1,...s.hourly);$('#bars').innerHTML=s.hourly.map((v,h)=>'<div class="'+(h==s.hour?'now':v?'':'zero')+'" style="height:'+(v?Math.max(6,v/mx*100):3)+'%"></div>').join('');
$('#orders').innerHTML=s.orders.length?s.orders.map(o=>'<div class="row"><span class="id">#'+o.id+'</span><span class="t">'+esc(o.title)+'</span><span class="a">'+(o.amount?grp(o.amount):'-')+'</span><span class="pill s-'+o.status+'">'+o.status+'</span></div>').join(''):'<p class="empty">Nothing queued.</p>';}
function refresh(){fetch('/api/state').then(r=>r.json()).then(render).catch(()=>{})}
try{const t=localStorage.getItem('tab');if(t&&$('#'+t)&&!document.body.dataset.portal){tab(t,$('[data-t='+t+']'))}}catch(e){}
if(!document.body.dataset.portal){refresh();setInterval(refresh,5000)}
)JS";

bool authed() {
  if (portalMode || cfg.adminPin.isEmpty()) return true;
  if (server.authenticate("admin", cfg.adminPin.c_str())) return true;
  server.requestAuthentication();
  return false;
}

String sel(bool on) { return on ? " selected" : ""; }
String chk(bool on) { return on ? " checked" : ""; }

String pageHead(const String& title) {
  String h = "<!doctype html><html lang=en><head><meta charset=utf-8>"
             "<meta name=viewport content='width=device-width,initial-scale=1,viewport-fit=cover'>"
             "<meta name=theme-color content='#0D1F33'><title>" + esc(title) + "</title>";
  if (!portalMode) {
    h += "<link rel=preconnect href=https://fonts.gstatic.com crossorigin>"
         "<link rel=stylesheet href='https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;500;600&family=Space+Grotesk:wght@500;700&display=swap'>";
  }
  h += "<style>";
  h += WEB_CSS;
  h += "</style></head>";
  return h;
}

String hourlyBars() {
  struct tm t;
  int nowH = getLocalTime(&t, 0) ? t.tm_hour : -1;
  uint32_t mx = 1;
  for (int i = 0; i < 24; i++) mx = max(mx, hourly[i]);
  String b;
  for (int i = 0; i < 24; i++) {
    String cls = i == nowH ? "now" : hourly[i] ? "" : "zero";
    int pct = hourly[i] ? max(6, (int)(hourly[i] * 100ULL / mx)) : 3;
    b += "<div class='" + cls + "' style='height:" + String(pct) + "%'></div>";
  }
  return b;
}

String orderRows() {
  if (!orderCount) return "<p class=empty>Nothing queued.</p>";
  String r;
  for (int i = 0; i < orderCount; i++) {
    r += "<div class=row><span class=id>#" + String(orders[i].id) + "</span><span class=t>" + esc(orders[i].title) +
         "</span><span class=a>" + (orders[i].amount ? fmtAmt(orders[i].amount) : String("-")) +
         "</span><span class='pill s-" + orders[i].status + "'>" + orders[i].status + "</span></div>";
  }
  return r;
}

void handleRoot() {
  if (!authed()) return;
  String name = cfg.bizName.length() ? cfg.bizName : String("Business Hub");
  uint32_t avg = totalTransactions ? todaySales / totalTransactions : 0;
  String ip = portalMode ? String("192.168.4.1") : WiFi.localIP().toString();

  String h;
  h.reserve(20000);
  h += pageHead(name);
  h += String("<body") + (portalMode ? " data-portal=1" : "") + "><div class=strip></div>";

  // Header
  h += "<header class=wrap><div class=eyebrow>" + String(portalMode ? "Setup mode" : "Payment soundbox") +
       " <b>/ v" FW_VERSION "</b></div>"
       "<h1>" + esc(name) + (name.endsWith(".") ? "" : ".") + "</h1>"
       "<div class=meta>";
  if (portalMode) {
    h += "<span><span class='dot sync'></span>Hotspot " AP_NAME "</span><span>" + ip + "</span></div></header>";
  } else {
    h += "<span><span id=ndot class='dot" + String(netState == NET_ERR ? " err" : "") + "'></span><span id=clock>" +
         ui_clockText() + "</span></span><span>" + ip + "</span><span>" MDNS_NAME ".local</span>"
         "<span id=openpill class='pill " + String(cfg.storeOpen ? "open'>Open" : "closed'>Closed") + "</span></div></header>";
  }

  // Tabs
  const char* tabIds[5] = {"t-today", "t-qr", "t-brand", "t-gmail", "t-sys"};
  const char* tabNames[5] = {"Today", "QR", "Brand", "Gmail", "System"};
  int active = portalMode ? 4 : 0;
  h += "<nav class=tabs><div class=wrap>";
  for (int i = 0; i < 5; i++) {
    h += "<button class='tab" + String(i == active ? " on" : "") + "' data-t=" + tabIds[i] +
         " onclick=\"tab('" + tabIds[i] + "',this)\"><i>0" + String(i + 1) + "</i>" + tabNames[i] + "</button>";
  }
  h += "</div></nav><main class=wrap>";

  if (portalMode) {
    h += "<div class=banner><div class=eyebrow>Step 01</div><h2 style='margin:6px 0 4px'>Connect me to Wi-Fi.</h2>"
         "Pick your shop's network below and hit Apply. I'll restart and show up at <b>" MDNS_NAME ".local</b>.</div>";
  }

  // 01 Today
  h += String("<section id=t-today class='pane") + (active == 0 ? " on" : "") + "'>"
       "<div class=card><div class=eyebrow>01 <b>/ Today</b></div>"
       "<div class=big><span>" + esc(cfg.currency) + "</span><strong id=sales>" + fmtAmt(todaySales) + "</strong></div>"
       "<div class=tiles><div class=tile><span class=k>Payments</span><div class=v id=tx>" + String(totalTransactions) + "</div></div>"
       "<div class=tile><span class=k>Avg ticket</span><div class=v id=avg>" + esc(money(avg)) + "</div></div>"
       "<div class=tile><span class=k>Last</span><div class=v id=last>" + (lastPayHM.length() ? lastPayHM : String("--:--")) + "</div></div></div>"
       "<div class=bars id=bars>" + hourlyBars() + "</div><div class=axis><span>00</span><span>06</span><span>12</span><span>18</span><span>23</span></div></div>"

       "<div class=card><div class=eyebrow>02 <b>/ Orders</b></div><h2>Queue.</h2><div id=orders>" + orderRows() + "</div>"
       "<button class='btn ghost' onclick=\"act('/api/clear','CLEARED PAID ORDERS')\">Clear paid orders</button></div>"

       "<div class=card><div class=eyebrow>03 <b>/ New order</b></div><h2>Push to screen.</h2>"
       "<label>Item</label><input id=p_item placeholder='2x Cappuccino'>"
       "<label>Amount (" + esc(cfg.currency) + ")</label><input id=p_amt type=number inputmode=numeric placeholder='450'>"
       "<button class=btn onclick='addOrder()'>Send to screen</button></div>"

       "<div class=grid2><a class='btn cyan' href='/test?amt=100'>Test voice</a>"
       "<button class='btn danger' onclick=\"if(confirm('Reset today to 0?'))act('/api/reset','TODAY RESET')\">Reset today</button></div>"
       "</section>";

  // 02 QR
  h += String("<section id=t-qr class='pane") + (active == 1 ? " on" : "") + "'><form method=post action=/save>"
       "<div class=card><div class=eyebrow>01 <b>/ Payment QR</b></div><h2>What customers scan.</h2>"
       "<label>QR text or link</label>"
       "<textarea name=qr id=qr_input rows=5 placeholder='Paste the text inside your eSewa / Fonepay / Khalti merchant QR'>" + esc(cfg.qr) + "</textarea>"
       "<small>Scan your printed shop QR with any QR reader app, copy the text it shows, paste it here.</small>"
       "<label>Quick fill</label><select onchange='presetQR(this.value)'><option value=''>Choose…</option>"
       "<option value=upi>UPI link (India)</option><option value=paypal>PayPal.me link</option></select>"
       "<label>Line under the headline</label><input name=qrsub value='" + esc(cfg.qrSub) + "' placeholder='Scan with eSewa, Khalti or any bank app'>"
       "<button class=btn>Save QR</button></div></form></section>";

  // 03 Brand
  h += String("<section id=t-brand class='pane") + (active == 2 ? " on" : "") + "'><form method=post action=/save>"
       "<div class=card><div class=eyebrow>01 <b>/ Brand</b></div><h2>Shop details.</h2>"
       "<label>Business name</label><input name=bname value='" + esc(cfg.bizName) + "' placeholder='Roast & Co'>"
       "<div class=grid2><div><label>Currency</label><input name=curr value='" + esc(cfg.currency) + "' placeholder='Rs'></div>"
       "<div><label>Status</label><select name=sopen><option value=1" + sel(cfg.storeOpen) + ">Open</option><option value=0" + sel(!cfg.storeOpen) + ">Closed</option></select></div></div>"
       "<label>Today's special</label><input name=snote value='" + esc(cfg.storeNote) + "' placeholder='Iced latte + croissant for Rs 380'>"
       "<small>Shows on the Board screen customers see.</small>"
       "<div class=grid2><div><label>Auto-flip QR / Board</label><select name=cycle><option value=1" + sel(cfg.autoCycle) + ">On</option><option value=0" + sel(!cfg.autoCycle) + ">Off</option></select></div>"
       "<div><label>Every (seconds)</label><input type=number name=csec min=5 max=120 value='" + String(cfg.cycleSecs) + "'></div></div>"
       "<button class=btn>Save</button></div></form></section>";

  // 04 Gmail
  h += String("<section id=t-gmail class='pane") + (active == 3 ? " on" : "") + "'><form method=post action=/save>"
       "<div class=card><div class=eyebrow>01 <b>/ Payment alerts</b></div><h2>Listen for payments.</h2>"
       "<label>Source</label><select name=emode>"
       "<option value=0" + sel(cfg.emailMode == 0) + ">Gmail inbox (direct IMAP)</option>"
       "<option value=1" + sel(cfg.emailMode == 1) + ">Google Apps Script webhook</option>"
       "<option value=2" + sel(cfg.emailMode == 2) + ">Local API only</option></select>"
       "<label>Gmail address</label><input type=email name=guser value='" + esc(cfg.gmailUser) + "' placeholder='yourshop@gmail.com' autocomplete=off>"
       "<label>App password</label><input type=password name=gpass value='" + esc(cfg.gmailPass) + "' placeholder='xxxx xxxx xxxx xxxx' autocomplete=off>"
       "<small>16 characters, made at <a href='https://myaccount.google.com/apppasswords' target=_blank>myaccount.google.com/apppasswords</a></small>"
       "<label>Search filter</label><input name=query value='" + esc(cfg.imapQuery) + "' placeholder='UNSEEN FROM \"esewa\"' class=mono>"
       "<div class=stat>STATUS · <span id=imap>" + esc(lastImapStatus) + "</span></div>"
       "<button class=btn>Save</button></div></form></section>";

  // 05 System
  h += String("<section id=t-sys class='pane") + (active == 4 ? " on" : "") + "'><form method=post action=/save>"
       "<div class=card><div class=eyebrow>01 <b>/ Wi-Fi</b></div><h2>Network.</h2>"
       "<label>Network name</label><input name=ssid value='" + esc(cfg.ssid) + "' autocapitalize=off autocorrect=off>"
       "<label>Password</label><input type=password name=pass placeholder='Leave blank to keep current'>"
       "<label>Admin PIN (optional)</label><input type=password name=pin inputmode=numeric value='" + esc(cfg.adminPin) + "' placeholder='Protects this page'>"
       "<button class=btn>Apply</button></div>"

       "<div class=card><div class=eyebrow>02 <b>/ Sound + screen</b></div><h2>Feel.</h2>"
       "<label>Volume · <span id=vl>" + String(cfg.volume) + "</span>%</label>"
       "<input type=range name=vol min=5 max=100 step=5 value='" + String(cfg.volume) + "' oninput=\"$('#vl').textContent=this.value\">"
       "<small>Around 25% keeps the screen from flickering on USB power.</small>"
       "<label>Announcement</label><select name=vmode>"
       "<option value=0" + sel(cfg.voiceMode == 0) + ">\"Rupees 500 received\"</option>"
       "<option value=1" + sel(cfg.voiceMode == 1) + ">\"Payment of 500 confirmed, thank you\"</option>"
       "<option value=2" + sel(cfg.voiceMode == 2) + ">\"500 received\"</option></select>"
       "<label>Brightness · <span id=bl>" + String(cfg.brightness) + "</span>%</label>"
       "<input type=range name=bright min=10 max=100 step=10 value='" + String(cfg.brightness) + "' oninput=\"$('#bl').textContent=this.value\">"
       "<button class=btn>Save</button></div>"

       "<div class=card><div class=eyebrow>03 <b>/ Touch</b></div><h2>Calibration.</h2>"
       "<label class=check><input type=checkbox name=tswap value=1" + chk(cfg.swapTouch) + "> Swap X and Y</label>"
       "<label class=check><input type=checkbox name=tinvx value=1" + chk(cfg.invertTouchX) + "> Flip X</label>"
       "<label class=check><input type=checkbox name=tinvy value=1" + chk(cfg.invertTouchY) + "> Flip Y</label>"
       "<button class='btn ghost'>Save touch</button></div></form></section>";

  h += "</main><footer>27.7° N, 85.3° E · MADE IN KATHMANDU</footer><div id=toast class=toast></div><script>";
  h += WEB_JS;
  h += "</script></body></html>";

  server.send(200, "text/html; charset=utf-8", h);
}

void handleSave() {
  if (!authed()) return;
  String nssid = server.arg("ssid"), npass = server.arg("pass");
  bool wifiChanged = (nssid.length() && nssid != cfg.ssid) || npass.length();
  if (nssid.length()) cfg.ssid = nssid;
  if (npass.length()) cfg.pass = npass;

  if (server.hasArg("bname"))  cfg.bizName   = server.arg("bname");
  if (server.hasArg("curr"))   cfg.currency  = server.arg("curr");
  if (server.hasArg("snote"))  cfg.storeNote = server.arg("snote");
  if (server.hasArg("sopen"))  cfg.storeOpen = (server.arg("sopen") == "1");
  if (server.hasArg("qr"))     { cfg.qr = server.arg("qr"); cfg.qr.trim(); }
  if (server.hasArg("qrsub"))  cfg.qrSub = server.arg("qrsub");

  if (server.hasArg("vol"))    cfg.volume = constrain(server.arg("vol").toInt(), 5, 100);
  if (server.hasArg("bright")) {
    cfg.brightness = constrain(server.arg("bright").toInt(), 10, 100);
    setBacklight(cfg.brightness);
  }
  if (server.hasArg("vmode"))  cfg.voiceMode = server.arg("vmode").toInt();
  if (server.hasArg("cycle"))  cfg.autoCycle = (server.arg("cycle") == "1");
  if (server.hasArg("csec"))   cfg.cycleSecs = constrain(server.arg("csec").toInt(), 5, 120);

  if (server.hasArg("emode"))  cfg.emailMode = server.arg("emode").toInt();
  if (server.hasArg("guser"))  { cfg.gmailUser = server.arg("guser"); cfg.gmailUser.trim(); }
  if (server.hasArg("gpass"))  { cfg.gmailPass = server.arg("gpass"); cfg.gmailPass.trim(); }
  if (server.hasArg("query"))  { cfg.imapQuery = server.arg("query"); cfg.imapQuery.trim(); }

  if (server.hasArg("pin"))    cfg.adminPin = server.arg("pin");
  // Touch checkboxes only exist on the touch form, which is the one with the volume slider
  if (server.hasArg("vol")) {
    cfg.swapTouch    = server.hasArg("tswap");
    cfg.invertTouchX = server.hasArg("tinvx");
    cfg.invertTouchY = server.hasArg("tinvy");
  }

  saveCfg();

  String msg = pageHead("Saved");
  msg += "<body><div class=strip></div><main class=wrap style='padding-top:48px'>"
         "<div class=eyebrow style='color:var(--green)'>Saved</div>";
  if (wifiChanged || portalMode) {
    msg += "<h1>Joining " + esc(cfg.ssid) + ".</h1><p style='color:var(--muted)'>Switch your phone back to your normal Wi-Fi, "
           "then open <a href='http://" MDNS_NAME ".local'>" MDNS_NAME ".local</a>. If that doesn't load, the IP is on the device's Setup screen.</p></main></body></html>";
    server.send(200, "text/html; charset=utf-8", msg);
    delay(1500);
    ESP.restart();
  } else {
    msg += "<h1>Done.</h1><p style='color:var(--muted)'>The screen has updated.</p>"
           "<a class=btn href='/' style='margin-top:24px'>Back to dashboard</a></main></body></html>";
    server.send(200, "text/html; charset=utf-8", msg);
    pendingRefresh = true;
  }
}

void handleTest() {
  if (!authed()) return;
  pendingAmt = max(1, (int)server.arg("amt").toInt());
  server.sendHeader("Location", "/");
  server.send(302);
}

void handleApiState() {
  if (!authed()) return;
  JsonDocument doc;
  struct tm t;
  bool tv = getLocalTime(&t, 0);
  doc["sales"] = todaySales;
  doc["tx"] = totalTransactions;
  doc["avg"] = totalTransactions ? todaySales / totalTransactions : 0;
  doc["last"] = lastPayHM;
  doc["open"] = cfg.storeOpen;
  doc["cur"] = cfg.currency;
  doc["time"] = ui_clockText();
  doc["hour"] = tv ? t.tm_hour : -1;
  doc["imap"] = lastImapStatus;
  doc["net"] = (int)netState;
  JsonArray hr = doc["hourly"].to<JsonArray>();
  for (int i = 0; i < 24; i++) hr.add(hourly[i]);
  JsonArray os = doc["orders"].to<JsonArray>();
  for (int i = 0; i < orderCount; i++) {
    JsonObject o = os.add<JsonObject>();
    o["id"] = orders[i].id;
    o["title"] = orders[i].title;
    o["amount"] = orders[i].amount;
    o["status"] = orders[i].status;
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleApiOrder() {
  if (server.method() == HTTP_POST) {
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));
    String title = doc["title"] | "New Order";
    uint32_t amt = doc["amount"] | 0;
    String status = doc["status"] | "NEW";
    addOrder(title, amt, status);
    server.send(200, "application/json", "{\"status\":\"ok\",\"orderId\":" + String(nextOrderId - 1) + "}");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void handleApiPayment() {
  if (server.method() == HTTP_POST) {
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));
    uint32_t amt = doc["amount"] | 0;
    String note = doc["note"] | "Instant Payment";
    if (amt > 0) {
      addOrder(note, amt, "PAID");
      announce(amt, note);
    }
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

void handleApiReset() {
  if (!authed()) return;
  todaySales = 0;
  totalTransactions = 0;
  lastPayHM = "";
  memset(hourly, 0, sizeof(hourly));
  saveSales();
  if (currentScreen == SCREEN_STATS) drawScreen();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleApiClear() {
  if (!authed()) return;
  int keep = 0;
  for (int i = 0; i < orderCount; i++) {
    if (orders[i].status != "PAID") {
      orders[keep++] = orders[i];
    }
  }
  orderCount = keep;
  if (currentScreen == SCREEN_ORDERS) drawScreen();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}
