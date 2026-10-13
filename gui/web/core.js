/* FIELD device/data layer. Plain script for file://, CommonJS for node --test. */
(function(root,factory){const api=factory();if(typeof module==='object'&&module.exports)module.exports=api;else root.FieldCore=api;})(globalThis,function(){
'use strict';
const NUMBER='[+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?';
const sampleRE=new RegExp('^sensor(\\d+)\\s*:\\s*('+NUMBER+')\\s*,\\s*('+NUMBER+')\\s*,\\s*('+NUMBER+')\\s*$');
const axisRE=new RegExp('^([XYZ]):\\s*('+NUMBER+')$');
const mapRE=new RegExp('^Mapping:\\s*X=('+NUMBER+'),\\s*Y=('+NUMBER+'),\\s*Z=('+NUMBER+')$');
function parseLine(text){
  const line=text.trim();let m=sampleRE.exec(line);
  if(m){const values=m.slice(2).map(Number);return values.every(Number.isFinite)?{kind:'sample',channel:Number(m[1]),values,line}:{kind:'invalid',line};}
  if(/^sensor\d*\s*:/.test(line))return{kind:'invalid',line};
  if((m=/^FLAG_RES:\s*0x([\da-f]{1,2})$/i.exec(line)))return{kind:'flag',value:parseInt(m[1],16),line};
  if((m=axisRE.exec(line)))return{kind:'axis',axis:m[1],value:Number(m[2]),line};
  if((m=mapRE.exec(line)))return{kind:'maps',values:m.slice(1).map(Number),line};
  return{kind:'message',line};
}
class LineFramer{
  constructor(onLine,onOverflow=()=>{},limit=4096){this.onLine=onLine;this.onOverflow=onOverflow;this.limit=limit;this.buffer='';this.dropping=false;this.decoder=new TextDecoder();}
  push(bytes){const text=typeof bytes==='string'?bytes:this.decoder.decode(bytes,{stream:true});for(const char of text){if(char==='\n'||char==='\r'){if(!this.dropping&&this.buffer.trim())this.onLine(this.buffer);this.buffer='';this.dropping=false;}else if(!this.dropping){this.buffer+=char;if(this.buffer.length>this.limit){this.buffer='';this.dropping=true;this.onOverflow();}}}}
}
class SampleBuffer{
  constructor(capacity=12000){this.capacity=capacity;this.items=new Array(capacity);this.clear();}
  clear(){this.start=0;this.length=0;this.total=0;}
  push(p){if(this.length<this.capacity){this.items[(this.start+this.length)%this.capacity]=p;this.length++;}else{this.items[this.start]=p;this.start=(this.start+1)%this.capacity;}this.total++;}
  at(index){if(index<0)index+=this.length;return index>=0&&index<this.length?this.items[(this.start+index)%this.capacity]:undefined;}
  array(){return Array.from({length:this.length},(_,i)=>this.at(i));}
  recent(seconds=30){const end=this.at(-1);if(!end)return[];const out=[];for(let i=this.length-1;i>=0;i--){const p=this.at(i);if(p.t<end.t-seconds)break;out.push(p);}return out.reverse();}
}
function validateConfig(c){if(!c||!Number.isInteger(c.flags)||c.flags<0||c.flags>255||!Array.isArray(c.maps)||c.maps.length!==3||!c.maps.every(n=>typeof n==='number'&&Number.isFinite(n)&&Math.abs(n)<=1000))throw new Error('配置无效：标志位需为 0–255，映射系数需为 −1000–1000 的有限数值。');return{flags:c.flags,maps:c.maps.map(n=>Number(n.toFixed(3)))};}
function configCommands(config){const c=validateConfig(config);return[`ML CHFLAG 0x${c.flags.toString(16).toUpperCase().padStart(2,'0')}`,...['X','Y','Z'].map((a,i)=>`ML SETMAP ${a} ${c.maps[i].toFixed(3)}`)];}
function csv(samples){return '\uFEFF'+['elapsed_s,received_at,x,y,z,source,unit',...samples.map(p=>[p.t.toFixed(3),new Date(p.time).toISOString(),...p.v.map(n=>n.toFixed(6)),p.source,p.unit].join(','))].join('\r\n');}
function percentile95(values){if(!values.length)return 1;const sorted=[...values].sort((a,b)=>a-b),f=(sorted.length-1)*.95,i=Math.floor(f);return sorted[i]+(sorted[Math.min(i+1,sorted.length-1)]-sorted[i])*(f-i);}
function vectorStyle(values,magnitudes){const[x,y,z]=values,max=Math.max(.01,magnitudes.length>=5?percentile95(magnitudes):1),xy=Math.hypot(x,y),ratio=Math.min(Math.abs(z)/max,1);let color;if(ratio<.33){const t=ratio/.33;color=[.4+t*.15,.9+t*.1,1-t*.3];}else if(ratio<.66){const t=(ratio-.33)/.33;color=[.55+t*.45,1,.7-t*.45];}else{const t=(ratio-.66)/.34;color=[1,1-t*.4,.25+t*.1];}return{direction:xy>1e-8?[-x/xy,-y/xy,0]:[0,0,0],length:Math.min(xy/max,1)*.36,color:color.map(v=>Math.round(v*255)),reference:max,zRatio:ratio};}
// Cylinder + cone, Rodrigues rotation from +Z to the force direction. Mirrors
// DISPLAY/gui/pages/force_3d_page.py, scaled to the flat single-channel pad.
function arrowMesh(start,end,shaftRadius=.019,headRadius=.05,sections=8){
  const d=end.map((v,i)=>v-start[i]),length=Math.hypot(...d);if(length<1e-8)return{vertices:[],faces:[]};const u=d.map(v=>v/length),v=[-u[1],u[0],0],s=Math.hypot(...v),c=u[2];
  const cross=(a,b)=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
  const rotate=p=>{let q;if(s<1e-10)q=c>0?p:[p[0],p[1],-p[2]];else{const a=cross(v,p),b=cross(v,a);q=p.map((x,i)=>x+a[i]+b[i]*(1-c)/(s*s));}return q.map((x,i)=>x+start[i]);};
  const shaft=length*.65,vertices=[],faces=[];
  for(const[r,z]of[[shaftRadius,0],[shaftRadius,shaft],[headRadius,shaft]])for(let i=0;i<sections;i++){const a=i/sections*Math.PI*2;vertices.push(rotate([Math.cos(a)*r,Math.sin(a)*r,z]));}
  vertices.push([...end]);for(let i=0;i<sections;i++){const j=(i+1)%sections;faces.push([i,j,sections+j],[i,sections+j,sections+i],[sections*2+i,sections*2+j,sections*3]);}return{vertices,faces};
}
class SerialClient{
  constructor(serialApi,options={}){this.api=serialApi;this.timeout=options.timeout||5000;this.listeners=new Map();this.port=null;this.reader=null;this.pending=null;this.queue=Promise.resolve();this.state='disconnected';this.generation=0;this.closing=null;this.writeTask=null;this.writer=null;this.onDisconnect=e=>{if(e.target===this.port||e.port===this.port)this.disconnect('设备已拔出');};this.api?.addEventListener?.('disconnect',this.onDisconnect);}
  on(name,fn){if(!this.listeners.has(name))this.listeners.set(name,new Set());this.listeners.get(name).add(fn);return()=>this.listeners.get(name).delete(fn);}
  emit(name,value){for(const fn of this.listeners.get(name)||[])fn(value);}
  get connected(){return this.state==='connected';}
  setState(state,detail=''){this.state=state;this.emit('state',{state,detail});}
  async connect(baudRate=115200){
    if(!this.api)throw new Error('当前浏览器未提供 Web Serial，请用桌面版 Edge / Chrome 打开此页面。');
    if(this.state!=='disconnected')throw new Error('连接正在使用，请先断开。');
    this.setState('connecting');let port;
    try{port=await this.api.requestPort();await port.open({baudRate:Number(baudRate),dataBits:8,stopBits:1,parity:'none',flowControl:'none',bufferSize:65536});this.port=port;this.generation++;this.queue=Promise.resolve();this.framer=new LineFramer(line=>this.handleLine(line),()=>this.emit('warning','串口文本行过长，已丢弃并重新同步。'));this.setState('connected');this.readTask=this.readLoop(port);return port.getInfo?.()||{};}
    catch(error){if(port)try{await port.close();}catch{}this.port=null;this.setState('disconnected',error.name==='NotFoundError'?'已取消选择串口':error.message);throw error;}
  }
  async readLoop(port){
    let fault;
    try{this.reader=port.readable.getReader();while(this.connected&&this.port===port){const{value,done}=await this.reader.read();if(done){if(this.connected)fault=new Error('串口数据流已关闭');break;}if(value)this.framer.push(value);}}
    catch(error){if(this.connected)fault=error;}
    finally{try{this.reader?.releaseLock();}catch{}this.reader=null;}
    // Schedule after this task resolves, so disconnect can safely await it.
    if(fault)queueMicrotask(()=>this.disconnect('串口读取中断：'+fault.message));
  }
  async disconnect(reason='串口已断开'){
    if(this.closing)return this.closing;if(this.state==='disconnected')return;
    this.setState('disconnecting',reason);this.generation++;this.pending?.reject(new Error(reason));
    this.closing=(async()=>{try{await this.reader?.cancel();}catch{}await this.readTask;try{await this.writer?.abort(reason);}catch{}try{await this.writeTask;}catch{}try{await this.port?.close();}catch(error){this.emit('warning','关闭串口：'+error.message);}this.port=null;this.reader=null;this.readTask=null;this.setState('disconnected',reason);})();
    try{await this.closing;}finally{this.closing=null;}
  }
  handleLine(line){const event=parseLine(line);this.emit('line',event);if(event.kind==='sample'){if(event.channel===0)this.emit('sample',event.values);}else if(event.kind==='invalid')this.emit('warning','丢弃格式无效的采样行：'+event.line);if(event.line==='System_Reset'&&this.pending){this.pending.reject(new Error('设备在命令执行期间重启，请重新连接。'));void this.disconnect('设备已重启');return;}this.pending?.accept(event);}
  transaction(fn){const generation=this.generation;const result=this.queue.then(()=>{if(!this.connected||this.generation!==generation)throw new Error('串口未连接或连接已改变。');return fn();});this.queue=result.catch(()=>{});return result;}
  async request(command,match,timeout=this.timeout){
    if(!this.connected)throw new Error('串口未连接。');if(command.length>62||/[\r\n]/.test(command))throw new Error('无效的串口命令。');
    let settle;const answer=new Promise((resolve,reject)=>{let timer;const finish=(error,value)=>{if(this.pending!==pending)return;clearTimeout(timer);this.pending=null;error?reject(error):resolve(value);};const pending={reject:error=>finish(error),accept:event=>{try{const result=match(event);if(result!==undefined)finish(null,result);}catch(error){finish(error);}}};this.pending=pending;settle=pending;timer=setTimeout(()=>{finish(new Error(`命令超时：${command}。请重新连接后读取设备状态。`));void this.disconnect('命令超时，连接已关闭以避免迟到回执混淆。');},timeout);});
    // Register the matcher before writing: a fast device can reply immediately.
    answer.catch(()=>{});
    this.emit('tx',command);
    try{const writer=this.port.writable.getWriter();this.writer=writer;this.writeTask=(async()=>{try{await writer.write(new TextEncoder().encode(command+'\n'));}finally{writer.releaseLock();if(this.writer===writer)this.writer=null;}})();await this.writeTask;}
    catch(error){settle.reject(new Error('串口发送失败：'+error.message));void this.disconnect('串口发送失败');}
    return answer;
  }
  ack(command,success,failure,timeout){return this.request(command,e=>{if(failure.test(e.line)||/^Error:|^Unknown command:/.test(e.line))throw new Error(e.line);if(success.test(e.line))return true;},timeout);}
  start(){return this.transaction(()=>this.ack('ML START',/^Sensor started successfully$/, /Failed to start|Sensor start failed/i,15000));}
  stop(){return this.transaction(()=>this.ack('ML STOP',/^Sensor stopped$/, /Failed to stop/i));}
  calibrate(){return this.transaction(()=>this.ack('ML CALIB',/^Calibration completed$/, /Calibration failed|calibrate failed|not running|please start/i,15000));}
  async readConfigDirect(){const flags=await this.request('ML FLAG',e=>{if(e.kind==='flag')return e.value;if(/^Error:|^Unknown command:/.test(e.line))throw new Error(e.line);});const maps={};let begun=false;await this.request('ML MAPPING',e=>{if(/^Error:|^Unknown command:/.test(e.line))throw new Error(e.line);if(e.line==='Mapping values:')begun=true;if(begun&&e.kind==='axis'){maps[e.axis]=e.value;if(['X','Y','Z'].every(a=>Number.isFinite(maps[a])))return true;}});return validateConfig({flags,maps:['X','Y','Z'].map(a=>maps[a])});}
  readConfig(){return this.transaction(()=>this.readConfigDirect());}
  writeConfig(input){const config=validateConfig(input);return this.transaction(async()=>{
    const commands=configCommands(config);for(let i=0;i<commands.length;i++){
      let saved=false,updated=false,saveError=false;
      await this.request(commands[i],e=>{
        if(/^Error:|^Unknown command:/.test(e.line))throw new Error(e.line);
        if(e.line==='Failed to save config to Flash'){saveError=true;if(i===0)throw new Error('Flash 保存失败，设备内存参数可能已改变；请读取配置核对。');}
        if(e.line==='Config saved to Flash:')saved=true;
        if(i===0&&/^FLAG_RES updated:/.test(e.line))updated=true;
        if(i===0&&updated&&saved&&e.kind==='maps')return true;
        if(i>0&&e.line.startsWith(['','X','Y','Z'][i]+'_MAPPING_VALUE updated:')){if(saveError||!saved)throw new Error('设备未确认 Flash 保存，参数可能仅在内存中改变。');return true;}
      });
    }
    const actual=await this.readConfigDirect();if(actual.flags!==config.flags||actual.maps.some((v,i)=>Math.abs(v-config.maps[i])>.00051))throw new Error('设备读回配置与期望值不一致，请重新读取并核对。');return actual;
  });}
}
return{parseLine,LineFramer,SampleBuffer,validateConfig,configCommands,csv,percentile95,vectorStyle,arrowMesh,SerialClient};
});
