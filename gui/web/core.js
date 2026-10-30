/* FIELD device/data layer. Plain script for file://, CommonJS for node --test. */
(function(root,factory){const api=factory();if(typeof module==='object'&&module.exports)module.exports=api;else root.FieldCore=api;})(globalThis,function(){
'use strict';
const localize=(zh,en)=>globalThis.FieldI18n?FieldI18n.t(zh,en):zh;
const NUMBER='[+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?';
const sampleRE=new RegExp('^sensor(\\d+)\\s*:\\s*('+NUMBER+')\\s*,\\s*('+NUMBER+')\\s*,\\s*('+NUMBER+')\\s*$');
const axisRE=new RegExp('^([XYZ]):\\s*('+NUMBER+')$');
const mapRE=new RegExp('^Mapping:\\s*X=('+NUMBER+'),\\s*Y=('+NUMBER+'),\\s*Z=('+NUMBER+')$');
function parseLine(text){
  const raw=text.trim(), line=raw.replace(/^\[[EWID]\]\[[^\]\r\n]+\]\s*/, '');let m=sampleRE.exec(line);
  if(m){const values=m.slice(2).map(Number);return values.every(Number.isFinite)?{kind:'sample',channel:Number(m[1]),values,line}:{kind:'invalid',line};}
  if(/^sensor\d*\s*:/.test(line))return{kind:'invalid',line};
  if((m=/^FLAG_RES:\s*0x([\da-f]{1,2})$/i.exec(line)))return{kind:'flag',value:parseInt(m[1],16),line};
  if((m=axisRE.exec(line)))return{kind:'axis',axis:m[1],value:Number(m[2]),line};
  if((m=mapRE.exec(line)))return{kind:'maps',values:m.slice(1).map(Number),line};
  if((m=new RegExp('^X=('+NUMBER+') Y=('+NUMBER+') Z=('+NUMBER+')$').exec(line)))return{kind:'maps',values:m.slice(1).map(Number),line};
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
function validateConfig(c){if(!c||!Number.isInteger(c.flags)||c.flags<0||c.flags>255||!Array.isArray(c.maps)||c.maps.length!==3||!c.maps.every(n=>typeof n==='number'&&Number.isFinite(n)&&Number.isFinite(Math.fround(n))))throw new Error(localize('配置无效：标志位需为 0–255，映射系数需为有限 float32 数值。','Invalid configuration: flags must be 0–255; mapping factors must be finite float32 values.'));return{flags:c.flags,maps:[...c.maps]};}
function configCommands(config){const c=validateConfig(config);return[`ML CHFLAG 0x${c.flags.toString(16).toUpperCase().padStart(2,'0')}`,...['X','Y','Z'].map((a,i)=>`ML SETMAP ${a} ${c.maps[i].toFixed(3)}`)];}
function csv(samples){return '\uFEFF'+['elapsed_s,received_at,x,y,z,source,unit',...samples.map(p=>[p.t.toFixed(3),new Date(p.time).toISOString(),...p.v.map(n=>n.toFixed(6)),p.source,p.unit].join(','))].join('\r\n');}
// Separate archive from the 150-row UI preview and the bounded sample buffer.
// Immutable text chunks keep repeated exports from clearing ongoing capture.
class SerialLog{
  constructor(){this.parts=[];this.tail=[];this.count=0;this.sessions=0;this.active=false;this.startedAt=null;}
  begin(port,baud,time=Date.now()){
    this.startedAt??=time;this.sessions++;this.active=true;
    this.append(localize('连接','Connect'),localize(`会话 ${this.sessions} · ${port} · ${baud} bps · 8N1`,`Session ${this.sessions} · ${port} · ${baud} bps · 8N1`),time);
  }
  append(type,message,time=Date.now()){
    if(!this.sessions)return;
    // 排除传感器数据行（sensor0:... 格式）
    const msgStr = String(message);
    if(type === 'RX' && /^sensor\d+\s*:/.test(msgStr.trim())) return;
    const prefix=`[${new Date(time).toISOString()}] [${type}] `;
    this.tail.push(prefix+msgStr.replace(/\r\n|\r|\n/g,'\r\n'+prefix)+'\r\n');this.count++;
    if(this.tail.length>=256){this.parts.push(this.tail.join(''));this.tail=[];}
  }
  end(reason,time=Date.now()){if(!this.active)return;this.append(localize('断开','Disconnect'),reason||localize('串口已断开','Serial port disconnected'),time);this.active=false;}
  snapshot(){return[localize('\uFEFFFIELD 串口通信日志\r\n时间戳：UTC（ISO 8601）；RX 为设备原文，TX 为发送命令。\r\n','\uFEFFFIELD serial communication log\r\nTimestamp: UTC(ISO 8601); RX contains raw device text; TX contains sent commands.\r\n')+localize(`会话数：${this.sessions}；记录数：${this.count}\r\n\r\n`,`Sessions: ${this.sessions}; Records: ${this.count}\r\n\r\n`),...this.parts,this.tail.join('')];}
}
function percentile95(values){if(!values.length)return 1;const sorted=[...values].sort((a,b)=>a-b),f=(sorted.length-1)*.95,i=Math.floor(f);return sorted[i]+(sorted[Math.min(i+1,sorted.length-1)]-sorted[i])*(f-i);}
function vectorStyle(values,magnitudes){const[x,y,z]=values,max=Math.max(.01,magnitudes.length>=5?percentile95(magnitudes):1),xy=Math.hypot(x,y),ratio=Math.min(Math.abs(z)/max,1);let color;if(ratio<.33){const t=ratio/.33;color=[.4+t*.15,.9+t*.1,1-t*.3];}else if(ratio<.66){const t=(ratio-.33)/.33;color=[.55+t*.45,1,.7-t*.45];}else{const t=(ratio-.66)/.34;color=[1,1-t*.4,.25+t*.1];}return{direction:xy>1e-8?[-x/xy,-y/xy,0]:[0,0,0],length:Math.min(xy/max,1)*.36,color:color.map(v=>Math.round(v*255)),reference:max,zRatio:ratio};}
// Single-point deformation illustration, not a measured spatial field.
// Pin the boundary, retain the sign of Z, and use XY for local surface tilt.
function surfaceHeight(x,y,values,reference=1){
  if(Math.abs(x)>=2.5||Math.abs(y)>=2.5)return 0;
  const scale=Math.max(.01,reference),[nx,ny,nz]=values.map(v=>Math.max(-1,Math.min(1,v/scale)));
  const envelope=Math.cos(x*Math.PI/5)**2*Math.cos(y*Math.PI/5)**2;
  return envelope*(1.55*nz-1.1*(nx*x+ny*y));
}
// Follow the local downhill gradient, independently of the global XY vector.
function surfaceArrow(x,y,values,reference=1){
  const height=(a,b)=>surfaceHeight(a,b,values,reference),e=.01;
  const dx=(height(x-e,y)-height(x+e,y))/(2*e),dy=(height(x,y-e)-height(x,y+e))/(2*e),slope=Math.hypot(dx,dy);
  if(slope<1e-4)return null;
  const z=height(x,y),ux=dx/slope,uy=dy/slope;
  // Shorten near extrema so the tip cannot overshoot the depression and climb.
  for(let length=.36*Math.tanh(slope*2);length>=.012;length*=.5){
    const tx=x+ux*length,ty=y+uy*length,tz=height(tx,ty);
    if(Math.abs(tx)<=2.5&&Math.abs(ty)<=2.5&&tz<z-1e-8)return{start:[x,y,z+.075],end:[tx,ty,tz+.075]};
  }
  return null;
}
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
  constructor(serialApi,options={}){this.api=serialApi;this.timeout=options.timeout||5000;this.listeners=new Map();this.port=null;this.reader=null;this.pending=null;this.queue=Promise.resolve();this.state='disconnected';this.generation=0;this.closing=null;this.writeTask=null;this.writer=null;this.onDisconnect=e=>{if(e.target===this.port||e.port===this.port)this.disconnect(localize('设备已拔出','Device unplugged'));};this.api?.addEventListener?.('disconnect',this.onDisconnect);}
  on(name,fn){if(!this.listeners.has(name))this.listeners.set(name,new Set());this.listeners.get(name).add(fn);return()=>this.listeners.get(name).delete(fn);}
  emit(name,value){for(const fn of this.listeners.get(name)||[])fn(value);}
  get connected(){return this.state==='connected';}
  setState(state,detail=''){this.state=state;this.emit('state',{state,detail});}
  async connect(baudRate=921600){
    if(!this.api)throw new Error(localize('当前浏览器未提供 Web Serial，请用桌面版 Edge / Chrome 打开此页面。','This browser does not provide Web Serial. Use desktop Edge / Chrome to open this page.'));
    if(this.state!=='disconnected')throw new Error(localize('连接正在使用，请先断开。','Connection is in use. Disconnect first.'));
    this.setState('connecting');let port;
    try{port=await this.api.requestPort();await port.open({baudRate:Number(baudRate),dataBits:8,stopBits:1,parity:'none',flowControl:'none',bufferSize:65536});this.port=port;this.generation++;this.queue=Promise.resolve();this.framer=new LineFramer(line=>this.handleLine(line),()=>this.emit('warning',localize('串口文本行过长，已丢弃并重新同步。','Serial line too long; discarded and resynchronized.')));this.setState('connected');this.readTask=this.readLoop(port);return port.getInfo?.()||{};}
    catch(error){if(port)try{await port.close();}catch{}this.port=null;this.setState('disconnected',error.name==='NotFoundError'?localize('已取消选择串口','Serial port selection canceled'):error.message);throw error;}
  }
  async readLoop(port){
    let fault;
    try{this.reader=port.readable.getReader();while(this.connected&&this.port===port){const{value,done}=await this.reader.read();if(done){if(this.connected)fault=new Error(localize('串口数据流已关闭','Serial data stream closed'));break;}if(value)this.framer.push(value);}}
    catch(error){if(this.connected)fault=error;}
    finally{try{this.reader?.releaseLock();}catch{}this.reader=null;}
    // Schedule after this task resolves, so disconnect can safely await it.
    if(fault)queueMicrotask(()=>this.disconnect(localize('串口读取中断：','Serial read interrupted: ')+fault.message));
  }
  async disconnect(reason=localize('串口已断开','Serial port disconnected')){
    if(this.closing)return this.closing;if(this.state==='disconnected')return;
    this.setState('disconnecting',reason);this.generation++;this.pending?.reject(new Error(reason));
    this.closing=(async()=>{try{await this.reader?.cancel();}catch{}await this.readTask;try{await this.writer?.abort(reason);}catch{}try{await this.writeTask;}catch{}try{await this.port?.close();}catch(error){this.emit('warning',localize('关闭串口：','Closing serial port: ')+error.message);}this.port=null;this.reader=null;this.readTask=null;this.setState('disconnected',reason);})();
    try{await this.closing;}finally{this.closing=null;}
  }
  handleLine(line){const event=parseLine(line);event.raw=line;this.emit('line',event);if(event.kind==='sample'){if(event.channel===0)this.emit('sample',event.values);}else if(event.kind==='invalid')this.emit('warning',localize('丢弃格式无效的采样行：','Discarded malformed sample: ')+event.line);if(event.line==='System_Reset'&&this.pending){this.pending.reject(new Error(localize('设备在命令执行期间重启，请重新连接。','Device restarted during a command. Please reconnect.')));void this.disconnect(localize('设备已重启','Device restarted'));return;}this.pending?.accept(event);}
  transaction(fn){const generation=this.generation;const result=this.queue.then(()=>{if(!this.connected||this.generation!==generation)throw new Error(localize('串口未连接或连接已改变。','Serial port is not connected or the session changed.'));return fn();});this.queue=result.catch(()=>{});return result;}
  async request(command,match,timeout=this.timeout){
    if(!this.connected)throw new Error(localize('串口未连接。','Serial port is not connected.'));if(command.length>62||/[\r\n]/.test(command))throw new Error(localize('无效的串口命令。','Invalid serial command.'));
    let settle;const answer=new Promise((resolve,reject)=>{let timer,received=0,lastReply='';const finish=(error,value)=>{if(this.pending!==pending)return;clearTimeout(timer);this.pending=null;error?reject(error):resolve(value);};const pending={reject:error=>finish(error),accept:event=>{received++;if(event.kind!=='sample')lastReply=event.line.slice(0,120);try{const result=match(event);if(result!==undefined)finish(null,result);}catch(error){finish(error);}}};this.pending=pending;settle=pending;timer=setTimeout(()=>{const diagnostic=lastReply?localize(`最后回复：${lastReply}`,`Last reply: ${lastReply}`):received?localize('收到采样数据，但未收到命令确认。','Samples received, but no command acknowledgement.'):localize('等待期间未收到完整串口数据，请检查端口、波特率及设备状态。','No complete serial data received. Check the port, baud rate and device state.');const reason=localize(`命令超时：${command}（${timeout/1000} 秒）。${diagnostic} 连接已关闭，请重新连接。`,`Command timed out: ${command}(${timeout/1000} seconds). ${diagnostic} Connection closed. Please reconnect.`);finish(new Error(reason));void this.disconnect(reason);},timeout);});
    // Register the matcher before writing: a fast device can reply immediately.
    answer.catch(()=>{});
    this.emit('tx',command);
    try{const writer=this.port.writable.getWriter();this.writer=writer;this.writeTask=(async()=>{try{await writer.write(new TextEncoder().encode(command+'\n'));}finally{writer.releaseLock();if(this.writer===writer)this.writer=null;}})();await this.writeTask;}
    catch(error){settle.reject(new Error(localize('串口发送失败：','Serial transmission failed: ')+error.message));void this.disconnect(localize('串口发送失败','Serial transmission failed'));}
    return answer;
  }
  ack(command,success,failure,timeout){return this.request(command,e=>{if(failure.test(e.line)||/^Error:|^Unknown(?: command)?:/.test(e.line))throw new Error(e.line);if(success.test(e.line))return true;},timeout);}
  start(){return this.transaction(()=>this.ack('ML START',/^Sensor started(?: successfully)?$/, /Failed to start|Sensor start failed|^Start failed/i,15000));}
  stop(){return this.transaction(()=>this.ack('ML STOP',/^(Sensor stopped|Stopped)$/, /Failed to stop|Stop failed/i));}
  calibrate(){return this.transaction(()=>this.ack('ML CALIB',/^Calibration (completed|OK)$/, /Calibration fail|calibrate failed|not running|please start/i,15000));}
  reset(){return this.transaction(()=>this.ack('ML RESET',/^System resetting\.\.\.$/, /Reset failed/i));}
  async readConfigDirect(){const flags=await this.request('ML FLAG',e=>{if(e.kind==='flag')return e.value;if(/^Error:|^Unknown/.test(e.line))throw new Error(e.line);});const maps={};let begun=false;await this.request('ML MAPPING',e=>{if(/^Error:|^Unknown/.test(e.line))throw new Error(e.line);if(e.kind==='maps'){['X','Y','Z'].forEach((a,i)=>maps[a]=e.values[i]);return true;}if(e.line==='Mapping values:')begun=true;if(begun&&e.kind==='axis'){maps[e.axis]=e.value;if(['X','Y','Z'].every(a=>Number.isFinite(maps[a])))return true;}});return validateConfig({flags,maps:['X','Y','Z'].map(a=>maps[a])});}
  readConfig(){return this.transaction(()=>this.readConfigDirect());}
  async readFullConfigDirect(){
    let begun=false,payload='';
    try{return await this.request('ML GETCFG',e=>{
      if(/^Unknown|^Error:/.test(e.line))throw new Error(localize('设备不支持完整配置读取，请更新固件。','Full configuration readback is unsupported. Update the firmware.'));
      if(e.line==='CFG_BEGIN'){if(begun)throw new Error(localize('配置回包重复开始。','Duplicate configuration start marker.'));begun=true;return;}
      if(e.line==='CFG_END'){
        if(!begun)throw new Error(localize('配置回包缺少开始标记。','Configuration response is missing its start marker.'));
        const prepared=prepareImport(payload);
        if(prepared.missing.length)throw new Error(localize('设备返回的配置缺少字段：','Device configuration is missing fields: ')+prepared.missing.join(', '));
        return prepared.config;
      }
      if(e.kind==='sample'||/^\s*\[[EWID]\]/.test(e.raw||''))return;
      if(begun){payload+=e.line;if(payload.length>1023)throw new Error(localize('设备配置回包过长。','Device configuration response is too long.'));}
      else if(e.line.startsWith('{'))throw new Error(localize('设备使用旧版配置回包，请更新固件后读取完整配置。','Device uses an older configuration response. Update the firmware for full readback.'));
    });}catch(error){throw new Error(localize('完整配置读取失败：','Full configuration read failed: ')+error.message);}
  }
  readFullConfig(){return this.transaction(()=>this.readFullConfigDirect());}
  resetImport({allowUnsupported=false}={}){return this.transaction(()=>this.request('ML IMPORT_CANCEL',e=>{
    if(e.line==='IMPORT_CANCELLED')return true;
    if(/^Unknown(?: command)?:/.test(e.line)){if(allowUnsupported)return false;throw new Error(localize('当前固件不支持配置导入，请更新固件。','The firmware does not support configuration import. Update the firmware.'));}
    if(/^IMPORT_ERROR:|^Error:/.test(e.line))throw new Error(e.line);
  }));}
  async readLogLevelDirect(){return this.request('ML LOGLEVEL',e=>{const m=/^LogLevel=([0-4])(?:\s|$)/.exec(e.line);if(m)return Number(m[1]);if(/^Unknown|^Error:/.test(e.line))throw new Error(e.line);});}
  readLogLevel(){return this.transaction(()=>this.readLogLevelDirect());}
  writeConfig(input){const config=validateConfig(input);return this.transaction(async()=>{
    const commands=configCommands(config);for(let i=0;i<commands.length;i++){
      let saved=false,updated=false,saveError=false;
      await this.request(commands[i],e=>{
        if(/^Error:|^Unknown|^Out of range|^Usage:/.test(e.line))throw new Error(e.line);
        if(/Config save FAILED/.test(e.line))throw new Error(localize('Flash 保存失败。','Flash save failed.'));
        if(i===0&&/^FLAG_RES=0x[\dA-F]{2}$/i.test(e.line))return true;
        if(i>0&&new RegExp('^'+['','X','Y','Z'][i]+'='+NUMBER+'$').test(e.line))return true;
        if(e.line==='Failed to save config to Flash'){saveError=true;if(i===0)throw new Error(localize('Flash 保存失败，设备内存参数可能已改变；请读取配置核对。','Flash save failed. Device memory may have changed; read back the configuration to verify.'));}
        if(e.line==='Config saved to Flash:')saved=true;
        if(i===0&&/^FLAG_RES updated:/.test(e.line))updated=true;
        if(i===0&&updated&&saved&&e.kind==='maps')return true;
        if(i>0&&e.line.startsWith(['','X','Y','Z'][i]+'_MAPPING_VALUE updated:')){if(saveError||!saved)throw new Error(localize('设备未确认 Flash 保存，参数可能仅在内存中改变。','Device did not confirm Flash save; parameters may have changed only in memory.'));return true;}
      });
    }
    const actual=await this.readConfigDirect();if(actual.flags!==config.flags||actual.maps.some((v,i)=>!readbackEqual(v,config.maps[i])))throw new Error(localize('设备读回配置与期望值不一致，请重新读取并核对。','Device readback differs from the expected configuration. Read and verify again.'));return actual;
  });}
  importConfig(input,{onProgress=()=>{},signal,startAfter=false}={}){
    const prepared=prepareImport(input);
    return this.transaction(async()=>{
      let applied=false,ended=false;
      const checkAbort=()=>{if(signal?.aborted){const error=new Error(localize('配置传输已取消，未提交应用。','Configuration transfer canceled before applying.'));error.name='AbortError';throw error;}};
      const exchange=(command,match,timeout)=>this.request(command,e=>{if(/^IMPORT_ERROR:|^Unknown|^Error:/.test(e.line))throw new Error(e.line);return match(e.line);},timeout);
      const reset=()=>exchange('ML IMPORT_CANCEL',line=>line==='IMPORT_CANCELLED'?true:undefined);
      try{
        checkAbort();onProgress({stage:'prepare',sent:0,total:prepared.bytes});
        await reset();checkAbort();
        // Check readback support before any configuration write (older firmware
        // emits a truncated GETCFG response and cannot verify the import).
        await this.readFullConfigDirect();checkAbort();
        await exchange('ML IMPORT_START '+prepared.bytes,line=>line==='READY'?true:undefined);
        let sent=0;
        for(const chunk of prepared.chunks){checkAbort();sent+=chunk.length;await exchange('ML IMPORT_DATA '+chunk,line=>{const m=/^ACK_(\d+)$/.exec(line);if(m){if(Number(m[1])!==sent)throw new Error(localize(`分块确认字节数不一致：预期 ${sent}，收到 ${m[1]}。`,`Chunk acknowledgement mismatch: expected ${sent}, received ${m[1]}.`));return true;}});onProgress({stage:'sending',sent,total:prepared.bytes});}
        checkAbort();ended=true;onProgress({stage:'applying',sent,total:prepared.bytes});
        await exchange('ML IMPORT_END',line=>/^IMPORT_OK:/.test(line)?true:undefined,20000);applied=true;
        // Firmware retains COMPLETE/ERROR state until CANCEL, including after success.
        await reset();
        if(startAfter){onProgress({stage:'starting',sent,total:prepared.bytes});await this.ack('ML START',/^Sensor started(?: successfully)?$/, /Failed to start|Sensor start failed|^Start failed/i,15000);}
        onProgress({stage:'verifying',sent,total:prepared.bytes});
        const actual=await this.readFullConfigDirect(),expected=prepared.config;
        if(actual.product!==expected.product)throw new Error(localize('配置读回不一致：product。','Configuration readback mismatch: product.'));
        for(const [group,fields]of Object.entries(IMPORT_FIELDS))for(const key of Object.keys(fields))if(Math.fround(actual[group][key])!==Math.fround(expected[group][key]))throw new Error(localize(`配置读回不一致：${group}.${key}。`,`Configuration readback mismatch: ${group}.${key}.`));
        const runtime={flags:actual.algo.flag,maps:[actual.algo.xmap,actual.algo.ymap,actual.algo.zmap]};
        onProgress({stage:'done',sent,total:prepared.bytes});return{config:actual,runtime,logLevel:actual.log.level,verification:'full-config',started:startAfter};
      }catch(error){
        if(this.connected){try{await reset();}catch(cleanup){error.message+=localize('；退出导入状态失败：','; failed to exit import mode: ')+cleanup.message;}}
        if(applied)error.message=localize('设备已确认应用并保存，但后续启动或校验未完成：','Device confirmed application and save, but startup or verification is incomplete: ')+error.message;
        else if(ended)error.message=localize('已提交应用，设备状态需核对：','Application submitted; verify the device state: ')+error.message;
        error.applied=applied;error.submitted=ended;throw error;
      }
    });
  }
}
function readbackEqual(actual,expected){return Math.abs(actual-Math.fround(expected))<=Math.max(.00051,Math.abs(expected)*1e-6);}
const IMPORT_DEFAULTS={product:'MA02',sensor:{type:1,gain:5,osr:4,filt:0,resx:0,resy:0,resz:0,tcmp:0,range:0,opmode:2,magch:7,version:0},algo:{flag:0,xmap:1,ymap:1,zmap:1,dead:8.5,spike:20,stab:.1},log:{level:4}};
function productDefaults(product='MA02'){
  if(product!=='MA01'&&product!=='MA02')throw new Error(localize('product 产品型号必须是 MA01（MLX90393）或 MA02（TMAG3001）。','Product model must be MA01(MLX90393) or MA02(TMAG3001).'));
  const config=JSON.parse(JSON.stringify(IMPORT_DEFAULTS));config.product=product;
  if(product==='MA01'){Object.assign(config.sensor,{type:0,gain:7,osr:2,opmode:0});Object.assign(config.algo,{flag:123,xmap:.1,ymap:.1,zmap:-.1});}
  return config;
}
function sensorFieldActive(product,key){
  if(['type','gain','osr'].includes(key))return true;
  return(product==='MA01'?['filt','resx','resy','resz','tcmp']:['range','opmode','magch','version']).includes(key);
}
const IMPORT_FIELDS={sensor:{type:[localize('传感器类型','Sensor type'),0,1],gain:[localize('增益档位','Gain setting'),0,15],osr:[localize('过采样档位','Oversampling setting'),0,5],filt:[localize('数字滤波等级','Digital filter level'),0,7],resx:[localize('X 轴分辨率','X axis resolution'),0,3],resy:[localize('Y 轴分辨率','Y axis resolution'),0,3],resz:[localize('Z 轴分辨率','Z axis resolution'),0,3],tcmp:[localize('温度补偿','Temperature compensation'),0,1],range:[localize('磁场量程','Magnetic field range'),0,1],opmode:[localize('工作模式','Operating mode'),0,3],magch:[localize('磁场测量通道','Magnetic measurement channels'),0,7],version:[localize('传感器版本','Sensor version'),0,1]},algo:{flag:[localize('算法功能标志','Algorithm flags'),0,255],xmap:[localize('X 轴映射系数','X axis mapping factor')],ymap:[localize('Y 轴映射系数','Y axis mapping factor')],zmap:[localize('Z 轴映射系数','Z axis mapping factor')],dead:[localize('死区倍数','Dead-zone multiplier')],spike:[localize('尖峰过滤倍数','Spike filter multiplier')],stab:[localize('稳定性阈值','Stability threshold')]},log:{level:[localize('日志等级','Log level'),0,4]}};
const ALIASES={sensor:{sensor_type:'type',gain_sel:'gain',dig_filt:'filt',res_x:'resx',res_y:'resy',res_z:'resz',tcmp_en:'tcmp',op_mode:'opmode',mode:'opmode',mag_ch:'magch',channel:'magch',sensor_version:'version'},algo:{FLAG_RES:'flag',flag_res:'flag',X_MAPPING_VALUE:'xmap',Y_MAPPING_VALUE:'ymap',Z_MAPPING_VALUE:'zmap',x_map:'xmap',y_map:'ymap',z_map:'zmap',dead_value:'dead',spike_multiplier:'spike',stability_threshold:'stab',stability:'stab'},log:{}};
function normalizeSensorType(value){
  if(typeof value==='string'){
    const name=value.trim().toUpperCase();
    if(name==='TMAG3001')return 1;
    if(name==='MLX90393')return 0;
  }
  return value;
}
function prepareImport(input){
  let obj=input;
  if(typeof input==='string'){if(input.length>65536)throw new Error(localize('配置文件不能超过 64 KB。','Configuration file must not exceed 64 KB.'));try{obj=JSON.parse(input.replace(/^\uFEFF/,''));}catch(e){throw new Error(localize('JSON 格式错误：','JSON syntax error: ')+e.message);}}
  const plain=v=>v!==null&&typeof v==='object'&&!Array.isArray(v);
  if(!plain(obj))throw new Error(localize('配置顶层必须是 JSON 对象。','Configuration root must be a JSON object.'));
  const config=productDefaults(Object.hasOwn(obj,'product')?obj.product:IMPORT_DEFAULTS.product),seen=new Set(),groups=new Set();
  for(const[group,value]of Object.entries(obj)){
    if(group==='$schema'){if(value!=='sensor_config_v1')throw new Error(localize('不支持的 $schema。','Unsupported $schema.'));continue;}
    if(group==='product'){
      if(typeof value!=='string'||!/^[A-Za-z0-9_.-]{1,7}$/.test(value))throw new Error(localize('product 产品型号须为 1–7 个字母、数字、下划线、点或短横线。','Product model must contain 1–7 letters, digits, underscores, dots or hyphens.'));
      config.product=value;seen.add('product');continue;
    }
    const section=group==='algorithm'?'algo':group;
    if(!Object.hasOwn(IMPORT_FIELDS,section))throw new Error(localize('未知配置分组：','Unknown configuration section: ')+group);
    if(groups.has(section))throw new Error(localize('配置分组重复：','Duplicate configuration section: ')+section);groups.add(section);
    if(!plain(value))throw new Error(group+localize(' 必须是对象。',' must be an object.'));
    for(const[key,raw]of Object.entries(value)){
      const name=Object.hasOwn(ALIASES[section],key)?ALIASES[section][key]:key,field=section+'.'+name;
      if(!Object.hasOwn(IMPORT_FIELDS[section],name))throw new Error(localize('未知配置字段：','Unknown configuration field: ')+group+'.'+key);
      if(seen.has(field))throw new Error(localize('同一字段使用了重复别名：','Duplicate aliases for the same field: ')+field);seen.add(field);
      let n=field==='algo.flag'&&typeof raw==='string'&&/^0x[\da-f]{1,2}$/i.test(raw)?parseInt(raw,16):raw;
      if(field==='sensor.type')n=normalizeSensorType(n);
      const rule=IMPORT_FIELDS[section][name];
      if(typeof n!=='number'||!Number.isFinite(n)||!Number.isFinite(Math.fround(n)))throw new Error(field+localize(' 必须是有限 float32 数值。',' must be a finite float32 value.'));
      if(rule.length===3&&(!Number.isInteger(n)||n<rule[1]||n>rule[2]))throw new Error(localize(`${field} 必须是 ${rule[1]}–${rule[2]} 的整数。`,`${field} must be an integer in ${rule[1]}–${rule[2]}.`));
      config[section][name]=n;
    }
  }
  if(!seen.size)throw new Error(localize('配置文件未包含任何可用参数。','Configuration contains no usable parameters.'));
  const expectedType=config.product==='MA01'?0:1;
  if(config.sensor.type!==expectedType)throw new Error(localize(`sensor.type 与产品型号不一致：${config.product} 必须为 ${expectedType===0?'MLX90393（0）':'TMAG3001（1）'}。`,`sensor.type does not match the product: ${config.product} must be ${expectedType===0?'MLX90393(0)':'TMAG3001(1)'}.`));
  if(config.product==='MA01'&&(config.sensor.gain>7||config.sensor.osr>3))throw new Error(localize('MA01 的 sensor.gain 必须为 0–7，sensor.osr 必须为 0–3。','MA01 sensor.gain must be 0–7, sensor.osr must be 0–3.'));
  const missing=seen.has('product')?[]:['product'];for(const[group,fields]of Object.entries(IMPORT_FIELDS))for(const key of Object.keys(fields))if(!seen.has(group+'.'+key))missing.push(group+'.'+key);
  const wire=JSON.stringify({...config,algo:{...config.algo,flag:'0x'+config.algo.flag.toString(16).toUpperCase().padStart(2,'0')}}),bytes=new TextEncoder().encode(wire).length;
  if(bytes>1023)throw new Error(localize('规范化配置超过 1023 字节，无法导入。','Normalized configuration exceeds 1023 bytes; import unavailable.'));
  // All canonical keys/values are ASCII. 15-byte prefix + 47-byte chunk = 62.
  const chunks=wire.match(/.{1,47}/g)||[];
  return{config,wire,bytes,chunks,missing};
}
return{parseLine,LineFramer,SampleBuffer,validateConfig,configCommands,csv,SerialLog,percentile95,vectorStyle,surfaceHeight,surfaceArrow,arrowMesh,SerialClient,prepareImport,IMPORT_DEFAULTS,IMPORT_FIELDS,productDefaults,sensorFieldActive,readbackEqual};
});
