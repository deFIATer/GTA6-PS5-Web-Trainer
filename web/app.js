let allVehicles = [];
let categories = [];
let activeCategory = "Wszystkie";
let currentView = window.innerWidth >= 900 ? "all" : "self";
let currentLang = "en";
try { currentLang = localStorage.getItem("trainer-lang") || "en"; } catch (e) {}

const CAT_EN = {
  "Wszystkie": "All",
  "Supersamochody": "Supercars",
  "Sportowe": "Sports",
  "Muscle": "Muscle",
  "Sedany i Coupe": "Sedans & Coupes",
  "Kompakty": "Compacts",
  "SUV i Kombi": "SUVs & Wagons",
  "Terenowe i Pickupy": "Off-Road & Pickups",
  "Motocykle": "Motorcycles",
  "Rowery": "Bicycles",
  "Samoloty": "Planes",
  "Helikoptery": "Helicopters",
  "Służby i Wojskowe": "Emergency & Military",
  "Łodzie": "Boats",
  "Vany i Ciężarówki": "Vans & Trucks",
  "Inne": "Other",
};

const I18N = {
  en: {
    nav_dashboard: "Dashboard", nav_self: "Player", nav_vehicles: "Vehicles", nav_vehicles_full: "Vehicles (254)",
    nav_world: "World", nav_world_full: "World & Weather", nav_lektor: "Voice", nav_lektor_full: "Voiceover",
    nav_money: "Money", nav_ps5_full: "Money & PS5",
    credits_offsets: "Game offsets:", credits_cats: "Categories:", credits_footer: "Game offsets & vehicle list based on",
    connect_ps5: "Connect PS5", refresh_status: "Refresh Status", disconnected: "Disconnected",
    quick_actions: "Quick Actions", btn_heal_all: "⚡ All-in-One (Heal + Armor + Repair)",
    btn_heal: "❤️ Heal", btn_armor: "🛡️ Armor", btn_repair: "🔧 Repair",
    protection_bg: "Protection (Background)", god_mode_sub: "Auto-Heal + Auto-Repair vehicle every 0.25s",
    never_wanted_sub: "Automatically clears police wanted stars",
    ps5_notify_lbl: "🔔 PS5 Notifications", ps5_notify_sub: "Shows action toasts on your TV screen",
    wanted_title: "Wanted Level", time_title: "Time of Day",
    time_morning: "🌅 Morning (08:00)", time_noon: "☀️ Noon (12:00)", time_sunset: "🌇 Sunset (19:00)", time_night: "🌙 Midnight (00:00)",
    btn_set: "Set", freeze_sub: "Locks the in-game clock at the current hour",
    weather_title: "Weather", btn_weather: "🌦️ Next Weather",
    money_title: "Money", wallet_none: "No Wallet",
    money_intro: "Choose a mode below: Manual (by exact cash) or Auto-Detect (works from $0).",
    money_m1: "Mode 1 · Manual (when cash > $0)", money_val_ph: "Enter exact cash shown in game", btn_search: "🔍 Search",
    money_m2: "Mode 2 · Auto-Detect (works from $0 — then change cash in game)",
    money_auto_ph: "Current cash (e.g. 0 or 250 — optional)", btn_auto: "🤖 Auto-Detect",
    btn_set_1m: "Set $1,000,000", btn_set_99m: "Set $99,999,999",
    ps5_conn_title: "PS5 Connection (PS5Debug-NG)", stat_game: "Game", stat_hook: "Hook / Vehicle",
    log_title: "Event Log", log_ready: "Ready. Click \"Connect PS5\" in the top-right corner.",
    lek_title: "Live Subtitle Voiceover (100% Local, No API)", lek_off: "Disabled",
    lek_listen: "🎙️ Listen to PS5 Subtitles", lek_listen_sub: "Reads live in-game dialogue from RAM on a dedicated channel (~1.5s)",
    lek_mac: "💻 Read on Mac (macOS say)", lek_mac_sub: "Local macOS voice synthesizer — FIFO queue, zero latency",
    lek_web: "📱 Read in Browser / Phone", lek_web_sub: "Local Web Speech API (matches selected language)",
    lek_spk: "Read Speaker Name", lek_spk_sub: "e.g. \"Jason: Got your sunscreen on?\"",
    lek_speed: "Speed", lek_cur_title: "Current In-Game Subtitle",
    lek_waiting: "Enable listening above and trigger a dialogue in GTA VI...",
    lek_hist_title: "Dialogue History (Click to Replay)", lek_hist_empty: "No subtitles captured yet.",
    btn_filter_veh: "🔍 Available in Game Only", veh_search_ph: "Search vehicle (e.g. banshee, apc, pmp 700)...",
    quick_spawn: "Quick Spawn",
  },
  pl: {
    nav_dashboard: "Dashboard", nav_self: "Postać", nav_vehicles: "Pojazdy", nav_vehicles_full: "Pojazdy (254)",
    nav_world: "Świat", nav_world_full: "Świat i Pogoda", nav_lektor: "Lektor", nav_lektor_full: "Lektor PL",
    nav_money: "Kasa", nav_ps5_full: "Kasa i PS5",
    credits_offsets: "Offsety gry:", credits_cats: "Kategorie:", credits_footer: "Offsety gry i lista pojazdów na podstawie",
    connect_ps5: "Połącz z PS5", refresh_status: "Odśwież status", disconnected: "Rozłączono",
    quick_actions: "Szybkie akcje", btn_heal_all: "⚡ Wszystko (Heal + Pancerz + Auto)",
    btn_heal: "❤️ Heal", btn_armor: "🛡️ Pancerz", btn_repair: "🔧 Auto",
    protection_bg: "Ochrona (w tle)", god_mode_sub: "Auto-Heal + Auto-Naprawa pojazdu co 0.25s",
    never_wanted_sub: "Automatycznie zeruje gwiazdki policji",
    ps5_notify_lbl: "🔔 Powiadomienia na PS5", ps5_notify_sub: "Wyświetla akcje w rogu ekranu telewizora",
    wanted_title: "Poziom Policji (Wanted Level)", time_title: "Pora dnia",
    time_morning: "🌅 Rano (08:00)", time_noon: "☀️ Południe (12:00)", time_sunset: "🌇 Zachód (19:00)", time_night: "🌙 Noc (00:00)",
    btn_set: "Ustaw", freeze_sub: "Zatrzymuje zegar na obecnej godzinie",
    weather_title: "Pogoda", btn_weather: "🌦️ Następna pogoda",
    money_title: "Pieniądze (Money)", wallet_none: "Brak portfela",
    money_intro: "Wybierz tryb poniżej: Ręczny (po kwocie) lub Auto-wykrycie (od $0).",
    money_m1: "Tryb 1 · Ręczny (gdy masz > $0)", money_val_ph: "Wpisz dokładną gotówkę z ekranu gry", btn_search: "🔍 Szukaj",
    money_m2: "Tryb 2 · Auto-wykrycie (działa od $0 — potem zmień saldo w grze)",
    money_auto_ph: "Obecna kwota (np. 0 lub 250 — opcjonalnie)", btn_auto: "🤖 Auto-wykrycie",
    btn_set_1m: "Ustaw $1 000 000", btn_set_99m: "Ustaw $99 999 999",
    ps5_conn_title: "Połączenie z PS5 (PS5Debug-NG)", stat_game: "Gra", stat_hook: "Hook / Pojazd",
    log_title: "Log zdarzeń", log_ready: "Gotowy. Kliknij „Połącz PS5” w prawym górnym rogu.",
    lek_title: "Polski Lektor (100% lokalnie, bez API)", lek_off: "Wyłączony",
    lek_listen: "🎙️ Nasłuch napisów z PS5", lek_listen_sub: "Czyta dialogi z RAM-u gry na osobnym kanale (~1.5s)",
    lek_mac: "💻 Czytaj na Macu (Zosia PL)", lek_mac_sub: "Lokalny syntezator macOS — kolejka FIFO, zero opóźnień",
    lek_web: "📱 Czytaj w przeglądarce / na telefonie", lek_web_sub: "Lokalny głos Web Speech API (zgodny z językiem)",
    lek_spk: "Czytaj imię postaci", lek_spk_sub: "Np. „Jason: Krem z filtrem nałożony?”",
    lek_speed: "Szybkość", lek_cur_title: "Aktualny napis z gry",
    lek_waiting: "Włącz nasłuch powyżej i podejdź do dialogu w GTA VI...",
    lek_hist_title: "Historia dialogów (kliknij, aby powtórzyć)", lek_hist_empty: "Brak przechwyconych napisów.",
    btn_filter_veh: "🔍 Tylko dostępne w grze", veh_search_ph: "Szukaj pojazdu (np. banshee, apc, pmp 700)...",
    quick_spawn: "Szybki spawn",
  }
};

function catLabel(cat) {
  return currentLang === "en" ? (CAT_EN[cat] || cat) : cat;
}

function applyLang() {
  const dict = I18N[currentLang] || I18N.en;
  document.documentElement.lang = currentLang;
  const lb = document.getElementById("lang-btn");
  if (lb) lb.textContent = currentLang === "en" ? "🌐 EN" : "🌐 PL";
  document.querySelectorAll("[data-i18n]").forEach(el => {
    const k = el.getAttribute("data-i18n");
    if (dict[k]) el.textContent = dict[k];
  });
  document.querySelectorAll("[data-i18n-ph]").forEach(el => {
    const k = el.getAttribute("data-i18n-ph");
    if (dict[k]) el.placeholder = dict[k];
  });
  if (allVehicles.length) { renderCategories(); renderVehicles(); }
}

function t(en, pl) {
  return currentLang === "pl" ? pl : en;
}

function toggleLang() {
  currentLang = currentLang === "en" ? "pl" : "en";
  try { localStorage.setItem("trainer-lang", currentLang); } catch (e) {}
  applyLang();
  refreshStatus();
  showToast(currentLang === "en" ? "Language: English" : "Język: Polski");
}

let state = {
  god_mode: false, never_wanted: false, freeze_time: false, ps5_notify: true,
  lektor_enabled: false, lektor_mac_say: true, lektor_read_speaker: false,
};
let browserSpeech = false;
let lastSpokenSeq = 0;
let toastTimer = null;

/* Theme toggle following modern-web-guidance (2-state system-aware override) */
const themeMeta = document.querySelector('meta[name="color-scheme"]');
const darkMQ = window.matchMedia("(prefers-color-scheme: dark)");

function getSystemScheme() { return darkMQ.matches ? "dark" : "light"; }
function getRenderedScheme() {
  let stored = null;
  try { stored = localStorage.getItem("color-scheme"); } catch (e) {}
  return stored === "light" || stored === "dark" ? stored : getSystemScheme();
}
function applyMetaScheme(val) {
  themeMeta.content = val === "light" || val === "dark" ? val : "light dark";
}
function toggleTheme() {
  const target = getRenderedScheme() === "dark" ? "light" : "dark";
  const sys = getSystemScheme();
  try {
    if (target === sys) {
      localStorage.removeItem("color-scheme");
      applyMetaScheme("light dark");
    } else {
      localStorage.setItem("color-scheme", target);
      applyMetaScheme(target);
    }
  } catch (e) {
    applyMetaScheme(target);
  }
  showToast(target === "dark" ? t("Theme: Dark", "Motyw: Ciemny") : t("Theme: Light", "Motyw: Jasny"));
}
window.addEventListener("storage", (e) => {
  if (e.key === "color-scheme") applyMetaScheme(e.newValue ?? "light dark");
});

function switchTab(name) {
  currentView = name;
  const mainRoot = document.getElementById("main-root");
  const isDesktop = window.innerWidth >= 900;
  if (name === "all" && isDesktop) {
    mainRoot.classList.add("dashboard-all");
    document.querySelectorAll(".tab-panel").forEach(p => p.classList.add("active"));
  } else {
    mainRoot.classList.remove("dashboard-all");
    const target = name === "all" ? "self" : name;
    document.querySelectorAll(".tab-panel").forEach(p => p.classList.remove("active"));
    document.getElementById("tab-" + target).classList.add("active");
  }
  document.querySelectorAll(".nav-btn").forEach(b => b.classList.toggle("active", b.dataset.tab === name));
  document.querySelectorAll(".side-btn").forEach(b => b.classList.toggle("active", b.dataset.side === name));
  window.scrollTo({ top: 0, behavior: "instant" });
}

window.addEventListener("resize", () => {
  if (window.innerWidth >= 900 && currentView === "all") {
    switchTab("all");
  } else if (window.innerWidth < 900 && currentView === "all") {
    switchTab("self");
  }
});

function showToast(msg, cls="") {
  const t = document.getElementById("toast");
  t.textContent = msg;
  t.className = "show " + cls;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { t.className = ""; }, 2600);
}

function addLog(msg, cls="") {
  const el = document.getElementById("log");
  const line = document.createElement("div");
  line.className = cls;
  line.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
  el.prepend(line);
}

async function callApi(url, body=null, silent=false) {
  try {
    const res = await fetch(url, {
      method: body === null && url === "/api/status" ? "GET" : "POST",
      headers: { "Content-Type": "application/json" },
      body: body === null ? "{}" : JSON.stringify(body),
    });
    const data = await res.json();
    if (!res.ok || data.error) {
      const err = data.error || `HTTP ${res.status}`;
      addLog(err, "log-err");
      if (!silent) showToast(err, "err");
      return null;
    }
    if (data.message) {
      addLog(data.message, "log-ok");
      if (!silent) showToast(data.message, "ok");
    }
    if (data.status) applyStatus(data.status);
    return data;
  } catch (e) {
    addLog(t("Network error: ", "Błąd sieci: ") + e, "log-err");
    if (!silent) showToast(t("Network error", "Błąd sieci"), "err");
    return null;
  }
}

function applyStatus(s) {
  state.god_mode = !!s.god_mode;
  state.never_wanted = !!s.never_wanted;
  state.freeze_time = !!s.freeze_time;
  state.ps5_notify = s.ps5_notify !== false;
  state.lektor_enabled = !!s.lektor_enabled;
  state.lektor_mac_say = s.lektor_mac_say !== false;
  state.lektor_read_speaker = !!s.lektor_read_speaker;

  const b = document.getElementById("badge");
  const ps5Badge = document.getElementById("ps5-conn-badge");
  const btnConn = document.getElementById("btn-connect");
  const connActions = document.getElementById("ps5-conn-actions");
  if (s.connected && s.version_ok) {
    b.textContent = "● PS5 OK"; b.className = "pill ok";
    ps5Badge.textContent = t("● Connected", "● Połączono"); ps5Badge.className = "pill ok";
    btnConn.style.display = "none";
    connActions.style.gridTemplateColumns = "1fr";
  } else if (s.connected) {
    b.textContent = t("● Wrong version", "● Zła wersja"); b.className = "pill err";
    ps5Badge.textContent = t("● Wrong version", "● Zła wersja"); ps5Badge.className = "pill err";
    btnConn.style.display = "";
    connActions.style.gridTemplateColumns = "";
  } else {
    b.textContent = t("Connect PS5", "Połącz PS5"); b.className = "pill";
    ps5Badge.textContent = t("Disconnected", "Rozłączono"); ps5Badge.className = "pill";
    btnConn.style.display = "";
    connActions.style.gridTemplateColumns = "";
  }

  const clk = s.hour >= 0 ? `${String(s.hour).padStart(2,"0")}:${String(s.minute).padStart(2,"0")}` : "--:--";
  document.getElementById("mini-clock").textContent = clk;
  document.getElementById("mini-wanted").textContent = "★ " + (s.wanted >= 0 ? s.wanted : "-");

  document.getElementById("st-game").textContent = s.titleid ? `${s.titleid} ${s.app_ver}` : "—";
  document.getElementById("st-pid").textContent = s.pid ? `FW ${s.fw || "?"} / PID ${s.pid}` : "—";
  document.getElementById("st-env").textContent = s.connected
    ? `${s.kstuff ? "kstuff OK" : t("no kstuff", "brak kstuff")} · ${s.etahen ? "etaHEN" : t("none", "brak")}`
    : "—";
  document.getElementById("st-veh").textContent = s.connected
    ? `${s.hook ? "Hook OK" : t("none", "brak")} · ${s.in_vehicle ? t("In Vehicle", "W aucie") : t("On Foot", "Pieszo")}`
    : "—";

  document.getElementById("sw-gm").classList.toggle("on", state.god_mode);
  document.getElementById("sw-nw").classList.toggle("on", state.never_wanted);
  document.getElementById("sw-ntf").classList.toggle("on", state.ps5_notify);
  document.getElementById("sw-ft").classList.toggle("on", state.freeze_time);
  document.getElementById("sw-lek").classList.toggle("on", state.lektor_enabled);
  document.getElementById("sw-macsay").classList.toggle("on", state.lektor_mac_say);
  document.getElementById("sw-webtts").classList.toggle("on", browserSpeech);
  document.getElementById("sw-spk").classList.toggle("on", state.lektor_read_speaker);

  const lb = document.getElementById("lektor-badge");
  lb.textContent = state.lektor_enabled ? t("● Listening", "● Nasłuch aktywny") : t("Disabled", "Wyłączony");
  lb.className = state.lektor_enabled ? "pill ok" : "pill";

  if (s.lektor_rate) {
    document.getElementById("lek-rate").value = s.lektor_rate;
    document.getElementById("lek-rate-val").textContent = s.lektor_rate;
  }

  if (s.subtitle_current) {
    document.getElementById("sub-cur-spk").textContent = `${s.subtitle_current.speaker || "Dialog"} · ${s.subtitle_current.time}`;
    document.getElementById("sub-cur-txt").textContent = s.subtitle_current.text;
    if (s.subtitle_seq && s.subtitle_seq > lastSpokenSeq) {
      const prevSeq = lastSpokenSeq;
      lastSpokenSeq = s.subtitle_seq;
      if (browserSpeech) {
        const toSpeak = (Array.isArray(s.subtitle_history) ? s.subtitle_history : [])
          .filter(it => prevSeq === 0 ? it.seq === s.subtitle_seq : it.seq > prevSeq)
          .sort((a, b) => a.seq - b.seq);
        for (const it of toSpeak) {
          const line = (state.lektor_read_speaker && it.speaker) ? `${it.speaker}: ${it.text}` : it.text;
          speakInBrowser(line, false);
        }
      }
    }
  }
  if (Array.isArray(s.subtitle_history) && s.subtitle_history.length) {
    const box = document.getElementById("sub-history");
    box.innerHTML = "";
    for (const item of s.subtitle_history) {
      const btn = document.createElement("button");
      btn.className = "veh-card";
      btn.style.width = "100%";
      btn.innerHTML = `<div class="veh-cat"></div><div style="font-size:13.5px;font-weight:600;margin-top:2px;white-space:normal"></div>`;
      btn.firstChild.textContent = `[${item.time}] ${item.speaker || "Dialog"}`;
      btn.lastChild.textContent = item.text;
      btn.onclick = () => replaySubtitle(item.speaker, item.text);
      box.appendChild(btn);
    }
  }
  document.getElementById("ft-sub").textContent = state.freeze_time
    ? t(`Frozen at ${s.frozen_time}`, `Zatrzymany na ${s.frozen_time}`)
    : t("Locks the in-game clock at the current hour", "Zatrzymuje zegar na obecnej godzinie");

  if (typeof s.money === "number" && s.money >= 0) {
    const formatted = "$" + s.money.toLocaleString(currentLang === "pl" ? "pl-PL" : "en-US");
    document.getElementById("money-live").textContent = formatted;
    document.getElementById("mini-money").textContent = formatted;
  } else {
    document.getElementById("money-live").textContent = "$—";
    document.getElementById("mini-money").textContent = "$—";
  }

  const mode = s.money_mode || "idle";
  const mb = document.getElementById("money-mode-badge");
  const cancelBtn = document.getElementById("money-cancel-btn");
  const controls = document.getElementById("money-controls");
  const presets = document.getElementById("money-presets");

  if (mode === "locked") {
    mb.textContent = t("✅ Locked", "✅ Przechwycony"); mb.className = "pill ok";
    cancelBtn.style.display = "inline-flex";
    cancelBtn.textContent = "↻ Reset";
    controls.style.display = "none";
    presets.style.opacity = "1";
  } else if (mode === "manual_scanning") {
    mb.textContent = t("🔄 MANUAL", "🔄 Tryb RĘCZNY"); mb.className = "pill";
    cancelBtn.style.display = "inline-flex";
    cancelBtn.textContent = t("✖ Cancel", "✖ Anuluj");
    controls.style.display = "none";
    presets.style.opacity = "0.45";
  } else if (mode === "auto_scanning" || mode === "auto_watching") {
    mb.textContent = mode === "auto_scanning" ? t("🔄 AUTO (scan)", "🔄 AUTO (skan)") : t("🤖 AUTO (waiting)", "🤖 AUTO (czeka)");
    mb.className = "pill";
    cancelBtn.style.display = "inline-flex";
    cancelBtn.textContent = t("✖ Cancel", "✖ Anuluj");
    controls.style.display = "none";
    presets.style.opacity = "0.45";
  } else {
    mb.textContent = t("No Wallet", "Brak portfela"); mb.className = "pill";
    cancelBtn.style.display = "none";
    controls.style.display = "block";
    presets.style.opacity = "0.45";
  }
  if (s.money_watch_note && mode !== "idle") {
    document.getElementById("money-info").textContent = s.money_watch_note;
  } else if (mode === "idle") {
    document.getElementById("money-info").textContent = t(
      "Choose a mode below: Manual (by exact cash) or Auto-Detect (works from $0).",
      "Wybierz tryb poniżej: Ręczny (po kwocie) lub Auto-wykrycie (od $0)."
    );
  }
}

async function doConnect() {
  const ip = document.getElementById("ip").value.trim();
  const port = parseInt(document.getElementById("port").value, 10) || 744;
  showToast(t(`Connecting to ${ip}...`, `Łączenie z ${ip}...`));
  await callApi("/api/connect", { ip, port });
}

async function refreshStatus() {
  try {
    const res = await fetch("/api/status");
    applyStatus(await res.json());
  } catch (e) {}
}

function toggleBrowserSpeech() {
  browserSpeech = !browserSpeech;
  document.getElementById("sw-webtts").classList.toggle("on", browserSpeech);
  showToast(
    t("Browser voiceover: ", "Lektor w przeglądarce: ") + (browserSpeech ? t("ON", "WŁĄCZONY") : t("OFF", "WYŁĄCZONY")),
    "ok"
  );
  if (browserSpeech) speakInBrowser(t("Browser voiceover is ready.", "Polski lektor w przeglądarce gotowy."), true);
  else if ("speechSynthesis" in window) window.speechSynthesis.cancel();
}

function speakInBrowser(text, interrupt = false) {
  if (!("speechSynthesis" in window)) return;
  try {
    if (interrupt) window.speechSynthesis.cancel();
    const u = new SpeechSynthesisUtterance(text);
    u.lang = currentLang === "pl" ? "pl-PL" : "en-US";
    const rateVal = parseInt(document.getElementById("lek-rate").value, 10) || 195;
    u.rate = Math.max(0.7, Math.min(1.8, rateVal / 190));
    window.speechSynthesis.speak(u);
  } catch (e) {}
}

function setLektorRate(val) {
  callApi("/api/lektor/rate", { rate: parseInt(val, 10) || 195 });
}

function testLektor() {
  const sample = t(
    "Welcome to Vice City! Local voiceover is working.",
    "Witaj w Vice City! Polski lektor działa lokalnie."
  );
  if (browserSpeech) speakInBrowser(sample, true);
  if (state.lektor_mac_say) callApi("/api/lektor/speak", { text: sample });
  if (!browserSpeech && !state.lektor_mac_say) {
    showToast(t("Enable Mac or Browser voiceover first", "Włącz lektora na Macu lub w przeglądarce"), "err");
  }
}

function replaySubtitle(speaker, text) {
  const line = (state.lektor_read_speaker && speaker) ? `${speaker}: ${text}` : text;
  if (browserSpeech) speakInBrowser(line, true);
  if (state.lektor_mac_say) callApi("/api/lektor/speak", { text: line }, true);
}

function setTime() {
  const hour = parseInt(document.getElementById("hour").value, 10) || 0;
  const minute = parseInt(document.getElementById("minute").value, 10) || 0;
  callApi("/api/time", { hour, minute });
}

function toggleOpt(name) {
  callApi("/api/toggle", { name, enabled: !state[name] });
}

function spawnVeh(name) {
  showToast(t(`Spawning ${name}...`, `Spawnowanie ${name}...`));
  callApi("/api/spawn", { name });
}

async function filterAvailableVehicles() {
  showToast(t("Scanning available vehicles...", "Skanuję dostępne pojazdy..."));
  const data = await callApi("/api/filter_vehicles", {});
  if (data && data.vehicles) {
    allVehicles = data.vehicles;
    renderCategories();
    renderVehicles();
  }
}

async function moneyFind() {
  const val = parseInt(document.getElementById("money-val").value, 10);
  if (isNaN(val) || val <= 0) {
    showToast(t("Enter an exact amount > 0 (use Auto-Detect for $0)", "Wpisz dokładną kwotę > 0 (dla $0 użyj trybu Auto)"), "err");
    return;
  }
  await callApi("/api/money/find", { amount: val });
}

async function moneyWatch() {
  const raw = document.getElementById("money-auto-val").value.trim();
  const amount = raw === "" ? null : parseInt(raw, 10);
  await callApi("/api/money/watch", { amount: (amount !== null && !isNaN(amount) && amount >= 0) ? amount : null });
}

async function moneyReset() {
  await callApi("/api/money/reset", {});
}

async function moneyAdd(delta) {
  await callApi("/api/money/add", { delta });
}

async function moneySet(amount) {
  await callApi("/api/money/set", { amount });
}

function renderCategories() {
  const bar = document.getElementById("veh-cats");
  bar.innerHTML = "";
  const counts = { Wszystkie: allVehicles.length };
  for (const v of allVehicles) {
    const c = v.category || "Inne";
    counts[c] = (counts[c] || 0) + 1;
  }
  const list = categories.length ? categories : Object.keys(counts);
  for (const cat of list) {
    const cnt = counts[cat] || 0;
    if (cat !== "Wszystkie" && cnt === 0) continue;
    const btn = document.createElement("button");
    btn.className = "chip" + (activeCategory === cat ? " active" : "");
    btn.textContent = `${catLabel(cat)} (${cnt})`;
    btn.onclick = () => {
      activeCategory = cat;
      renderCategories();
      renderVehicles();
    };
    bar.appendChild(btn);
  }
}

function renderVehicles() {
  const q = document.getElementById("veh-search").value.trim().toLowerCase();
  const filtered = allVehicles.filter(v => {
    if (activeCategory !== "Wszystkie" && (v.category || "Inne") !== activeCategory) return false;
    if (q) {
      const disp = (v.display || v.name).toLowerCase();
      if (!disp.includes(q) && !v.name.toLowerCase().includes(q)) return false;
    }
    return true;
  });
  document.getElementById("veh-count").textContent = filtered.length;
  const grid = document.getElementById("veh-grid");
  grid.innerHTML = "";
  for (const v of filtered) {
    const btn = document.createElement("button");
    btn.className = "veh-card";
    btn.innerHTML = `<div class="veh-name"></div><div class="veh-cat"></div>`;
    btn.firstChild.textContent = v.display || v.name;
    btn.lastChild.textContent = catLabel(v.category || "Inne");
    btn.onclick = () => spawnVeh(v.name);
    grid.appendChild(btn);
  }
}

async function init() {
  applyLang();
  switchTab(currentView);
  const res = await fetch("/api/vehicles");
  const data = await res.json();
  allVehicles = data.vehicles || [];
  categories = data.categories || [];
  const quick = document.getElementById("quick-veh");
  for (const name of (data.quick || [])) {
    const btn = document.createElement("button");
    btn.className = "chip";
    btn.textContent = "⚡ " + name;
    btn.onclick = () => spawnVeh(name);
    quick.appendChild(btn);
  }
  renderCategories();
  renderVehicles();
  refreshStatus();
  setInterval(refreshStatus, 1500);
}
init();

/* ---------- DualSense / Gamepad & D-Pad Navigation ---------- */
(function initGamepadNav() {
  const TAB_ORDER = ["self", "vehicles", "world", "lektor", "ps5"];
  let focusIdx = -1;
  let lastMove = 0;
  let prevButtons = [];

  function getFocusables() {
    const root = document.getElementById("main-root");
    if (!root) return [];
    const selector = ".tab-panel.active button:not([style*='display: none']), .tab-panel.active .switch-row";
    return Array.from(root.querySelectorAll(selector)).filter(el => el.offsetParent !== null);
  }

  function setFocus(idx) {
    const items = getFocusables();
    document.querySelectorAll(".gp-focus").forEach(el => el.classList.remove("gp-focus"));
    if (!items.length) { focusIdx = -1; return; }
    focusIdx = ((idx % items.length) + items.length) % items.length;
    const el = items[focusIdx];
    el.classList.add("gp-focus");
    el.scrollIntoView({ block: "nearest", behavior: "smooth" });
  }

  function stepTab(dir) {
    let cur = TAB_ORDER.indexOf(currentView);
    if (cur < 0) cur = 0;
    const next = TAB_ORDER[(cur + dir + TAB_ORDER.length) % TAB_ORDER.length];
    switchTab(next);
    setTimeout(() => setFocus(0), 30);
  }

  function pressSelected() {
    const items = getFocusables();
    if (focusIdx >= 0 && focusIdx < items.length) {
      items[focusIdx].click();
    } else if (items.length) {
      setFocus(0);
    }
  }

  /* Keyboard / PS5 browser D-pad fallback */
  window.addEventListener("keydown", (e) => {
    if (document.activeElement && document.activeElement.tagName === "INPUT") return;
    if (e.key === "ArrowDown" || e.key === "ArrowRight") { e.preventDefault(); setFocus(focusIdx + 1); }
    else if (e.key === "ArrowUp" || e.key === "ArrowLeft") { e.preventDefault(); setFocus(focusIdx - 1); }
    else if (e.key === "Enter" || e.key === " ") {
      if (focusIdx >= 0) { e.preventDefault(); pressSelected(); }
    } else if (e.key === "PageUp") { e.preventDefault(); stepTab(-1); }
    else if (e.key === "PageDown") { e.preventDefault(); stepTab(1); }
  });

  /* Standard Gamepad API polling (DualSense: 0=Cross, 1=Circle, 4=L1, 5=R1, 12..15=D-pad) */
  function pollGamepad() {
    const pads = navigator.getGamepads ? navigator.getGamepads() : [];
    const gp = pads && Array.from(pads).find(Boolean);
    if (gp) {
      const now = performance.now();
      const pressed = (i) => gp.buttons[i] && gp.buttons[i].pressed;
      const justPressed = (i) => pressed(i) && !prevButtons[i];

      if (justPressed(4)) stepTab(-1);      // L1
      else if (justPressed(5)) stepTab(1);  // R1
      else if (justPressed(0)) pressSelected(); // Cross (X)
      else if (justPressed(1)) {            // Circle (O)
        document.querySelectorAll(".gp-focus").forEach(el => el.classList.remove("gp-focus"));
        focusIdx = -1;
      }

      const axY = gp.axes && gp.axes.length > 1 ? gp.axes[1] : 0;
      const axX = gp.axes && gp.axes.length > 0 ? gp.axes[0] : 0;
      const down = pressed(13) || pressed(15) || axY > 0.6 || axX > 0.6;
      const up = pressed(12) || pressed(14) || axY < -0.6 || axX < -0.6;
      if ((down || up) && (now - lastMove) > 170) {
        lastMove = now;
        setFocus(focusIdx + (down ? 1 : -1));
      }
      prevButtons = gp.buttons.map(b => !!b.pressed);
    }
    requestAnimationFrame(pollGamepad);
  }
  requestAnimationFrame(pollGamepad);
})();
