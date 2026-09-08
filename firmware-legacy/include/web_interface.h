#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="ro">
<head>
  <meta charset="UTF-8">
  <title>Edge Maintenance Dashboard</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f0f2f5; margin: 0; text-align: center; }
    .header { background: #1a73e8; color: white; padding: 1rem; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
    .container { padding: 20px; display: flex; flex-direction: column; align-items: center; }
    .card { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); width: 90%; max-width: 400px; margin-bottom: 20px; }
    .status-box { font-size: 1.5rem; font-weight: bold; padding: 15px; border-radius: 8px; margin: 15px 0; border: 2px solid #ddd; transition: all 0.3s ease; }
    .normal { color: #2e7d32; border-color: #2e7d32; background: #e8f5e9; }
    .warning { color: #d32f2f; border-color: #d32f2f; background: #ffebee; }
    .repaus { color: #555; border-color: #555; background: #f5f5f5; }
    .info { font-size: 0.9rem; color: #666; margin-top: 5px; }
    canvas { background: #fafafa; border: 1px solid #eee; border-radius: 8px; margin-top: 15px; width: 100%; height: 120px; }
  </style>
  <script>
  /* Smoothie Charts (Syntax-Error Free Version) */
  function TimeSeries(a){this.options=a||{};this.clear()}TimeSeries.prototype.clear=function(){this.data=[];this.maxValue=NaN;this.minValue=NaN};TimeSeries.prototype.resetBounds=function(){if(this.data.length){this.maxValue=this.data[0][1];this.minValue=this.data[0][1];for(var a=1;a<this.data.length;a++){var b=this.data[a][1];b>this.maxValue&&(this.maxValue=b);b<this.minValue&&(this.minValue=b)}}else this.maxValue=NaN,this.minValue=NaN};TimeSeries.prototype.append=function(a,b,c){for(var d=this.data.length-1;0<=d&&this.data[d][0]>a;)d--;-1===d?this.data.splice(0,0,[a,b]):0<this.data.length&&this.data[d][0]===a?c?(this.data[d][1]+=b,b=this.data[d][1]):this.data[d][1]=b:d<this.data.length-1?this.data.splice(d+1,0,[a,b]):this.data.push([a,b]);this.maxValue=isNaN(this.maxValue)?b:Math.max(this.maxValue,b);this.minValue=isNaN(this.minValue)?b:Math.min(this.minValue,b)};TimeSeries.prototype.dropOldData=function(a,b){for(var c=0,d=this.data.length-1;0<=d;d--)if(this.data[d][0]<a||this.data.length-d>b)c++;else break;1<c&&this.data.splice(0,this.data.length-c)};function SmoothieChart(a){this.options=Object.assign({},SmoothieChart.defaultChartOptions,a);this.series=[];this.history=[];this.canvas=this.options.canvas;this.delay=0;this.frame=null;this.render=this.render.bind(this)}SmoothieChart.defaultChartOptions={millisPerPixel:20,enableDpiScaling:!0,yMinFormatter:function(a,b){return parseFloat(a).toFixed(b)},yMaxFormatter:function(a,b){return parseFloat(a).toFixed(b)},maxValueScale:1,minValueScale:1,interpolation:"bezier",scaleSmoothing:.125,maxDataSetLength:2,scrollBackwards:!1,grid:{fillStyle:"#000000",strokeStyle:"#777777",lineWidth:1,sharpLines:!1,millisPerLine:1E3,verticalSections:2,borderVisible:!0},labels:{fillStyle:"#ffffff",disabled:!1,fontSize:10,fontFamily:"monospace",precision:2},horizontalLines:[]};SmoothieChart.prototype.addTimeSeries=function(a,b){this.series.push({timeSeries:a,options:Object.assign({},SmoothieChart.defaultSeriesOptions,b)});a.options.resetBounds&&a.options.resetBoundsInterval&&(a.resetBoundsTimer=setInterval(function(){a.resetBounds()},a.options.resetBoundsInterval))};SmoothieChart.prototype.removeTimeSeries=function(a){for(var b=0;b<this.series.length;b++)if(this.series[b].timeSeries===a){this.series.splice(b,1);break}a.resetBoundsTimer&&clearInterval(a.resetBoundsTimer)};SmoothieChart.prototype.streamTo=function(a,b){this.canvas=a;this.delay=b;this.start()};SmoothieChart.prototype.resize=function(){var a=this.options.enableDpiScaling&&window?window.devicePixelRatio:1,b=this.canvas.clientWidth,c=this.canvas.clientHeight;if(this.canvas.width!==Math.floor(b*a)||this.canvas.height!==Math.floor(c*a))this.canvas.width=Math.floor(b*a),this.canvas.height=Math.floor(c*a),this.canvas.getContext("2d").scale(a,a)};SmoothieChart.prototype.start=function(){if(!this.frame){var a=this,b=function(){a.frame=requestAnimationFrame(function(){a.render();b()})};b()}};SmoothieChart.prototype.stop=function(){this.frame&&(cancelAnimationFrame(this.frame),this.frame=null)};SmoothieChart.prototype.updateValueRange=function(){for(var a=this.options,b=NaN,c=NaN,d=0;d<this.series.length;d++){var e=this.series[d].timeSeries;isNaN(e.maxValue)||(b=isNaN(b)?e.maxValue:Math.max(b,e.maxValue));isNaN(e.minValue)||(c=isNaN(c)?e.minValue:Math.min(c,e.minValue))}a.yRangeFunction&&(e=a.yRangeFunction({min:c,max:b}),c=e.min,b=e.max);if(!isNaN(b)&&!isNaN(c)){if(b!==c){b+=(b-c)*(a.maxValueScale-1);c-=(b-c)*(1-a.minValueScale)}else{b*=a.maxValueScale;c-=Math.abs(c*(a.minValueScale-1))}b===c&&(b+=.1);this.valueRange={min:c,max:b};this.valueRange.range=b-c}};SmoothieChart.prototype.render=function(a,b){var c=Date.now();if(this.options.scrollBackwards){this.history.push({time:c,valueRange:this.valueRange,series:this.series.map(function(a){return{timeSeries:a.timeSeries,options:a.options,data:a.timeSeries.data.slice()}})});for(;0<this.history.length&&this.history[0].time<c-this.options.millisPerPixel*this.canvas.width;)this.history.shift();var d=this.history.find(function(a){return a.time>=b})||this.history[this.history.length-1];this.valueRange=d.valueRange;this.series=d.series}b=b||c-(this.delay||0);b=16*Math.round(b/16);this.resize();this.updateValueRange();var e=this.options,f=this.canvas.width,g=this.canvas.height,h=b-f*e.millisPerPixel,k=this.valueRange,l=this.canvas.getContext("2d");l.font=e.labels.fontSize+"px "+e.labels.fontFamily;l.save();l.clearRect(0,0,f,g);l.fillStyle=e.grid.fillStyle;l.fillRect(0,0,f,g);l.save();l.strokeStyle=e.grid.strokeStyle;l.lineWidth=e.grid.lineWidth;e.grid.sharpLines&&l.translate(.5,.5);if(0<e.grid.millisPerLine)for(var m=Math.round(f/(e.grid.millisPerLine/e.millisPerPixel)),n=1;n<=m;n++){var p=Math.round(n*(e.grid.millisPerLine/e.millisPerPixel));l.beginPath();l.moveTo(p,0);l.lineTo(p,g);l.stroke();l.closePath()}for(n=1;n<=e.grid.verticalSections;n++)p=Math.round(n*g/e.grid.verticalSections),l.beginPath(),l.moveTo(0,p),l.lineTo(f,p),l.stroke(),l.closePath();e.grid.borderVisible&&(l.beginPath(),l.moveTo(0,0),l.lineTo(f,0),l.lineTo(f,g),l.lineTo(0,g),l.lineTo(0,0),l.stroke(),l.closePath());l.restore();if(e.horizontalLines&&e.horizontalLines.length)for(n=0;n<e.horizontalLines.length;n++)m=e.horizontalLines[n],p=Math.round((k.max-m.value)/k.range*g),l.strokeStyle=m.color||"#ffffff",l.lineWidth=m.lineWidth||1,l.beginPath(),l.moveTo(0,p),l.lineTo(f,p),l.stroke(),l.closePath();for(n=0;n<this.series.length;n++){l.save();var q=this.series[n].timeSeries;this.options.scrollBackwards&&(q.data=this.series[n].data);var r=q.data,t=this.series[n].options;q.dropOldData(h,e.maxDataSetLength);l.lineWidth=t.lineWidth;l.strokeStyle=t.strokeStyle;l.fillStyle=t.fillStyle;var u=0,v=0,w=0,x=0;l.beginPath();for(q=0;q<r.length;q++){var y=Math.round((r[q][0]-h)/e.millisPerPixel),z=Math.round(g-(r[q][1]-k.min)/k.range*g);if(0===q)u=y,v=z,l.moveTo(y,z);else switch(e.interpolation){case "linear":case "line":l.lineTo(y,z);break;case "bezier":default:l.bezierCurveTo(Math.round((w+y)/2),x,Math.round((w+y)/2),z,y,z);break;case "step":l.lineTo(y,x);l.lineTo(y,z)}w=y;x=z}if(1<r.length){if(t.fillStyle){var A=new Path2D(l);A.lineTo(f,v);A.lineTo(w,g);A.lineTo(u,g);A.closePath();l.fill(A)}l.stroke()}l.restore()}if(!e.labels.disabled&&!isNaN(k.min)&&!isNaN(k.max)){l.fillStyle=e.labels.fillStyle;l.font=e.labels.fontSize+"px "+e.labels.fontFamily;m=e.yMaxFormatter(k.max,e.labels.precision);q=e.yMinFormatter(k.min,e.labels.precision);l.fillText(m,f-l.measureText(m).width-2,e.labels.fontSize);l.fillText(q,f-l.measureText(q).width-2,g-2)}if(e.horizontalLines&&e.horizontalLines.length)for(n=0;n<e.horizontalLines.length;n++)m=e.horizontalLines[n],m.label&&(p=Math.round((k.max-m.value)/k.range*g),l.fillStyle=m.label.fillStyle||"#000000",l.font=(m.label.fontSize||12)+"px "+(m.label.fontFamily||"monospace"),l.fillText(m.label.text,2,p-("middle"===m.label.textBaseline?0:2)));l.restore()};SmoothieChart.defaultSeriesOptions={lineWidth:1,strokeStyle:"#ffffff",fillStyle:"rgba(0,0,0,0.0)"};
  </script>
</head>
<body>
  <div class="header">
    <h1>Smart Motor Monitor</h1>
    <p>TinyML Edge Inference</p>
  </div>
  <div class="container">
    <div class="card">
      <h3 style="margin-top: 0;">Stare Curentă</h3>
      <div id="status" class="status-box repaus">Așteptare date...</div>
      <div class="info">Încredere Model AI: <span id="conf-val">0</span>%</div>
      
      <canvas id="confidenceChart" width="350" height="120"></canvas>
    </div>
    
    <div class="card">
      <h3 style="margin-top: 0;">Vibrații Rotaționale (Giroscop)</h3>
      <canvas id="sensorChart" width="350" height="120"></canvas>
      <div class="info">Magnitudinea vectorului giroscopic (rad/s)</div>
    </div>

    <div class="card">
      <h3 style="margin-top: 0;">Vibrații Liniare (Accelerometru)</h3>
      <canvas id="accelChart" width="350" height="120"></canvas>
      <div class="info">Deviația de la accelerația de repaus (m/s²)</div>
    </div>

    <div class="card">
      <h3 style="margin-top: 0;">Parametri Sistem</h3>
      <p style="margin: 5px 0;">Control PWM: <span id="pwm" style="font-weight: bold;">--</span></p>
      <p style="margin: 5px 0;">Conexiune Date: <span id="conn-status" style="font-weight: bold; color: #ff9800;">Inițializare...</span></p>
      <p style="margin: 5px 0;">Uptime: <span id="uptime" style="font-weight: bold;">--</span> s</p>
    </div>

    <div class="card">
      <h3 style="margin-top: 0;">Control Motor (PWM)</h3>
      <input type="range" id="pwmSlider" min="0" max="255" value="0" style="width: 100%;" onchange="setPWM(this.value)">
      <p style="margin: 5px 0;">Comandă trimisă: <span id="pwmLabel">0</span></p>
    </div>
  </div>

<script>
  // --- Initializare Grafice Smoothie ---
  
  // Grafic Incredere
  var confChart = new SmoothieChart({
    millisPerPixel: 50,
    grid: { strokeStyle: '#e0e0e0', lineWidth: 1, millisPerLine: 1000, verticalSections: 4 },
    labels: { fillStyle: '#555', fontSize: 12, precision: 0 },
    maxValue: 105,
    minValue: -5
  });
  confChart.streamTo(document.getElementById('confidenceChart'), 500);
  var confSeries = new TimeSeries();
  confChart.addTimeSeries(confSeries, { strokeStyle: '#1a73e8', fillStyle: 'rgba(26, 115, 232, 0.1)', lineWidth: 2 });

  // Grafic Senzor
  var sensorChart = new SmoothieChart({
    millisPerPixel: 20,
    grid: { strokeStyle: '#e0e0e0', lineWidth: 1, millisPerLine: 1000, verticalSections: 5 },
    labels: { fillStyle: '#555', fontSize: 12, precision: 2 }
  });
  sensorChart.streamTo(document.getElementById('sensorChart'), 500);
  var sensorSeries = new TimeSeries();
  sensorChart.addTimeSeries(sensorSeries, { strokeStyle: '#e53935', lineWidth: 2 });

  // Grafic Accelerometru
  var accelChart = new SmoothieChart({
    millisPerPixel: 20,
    grid: { strokeStyle: '#e0e0e0', lineWidth: 1, millisPerLine: 1000, verticalSections: 5 },
    labels: { fillStyle: '#555', fontSize: 12, precision: 2 }
  });
  accelChart.streamTo(document.getElementById('accelChart'), 500);
  var accelSeries = new TimeSeries();
  accelChart.addTimeSeries(accelSeries, { strokeStyle: '#ff9800', lineWidth: 2 }); // Culoare portocalie

  // --- Conectare la ESP32 prin AJAX Polling (Bulletproof pentru Mobil) ---
  function fetchData() {
    try {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', '/data?t=' + new Date().getTime(), true);
      
      xhr.onreadystatechange = function() {
        if (xhr.readyState === 4) { 
          if (xhr.status === 200) {
            try {
              var obj = JSON.parse(xhr.responseText);
              document.getElementById('conn-status').innerHTML = "Conectat";
              document.getElementById('conn-status').style.color = "#2e7d32";
              
              var statusEl = document.getElementById("status");
              statusEl.innerHTML = obj.label;
              statusEl.className = "status-box " + 
                (obj.label === "Normal" ? "normal" : 
                ((obj.label === "Repaus" || obj.label === "Așteptare...") ? "repaus" : "warning"));
  
              document.getElementById("conf-val").innerHTML = obj.conf;
              document.getElementById("pwm").innerHTML = obj.pwm;
              document.getElementById("uptime").innerHTML = obj.uptime;
              
              var slider = document.getElementById('pwmSlider');
              if (document.activeElement !== slider) {
                slider.value = obj.pwm;
                document.getElementById('pwmLabel').innerText = obj.pwm;
              }
              
              var now = new Date().getTime();
              confSeries.append(now, parseFloat(obj.conf));
              sensorSeries.append(now, parseFloat(obj.g_mag));
              accelSeries.append(now, parseFloat(obj.a_lin_vib));
              
              setTimeout(fetchData, 150);
            } catch (e) {
              document.getElementById('conn-status').innerHTML = "Eroare Parse JSON";
              document.getElementById('conn-status').style.color = "#d32f2f";
              setTimeout(fetchData, 1000);
            }
          } else if (xhr.status !== 0) { // 0 este statusul temporar cand inca se incarca / anulat
            document.getElementById('conn-status').innerHTML = "Eroare HTTP: " + xhr.status;
            document.getElementById('conn-status').style.color = "#d32f2f";
            setTimeout(fetchData, 1000);
          }
        }
      };
      
      xhr.onerror = function() {
        document.getElementById('conn-status').innerHTML = "Rețea întreruptă (Network Error)";
        document.getElementById('conn-status').style.color = "#d32f2f";
        setTimeout(fetchData, 1000);
      };

      xhr.send();
    } catch (e) {
      // AICI ERA GAURA DE LOGICA: ascundeam eroarea in fundal fara s-o afisez pe ecran
      document.getElementById('conn-status').innerHTML = "Crash JS Intern: " + e.message;
      document.getElementById('conn-status').style.color = "#d32f2f";
      setTimeout(fetchData, 1000);
    }
  }
  
  function setPWM(val) {
    document.getElementById('pwmLabel').innerText = val;
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/set_pwm?v=' + val, true);
    xhr.send();
  }

  // Pornim prima bucla de cereri
  fetchData();
</script>
</body>
</html>
)rawliteral";

#endif