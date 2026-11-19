(function(){
'use strict';
const M=AlgorithmModel,$=id=>document.getElementById(id),colors=['#247a65','#c18c45'];
const frames=new M.Frames(300);let state=null,selected=4,demo=false,paused=false,busy=false,supported=false;
let latest=null,displayed=null,lastFrame=0,demoTimer=null,demoSeq=0,axis=0,api,device,local=false;
const fmt=v=>Number.isFinite(v)?v.toFixed(3):'—';
function stageInfo(id){
  const s={...M.stages[id]};
  if(state?.profile===0){
    if(id===5){s.name='滑动平均';s.description='legacy 时间窗口平均';}
    if(id===6){s.name='噪声死区';s.description='实际通道噪声阈值';}
    if(id===7){s.name='Z 轴正值裁剪';s.description='legacy 正半轴置零';}
  }
  return s;
}
const announce=(message,error=false)=>{$('notice').textContent=message;$('notice').classList.toggle('error',error);};
function reset(){frames.clear();latest=null;displayed=null;lastFrame=0;render();}
function controls(){
  const connected=!!device?.connected;
  $('connect').textContent=connected?'断开设备':'连接设备';$('connect').disabled=busy;
  $('port').disabled=connected||busy;$('refresh').disabled=connected||busy;
  for(const id of ['start','stop','sync'])$(id).disabled=busy||!connected||!supported;
  $('demo').disabled=busy||connected;$('demo').textContent=demo?'退出演示':'体验演示';
  $('connectionDot').classList.toggle('connected',connected);
  $('sourceBadge').className='source-badge'+(demo?' demo':connected&&supported?' live':'');
  $('sourceBadge').textContent=demo?'● 演示数据':connected?(supported?'● 设备已连接':'● 固件待更新'):'● 离线';
  $('profile').value=String(state?.profile??0);$('profile').disabled=busy||demo||!connected||!supported;
  $('enabledCount').textContent=state?`${M.order(state.profile).filter(id=>['on','fixed'].includes(M.availability(state,id))).length} / ${M.order(state.profile).length} 节点已启用`:'等待设备状态';
  $('export').disabled=!frames.history.length;
  renderNodes();
}
function renderNodes(){
  const ids=state?M.order(state.profile):M.order(1);
  const nodes=ids.map(id=>{
    const s=stageInfo(id),status=state?M.availability(state,id):'unknown';
    const item=document.createElement('div');item.className='node'+(selected===id?' selected':'')+(status==='bypass'?' bypassed':'')+(id===0?' fixed':'');
    item.tabIndex=0;item.setAttribute('role','button');item.setAttribute('aria-label',`查看 S${id} ${s.name}`);item.setAttribute('aria-pressed',String(selected===id));
    const number=document.createElement('span');number.className='node-id';number.textContent='S'+id;
    const copy=document.createElement('div');copy.className='node-copy';
    const title=document.createElement('div');title.className='node-name';title.textContent=s.name;
    const description=document.createElement('div');description.className='node-desc';
    description.textContent=status==='uncalibrated'?'等待有效标定':id===5&&state?.profile===0?'legacy 滑动窗口':s.description;
    copy.append(title,description);item.append(number,copy);
    if(id===0){const tag=document.createElement('span');tag.className='node-tag';tag.textContent='必选';item.append(tag);}
    else{
      const label=document.createElement('label');label.className='toggle';
      const input=document.createElement('input');input.type='checkbox';input.setAttribute('role','switch');input.setAttribute('aria-label',`启用 S${id} ${s.name}`);
      input.checked=!!state&&!['bypass','absent'].includes(status);input.disabled=busy||!state||(!demo&&(!device?.connected||!supported));
      input.addEventListener('click',e=>e.stopPropagation());label.addEventListener('click',e=>e.stopPropagation());
      input.addEventListener('change',()=>void setNode(id,input.checked));label.append(input);item.append(label);
    }
    const choose=()=>{selected=id;render();};item.addEventListener('click',choose);
    item.addEventListener('keydown',e=>{if(e.target===item&&['Enter',' '].includes(e.key)){e.preventDefault();choose();}});return item;
  });
  $('nodes').replaceChildren(...nodes);
}
async function operation(fn){if(busy)return;busy=true;controls();try{await fn();}catch(e){announce(e.message,true);}finally{busy=false;controls();render();}}
function request(command,success){return device.transaction(()=>device.request(command,e=>{
  if(/^Error:|^Unknown|^Usage:/.test(e.line))throw new Error(e.line);
  if(success.test(e.line))return e.line;
}));}
async function readState(){const line=await request('ML NODES',/^NODES /),next=M.parse(line);if(!next||next.kind!=='state')throw new Error('设备返回的节点状态无效。');state=next;if(!M.order(state.profile).includes(selected))selected=4;return next;}
async function setNode(id,enabled){
  await operation(async()=>{
    if(demo){if(enabled){state.bypass&=~(1<<id);state.flags|=M.stages[id].bit;}else state.bypass|=1<<id;}
    else{await request(`ML NODE ${id} ${+enabled}`,new RegExp(`^NODE s=${id} enabled=${+enabled}$`));await readState();}
    reset();announce(`${demo?'演示 · ':''}S${id} ${M.stages[id].name}已${enabled?'启用':'旁路'}，已清空当前曲线。${demo?'':'本次修改仅在 RAM 中生效。'}`);
  });
}
async function initPorts(){
  if(local){const ports=await api.getPorts();$('port').replaceChildren(...ports.map(name=>new Option(name,name)));if(!ports.length)$('port').add(new Option('未检测到串口',''));}
  else{$('port').replaceChildren(new Option('浏览器选择串口','browser'));}
}
function accept(frame){if(!frame)return;frame.receivedAt=performance.now();latest=frame;lastFrame=frame.receivedAt;if(!paused)displayed=frame;renderReadouts();controlsExport();}
function controlsExport(){$('export').disabled=!frames.history.length;}
function renderReadouts(){
  const n=displayed?.valid?displayed.nodes[selected]:null;
  for(let i=0;i<3;i++){const value=$('actual'+i),reference=$('reference'+i);if(value)value.firstChild.textContent=fmt(n?.actual[i]);if(reference)reference.textContent=fmt(n?.reference[i]);}
  const status=state?M.availability(state,selected):'unknown';
  const stale=!demo&&lastFrame&&performance.now()-lastFrame>2500;
  $('stageState').textContent=paused?'显示已暂停':state?.running===0?'采集已停止':stale?'遥测已中断':displayed&&!displayed.valid?`无效帧 · 错误 ${displayed.error}`:
    n?(n.run?'节点已执行':'节点已旁路'):({uncalibrated:'等待标定',bypass:'已旁路',fixed:'必选校验',on:'等待数据'})[status]||'等待数据';
  $('sequence').textContent=displayed?`SEQ ${displayed.seq} · ${(displayed.t/1e6).toFixed(3)} s`:'SEQ —';
  const ids=state?M.order(state.profile):[],prevId=ids[ids.indexOf(selected)-1],previous=displayed?.nodes[prevId];
  $('observation').textContent=n&&previous?`相对 S${prevId}，实际通道 ΔXYZ = ${n.actual.map((v,i)=>fmt(v-previous.actual[i])).join(' / ')}。${selected>=8?'已应用映射的节点单位取决于配置。':'数值单位 µT。'}`:
    selected===0?'S0 是强制校验入口；无效样本不进入后续节点。':'选择节点后，这里会显示该节点与上一执行节点的差值。';
  $('lost').textContent=`${frames.invalid} / ${frames.incomplete}`;
  const recent=frames.history.filter(f=>performance.now()-f.receivedAt<5000);
  const hz=recent.length>1?(recent.length-1)*1000/(recent.at(-1).receivedAt-recent[0].receivedAt):0;
  $('rate').firstChild.textContent=hz?hz.toFixed(1):'—';
}
function render(){
  const s=stageInfo(selected);$('stageCode').textContent=`STAGE ${String(selected).padStart(2,'0')}`;$('stageTitle').textContent=s.name;
  $('stageDescription').textContent=selected===5&&state?.profile===0?'legacy 滑动窗口平均':selected===7&&state?.profile===0?'legacy Z 正值裁剪':s.description;
  $('readouts').replaceChildren(...['X','Y','Z'].map((name,i)=>{
    const unit=selected>=8?'映射单位':'µT',box=document.createElement('div');box.className='readout';
    box.innerHTML=`<div class="readout-head"><span class="axis-icon">${name} <span>轴</span></span><span>实际通道</span></div><strong id="actual${i}">—<small>${unit}</small></strong><div class="readout-ref"><span>参考通道 · µT</span><b id="reference${i}">—</b></div>`;return box;
  }));controls();renderReadouts();draw();
}
function draw(){
  const canvas=$('chart'),rect=canvas.getBoundingClientRect(),dpr=window.devicePixelRatio||1;
  if(!rect.width||!rect.height)return;canvas.width=Math.round(rect.width*dpr);canvas.height=Math.round(rect.height*dpr);
  const c=canvas.getContext('2d');c.scale(dpr,dpr);const w=rect.width,h=rect.height,l=53,r=w-16,top=22,bottom=h-30;
  const cutoff=displayed?.receivedAt??performance.now(),data=frames.history.filter(f=>f.receivedAt<=cutoff&&f.receivedAt>cutoff-30000);
  const values=data.flatMap(f=>f.nodes[selected]?[f.nodes[selected].actual[axis],f.nodes[selected].reference[axis]]:[]);
  const unit=selected>=8?'实际：映射单位 / 参考：µT':'µT';
  let low=values.length?Math.min(...values):-.5,high=values.length?Math.max(...values):.5;
  let padding=Math.max((high-low)*.18,.05);low-=padding;high+=padding;
  c.font='10px Consolas, monospace';c.textAlign='right';c.textBaseline='middle';
  for(let i=0;i<=4;i++){const y=top+i*(bottom-top)/4;c.strokeStyle='#edf1eb';c.beginPath();c.moveTo(l,y);c.lineTo(r,y);c.stroke();c.fillStyle='#9ba79c';c.fillText((high-(high-low)*i/4).toFixed(2),l-10,y);}
  c.textAlign='center';for(let i=0;i<=6;i++){const x=l+i*(r-l)/6;c.fillStyle='#a6b0a5';c.fillText(`${i*5-30}s`,x,bottom+18);}
  c.textAlign='left';c.fillStyle='#a0aa9e';c.font='9px Segoe UI, Microsoft YaHei';c.fillText(unit,l,9);
  for(let channel=0;channel<2;channel++){
    c.strokeStyle=colors[channel];c.lineWidth=channel===0?1.9:1.5;c.setLineDash(channel?[4,4]:[]);c.beginPath();let pen=false,last=0;
    for(const f of data){const n=f.nodes[selected];if(!f.valid||!n){pen=false;continue;}const x=l+(f.receivedAt-cutoff+30000)/30000*(r-l),value=(channel?n.reference:n.actual)[axis],y=bottom-(value-low)/(high-low)*(bottom-top);
      if(!pen||f.receivedAt-last>600)c.moveTo(x,y);else c.lineTo(x,y);pen=true;last=f.receivedAt;
    }c.stroke();
  }c.setLineDash([]);$('chartEmpty').hidden=values.length>0;
}
function demoFrame(){
  const seq=++demoSeq,t=seq/10,base=[2.3*Math.sin(t*.8),1.8*Math.sin(t*.8+.8),-2-Math.sin(t*.6)],ref=[.7*Math.sin(t*.3),.5*Math.cos(t*.4),-.4*Math.sin(t*.5)];
  let a=base.map((v,i)=>v+ref[i]+[12,-6,4][i]),r=ref.map((v,i)=>v+[2,-3,1][i]);
  for(const id of M.order(1)){
    const run=['on','fixed'].includes(M.availability(state,id));
    if(run){if(id===2){a=a.map((v,i)=>v-[12,-6,4][i]);r=r.map((v,i)=>v-[2,-3,1][i]);}
      if(id===4)a=a.map((v,i)=>v-r[i]);if(id===5)a=a.map(v=>v*.93);if(id===6)a=a.map(v=>Math.abs(v)<.12?0:v);if(id===7)a[2]=Math.max(0,-a[2]);if(id===8)a=a.map(v=>v*1.2);}
    accept(frames.push({kind:'node',seq,t:seq*100000,profile:1,id,run,valid:true,error:0,actual:[...a],reference:[...r]}));
  }
}
function setDemo(enabled){
  clearInterval(demoTimer);demoTimer=null;demo=enabled;state=enabled?{profile:1,flags:255,bypass:0,calibrated:1,trace:1,running:1}:null;reset();
  if(enabled){for(let i=0;i<160;i++){demoFrame();const f=frames.history.at(-1);f.receivedAt=performance.now()-(159-i)*100;}latest=displayed=frames.history.at(-1);demoTimer=setInterval(demoFrame,100);}
  $('deviceInfo').textContent=enabled?'演示 · 合成信号，仅用于界面体验':'尚未连接设备';
  announce(enabled?'演示模式：曲线是合成示意数据，开关仅改变演示，不连接设备，也不代表固件精度。':'连接设备以读取真实节点数据，或体验演示。');render();
}
async function initialize(){
  try{api=new LocalSerialApi();await api.init();local=true;}catch{api=navigator.serial;local=false;}
  device=new FieldCore.SerialClient(api);
  device.on('line',event=>{const n=M.parse(event.line);if(n?.kind==='node')accept(frames.push(n));});
  device.on('state',({state:status,detail})=>{if(status==='disconnected'){supported=false;state=null;reset();if(detail)announce(detail);}controls();});
  try{await initPorts();}catch(e){announce(e.message,true);}
  $('refresh').onclick=()=>void operation(initPorts);
  $('connect').onclick=()=>void operation(async()=>{
    if(device.connected){if(supported)await request('ML TRACE 0',/^TRACE 0$/);await device.disconnect();return;}
    if(demo)setDemo(false);if(local){if(!$('port').value)throw new Error('未检测到串口，请检查 USB 与供电。');api.selected=$('port').value;}
    await device.connect(921600);const version=await device.readVersion();$('deviceInfo').textContent=version?`${version.board} · ${version.sensors} · FW ${version.fw}`:'设备版本未知';
    try{await readState();supported=true;}catch{supported=false;throw new Error('当前固件不支持节点遥测，请烧录本次构建后重新连接。');}
    await request('ML TRACE 1',/^TRACE 1$/);await readState();reset();
    announce('节点遥测已开启。若设备已停止，请点击开始采集并保持传感器静止完成标定。');
  });
  $('sync').onclick=()=>void operation(async()=>{await readState();announce('已读取设备节点状态。');});
  $('start').onclick=()=>void operation(async()=>{await device.start();await readState();reset();announce('标定完成，正在采集真实节点数据。');});
  $('stop').onclick=()=>void operation(async()=>{await device.stop();await readState();announce('采集已停止，保留当前曲线。');});
}
$('demo').onclick=()=>{if(!busy&&!device?.connected)setDemo(!demo);};
$('axis').onchange=()=>{axis=Number($('axis').value);draw();};
$('profile').onchange=()=>{const profile=Number($('profile').value);void operation(async()=>{
  await device.stop();await request(`ML SET algo.profile ${profile}`,/^SET algo\.profile=/);await readState();reset();
  announce('已切换算法配置档并停止采集。请点击开始采集，保持传感器静止完成重新标定；配置未写入 Flash。');
});};
$('freeze').onclick=()=>{paused=!paused;$('freeze').textContent=paused?'继续显示':'暂停显示';$('freeze').setAttribute('aria-pressed',String(paused));if(!paused)displayed=latest;renderReadouts();draw();};
$('export').onclick=()=>{const blob=new Blob([M.toCSV(frames.history)],{type:'text/csv;charset=utf-8'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=`field-nodes-${demo?'demo':'device'}-${Date.now()}.csv`;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);};
window.addEventListener('resize',draw);
window.addEventListener('pagehide',()=>{if(local&&api?.port?.id)fetch('/api/disconnect',{method:'POST',headers:{'Content-Type':'application/json','X-Field-Key':api.token,'X-Field-Connection':api.port.id},body:'{}',keepalive:true}).catch(()=>{});});
render();void initialize();if(new URLSearchParams(location.search).get('demo')==='1')setDemo(true);
setInterval(()=>{renderReadouts();draw();},150);
})();
