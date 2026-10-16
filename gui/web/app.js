/* FIELD workspace: presentation, local preferences and device workflows. */
(() => {
'use strict';
const $=id=>document.getElementById(id), axes=['X','Y','Z'], colors=['#507fe0','#20a28d','#dba253'];
const C=FieldCore, storageKey='field-workspace-v2';
let config={flags:59,maps:[.1,.1,-.1]}, settings={port:'',baud:'921600'}, autoLoad=true, showArrows=true, storageAvailable=true;
try{const saved=JSON.parse(localStorage.getItem(storageKey)||'null');if(saved){showArrows=saved.showArrows!==false;autoLoad=saved.autoLoad!==false;if(saved.settings&&['9600','57600','115200','230400','460800','921600'].includes(saved.settings.baud))settings={port:String(saved.settings.port||''),baud:saved.baudDefaultVersion!==1&&saved.settings.baud==='115200'?'921600':saved.settings.baud};}}catch{storageAvailable=false;}
const api=new LocalSerialApi(), device=new C.SerialClient(api), samples=new C.SampleBuffer(12000),serialLog=new C.SerialLog();
let mode='device',running=false,acquisition='waiting',busy=false,configVerified=false,logs=[],tick=0,epoch=performance.now(),wallEpoch=Date.now(),dirty=true;
let yaw=-.64,pitch=.65,zoom=1,topView=false,drag=null,toastTimer,portTimer,scanning=false,magnitudes=[],lastSampleAt=0,lastWarningAt=0,calibTimer=null;
let importPanel=null,deviceLogLevel=null;
let rateMarks=[], hover=[null,null,null],lastStatus=0,calibrationActive=false;
const fmt=x=>(x>=0?'+':'')+x.toFixed(3), icon=name=>`<svg class="icon"><use href="#i-${name}"/></svg>`;
function toast(message){$('toast').textContent=message;$('toast').classList.add('show');clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('toast').classList.remove('show'),4500);}
function persist(){try{localStorage.setItem(storageKey,JSON.stringify({settings,autoLoad,showArrows,baudDefaultVersion:1}));return true;}catch{toast('浏览器无法保存本地设置，本次会话内仍然有效。');return false;}}
function log(message,type='系统',record=true){if(record&&(serialLog.active||(mode==='device'&&type==='错误')))serialLog.append(type,message);const time=new Date().toLocaleTimeString('zh-CN',{hour12:false});logs.unshift({time,type,message});logs=logs.slice(0,150);$('eventTime').textContent=time;$('eventText').textContent=message;if($('logDialog').open)renderLogs();}
function renderLogs(){$('recordCount').textContent=samples.length.toLocaleString();$('logList').replaceChildren(...logs.map(l=>{const row=document.createElement('div');row.className='log-row';for(const[tag,cls,value]of[['time','mono',l.time],['span','log-type',l.type],['span','log-description',l.message]]){const el=document.createElement(tag);el.className=cls;el.textContent=value;row.append(el);}return row;}));}
function currentUnit(){return 'N';}
function unitCode(){return 'N';}
function demoValues(t){const raw=[7.3*Math.sin(t*.52)+1.7*Math.sin(t*1.54)+.25*Math.sin(t*16),5.2*Math.sin(t*.52+1.35)+1.3*Math.cos(t*1.3)+.18*Math.sin(t*14),-3-2.7*Math.sin(t*.52+2.5)-.65*Math.cos(t*1.8)-.12*Math.sin(t*18)];return raw.map((v,i)=>config.flags&16?v*config.maps[i]:v);}
function addSample(v,t,time){samples.push({v:[...v],t,time,source:mode,unit:unitCode()});magnitudes.push(Math.hypot(...v));if(magnitudes.length>100)magnitudes.shift();dirty=true;}
function resetSession(next,seed=false){mode=next;samples.clear();magnitudes=[];rateMarks=[];tick=seed?600:0;epoch=performance.now()-(seed?30000:0);wallEpoch=Date.now()-(seed?30000:0);lastSampleAt=0;configVerified=false;acquisition='waiting';running=next==='demo';if(seed)for(let i=1;i<=600;i++)addSample(demoValues(i/20),i/20,wallEpoch+i*50);hover=[null,null,null];document.querySelectorAll('.chart-tooltip').forEach(t=>t.style.display='none');dirty=true;syncStatus();}
function syncStatus(){
  const connected=device.connected,stale=connected&&acquisition!=='stopped'&&lastSampleAt&&performance.now()-lastSampleAt>2500;
  importPanel?.sync(busy);$('exportSerialLog').disabled=!serialLog.count;$('serialLogCount').textContent=serialLog.count.toLocaleString();
  $('sourceSelect').value=mode;
  $('systemTag').textContent=mode==='demo'?'演示模式':connected?(stale?'等待数据':'设备连接'):'已断开';
  $('connectButton').innerHTML=icon('plug')+(connected?'断开串口':'连接串口');$('connectButton').disabled=device.state==='connecting'||device.state==='disconnecting';
  const pauseName=mode==='demo'?(running?'暂停演示':'继续演示'):(acquisition==='stopped'?'启动采集':'停止采集');
  $('pause').innerHTML=icon(mode==='demo'?(running?'pause':'play'):(acquisition==='stopped'?'play':'pause'));$('pause').title=pauseName;$('pause').setAttribute('aria-label',pauseName);$('pause').disabled=busy||(mode==='device'&&!connected);
  $('sourceSelect').disabled=busy||device.state==='connecting'||device.state==='disconnecting';
  $('serialSummary').textContent=`${settings.port?settings.port+' · ':''}${settings.baud} bps · 8N1`;
  $('rateLabel').textContent=mode==='demo'?'演示采样':'接收速率';
  const now=performance.now();rateMarks=rateMarks.filter(t=>now-t<2000);$('rate').textContent=mode==='demo'?(running?'20 Hz':'0 Hz'):(rateMarks.length>1?((rateMarks.length-1)/Math.max(.1,(now-rateMarks[0])/1000)).toFixed(1):'0.0')+' Hz';
  $('sampleCount').textContent=samples.length.toLocaleString();const seconds=Math.floor(samples.at(-1)?.t||0);$('duration').textContent=String(Math.floor(seconds/60)).padStart(2,'0')+':'+String(seconds%60).padStart(2,'0');
  const unit=currentUnit();document.querySelectorAll('.unit').forEach(e=>e.textContent=unit);
  $('startCalib').disabled=busy||(mode==='device'&&!connected);$('autoLoad').disabled=busy;
}
async function operation(fn){if(busy)return;busy=true;syncStatus();try{return await fn();}catch(error){log(error.message,'错误');toast(error.message);return null;}finally{busy=false;dirty=true;syncStatus();}}
async function scanPorts(){if(scanning)return;scanning=true;$('refreshPorts').disabled=true;try{const ports=await api.getPorts(),select=$('port'),previous=select.value||settings.port;const signature=ports.join(',');if(select.dataset.ports!==signature){select.replaceChildren();if(!ports.length)select.add(new Option('未检测到串口',''));for(const port of ports)select.add(new Option(port,port));select.value=ports.includes(previous)?previous:ports[0]||'';select.dataset.ports=signature;}$('portStatus').textContent=ports.length?'':'未检测到串口，请插入设备后刷新。';$('serialSubmit').disabled=!ports.length||device.state!=='disconnected';}catch(error){$('port').replaceChildren(new Option('串口服务不可用',''));delete $('port').dataset.ports;$('portStatus').textContent=error.message;$('serialSubmit').disabled=true;}finally{scanning=false;$('refreshPorts').disabled=false;}}
function openDialog(id){if(id==='logDialog')renderLogs();if(id==='serialDialog'){$('baud').value=settings.baud;scanPorts();clearInterval(portTimer);portTimer=setInterval(scanPorts,2500);}if(id==='calibDialog'){$('calibNotice').textContent=mode==='demo'?'演示模式：模拟 10 组采样进度，不改变设备。':'向设备发送 ML CALIB，等待真实标定结果。请先启动采集。';$('startCalib').innerHTML=icon('target')+(mode==='demo'?'开始模拟标定':'开始设备标定');if(!calibrationActive){$('calibProgress').style.display='none';$('calibProgress').classList.remove('pending');}}$(id).showModal();}
document.querySelectorAll('[data-open]').forEach(b=>b.addEventListener('click',()=>openDialog(b.dataset.open)));
document.querySelectorAll('[data-close]').forEach(b=>b.addEventListener('click',()=>b.closest('dialog').close()));
document.querySelectorAll('dialog').forEach(d=>d.addEventListener('click',e=>{if(e.target===d){if(d.id==='importDialog'&&importPanel?.busy)return;const r=d.getBoundingClientRect();if(e.clientX<r.left||e.clientX>r.right||e.clientY<r.top||e.clientY>r.bottom)d.close();}}));
$('serialDialog').addEventListener('close',()=>clearInterval(portTimer));$('refreshPorts').onclick=scanPorts;
$('monitorTab').onclick=()=>window.scrollTo({top:0,behavior:'smooth'});
$('connectButton').onclick=()=>device.connected?operation(()=>device.disconnect()):openDialog('serialDialog');
$('serialForm').onsubmit=async e=>{e.preventDefault();if(busy)return;settings={port:$('port').value,baud:$('baud').value};api.selected=settings.port;$('serialSubmit').disabled=true;await operation(async()=>{await device.connect(Number(settings.baud));const supportsImport=await device.resetImport({allowUnsupported:true});if(!supportsImport)log('检测到旧版固件：可接收采样数据，完整配置导入与读取需要更新固件。','兼容');persist();$('serialDialog').close();log(`已连接 ${settings.port}，数据来源已切换到真实设备。`,'连接');if(autoLoad)await readDeviceConfig();});if($('serialDialog').open)scanPorts();};
$('sourceSelect').onchange=async e=>{const next=e.target.value;if(next===mode)return;if(next==='device'){if(!device.connected){resetSession('device');openDialog('serialDialog');}}else{if(device.connected)await operation(()=>device.disconnect());resetSession('demo',true);log('已切换到演示模式，开始新的数据记录。');}};
$('pause').onclick=async()=>{if(mode==='demo'){running=!running;syncStatus();log(running?'已继续演示采样。':'已暂停演示采样。','采集');}else await operation(async()=>{if(acquisition==='stopped'){await device.start();acquisition='waiting';log('设备已确认启动采集（启动过程包含标定）。','采集');}else{await device.stop();acquisition='stopped';rateMarks=[];log('设备已确认停止采集。','采集');}});};
$('autoLoad').checked=autoLoad;$('autoLoad').onchange=()=>{autoLoad=$('autoLoad').checked;persist();log(autoLoad?'自动加载已开启：连接时读取设备完整配置。':'自动加载已关闭，可在导入配置中手动读取设备。','配置');if(autoLoad&&device.connected)operation(readDeviceConfig);};
async function readDeviceConfig(){
  importPanel.status('正在读取设备完整配置…');
  try{const actual=await device.readFullConfig();config={flags:actual.algo.flag,maps:[actual.algo.xmap,actual.algo.ymap,actual.algo.zmap]};configVerified=true;deviceLogLevel=actual.log.level;importPanel.loadDeviceConfig(actual);log('已读取设备完整配置：传感器参数、算法参数与日志等级。','配置');toast('设备配置已加载');}
  catch(error){configVerified=false;deviceLogLevel=null;importPanel.status(error.message,'error');throw error;}
  finally{syncStatus();}
}
$('showArrows').checked=showArrows;$('showArrows').onchange=e=>{showArrows=e.target.checked;persist();dirty=true;draw();};
$('startCalib').onclick=()=>operation(async()=>{calibrationActive=true;$('calibProgress').style.display='block';$('progressBar').style.width='0%';try{if(mode==='demo'){let n=0;await new Promise(resolve=>{calibTimer=setInterval(()=>{n++;$('progressBar').style.width=n*10+'%';$('progressText').textContent=`模拟采集：${n} / 10`;if(n===10){clearInterval(calibTimer);resolve();}},160);});$('progressText').textContent='模拟标定完成 · 设备未改变';log('模拟标定完成。','标定');}else{$('calibProgress').classList.add('pending');$('progressText').textContent='设备正在标定，等待回执…';await device.calibrate();$('progressText').textContent='设备已确认标定完成';log('设备标定完成。','标定');}toast($('progressText').textContent);}catch(error){$('progressText').textContent='标定未完成：'+error.message;throw error;}finally{$('calibProgress').classList.remove('pending');$('progressBar').style.width='100%';calibrationActive=false;}});
function exportCsv(){if(!samples.length){toast('暂无采样数据可导出。');return;}const url=URL.createObjectURL(new Blob([C.csv(samples.array())],{type:'text/csv;charset=utf-8'})),a=document.createElement('a');a.href=url;a.download='FIELD_'+mode+'_'+new Date().toISOString().replace(/[:.]/g,'-')+'.csv';document.body.append(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);log(`已导出 ${samples.length.toLocaleString()} 个${mode==='demo'?'演示':'设备'}采样点。`,'导出');toast('CSV 数据已保存');}
function exportSerialLog(){
  if(!serialLog.count){toast('暂无串口日志，请先连接设备。');return;}
  const url=URL.createObjectURL(new Blob(serialLog.snapshot(),{type:'text/plain;charset=utf-8'})),a=document.createElement('a');
  a.href=url;a.download='FIELD_serial_'+new Date().toISOString().replace(/[:.]/g,'-')+'.log';document.body.append(a);a.click();a.remove();
  setTimeout(()=>URL.revokeObjectURL(url),1000);toast('串口日志已导出（.log）');
}
$('exportCsv').onclick=exportCsv;$('exportLog').onclick=exportCsv;$('exportSerialLog').onclick=exportSerialLog;
$('clearData').onclick=async()=>{
  if(!samples.length&&!serialLog.count){toast('暂无数据或日志可清空。');return;}
  const dataCount=samples.length;
  const logCount=serialLog.count;
  if(!confirm(`确定要清空当前缓存的 ${dataCount.toLocaleString()} 个数据点和 ${logCount} 条日志吗？此操作不可恢复。`))return;

  // 先暂停传感器采集
  if(mode==='device'&&device.connected&&acquisition!=='stopped'){
    await operation(async()=>{
      await device.stop();
      acquisition='stopped';
      rateMarks=[];
      log('清空数据前已自动停止采集。','系统');
    });
  }

  samples.clear();magnitudes=[];rateMarks=[];tick=0;
  epoch=performance.now();wallEpoch=Date.now();lastSampleAt=0;
  hover=[null,null,null];
  document.querySelectorAll('.chart-tooltip').forEach(t=>t.style.display='none');
  serialLog.clear();
  dirty=true;renderLogs();syncStatus();draw();
  log(`已清空 ${dataCount.toLocaleString()} 个数据点和 ${logCount} 条日志。`,'系统');
  toast('数据和日志已清空');
};
$('resetDevice').onclick=async()=>{
  if(mode!=='device'){toast('请先连接设备。');return;}
  if(!device.connected){toast('设备未连接。');return;}
  if(!confirm('确定要复位设备吗？设备将重新启动，当前连接会断开。'))return;

  await operation(async()=>{
    // 1. 先暂停传感器采集
    if(acquisition!=='stopped'){
      await device.stop();
      acquisition='stopped';
      rateMarks=[];
      log('复位前已停止采集。','系统');
    }

    // 2. 发送 ML RESET 命令
    device.send('ML RESET');
    log('已发送复位命令，设备将重启。','系统');
    toast('设备正在复位...');

    // 3. 等待串口发送完成后断开连接
    await new Promise(resolve=>setTimeout(resolve,200));
    await device.disconnect();
    log('设备复位完成，连接已断开。请重新连接设备。','系统');
  });
};
device.on('state',({state,detail})=>{if(state==='connected'){serialLog.begin(settings.port,settings.baud);resetSession('device');}if(state==='disconnected')serialLog.end(detail);if(state==='disconnected'&&detail){log(detail,'连接');if(mode==='device')toast(detail);deviceLogLevel=null;}syncStatus();});
device.on('sample',v=>{if(mode!=='device')return;const now=performance.now();lastSampleAt=now;rateMarks.push(now);if(rateMarks.length>10000)rateMarks=rateMarks.slice(-5000);acquisition='streaming';addSample(v,(now-epoch)/1000,Date.now());});
device.on('tx',command=>{serialLog.append('TX',command);log(command,'发送',false);});
device.on('line',event=>{serialLog.append('RX',event.raw??event.line);if(event.kind==='message'&&event.line)log(event.line,'设备',false);if(event.line==='System_Reset'){configVerified=false;syncStatus();log('检测到设备重启，请重新读取配置。','系统');}});
device.on('warning',message=>{serialLog.append('警告',message);if(performance.now()-lastWarningAt>1000){lastWarningAt=performance.now();log(message,'警告',false);}});
window.addEventListener('pagehide',()=>{if(api.port?.id)fetch('/api/disconnect',{method:'POST',headers:{'Content-Type':'application/json','X-Field-Key':api.token,'X-Field-Connection':api.port.id},body:'{}',keepalive:true}).catch(()=>{});});
// Local 3D projection: illustrative deformation and optional vector arrows.
function context(canvas){const r=canvas.getBoundingClientRect(),dpr=Math.min(window.devicePixelRatio||1,2),w=r.width,h=r.height;if(canvas.width!==Math.round(w*dpr)||canvas.height!==Math.round(h*dpr)){canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);}const ctx=canvas.getContext('2d');ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);return{ctx,w,h};}
function setView(v){topView=v;for(const[id,state]of[['perspective',!v],['topView',v]]){$(id).classList.toggle('selected',state);$(id).setAttribute('aria-pressed',String(state));}dirty=true;draw();}
function resetView(){yaw=-.64;pitch=.65;zoom=1;setView(false);}
$('perspective').onclick=()=>setView(false);$('topView').onclick=()=>setView(true);$('resetView').onclick=resetView;
function changeZoom(d){zoom=Math.max(.65,Math.min(1.45,zoom+d));dirty=true;draw();}
$('zoomIn').onclick=()=>changeZoom(.1);$('zoomOut').onclick=()=>changeZoom(-.1);
const surface=$('surface');surface.ondblclick=resetView;
surface.onpointerdown=e=>{drag={x:e.clientX,y:e.clientY};surface.setPointerCapture(e.pointerId);};surface.onpointermove=e=>{if(!drag)return;yaw+=(e.clientX-drag.x)*.007;pitch=Math.max(.15,Math.min(1.3,pitch+(e.clientY-drag.y)*.006));drag={x:e.clientX,y:e.clientY};dirty=true;draw();};surface.onpointerup=surface.onpointercancel=()=>drag=null;
surface.addEventListener('wheel',e=>{e.preventDefault();changeZoom(e.deltaY<0?.05:-.05);},{passive:false});surface.onkeydown=e=>{if(['ArrowLeft','ArrowRight','ArrowUp','ArrowDown','+','-','Home'].includes(e.key)){e.preventDefault();if(e.key==='Home')resetView();else if(e.key==='+')changeZoom(.1);else if(e.key==='-')changeZoom(-.1);else{yaw+=e.key==='ArrowLeft'?-.1:e.key==='ArrowRight'?.1:0;pitch=Math.max(.15,Math.min(1.3,pitch+(e.key==='ArrowUp'?.08:e.key==='ArrowDown'?-.08:0)));dirty=true;draw();}}};
function drawSurface(){const{ctx:c,w,h}=context(surface);if(!w||!h)return;const values=samples.at(-1)?.v||[0,0,0],style=C.vectorStyle(values,magnitudes),s=Math.min(w/8.1,h/5.6)*zoom,angle=topView?Math.PI/2:pitch;
  const project=(x,y,z=0)=>{const rx=x*Math.cos(yaw)-y*Math.sin(yaw),ry=x*Math.sin(yaw)+y*Math.cos(yaw);return[w*.48+rx*s,h*.51+ry*s*Math.sin(angle)-z*s*Math.cos(angle),ry*Math.cos(angle)+z*Math.sin(angle)];};
  function path(points,stroke,fill,width=1){c.beginPath();points.forEach((p,i)=>i?c.lineTo(p[0],p[1]):c.moveTo(p[0],p[1]));if(fill){c.closePath();c.fillStyle=fill;c.fill();}if(stroke){c.strokeStyle=stroke;c.lineWidth=width;c.stroke();}}
  const bounds=[[-2.55,-2.55],[2.55,-2.55],[2.55,2.55],[-2.55,2.55]];
  path(bounds.map(([x,y])=>project(x,y,-.12)),null,'#edf2ef');
  const faces=[],height=(x,y)=>C.surfaceHeight(x,y,values,style.reference);
  // Surface is always rendered; arrow visibility never changes its geometry.
  for(let ix=0;ix<24;ix++)for(let iy=0;iy<24;iy++){
    const x=-2.5+ix*5/24,y=-2.5+iy*5/24,step=5/24;
    const vertices=[[x,y],[x+step,y],[x+step,y+step],[x,y+step]].map(([a,b])=>[a,b,height(a,b)]),p=vertices.map(v=>project(...v));
    const strength=Math.min(1,Math.abs(height(x+step/2,y+step/2))/1.55),t=.12+.7*strength;
    const color=[244,249,248].map((v,i)=>Math.round(v*(1-t)+style.color[i]*t));
    faces.push({p,depth:p.reduce((n,p)=>n+p[2],0)/4,color:`rgb(${color.join(',')})`,stroke:'rgba(100,151,143,.26)'});
  }
  if(showArrows)for(let x=-2;x<=2.01;x+=.4)for(let y=-2;y<=2.01;y+=.4){
    const arrow=C.surfaceArrow(x,y,values,style.reference);if(!arrow)continue;
    const mesh=C.arrowMesh(arrow.start,arrow.end);
    for(const face of mesh.faces){const vertices=face.map(i=>mesh.vertices[i]),p=vertices.map(v=>project(...v)),a=vertices[1].map((v,i)=>v-vertices[0][i]),b=vertices[2].map((v,i)=>v-vertices[0][i]),normal=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]],norm=Math.hypot(...normal)||1,light=.62+.38*Math.abs((normal[0]*.3-normal[1]*.4+normal[2]*.85)/norm);faces.push({p,depth:p.reduce((n,p)=>n+p[2],0)/3,color:`rgb(${style.color.map(v=>Math.round(v*light)).join(',')})`});}
  }
  faces.sort((a,b)=>a.depth-b.depth);for(const face of faces)path(face.p,face.stroke||null,face.color,.65);
  const origin=project(-2.8,2.8,.02);[['X',project(-1.7,2.8,.02),colors[0]],['Y',project(-2.8,1.7,.02),colors[1]],['Z',project(-2.8,2.8,1.02),colors[2]]].forEach(([label,end,color])=>{path([origin,end],color,null,1.5);const a=Math.atan2(end[1]-origin[1],end[0]-origin[0]);path([[end[0]-5*Math.cos(a-.4),end[1]-5*Math.sin(a-.4)],end,[end[0]-5*Math.cos(a+.4),end[1]-5*Math.sin(a+.4)]],color,null,1.5);c.fillStyle=color;c.font='10px Segoe UI';c.fillText(label,end[0]+5,end[1]-4);});
  c.font='10px Segoe UI';c.fillStyle='#9aa9a5';c.textAlign='center';c.fillText(samples.length?(showArrows?'示意形变 · 箭头沿坡面指向低处':'示意形变 · 方向箭头已隐藏'):'',w/2,h-44);c.textAlign='left';
}
axes.forEach((axis,i)=>{const canvas=$('chart'+axis);canvas.onpointermove=e=>{hover[i]=e.offsetX;drawChart(i);};canvas.onpointerleave=()=>{hover[i]=null;canvas.nextElementSibling.style.display='none';drawChart(i);};});
function drawChart(i){const canvas=$('chart'+axes[i]),{ctx:c,w,h}=context(canvas);if(!w||!h)return;const data=samples.recent(30),left=36,right=w-9,upper=12,lower=h-23;let range=.2;for(const p of data)range=Math.max(range,Math.abs(p.v[i]));range*=1.2;const now=data.at(-1)?.t||0,px=t=>left+(t-(now-30))/30*(right-left),py=v=>upper+(1-v/range)/2*(lower-upper);c.font='9px Segoe UI';c.textBaseline='middle';
  for(const v of[-range,0,range]){const y=py(v);c.strokeStyle=v===0?'#dde5e8':'#edf1f3';c.lineWidth=.8;c.setLineDash(v===0?[3,4]:[]);c.beginPath();c.moveTo(left,y);c.lineTo(right,y);c.stroke();c.fillStyle='#a1acb2';c.textAlign='right';c.fillText(v===0?'0':Math.abs(v)>999?v.toExponential(0):v.toFixed(Math.abs(v)<10?1:0),left-8,y);}
  c.setLineDash([]);for(let t=-30;t<=0;t+=10){const x=px(now+t);c.strokeStyle='#f0f3f4';c.beginPath();c.moveTo(x,upper);c.lineTo(x,lower);c.stroke();c.fillStyle='#a1acb2';c.textAlign=t===0?'right':t===-30?'left':'center';c.fillText(t===0?'最新':`${t}s`,x,h-8);}
  if(!data.length){c.textAlign='center';c.fillStyle='#a8b5b8';c.fillText('等待采样数据',w/2,h/2);$('pp'+axes[i]).textContent='—';return;}
  c.save();c.beginPath();c.rect(left,upper,right-left,lower-upper);c.clip();c.beginPath();let previous=null;data.forEach(p=>{if(previous&&p.t-previous.t<1)c.lineTo(px(p.t),py(p.v[i]));else c.moveTo(px(p.t),py(p.v[i]));previous=p;});c.strokeStyle=colors[i];c.lineWidth=1.6;c.lineJoin='round';c.stroke();c.restore();const last=data.at(-1);c.beginPath();c.arc(right,py(last.v[i]),3,0,Math.PI*2);c.fillStyle=colors[i];c.fill();
  if(hover[i]!==null){const ratio=Math.max(0,Math.min(1,(hover[i]-left)/(right-left))),target=now-30+ratio*30,p=data.reduce((best,p)=>Math.abs(p.t-target)<Math.abs(best.t-target)?p:best,data[0]),x=px(p.t),y=py(p.v[i]);c.setLineDash([3,3]);c.strokeStyle='#aab7be';c.beginPath();c.moveTo(x,upper);c.lineTo(x,lower);c.stroke();c.setLineDash([]);c.beginPath();c.arc(x,y,3,0,Math.PI*2);c.fillStyle=colors[i];c.fill();const tip=canvas.nextElementSibling;tip.textContent=`${(p.t-now).toFixed(1)} s · ${fmt(p.v[i])} ${p.unit}`;tip.style.display='block';}
  let lo=Infinity,hi=-Infinity;for(const p of data){lo=Math.min(lo,p.v[i]);hi=Math.max(hi,p.v[i]);}$('pp'+axes[i]).textContent=(hi-lo).toFixed(3);
}
function updateReadouts(){const values=samples.at(-1)?.v;axes.forEach((a,i)=>{const value=values?fmt(values[i]):'—';$('value'+a).textContent=value;$('chartValue'+a).replaceChildren(document.createTextNode(value+' '));const unit=document.createElement('small');unit.className='unit';unit.textContent=currentUnit();$('chartValue'+a).append(unit);});$('magnitude').textContent=values?Math.hypot(...values).toFixed(3):'—';if($('logDialog').open)$('recordCount').textContent=samples.length.toLocaleString();}
function draw(){drawSurface();axes.forEach((_,i)=>drawChart(i));updateReadouts();dirty=false;}
let last=performance.now(),accumulator=0,lastDraw=0;
function frame(now){const dt=Math.min(now-last,250);last=now;if(mode==='demo'&&running){accumulator+=dt;while(accumulator>=50){accumulator-=50;tick++;addSample(demoValues(tick/20),tick/20,wallEpoch+tick*50);}}else accumulator=0;if(dirty&&now-lastDraw>=50){draw();lastDraw=now;}if(now-lastStatus>500){syncStatus();lastStatus=now;}requestAnimationFrame(frame);}
new ResizeObserver(()=>{dirty=true;draw();}).observe(document.querySelector('.workspace'));
importPanel=new FieldImportPanel({device,operation,toast,log,onRead:readDeviceConfig,onUncertain:()=>{configVerified=false;deviceLogLevel=null;syncStatus();},onApplied:result=>{config=result.runtime;configVerified=true;deviceLogLevel=result.logLevel;if(result.started)acquisition='waiting';persist();samples.clear();magnitudes=[];rateMarks=[];lastSampleAt=0;epoch=performance.now();dirty=true;syncStatus();}});
resetSession('device');draw();log('设备模式已就绪，请连接串口开始采集。');if(!storageAvailable)toast('本地设置不可读取，本次使用默认配置。');requestAnimationFrame(frame);
})();
