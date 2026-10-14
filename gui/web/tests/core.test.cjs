const {test}=require('node:test');
const assert=require('node:assert/strict');
const C=require('../core.js');

test('serial log retains full RX text and all sessions beyond UI preview limits',()=>{
  const archive=new C.SerialLog(),start=Date.UTC(2026,9,13,18,0,0);
  archive.append('系统','未连接的演示事件',start);assert.equal(archive.count,0);
  archive.begin('COM4',115200,start);
  for(let i=0;i<600;i++)archive.append('RX',`[I][DATA] sensor0:${i},0,0`,start+i);
  archive.append('TX','ML GETCFG',start+601);archive.end('连接中断',start+602);
  const first=archive.snapshot().join('');assert(first.startsWith('\uFEFF'));assert(first.includes('[RX] [I][DATA] sensor0:0,0,0'));assert(first.includes('sensor0:599,0,0'));assert(first.includes('[TX] ML GETCFG'));
  archive.begin('COM5',57600,start+603);archive.append('错误','第一行\n第二行',start+604);
  const second=archive.snapshot().join('');assert(second.includes('会话 1 · COM4'));assert(second.includes('会话 2 · COM5'));assert(second.includes('[错误] 第二行'));
  assert(!first.includes('COM5'));assert.equal(archive.count,605);assert(second.indexOf('连接中断')<second.indexOf('COM5'));
});

test('surface deformation retains Z sign, XY tilt, fixed edges and zero baseline',()=>{
  assert.equal(C.surfaceHeight(0,0,[0,0,1],1),1.55);
  assert.equal(C.surfaceHeight(0,0,[0,0,-1],1),-1.55);
  assert(C.surfaceHeight(-1,0,[1,0,0],1)>0);
  assert(C.surfaceHeight(1,0,[1,0,0],1)<0);
  for(const x of [-2.5,-1,0,1,2.5])for(const y of [-2.5,-1,0,1,2.5]){
    assert.equal(C.surfaceHeight(x,y,[0,0,0],1),0);
    const z=C.surfaceHeight(x,y,[.4,-.3,.8],1);
    assert(Number.isFinite(z));if(Math.abs(x)===2.5||Math.abs(y)===2.5)assert.equal(z,0);
  }
});

test('surface arrows converge on a depression even without any XY input',()=>{
  for(const [x,y]of [[1,0],[-1,0],[0,1],[0,-1],[1,1],[-1,-1]]){
    const arrow=C.surfaceArrow(x,y,[0,0,-1],1);assert(arrow);
    assert(Math.hypot(arrow.end[0],arrow.end[1])<Math.hypot(x,y));
    assert(arrow.end[2]<arrow.start[2]);
  }
  assert.equal(C.surfaceArrow(0,0,[0,0,-1],1),null);
  assert.equal(C.surfaceArrow(1,1,[0,0,0],1),null);
});
test('local arrows stay downhill and inside the surface for tilted and raised shapes',()=>{
  for(const values of [[.6,-.8,-.3],[.6,-.8,.3],[0,0,1],[1,0,0]])
    for(let x=-2;x<=2;x+=.2)for(let y=-2;y<=2;y+=.2){
      const arrow=C.surfaceArrow(x,y,values,1);if(!arrow)continue;
      assert(arrow.end.every(Number.isFinite));assert(arrow.end[2]<arrow.start[2]);
      assert(Math.abs(arrow.end[0])<=2.5&&Math.abs(arrow.end[1])<=2.5);
    }
});

function mockDevice(options={}){
  const state={flags:59,maps:[.1,.1,-.1],commands:[],closed:false}, encoder=new TextEncoder();let controller;
  const port={readable:new ReadableStream({start(c){controller=c;}}),writable:new WritableStream({write(bytes){
    const command=new TextDecoder().decode(bytes).trim();state.commands.push(command);let answer='';
    if(options.ignore)return;
    if(command==='ML FLAG')answer=`FLAG_RES: 0x${state.flags.toString(16)}\r\n`;
    else if(command==='ML MAPPING')answer=`Mapping values:\r\n  X: ${state.maps[0].toFixed(3)}\r\n  Y: ${state.maps[1].toFixed(3)}\r\n  Z: ${state.maps[2].toFixed(3)}\r\n`;
    else if(command.startsWith('ML CHFLAG')){
      const old=state.flags;state.flags=Number(command.split(' ')[2]);
      answer=`FLAG_RES updated: 0x${old.toString(16)} -> 0x${state.flags.toString(16)}\r\n`+(options.flashFail?'Failed to save config to Flash\r\n':saved());
    }else if(command.startsWith('ML SETMAP')){
      const[, ,axis,value]=command.split(' ');state.maps['XYZ'.indexOf(axis)]=Number(value);
      answer=saved()+`${axis}_MAPPING_VALUE updated: ${Number(value).toFixed(3)}\r\n`;
    }else if(command==='ML CALIB')answer=options.calibFail?'Sensor not running, skip calibration.\r\nCalibration failed\r\n':'Calibrating sensors (10 samples)...\r\nCalibration complete! Sensor calibrated.\r\nCalibration completed\r\n';
    else if(command==='ML START')answer='Sensor started successfully\r\nRunning sensor: 0\r\n';
    else if(command==='ML STOP')answer='Sensor stopped\r\n';
    // Split every reply across arbitrary transport chunks, including CRLF.
    const bytesOut=encoder.encode('sensor0:0.2,-0.3,0.4\n'+answer);
    queueMicrotask(()=>{for(let i=0;i<bytesOut.length;i+=7)controller.enqueue(bytesOut.slice(i,i+7));});
    function saved(){return`Config saved to Flash:\r\n  FLAG_RES = 0x${state.flags.toString(16)}\r\n  Mapping: X=${state.maps[0].toFixed(2)}, Y=${state.maps[1].toFixed(2)}, Z=${state.maps[2].toFixed(2)}\r\n`;}
  }}),async open(){},async close(){state.closed=true;},getInfo(){return{};}};
  const api={requestPort:async()=>port};const client=new C.SerialClient(api,{timeout:options.timeout||300});
  return{client,state,inject(text){controller.enqueue(encoder.encode(text));}};
}

test('framing handles split CRLF, multiple lines, Unicode and overlength recovery',()=>{
  const lines=[];let overflow=0;const f=new C.LineFramer(l=>lines.push(l),()=>overflow++,24);
  const bytes=new TextEncoder().encode('中文\r\nsensor0:1,2,3\n');for(const b of bytes)f.push(Uint8Array.of(b));
  f.push('x'.repeat(25));f.push('sensor0:9,9,9\n');f.push('sensor0:4,5,6\r');f.push('\n');
  assert.deepEqual(lines,['中文','sensor0:1,2,3','sensor0:4,5,6']);assert.equal(overflow,1);
});
test('parser accepts signed floats and exponents, rejects malformed/nonfinite samples',()=>{
  assert.deepEqual(C.parseLine('sensor0: +1.2e-2, -.5, 3.').values,[.012,-.5,3]);
  for(const text of['sensor0:NaN,1,2','sensor0:1,2,3,4','sensor0:1e999,2,3'])assert.equal(C.parseLine(text).kind,'invalid');
  assert.equal(C.parseLine('FLAG_RES: 0xFB').value,251);
});
test('bounded ring buffer keeps chronological samples and counts all received data',()=>{
  const b=new C.SampleBuffer(3);for(let i=0;i<5;i++)b.push({t:i});assert.deepEqual(b.array(),[{t:2},{t:3},{t:4}]);assert.equal(b.total,5);assert.deepEqual(b.recent(1),[{t:3},{t:4}]);b.clear();assert.equal(b.at(-1),undefined);
});
test('queued commands read complete config amid fragmented live sensor traffic',async()=>{
  const {client,state}=mockDevice();let count=0;client.on('sample',()=>count++);await client.connect();
  const result=await Promise.all([client.readConfig(),client.stop(),client.start()]);
  assert.deepEqual(result[0],{flags:59,maps:[.1,.1,-.1]});assert.deepEqual(state.commands,['ML FLAG','ML MAPPING','ML STOP','ML START']);assert.equal(count,4);await client.disconnect();assert.equal(state.closed,true);
});
test('writes wait for Flash confirmations then verify all fields by readback',async()=>{
  const{client,state}=mockDevice();await client.connect();const expected={flags:251,maps:[.123,-.456,0]};assert.deepEqual(await client.writeConfig(expected),expected);
  assert.deepEqual(state.commands,['ML CHFLAG 0xFB','ML SETMAP X 0.123','ML SETMAP Y -0.456','ML SETMAP Z 0.000','ML FLAG','ML MAPPING']);await client.disconnect();
});
test('Flash failure is not treated as success and halts remaining writes',async()=>{
  const{client,state}=mockDevice({flashFail:true});await client.connect();await assert.rejects(client.writeConfig({flags:7,maps:[1,2,3]}),/Flash/);assert.equal(state.commands.length,1);await client.disconnect();
});
test('calibration uses final firmware acknowledgement and reports stopped sensor',async()=>{
  const good=mockDevice();await good.client.connect();assert.equal(await good.client.calibrate(),true);await good.client.disconnect();
  const bad=mockDevice({calibFail:true});await bad.client.connect();await assert.rejects(bad.client.calibrate(),/not running/);await bad.client.disconnect();
});
test('command timeout closes transport and invalidates queued commands',async()=>{
  const{client,state}=mockDevice({ignore:true,timeout:25});await client.connect();const results=await Promise.allSettled([client.readConfig(),client.stop()]);assert.equal(results[0].status,'rejected');assert.match(results[0].reason.message,/超时/);assert.equal(results[1].status,'rejected');assert.equal(state.commands.length,1);await client.disconnect();assert.equal(client.state,'disconnected');
});
test('disconnect immediately rejects pending command and releases reader',async()=>{
  const{client}=mockDevice({ignore:true});await client.connect();const pending=client.readConfig();await new Promise(r=>setImmediate(r));const check=assert.rejects(pending,/断开/);await client.disconnect();await check;assert.equal(client.reader,null);
});

test('timeout disconnect preserves the failing command and last reply for diagnosis',async()=>{
  const {client,inject}=mockDevice({ignore:true,timeout:30});await client.connect();let detail='';
  client.on('state',e=>{if(e.state==='disconnected')detail=e.detail;});
  const pending=client.readConfig();await new Promise(r=>setImmediate(r));inject('unexpected response\r\n');
  await assert.rejects(pending,/ML FLAG.*最后回复：unexpected response/);await client.disconnect();
  assert.match(detail,/ML FLAG.*最后回复：unexpected response/);
});
test('device restart while a command is pending cannot be accepted as a reply',async()=>{
  const{client,inject}=mockDevice({ignore:true});await client.connect();const pending=client.readConfig();await new Promise(r=>setImmediate(r));const check=assert.rejects(pending,/重启/);inject('System_Reset\r\n');await check;await client.disconnect();
});
test('vector plane follows reference XY inversion, magnitude P95 and Z color',()=>{
  const s=C.vectorStyle([3,4,-5],Array(100).fill(10));assert.deepEqual(s.direction,[-.6,-.8,0]);assert.equal(s.length,.18);assert.equal(s.zRatio,.5);assert.equal(s.reference,10);assert.deepEqual(C.vectorStyle([0,0,0],[]).direction,[0,0,0]);assert.equal(C.percentile95([0,10,20,30,40]),38);
});
test('arrow geometry stays finite for vertical, opposite and zero vectors',()=>{
  for(const end of[[0,0,1],[0,0,-1],[1,2,3]]){const g=C.arrowMesh([0,0,0],end);assert.deepEqual(g.vertices.at(-1),end);assert(g.vertices.flat().every(Number.isFinite));assert(g.faces.flat().every(i=>i<g.vertices.length));}assert.equal(C.arrowMesh([0,0,0],[0,0,0]).faces.length,0);
});
test('configuration validation preserves reserved bits and rejects unsafe input',()=>{
  assert.equal(C.validateConfig({flags:251,maps:[.1234,0,-.1]}).flags,251);
  assert.throws(()=>C.validateConfig({flags:256,maps:[1,2,3]}));assert.throws(()=>C.validateConfig({flags:0,maps:[NaN,2,3]}));
});
test('CSV marks source, unit and acquisition timestamp for each sample',()=>{
  const text=C.csv([{t:1,v:[1,-2,3],time:0,source:'device',unit:'uT'}]);assert.match(text,/received_at,x,y,z,source,unit/);assert.match(text,/1970-01-01T00:00:00.000Z/);assert.match(text,/device,uT/);
});
