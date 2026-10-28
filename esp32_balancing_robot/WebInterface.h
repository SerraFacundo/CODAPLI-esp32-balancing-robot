/*
 * WebInterface.h
 * 
 * Responsive retro web interface with USB Gamepad support.
 * Mobile: touch joystick (single column)
 * Desktop: gamepad control + visual mapping panel (two columns)
 * 
 * Gamepad mapping (Standard Gamepad Layout):
 *   Left Stick Y  -> Throttle (forward/backward)
 *   Left Stick X  -> Steering (left/right)
 *   A / Button 0  -> Buzzer
 *   B / Button 1  -> LED toggle
 *   X / Button 2  -> Servo toggle
 *   Y / Button 3  -> Mode toggle (Normal/Pro)
 *   Select / Btn 8 -> Calibrate
 *   D-pad U/D/L/R -> Digital throttle/steering
 */

#ifndef WEBINTERFACE_H_
#define WEBINTERFACE_H_

const char WEB_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no,maximum-scale=1">
<title>Balancin TEC CODAPLY</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Press+Start+2P&family=VT323&display=swap');
*{margin:0;padding:0;box-sizing:border-box;-webkit-touch-callout:none;-webkit-user-select:none;user-select:none}

body{
  font-family:'VT323',monospace;
  background:linear-gradient(135deg,#c3a4f7 0%,#a8d8ea 25%,#f7cae0 50%,#b8e6ff 75%,#d4b8ff 100%);
  background-attachment:fixed;
  color:#333;
  overflow-x:hidden;
  min-height:100vh;
  padding:10px;
}

/* === LAYOUT === */
.page-wrap{max-width:960px;margin:0 auto}
.two-col{display:flex;gap:10px;align-items:flex-start}
.col-left{flex:1;min-width:0}
.col-right{width:380px;flex-shrink:0}

/* === RETRO WINDOW === */
.win{
  background:#f0f0f0;
  border:3px solid #888;
  border-top-color:#fff;border-left-color:#fff;
  border-radius:12px;
  margin:8px auto;
  max-width:420px;
  overflow:hidden;
  box-shadow:4px 4px 0px rgba(0,0,0,0.15),inset 0 0 0 1px rgba(255,255,255,0.4);
}
.win-title{
  background:linear-gradient(90deg,#b388ff,#80d8ff,#ff80ab);
  padding:6px 10px;display:flex;align-items:center;gap:6px;
  border-bottom:2px solid #aaa;border-radius:9px 9px 0 0;
}
.win-dots{display:flex;gap:5px}
.win-dot{width:12px;height:12px;border-radius:50%;border:1.5px solid rgba(0,0,0,0.2)}
.dot-r{background:#ff6b6b}.dot-y{background:#ffd93d}.dot-g{background:#6bcb77}
.win-label{font-family:'Press Start 2P',cursive;font-size:9px;color:#fff;text-shadow:1px 1px 2px rgba(0,0,0,0.3);flex:1;text-align:center;letter-spacing:1px}
.win-body{padding:10px}

/* === STATUS === */
.status-bar{display:flex;justify-content:space-between;align-items:center;padding:0 4px 6px;flex-wrap:wrap;gap:4px}
.badge{font-family:'Press Start 2P',cursive;font-size:8px;padding:5px 12px;border-radius:20px;border:2px solid;letter-spacing:0.5px}
.connected{background:#d4ffda;color:#1b8a2e;border-color:#6bcb77}
.disconnected{background:#ffd4d4;color:#c0392b;border-color:#ff6b6b}
.gp-badge{background:#e8eaf6;color:#283593;border-color:#7986cb}
.gp-badge.gp-on{background:#c8e6c9;color:#1b5e20;border-color:#66bb6a}

/* === TELEMETRY === */
.tel-grid{display:flex;justify-content:space-around;gap:6px}
.tel-card{flex:1;text-align:center;border:2px solid;border-radius:10px;padding:8px 4px}
.tel-card.a-c{background:linear-gradient(180deg,#e3f2fd,#bbdefb);border-color:#64b5f6}
.tel-card.s-c{background:linear-gradient(180deg,#fff3e0,#ffe0b2);border-color:#ffb74d}
.tel-card.m-c{background:linear-gradient(180deg,#fce4ec,#f8bbd0);border-color:#f06292}
.tel-lbl{font-family:'Press Start 2P',cursive;font-size:7px;color:#666;letter-spacing:1px;margin-bottom:4px}
.tel-val{font-family:'VT323',monospace;font-size:28px;font-weight:700;color:#333;line-height:1}
.tel-unit{font-size:14px;color:#888}
.motor-indicator{display:inline-block;width:10px;height:10px;border-radius:50%;margin-left:4px;vertical-align:middle}
.motor-on{background:#4caf50;box-shadow:0 0 8px #4caf50}.motor-off{background:#e57373}

/* === JOYSTICK === */
.joystick-area{display:flex;justify-content:center;padding:6px 0}
#joystickCanvas{background:linear-gradient(135deg,#e8f5e9,#f3e5f5);border-radius:50%;border:3px solid #b388ff;box-shadow:inset 0 0 20px rgba(179,136,255,0.15),3px 3px 0px rgba(0,0,0,0.1);touch-action:none}

/* === BUTTONS === */
.btn-row{display:flex;gap:8px;justify-content:center;flex-wrap:wrap;padding:4px 0}
.rbtn{font-family:'Press Start 2P',cursive;font-size:9px;padding:10px 14px;border-radius:10px;cursor:pointer;transition:all .1s;letter-spacing:0.5px;min-width:65px;text-align:center;border:2.5px solid;box-shadow:3px 3px 0px rgba(0,0,0,0.15);position:relative;top:0}
.rbtn:active{box-shadow:1px 1px 0px rgba(0,0,0,0.15);top:2px}
.btn-normal{background:#ffe66d;color:#6b5900;border-color:#d4a800}
.btn-normal.active{background:#ffd93d;border-color:#b8860b;box-shadow:3px 3px 0px rgba(0,0,0,0.2),inset 0 0 10px rgba(255,255,255,0.5)}
.btn-pro{background:#ff9ff3;color:#7a0052;border-color:#d63384}
.btn-pro.active{background:#ff6bcb;border-color:#c2185b;box-shadow:3px 3px 0px rgba(0,0,0,0.2),inset 0 0 10px rgba(255,255,255,0.5)}
.btn-servo{background:linear-gradient(180deg,#a8e6cf,#88d8b0);color:#1b5e20;border-color:#4caf50}
.btn-servo.on{background:linear-gradient(180deg,#69f0ae,#00e676);border-color:#2e7d32}
.btn-led{background:linear-gradient(180deg,#ffd3b6,#ffaaa5);color:#bf360c;border-color:#ff7043}
.btn-led.on{background:linear-gradient(180deg,#ff9100,#ff6d00);color:#fff;border-color:#e65100}
.btn-buzz{background:linear-gradient(180deg,#c5cae9,#9fa8da);color:#283593;border-color:#5c6bc0}
.btn-cal{background:linear-gradient(180deg,#80d8ff,#40c4ff);color:#01579b;border-color:#0288d1}
.toggle-row{display:flex;align-items:center;justify-content:center;gap:10px;padding:4px 0}
.toggle-label{font-family:'Press Start 2P',cursive;font-size:9px;color:#999}

/* === PID === */
.pid-row{display:flex;align-items:center;margin:10px 0;gap:6px}
.pid-label{font-family:'Press Start 2P',cursive;font-size:8px;width:38px;text-align:right;color:#7c4dff;flex-shrink:0}
.pid-row input[type=range]{flex:1;-webkit-appearance:none;appearance:none;height:14px;background:linear-gradient(90deg,#80d8ff,#b388ff,#ff80ab);border-radius:7px;outline:none;border:2px solid #b388ff;touch-action:manipulation;margin:0;padding:0}
.pid-row input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:28px;height:28px;background:linear-gradient(135deg,#ffe66d,#ffd93d);border:2.5px solid #d4a800;border-radius:50%;cursor:pointer;box-shadow:2px 2px 0px rgba(0,0,0,0.15);margin-top:-7px}
.pid-row input[type=range]::-webkit-slider-runnable-track{height:14px;background:linear-gradient(90deg,#80d8ff,#b388ff,#ff80ab);border-radius:7px}
.pid-val{font-family:'VT323',monospace;font-size:18px;width:52px;text-align:left;color:#7c4dff;flex-shrink:0}

/* === GAMEPAD PANEL === */
.gp-panel{display:none}
#gpCanvas{width:100%;border-radius:8px;background:linear-gradient(135deg,#e8eaf6,#f3e5f5);border:2px solid #b388ff}
.gp-info{font-family:'VT323',monospace;font-size:16px;color:#555;text-align:center;padding:6px 0}
.gp-map-grid{display:grid;grid-template-columns:1fr 1fr;gap:4px 12px;padding:4px 0}
.gp-map-item{font-family:'VT323',monospace;font-size:15px;color:#555;display:flex;justify-content:space-between}
.gp-map-item .gk{color:#7c4dff;font-weight:700}
.gp-map-item .gv{color:#888}

/* === FOOTER === */
.footer{text-align:center;padding:12px;padding-bottom:40px;font-family:'Press Start 2P',cursive;font-size:7px;color:#b388ff;letter-spacing:1px}

/* === RESPONSIVE === */
@media(max-width:767px){
  .two-col{flex-direction:column}
  .col-left,.col-right{width:100%}
  .col-left .win,.col-right .win{max-width:420px;margin:8px auto}
  .gp-panel{display:none !important}
}
@media(min-width:768px){
  .gp-panel{display:block}
  .touch-only{display:none}
  .two-col .win{max-width:none}
}
</style>
</head>
<body>
<div class="page-wrap">

<!-- HEADER -->
<div class="win">
  <div class="win-title">
    <div class="win-dots"><div class="win-dot dot-r"></div><div class="win-dot dot-y"></div><div class="win-dot dot-g"></div></div>
    <span class="win-label">BALANCIN TEC CODAPLY</span>
  </div>
  <div class="win-body">
    <div class="status-bar">
      <span style="font-family:'Press Start 2P',cursive;font-size:8px;color:#888">STATUS:</span>
      <span id="wsStatus" class="badge disconnected">OFFLINE</span>
      <span id="gpStatus" class="badge gp-badge">GAMEPAD: --</span>
    </div>
    <div class="tel-grid">
      <div class="tel-card a-c"><div class="tel-lbl">ANGLE</div><div><span class="tel-val" id="telAngle">0.0</span><span class="tel-unit">&deg;</span></div></div>
      <div class="tel-card s-c"><div class="tel-lbl">SPEED</div><div><span class="tel-val" id="telSpeed">0</span></div></div>
      <div class="tel-card m-c"><div class="tel-lbl">MOTORS</div><div><span class="tel-val" id="telMotors">OFF</span><span class="motor-indicator motor-off" id="motorDot"></span></div></div>
    </div>
  </div>
</div>

<div class="two-col">
<div class="col-left">

<!-- JOYSTICK (touch, visible on mobile) -->
<div class="win touch-only">
  <div class="win-title"><div class="win-dots"><div class="win-dot dot-r"></div><div class="win-dot dot-y"></div><div class="win-dot dot-g"></div></div><span class="win-label">JOYSTICK</span></div>
  <div class="win-body">
    <div class="joystick-area"><canvas id="joystickCanvas" width="220" height="220"></canvas></div>
  </div>
</div>

<!-- MODE + CONTROLS -->
<div class="win">
  <div class="win-title"><div class="win-dots"><div class="win-dot dot-r"></div><div class="win-dot dot-y"></div><div class="win-dot dot-g"></div></div><span class="win-label">CONTROLS</span></div>
  <div class="win-body">
    <div class="toggle-row">
      <span class="toggle-label" id="lblNormal" style="color:#333">NORMAL</span>
      <button class="rbtn btn-normal active" id="btnNormal" onclick="setMode(0)" style="font-size:8px;padding:6px 10px;min-width:auto">&#9664;</button>
      <button class="rbtn btn-pro" id="btnPro" onclick="setMode(1)" style="font-size:8px;padding:6px 10px;min-width:auto">&#9654;</button>
      <span class="toggle-label" id="lblPro">PRO</span>
    </div>
    <div class="btn-row" style="margin-top:8px">
      <button class="rbtn btn-servo" id="btnServo" onclick="toggleServo()">SERVO</button>
      <button class="rbtn btn-led" id="btnLed" onclick="toggleLed()">LED</button>
      <button class="rbtn btn-buzz" onclick="buzzer()">BUZZ</button>
      <button class="rbtn" onclick="morseUTN()" style="background:linear-gradient(180deg,#fff9c4,#fff176);color:#f57f17;border-color:#fbc02d;font-size:8px">UTN</button>
      <button class="rbtn btn-cal" onclick="calibrate()">CALIB</button>
    </div>
  </div>
</div>

<!-- PID -->
<div class="win">
  <div class="win-title"><div class="win-dots"><div class="win-dot dot-r"></div><div class="win-dot dot-y"></div><div class="win-dot dot-g"></div></div><span class="win-label">PID TUNING</span></div>
  <div class="win-body">
    <div class="pid-row"><span class="pid-label">Kp</span><input type="range" id="slKp" min="0" max="100" value="50" oninput="updatePID()"><span class="pid-val" id="valKp">0.320</span></div>
    <div class="pid-row"><span class="pid-label">Kd</span><input type="range" id="slKd" min="0" max="100" value="50" oninput="updatePID()"><span class="pid-val" id="valKd">0.050</span></div>
    <div class="pid-row"><span class="pid-label">Kp_t</span><input type="range" id="slKpt" min="0" max="100" value="50" oninput="updatePID()"><span class="pid-val" id="valKpt">0.080</span></div>
    <div class="pid-row"><span class="pid-label">Ki_t</span><input type="range" id="slKit" min="0" max="100" value="50" oninput="updatePID()"><span class="pid-val" id="valKit">0.100</span></div>
    <div style="text-align:center;margin-top:8px"><button class="rbtn" onclick="resetPID()" style="background:linear-gradient(180deg,#e1bee7,#ce93d8);color:#4a148c;border-color:#ab47bc;font-size:8px;padding:8px 18px">RESET PID</button></div>
  </div>
</div>

</div><!-- col-left -->

<!-- RIGHT COLUMN: GAMEPAD (desktop only) -->
<div class="col-right">
<div class="win gp-panel" id="gpPanel">
  <div class="win-title"><div class="win-dots"><div class="win-dot dot-r"></div><div class="win-dot dot-y"></div><div class="win-dot dot-g"></div></div><span class="win-label">GAMEPAD</span></div>
  <div class="win-body">
    <div class="gp-info" id="gpName">Connect a USB gamepad...</div>
    <canvas id="gpCanvas" width="360" height="240"></canvas>
    <div style="margin-top:8px">
      <div class="gp-info" style="font-size:13px;color:#7c4dff;font-family:'Press Start 2P',cursive;font-size:8px;margin-bottom:6px">BUTTON MAP</div>
      <div class="gp-map-grid">
        <div class="gp-map-item"><span class="gk">L-Stick Y</span><span class="gv">Throttle</span></div>
        <div class="gp-map-item"><span class="gk">L-Stick X</span><span class="gv">Steering</span></div>
        <div class="gp-map-item"><span class="gk">D-Pad</span><span class="gv">Digital Move</span></div>
        <div class="gp-map-item"><span class="gk">A (Btn 0)</span><span class="gv">Buzzer</span></div>
        <div class="gp-map-item"><span class="gk">B (Btn 1)</span><span class="gv">LED</span></div>
        <div class="gp-map-item"><span class="gk">X (Btn 2)</span><span class="gv">Servo</span></div>
        <div class="gp-map-item"><span class="gk">Y (Btn 3)</span><span class="gv">Mode</span></div>
        <div class="gp-map-item"><span class="gk">Select (8)</span><span class="gv">Calibrate</span></div>
      </div>
    </div>
  </div>
</div>
</div><!-- col-right -->

</div><!-- two-col -->
<div class="footer">&#x2605; BALANCIN TEC CODAPLY &#x2605;</div>
</div><!-- page-wrap -->

<script>
// ============================================================================
// WebSocket
// ============================================================================
// Autenticación: el robot solo acepta comandos después de recibir "T:<token>".
// El token se pide antes de abrir la conexión (el robot cierra a los 5 s a quien
// no se autentica) y se recuerda en este navegador.
let ws=null,wsConnected=false,wsToken=null;
const TOKEN_KEY='bbToken';
function loadToken(){try{return localStorage.getItem(TOKEN_KEY)}catch(e){return null}}
function saveToken(t){try{localStorage.setItem(TOKEN_KEY,t)}catch(e){}}
function forgetToken(){wsToken=null;try{localStorage.removeItem(TOKEN_KEY)}catch(e){}}
function setWsStatus(txt,cls){const el=document.getElementById('wsStatus');el.textContent=txt;el.className='badge '+cls}
function connectWS(){
  if(!wsToken)wsToken=loadToken();
  if(!wsToken){
    const t=prompt('Token del robot:');
    if(!t){setWsStatus('SIN TOKEN','disconnected');return}  // click en el estado para reintentar
    wsToken=t.trim();
  }
  const host=window.location.hostname||'192.168.4.1';
  setWsStatus('CONECTANDO','disconnected');
  ws=new WebSocket('ws://'+host+'/ws');
  ws.onopen=function(){ws.send('T:'+wsToken)};
  ws.onclose=function(evt){
    wsConnected=false;
    if(evt.code===4001){forgetToken();setWsStatus('TOKEN INVALIDO','disconnected');setTimeout(connectWS,2500);return}
    setWsStatus('OFFLINE','disconnected');setTimeout(connectWS,1500)
  };
  ws.onerror=function(){ws.close()};
  ws.onmessage=function(evt){
    const d=evt.data;
    if(d==='T:OK'){wsConnected=true;saveToken(wsToken);setWsStatus('ONLINE','connected');return}
    if(d==='T:ERR'){forgetToken();return}
    if(d.startsWith('A:')){
      const p=d.substring(2).split(':');
      if(p.length>=3){
        document.getElementById('telAngle').textContent=parseFloat(p[0]).toFixed(1);
        document.getElementById('telSpeed').textContent=parseInt(p[1]);
        const m=p[2]==='1';
        document.getElementById('telMotors').textContent=m?'ON':'OFF';
        document.getElementById('motorDot').className=m?'motor-indicator motor-on':'motor-indicator motor-off';
      }
    }
  };
}
function send(msg){if(ws&&wsConnected)ws.send(msg)}

// ============================================================================
// Touch Joystick
// ============================================================================
const canvas=document.getElementById('joystickCanvas');
const ctx=canvas.getContext('2d');
const cX=canvas.width/2,cY=canvas.height/2,mR=80,kR=24;
let kX=cX,kY=cY,isDrag=false,jInt=null;

function drawJoystick(){
  ctx.clearRect(0,0,canvas.width,canvas.height);
  ctx.beginPath();ctx.arc(cX,cY,mR+10,0,Math.PI*2);ctx.strokeStyle='#b388ff';ctx.lineWidth=2;ctx.stroke();
  ctx.beginPath();ctx.setLineDash([4,4]);
  ctx.moveTo(cX,cY-mR-5);ctx.lineTo(cX,cY+mR+5);ctx.moveTo(cX-mR-5,cY);ctx.lineTo(cX+mR+5,cY);
  ctx.strokeStyle='#ce93d8';ctx.lineWidth=1;ctx.stroke();ctx.setLineDash([]);
  for(let r=mR/3;r<mR;r+=mR/3){ctx.beginPath();ctx.arc(cX,cY,r,0,Math.PI*2);ctx.strokeStyle='rgba(179,136,255,0.2)';ctx.lineWidth=1;ctx.stroke()}
  ctx.beginPath();ctx.arc(kX+2,kY+2,kR,0,Math.PI*2);ctx.fillStyle='rgba(0,0,0,0.12)';ctx.fill();
  ctx.beginPath();ctx.arc(kX,kY,kR,0,Math.PI*2);
  const g=ctx.createRadialGradient(kX-4,kY-4,2,kX,kY,kR);g.addColorStop(0,'#ffe66d');g.addColorStop(.7,'#ffd93d');g.addColorStop(1,'#f0c000');
  ctx.fillStyle=g;ctx.fill();ctx.strokeStyle='#d4a800';ctx.lineWidth=2.5;ctx.stroke();
  ctx.beginPath();ctx.arc(kX-5,kY-5,8,0,Math.PI*2);ctx.fillStyle='rgba(255,255,255,0.45)';ctx.fill();
}
function jMove(cx,cy){const r=canvas.getBoundingClientRect();let x=cx-r.left-cX,y=cy-r.top-cY;const d=Math.sqrt(x*x+y*y);if(d>mR){x=(x/d)*mR;y=(y/d)*mR}kX=cX+x;kY=cY+y;drawJoystick()}
function jRelease(){isDrag=false;kX=cX;kY=cY;drawJoystick();send('J:0.00:0.00');if(jInt){clearInterval(jInt);jInt=null}}
function jSend(){send('J:'+(-(kY-cY)/mR).toFixed(2)+':'+((kX-cX)/mR).toFixed(2))}
canvas.addEventListener('touchstart',function(e){e.preventDefault();isDrag=true;jMove(e.touches[0].clientX,e.touches[0].clientY);if(jInt)clearInterval(jInt);jInt=setInterval(jSend,50)},{passive:false});
canvas.addEventListener('touchmove',function(e){e.preventDefault();if(isDrag)jMove(e.touches[0].clientX,e.touches[0].clientY)},{passive:false});
canvas.addEventListener('touchend',function(e){e.preventDefault();jRelease()},{passive:false});
canvas.addEventListener('touchcancel',function(){jRelease()},{passive:false});
canvas.addEventListener('mousedown',function(e){isDrag=true;jMove(e.clientX,e.clientY);if(jInt)clearInterval(jInt);jInt=setInterval(jSend,50)});
document.addEventListener('mousemove',function(e){if(isDrag)jMove(e.clientX,e.clientY)});
document.addEventListener('mouseup',function(){if(isDrag)jRelease()});

// ============================================================================
// Mode / Buttons
// ============================================================================
let servoOn=false,ledOn=false,curMode=0;
function setMode(m){curMode=m;send('M:'+m);document.getElementById('btnNormal').className='rbtn btn-normal'+(m===0?' active':'');document.getElementById('btnPro').className='rbtn btn-pro'+(m===1?' active':'');document.getElementById('lblNormal').style.color=m===0?'#333':'#999';document.getElementById('lblPro').style.color=m===1?'#333':'#999'}
function toggleServo(){servoOn=!servoOn;send('S:'+(servoOn?'1':'0'));document.getElementById('btnServo').className='rbtn btn-servo'+(servoOn?' on':'')}
function toggleLed(){ledOn=!ledOn;send('L:'+(ledOn?'1':'0'));document.getElementById('btnLed').className='rbtn btn-led'+(ledOn?' on':'')}
function buzzer(){send('B')}
function morseUTN(){send('W')}
function calibrate(){if(confirm('Place the robot upright and still, then press OK to calibrate.'))send('C')}

// ============================================================================
// PID
// ============================================================================
const PD={kp:.32,kd:.05,kpt:.08,kit:.1};let pidT=null;
function s2v(s,d){return(s/50)*d}
function updatePID(){
  const kp=s2v(document.getElementById('slKp').value,PD.kp),kd=s2v(document.getElementById('slKd').value,PD.kd),kpt=s2v(document.getElementById('slKpt').value,PD.kpt),kit=s2v(document.getElementById('slKit').value,PD.kit);
  document.getElementById('valKp').textContent=kp.toFixed(3);document.getElementById('valKd').textContent=kd.toFixed(3);
  document.getElementById('valKpt').textContent=kpt.toFixed(3);document.getElementById('valKit').textContent=kit.toFixed(3);
  if(pidT)clearTimeout(pidT);pidT=setTimeout(function(){send('P:'+kp.toFixed(4)+':'+kd.toFixed(4)+':'+kpt.toFixed(4)+':'+kit.toFixed(4))},100);
}
function resetPID(){document.getElementById('slKp').value=50;document.getElementById('slKd').value=50;document.getElementById('slKpt').value=50;document.getElementById('slKit').value=50;updatePID()}

// ============================================================================
// GAMEPAD API
// ============================================================================
let gpConnected=false, gpIndex=-1, gpId='';
// Debounce flags for button toggles (prevent rapid toggling)
let btnCooldown=[false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false];
const COOLDOWN_MS=300;

function setBtnCooldown(i){btnCooldown[i]=true;setTimeout(function(){btnCooldown[i]=false},COOLDOWN_MS)}

// Gamepad connected
window.addEventListener('gamepadconnected',function(e){
  gpConnected=true;gpIndex=e.gamepad.index;gpId=e.gamepad.id;
  document.getElementById('gpStatus').textContent='GAMEPAD: ON';
  document.getElementById('gpStatus').className='badge gp-badge gp-on';
  document.getElementById('gpName').textContent=gpId.substring(0,40);
  console.log('Gamepad connected:',gpId);
});
window.addEventListener('gamepaddisconnected',function(e){
  gpConnected=false;gpIndex=-1;gpId='';
  document.getElementById('gpStatus').textContent='GAMEPAD: --';
  document.getElementById('gpStatus').className='badge gp-badge';
  document.getElementById('gpName').textContent='Connect a USB gamepad...';
});

// Gamepad poll loop (~30Hz, matches joystick send rate)
let gpThrottle=0,gpSteer=0;

function pollGamepad(){
  if(!gpConnected)return;
  let gps;
  try{gps=navigator.getGamepads()}catch(e){return}
  if(!gps||!gps[gpIndex])return;
  const gp=gps[gpIndex];

  // === AXES: Left stick ===
  let lx=gp.axes[0]||0, ly=gp.axes[1]||0;
  // Deadzone
  if(Math.abs(lx)<0.08)lx=0;
  if(Math.abs(ly)<0.08)ly=0;

  // === D-PAD: buttons 12-15 (digital override) ===
  const dU=gp.buttons[12]&&gp.buttons[12].pressed;
  const dD=gp.buttons[13]&&gp.buttons[13].pressed;
  const dL=gp.buttons[14]&&gp.buttons[14].pressed;
  const dR=gp.buttons[15]&&gp.buttons[15].pressed;
  if(dU)ly=-0.5; if(dD)ly=0.5;
  if(dL)lx=-0.5; if(dR)lx=0.5;

  gpThrottle=-ly;  // Invert Y: stick up = forward
  gpSteer=lx;
  send('J:'+gpThrottle.toFixed(2)+':'+gpSteer.toFixed(2));

  // === BUTTONS (with cooldown to prevent rapid toggling) ===
  // Btn 0 (A) = Buzzer (no toggle, fire once)
  if(gp.buttons[0]&&gp.buttons[0].pressed&&!btnCooldown[0]){buzzer();setBtnCooldown(0)}
  // Btn 1 (B) = LED toggle
  if(gp.buttons[1]&&gp.buttons[1].pressed&&!btnCooldown[1]){toggleLed();setBtnCooldown(1)}
  // Btn 2 (X) = Servo toggle
  if(gp.buttons[2]&&gp.buttons[2].pressed&&!btnCooldown[2]){toggleServo();setBtnCooldown(2)}
  // Btn 3 (Y) = Mode toggle
  if(gp.buttons[3]&&gp.buttons[3].pressed&&!btnCooldown[3]){setMode(curMode===0?1:0);setBtnCooldown(3)}
  // Btn 8 (Select) = Calibrate
  if(gp.buttons[8]&&gp.buttons[8].pressed&&!btnCooldown[8]){calibrate();setBtnCooldown(8)}

  // Draw visual
  drawGamepadVisual(gp);
}

// ============================================================================
// GAMEPAD VISUAL DRAWING
// ============================================================================
const gpC=document.getElementById('gpCanvas');
const gpCtx=gpC?gpC.getContext('2d'):null;

function drawGamepadVisual(gp){
  if(!gpCtx)return;
  const W=gpC.width,H=gpC.height;
  gpCtx.clearRect(0,0,W,H);

  // Body outline
  gpCtx.fillStyle='#e0d4f7';
  gpCtx.strokeStyle='#b388ff';
  gpCtx.lineWidth=3;
  gpCtx.beginPath();
  gpCtx.roundRect(40,30,W-80,H-60,30);
  gpCtx.fill();gpCtx.stroke();

  // Grips
  gpCtx.fillStyle='#d1c4e9';
  gpCtx.beginPath();gpCtx.roundRect(20,80,50,120,20);gpCtx.fill();gpCtx.stroke();
  gpCtx.beginPath();gpCtx.roundRect(W-70,80,50,120,20);gpCtx.fill();gpCtx.stroke();

  // Left stick base
  const lsX=110,lsY=140;
  gpCtx.beginPath();gpCtx.arc(lsX,lsY,30,0,Math.PI*2);gpCtx.fillStyle='#c5cae9';gpCtx.fill();gpCtx.strokeStyle='#9fa8da';gpCtx.lineWidth=2;gpCtx.stroke();
  // Left stick position
  const lx=gp.axes[0]||0, ly=gp.axes[1]||0;
  const sX=lsX+lx*22, sY=lsY+ly*22;
  gpCtx.beginPath();gpCtx.arc(sX,sY,14,0,Math.PI*2);
  gpCtx.fillStyle=(Math.abs(lx)>0.08||Math.abs(ly)>0.08)?'#7c4dff':'#b388ff';
  gpCtx.fill();gpCtx.strokeStyle='#5e35b1';gpCtx.lineWidth=2;gpCtx.stroke();

  // Right stick base
  const rsX=W-110,rsY=140;
  gpCtx.beginPath();gpCtx.arc(rsX,rsY,30,0,Math.PI*2);gpCtx.fillStyle='#c5cae9';gpCtx.fill();gpCtx.strokeStyle='#9fa8da';gpCtx.lineWidth=2;gpCtx.stroke();
  const rx=gp.axes[2]||0, ry=gp.axes[3]||0;
  const srX=rsX+rx*22, srY=rsY+ry*22;
  gpCtx.beginPath();gpCtx.arc(srX,srY,14,0,Math.PI*2);
  gpCtx.fillStyle=(Math.abs(rx)>0.08||Math.abs(ry)>0.08)?'#7c4dff':'#b388ff';
  gpCtx.fill();gpCtx.strokeStyle='#5e35b1';gpCtx.lineWidth=2;gpCtx.stroke();

  // Face buttons (A B X Y) — right side
  const fbX=W-110,fbY=75;
  const fbPos=[[0,18],[18,0],[-18,0],[0,-18]]; // A B X Y positions
  const fbCol=['#66bb6a','#ef5350','#42a5f5','#ffd93d'];
  const fbLbl=['A','B','X','Y'];
  for(let i=0;i<4;i++){
    const px=fbX+fbPos[i][0],py=fbY+fbPos[i][1];
    const pressed=gp.buttons[i]&&gp.buttons[i].pressed;
    gpCtx.beginPath();gpCtx.arc(px,py,12,0,Math.PI*2);
    gpCtx.fillStyle=pressed?fbCol[i]:'#e8e8e8';gpCtx.fill();
    gpCtx.strokeStyle=pressed?'#333':'#999';gpCtx.lineWidth=2;gpCtx.stroke();
    gpCtx.fillStyle=pressed?'#fff':'#666';gpCtx.font='bold 10px "Press Start 2P"';gpCtx.textAlign='center';gpCtx.textBaseline='middle';
    gpCtx.fillText(fbLbl[i],px,py+1);
  }

  // D-pad — left side
  const dpX=110,dpY=75;
  const dpDirs=[[0,-16],[0,16],[-16,0],[16,0]]; // U D L R
  const dpBtns=[12,13,14,15];
  const dpLbl=['\u25B2','\u25BC','\u25C0','\u25B6'];
  for(let i=0;i<4;i++){
    const px=dpX+dpDirs[i][0],py=dpY+dpDirs[i][1];
    const pressed=gp.buttons[dpBtns[i]]&&gp.buttons[dpBtns[i]].pressed;
    gpCtx.fillStyle=pressed?'#7c4dff':'#d0d0d0';
    gpCtx.fillRect(px-8,py-8,16,16);
    gpCtx.strokeStyle='#999';gpCtx.lineWidth=1;gpCtx.strokeRect(px-8,py-8,16,16);
    gpCtx.fillStyle=pressed?'#fff':'#888';gpCtx.font='10px sans-serif';gpCtx.textAlign='center';gpCtx.textBaseline='middle';
    gpCtx.fillText(dpLbl[i],px,py);
  }
  // D-pad center
  gpCtx.fillStyle='#c0c0c0';gpCtx.fillRect(dpX-6,dpY-6,12,12);

  // Bumpers LB RB
  const lbP=gp.buttons[4]&&gp.buttons[4].pressed;
  const rbP=gp.buttons[5]&&gp.buttons[5].pressed;
  gpCtx.fillStyle=lbP?'#7c4dff':'#d0d0d0';
  gpCtx.beginPath();gpCtx.roundRect(70,32,60,16,6);gpCtx.fill();gpCtx.stroke();
  gpCtx.fillStyle=lbP?'#fff':'#888';gpCtx.font='9px "Press Start 2P"';gpCtx.textAlign='center';gpCtx.fillText('LB',100,43);

  gpCtx.fillStyle=rbP?'#7c4dff':'#d0d0d0';
  gpCtx.beginPath();gpCtx.roundRect(W-130,32,60,16,6);gpCtx.fill();gpCtx.stroke();
  gpCtx.fillStyle=rbP?'#fff':'#888';gpCtx.fillText('RB',W-100,43);

  // Select / Start
  const selP=gp.buttons[8]&&gp.buttons[8].pressed;
  const staP=gp.buttons[9]&&gp.buttons[9].pressed;
  gpCtx.fillStyle=selP?'#ff80ab':'#d0d0d0';
  gpCtx.beginPath();gpCtx.roundRect(W/2-45,70,35,14,5);gpCtx.fill();gpCtx.stroke();
  gpCtx.fillStyle=selP?'#fff':'#888';gpCtx.font='7px "Press Start 2P"';gpCtx.textAlign='center';gpCtx.fillText('SEL',W/2-27,80);

  gpCtx.fillStyle=staP?'#ff80ab':'#d0d0d0';
  gpCtx.beginPath();gpCtx.roundRect(W/2+10,70,35,14,5);gpCtx.fill();gpCtx.stroke();
  gpCtx.fillStyle=staP?'#fff':'#888';gpCtx.fillText('STA',W/2+28,80);

  // Axes readout text
  gpCtx.fillStyle='#7c4dff';gpCtx.font='14px VT323';gpCtx.textAlign='center';
  gpCtx.fillText('THR: '+gpThrottle.toFixed(2)+'  STR: '+gpSteer.toFixed(2),W/2,H-16);
}

// ============================================================================
// Main loop: poll gamepad at 30Hz
// ============================================================================
let gpPollInterval=setInterval(pollGamepad,33);

// If no gamepad, send idle on gamepad poll (only when no touch joystick active)
// The touch joystick handles its own sending

// ============================================================================
// Init
// ============================================================================
drawJoystick();
updatePID();
document.getElementById('wsStatus').style.cursor='pointer';
document.getElementById('wsStatus').onclick=function(){if(this.textContent==='SIN TOKEN')connectWS()};
connectWS();

// Draw empty gamepad visual
if(gpCtx){
  gpCtx.fillStyle='#ccc';gpCtx.font='16px VT323';gpCtx.textAlign='center';
  gpCtx.fillText('Press any button on your gamepad',gpC.width/2,gpC.height/2);
}
</script>
</body>
</html>
)rawliteral";

#endif /* WEBINTERFACE_H_ */