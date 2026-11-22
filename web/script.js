// ===== Konfigurasi =====
const TOPIC_T = "smarthome/telemetry";
const TOPIC_C = "smarthome/control";
const LDR_TH = 2000; // sama dengan LDR_THRESHOLD di kode ESP32

const $ = id => document.getElementById(id);

// ===== State =====
let cli = null;          // client MQTT
let demo = null;         // interval mode demo
let st = { auto: 1, led_kuning: 0, led_merah: 0, buzzer: 0, servo: 0 };
const hist = { t: [], h: [] };

// ===== Helper tampilan =====
function log(m) {
  const l = $("log");
  l.textContent = new Date().toLocaleTimeString("id-ID") + "  " + m + "\n" + l.textContent.slice(0, 1500);
}

function setStatus(kind, txt) {
  $("dot").className = "dot " + kind;
  $("stat").textContent = txt;
}

// ===== Tampilkan data telemetri =====
function render(d) {
  st = Object.assign(st, d);

  $("t").innerHTML = Number(d.temperature).toFixed(1) + "<small> °C</small>";
  $("h").innerHTML = Number(d.humidity).toFixed(0) + "<small> %</small>";
  $("l").textContent = d.ldr;
  $("lt").textContent = d.ldr >= LDR_TH ? "Gelap" : "Terang";
  $("p").textContent = d.pir ? "Ada" : "Tidak ada";
  $("pt").textContent = d.pir ? "Gerakan terdeteksi" : "Area aman";

  $("modeTxt").textContent = "Mode: " + (st.auto ? "Otomatis" : "Manual");
  $("btnAuto").textContent = st.auto ? "Pindah ke manual" : "Aktifkan otomatis";

  $("door").setAttribute("transform", "rotate(" + (-st.servo) + " 60 170)");
  $("lampY").setAttribute("fill", st.led_kuning ? "var(--amber)" : "var(--off)");
  $("lampR").setAttribute("fill", st.led_merah ? "var(--red)" : "var(--off)");
  $("pir").setAttribute("fill", d.pir ? "var(--red)" : "var(--off)");
  $("pirHalo").setAttribute("class", d.pir ? "pulse" : "");
  $("pirHalo").setAttribute("opacity", d.pir ? ".8" : "0");

  $("cY").setAttribute("aria-pressed", !!st.led_kuning);
  $("cR").setAttribute("aria-pressed", !!st.led_merah);
  if ("buzzer" in d) {
    $("cB").setAttribute("aria-pressed", !!st.buzzer);
  }
  $("sv").value = st.servo;
  $("svv").textContent = st.servo;

  hist.t.push(d.temperature);
  hist.h.push(d.humidity);
  if (hist.t.length > 40) { hist.t.shift(); hist.h.shift(); }
  draw();
}

// ===== Grafik =====
function draw() {
  const c = $("cv"), r = devicePixelRatio || 1;
  const w = c.clientWidth, h = c.clientHeight;
  c.width = w * r;
  c.height = h * r;
  const x = c.getContext("2d");
  x.scale(r, r);
  x.clearRect(0, 0, w, h);

  const css = getComputedStyle(document.documentElement);
  x.strokeStyle = css.getPropertyValue("--line");
  x.lineWidth = 1;
  for (let i = 0; i <= 4; i++) {
    const y = 8 + i * (h - 24) / 4;
    x.beginPath(); x.moveTo(0, y); x.lineTo(w, y); x.stroke();
  }

  const line = (a, col) => {
    if (a.length < 2) return;
    x.strokeStyle = col;
    x.lineWidth = 2.5;
    x.lineJoin = "round";
    x.beginPath();
    a.forEach((v, i) => {
      const px = i * (w / 39);
      const py = 8 + (h - 24) * (1 - Math.min(Math.max(v, 0), 100) / 100);
      i ? x.lineTo(px, py) : x.moveTo(px, py);
    });
    x.stroke();
  };

  line(hist.h, css.getPropertyValue("--teal").trim());
  line(hist.t, css.getPropertyValue("--red").trim());
  x.fillStyle = css.getPropertyValue("--mute");
  x.font = "11px sans-serif";
  x.fillText("0–100", 2, h - 4);
}

// ===== Kirim perintah ke ESP32 =====
function send(o) {
  if (demo) {
    render(Object.assign({}, st, o, { auto: ("auto" in o) ? o.auto : 0 }));
    return;
  }
  if (!cli || !cli.connected) { log("Belum terhubung ke broker."); return; }
  cli.publish(TOPIC_C, JSON.stringify(o));
  log("Kirim: " + JSON.stringify(o));
}

// ===== Koneksi MQTT (WebSocket TLS) =====
function connect() {
  stopDemo();
  if (cli) { cli.end(true); cli = null; }

  const url = "wss://" + $("host").value.trim() + ":" + $("port").value + "/mqtt";
  setStatus("wait", "Menghubungkan…");
  log("Menghubungkan ke " + url);

  cli = mqtt.connect(url, {
    username: $("user").value,
    password: $("pass").value,
    clientId: "web-smarthome-" + Math.random().toString(16).slice(2, 8),
    reconnectPeriod: 3000,
    connectTimeout: 8000
  });

  cli.on("connect", () => {
    setStatus("on", "Terhubung ke EMQX");
    log("Terhubung.");
    cli.subscribe(TOPIC_T);
  });
  cli.on("reconnect", () => setStatus("wait", "Mencoba lagi…"));
  cli.on("close", () => setStatus("", "Terputus"));
  cli.on("error", e => { log("Gagal: " + e.message); setStatus("", "Gagal terhubung"); });
  cli.on("message", (t, m) => {
    try { render(JSON.parse(m.toString())); }
    catch (e) { log("JSON tidak valid"); }
  });
}

// ===== Mode demo (data simulasi) =====
function stopDemo() {
  if (demo) {
    clearInterval(demo);
    demo = null;
    $("btnDemo").textContent = "Coba mode demo";
  }
}

function startDemo() {
  if (cli) { cli.end(true); cli = null; }
  let k = 0;
  setStatus("wait", "Mode demo (data simulasi)");
  log("Mode demo aktif, tidak memakai broker.");

  demo = setInterval(() => {
    k++;
    const pir = (k % 8 < 3) ? 1 : 0;
    const ldr = Math.round(2000 + 1900 * Math.sin(k / 6));
    const d = {
      temperature: 27 + 2 * Math.sin(k / 5),
      humidity: 62 + 8 * Math.cos(k / 7),
      pir, ldr
    };
    if (st.auto) {
      d.servo = (pir || ldr < LDR_TH) ? 90 : 0;
      d.led_kuning = ldr >= LDR_TH ? 1 : 0;
      d.led_merah = pir;
      d.buzzer = pir;
    }
    render(d);
  }, 2000);

  $("btnDemo").textContent = "Hentikan demo";
}

// ===== Event tombol =====
$("btnConn").onclick = connect;
$("btnDemo").onclick = () => demo ? (stopDemo(), setStatus("", "Belum terhubung")) : startDemo();
$("btnAuto").onclick = () => send({ auto: st.auto ? 0 : 1 });
$("cY").onclick = () => send({ led_kuning: st.led_kuning ? 0 : 1 });
$("cR").onclick = () => send({ led_merah: st.led_merah ? 0 : 1 });
$("cB").onclick = e => {
  const nextBuzz = st.buzzer ? 0 : 1;
  st.buzzer = nextBuzz;
  e.currentTarget.setAttribute("aria-pressed", !!nextBuzz);
  send({ buzzer: nextBuzz });
};
$("sv").oninput = e => $("svv").textContent = e.target.value;
$("sv").onchange = e => send({ servo: parseInt(e.target.value, 10) });
$("btnTheme").onclick = () => {
  const r = document.documentElement;
  const dark = getComputedStyle(r).getPropertyValue("--bg").trim() === "#0e1a1c";
  r.dataset.theme = dark ? "light" : "dark";
  draw();
};

addEventListener("resize", draw);
draw();