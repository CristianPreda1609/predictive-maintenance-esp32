#pragma once
#include <Arduino.h>

// =====================================================================
//  web_page.h  ::  pagina servita de ESP32 (self-contained, fara CDN)
//  Separata intr-un singur fisier ca sa nu mai treci peste 150 de linii
//  de HTML/CSS/JS cand citesti logica C++.
//
//  NOTA desync: array-ul JS CLASSES de mai jos dubleaza ordinea claselor
//  din C++ (class_names). Tine-le sincron. PWMS dubleaza kAllowedPwm.
//  Sunt doar UI, dar daca le editezi pe una editeaz-o si pe cealalta.
// =====================================================================
const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="ro"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mentenanta Predictiva</title>
<style>
  :root{--bg:#0a0e12;--panel:#121821;--line:#1f2a36;--muted:#6b7d8f;--ink:#cdd9e3;
    --ax:#36f9b3;--ay:#ffcf4d;--az:#5ac8fa;--gx:#a78bfa;--gy:#f472b6;--gz:#7dd3fc;}
  *{box-sizing:border-box}
  body{margin:0;background:var(--bg);color:var(--ink);
    font-family:ui-monospace,"SF Mono",Menlo,Consolas,monospace;-webkit-font-smoothing:antialiased}
  .wrap{max-width:1180px;margin:0 auto;padding:18px 16px 40px;
    display:grid;grid-template-columns:340px 1fr;gap:14px;align-content:start}
  .wrap>header{grid-column:1 / -1;grid-row:1}
  .wrap>.panel{margin-bottom:0}
  .wrap>div:nth-of-type(1){grid-column:1;grid-row:2 / 4}
  .wrap>div:nth-of-type(2){grid-column:2;grid-row:2}
  .wrap>div:nth-of-type(3){grid-column:2;grid-row:3}
  .wrap>div:nth-of-type(4){grid-column:1 / -1;grid-row:4}
  header{display:flex;align-items:baseline;justify-content:space-between;
    border-bottom:1px solid var(--line);padding-bottom:10px;margin-bottom:0}
  h1{font-size:13px;letter-spacing:.18em;text-transform:uppercase;font-weight:600;margin:0;color:var(--muted)}
  .dot{font-size:11px;color:var(--muted)} .dot b{color:var(--ax)}
  .panel{border:1px solid var(--line);border-radius:6px;background:var(--panel)}
  .badge{padding:20px;position:relative;overflow:hidden}
  .lbl{font-size:11px;letter-spacing:.16em;text-transform:uppercase;color:var(--muted)}
  .cls{font-size:42px;font-weight:700;letter-spacing:-.01em;line-height:1.05;margin-top:6px}
  .meta{font-size:12px;color:var(--muted);margin-top:8px}
  .stripe{position:absolute;left:0;top:0;bottom:0;width:5px;background:var(--muted)}
  .bars{padding:14px 18px;border-top:1px solid var(--line)}
  .bar{display:grid;grid-template-columns:96px 1fr 48px;align-items:center;gap:10px;margin:7px 0;font-size:12px}
  .bar .nm{color:var(--muted)} .bar .val{text-align:right;color:var(--ink)}
  .track{height:8px;background:#0c1116;border-radius:4px;overflow:hidden}
  .fill{display:block;height:100%;width:0;border-radius:4px;transition:width .18s linear}
  .scope{padding:12px}
  .cap{display:flex;justify-content:space-between;font-size:11px;color:var(--muted);
    letter-spacing:.12em;text-transform:uppercase;margin-bottom:8px}
  canvas{width:100%;height:150px;display:block}
  .legend{display:flex;gap:16px;font-size:12px;margin-top:8px;color:var(--muted);flex-wrap:wrap}
  .legend i{display:inline-block;width:10px;height:2px;vertical-align:middle;margin-right:6px}
  .pwm{padding:14px 18px;display:flex;gap:8px;flex-wrap:wrap;align-items:center}
  .pwm .t{font-size:11px;letter-spacing:.16em;text-transform:uppercase;color:var(--muted);width:100%;margin-bottom:2px}
  .pwm button{font-family:inherit;font-size:13px;color:var(--ink);background:#0c1116;
    border:1px solid var(--line);border-radius:5px;padding:8px 12px;cursor:pointer}
  .pwm button.on{border-color:var(--ax);color:var(--ax)}
  .pwm button:active{transform:translateY(1px)}
  .off .cls{color:var(--muted)}
  @media(max-width:820px){
    .wrap{grid-template-columns:1fr}
    .wrap>header,.wrap>div:nth-of-type(1),.wrap>div:nth-of-type(2),
    .wrap>div:nth-of-type(3),.wrap>div:nth-of-type(4){grid-column:1;grid-row:auto}
  }
</style></head><body>
<div class="wrap">
  <header>
    <h1>Mentenanta predictiva &middot; CNN1D</h1>
    <div class="dot" id="hb">conexiune <b>&middot;</b></div>
  </header>

  <div class="panel">
    <div class="badge"><div class="stripe" id="stripe"></div>
      <div class="lbl">stare detectata</div>
      <div class="cls" id="cls">&mdash;</div>
      <div class="meta" id="meta">astept date de la senzor</div>
    </div>
    <div class="bars" id="bars"></div>
  </div>

  <div class="panel scope">
    <div class="cap"><span>accelerometru (m/s&sup2;)</span><span>DC scos doar pentru afisare</span></div>
    <canvas id="cvA" width="720" height="150"></canvas>
    <div class="legend">
      <span><i style="background:var(--ax)"></i>ax</span>
      <span><i style="background:var(--ay)"></i>ay</span>
      <span><i style="background:var(--az)"></i>az</span>
    </div>
  </div>

  <div class="panel scope">
    <div class="cap"><span>giroscop (rad/s)</span><span>discrimineaza Repaus &amp; Slabit</span></div>
    <canvas id="cvG" width="720" height="150"></canvas>
    <div class="legend">
      <span><i style="background:var(--gx)"></i>gx</span>
      <span><i style="background:var(--gy)"></i>gy</span>
      <span><i style="background:var(--gz)"></i>gz</span>
    </div>
  </div>

  <div class="panel"><div class="pwm" id="pwm"><span class="t">viteza motor (PWM antrenat)</span></div></div>
</div>

<script>
const CLASSES=["Repaus","Normal","Dezechilibru","Frecare","Slabit"];
const COLORS={Repaus:"#6b7d8f",Normal:"#36f9b3",Dezechilibru:"#ffb454",Frecare:"#ff7a59",Slabit:"#ff4d6d"};
const PWMS=[["Stop",0],["160",160],["180",180],["200",200],["220",220],["240",240],["255",255]];
const css=v=>getComputedStyle(document.documentElement).getPropertyValue(v).trim();
const cvA=document.getElementById("cvA"), cvG=document.getElementById("cvG");
const colA=[css("--ax"),css("--ay"),css("--az")], colG=[css("--gx"),css("--gy"),css("--gz")];

// bare de probabilitate
const barsEl=document.getElementById("bars");
CLASSES.forEach(c=>{
  const r=document.createElement("div");r.className="bar";
  r.innerHTML='<span class="nm">'+c+'</span><span class="track"><span class="fill" id="f_'+c+'" style="background:'+COLORS[c]+'"></span></span><span class="val" id="v_'+c+'">0%</span>';
  barsEl.appendChild(r);
});
// butoane PWM
const pwmEl=document.getElementById("pwm");
PWMS.forEach(([label,v])=>{
  const b=document.createElement("button");b.textContent=label;b.dataset.v=v;
  b.onclick=()=>fetch("/pwm?v="+v,{cache:"no-store"}).catch(()=>{});
  pwmEl.appendChild(b);
});

function grid(c){const x=c.getContext("2d"),w=c.width,h=c.height;
  x.clearRect(0,0,w,h);x.strokeStyle="#1a2430";x.lineWidth=1;
  for(let i=1;i<8;i++){let px=w*i/8;x.beginPath();x.moveTo(px,0);x.lineTo(px,h);x.stroke();}
  for(let i=1;i<4;i++){let py=h*i/4;x.beginPath();x.moveTo(0,py);x.lineTo(w,py);x.stroke();}
  x.strokeStyle="#243441";x.beginPath();x.moveTo(0,h/2);x.lineTo(w,h/2);x.stroke();}
function trace(c,arr,color,gain){const x=c.getContext("2d"),w=c.width,h=c.height,n=arr.length;
  if(!n)return;let m=0;for(const v of arr)m+=v;m/=n;       // medie scoasa DOAR la afisare
  x.strokeStyle=color;x.lineWidth=1.6;x.beginPath();
  for(let i=0;i<n;i++){const px=w*i/(n-1);let py=h/2-(arr[i]-m)*gain;
    py=Math.max(2,Math.min(h-2,py));i?x.lineTo(px,py):x.moveTo(px,py);}x.stroke();}

function setOffline(){document.getElementById("cls").parentElement.parentElement.classList.add("off");
  document.getElementById("hb").innerHTML='conexiune <b style="color:#ff4d6d">&middot;</b>';
  document.getElementById("meta").innerHTML="fara semnal &mdash; verifica alimentarea placii";}

async function tick(){
  try{
    const r=await fetch("/data",{cache:"no-store"});if(!r.ok)throw 0;const d=await r.json();
    document.querySelector(".badge").classList.remove("off");
    document.getElementById("hb").innerHTML='conexiune <b>&middot;</b> activa';
    const col=COLORS[d.cls]||"#cdd9e3";
    const clsEl=document.getElementById("cls");clsEl.textContent=d.cls;clsEl.style.color=col;
    document.getElementById("stripe").style.background=col;
    const lat=d.lat>0?(d.lat.toFixed(0)+" ms"):"filtru repaus";
    document.getElementById("meta").innerHTML="incredere "+d.conf.toFixed(1)+"%  &middot;  latenta "+lat+"  &middot;  #"+d.n;
    CLASSES.forEach((c,i)=>{const p=(d.p[i]*100);
      document.getElementById("f_"+c).style.width=Math.max(0,Math.min(100,p))+"%";
      document.getElementById("v_"+c).textContent=p.toFixed(0)+"%";});
    document.querySelectorAll(".pwm button").forEach(b=>b.classList.toggle("on",+b.dataset.v===d.pwm));
    grid(cvA);trace(cvA,d.ax,colA[0],10);trace(cvA,d.ay,colA[1],10);trace(cvA,d.az,colA[2],10);
    grid(cvG);trace(cvG,d.gx,colG[0],70);trace(cvG,d.gy,colG[1],70);trace(cvG,d.gz,colG[2],70);
  }catch(e){setOffline();}
}
grid(cvA);grid(cvG);setInterval(tick,200);tick();
</script>
</body></html>
)rawliteral";