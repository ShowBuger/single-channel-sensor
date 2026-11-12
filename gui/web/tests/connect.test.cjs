const {test}=require('node:test');
const assert=require('node:assert/strict');
const C=require('../core.js');

// Minimal device: replays a stale, truncated ST-Link buffer right after open
// (measured on hardware: lines lose their tail, e.g. "sensor0:0.00,-77.55,0."),
// then answers commands on their own lines.
function staleDevice({version=null,staleDelayMs=40}={}){
  const enc=new TextEncoder();const commands=[];let controller;
  const port={
    readable:new ReadableStream({start(c){controller=c;}}),
    writable:new WritableStream({write(bytes){
      const command=new TextDecoder().decode(bytes).trim();commands.push(command);let reply;
      if(command==='ML IMPORT_CANCEL')reply='IMPORT_CANCELLED';
      else if(command==='ML VERSION')reply=version??'Unknown: ML VERSION';
      else reply='Unknown: '+command;
      queueMicrotask(()=>controller.enqueue(enc.encode(reply+'\r\n')));
    }}),
    async open(){
      // stale replay: a complete old line, then a line cut mid-number (no CR/LF)
      setTimeout(()=>controller.enqueue(enc.encode('sensor0:0.00,-89.67,0.00\r\nsensor0:0.00,-77.55,0.')),staleDelayMs);
    },
    async close(){},getInfo(){return{};}
  };
  return{port,commands};
}

test('stale bytes after open are discarded, so the first reply is not glued to them',async()=>{
  const {port}=staleDevice();
  const lines=[];
  const client=new C.SerialClient({requestPort:async()=>port},{timeout:300,settleMs:150});
  client.on('line',e=>lines.push(e.raw));
  await client.connect();
  assert.equal(await client.resetImport({allowUnsupported:true}),true);
  assert.deepEqual(lines,['IMPORT_CANCELLED'],'no stale line may reach listeners or the request matcher');
  await client.disconnect();
});

test('without a settle window the same stale replay breaks the first command (regression guard)',async()=>{
  const {port}=staleDevice({staleDelayMs:0});
  const client=new C.SerialClient({requestPort:async()=>port},{timeout:120,settleMs:0});
  await client.connect();
  await new Promise(r=>setTimeout(r,20));            // stale bytes arrive before the command
  await assert.rejects(client.resetImport({allowUnsupported:true}),/超时|timed out/i);
});

test('readVersion parses the capability line and returns null on older firmware',async()=>{
  const line='VERSION proto=2 fw=1.1.0 board=ma03_h743 mcu=STM32H743 sensors=AK09974C channels=1 features=';
  let dev=staleDevice({version:line});
  let client=new C.SerialClient({requestPort:async()=>dev.port},{timeout:300,settleMs:100});
  await client.connect();
  const v=await client.readVersion();
  assert.equal(v.proto,'2');assert.equal(v.fw,'1.1.0');assert.equal(v.board,'ma03_h743');
  assert.equal(v.mcu,'STM32H743');assert.equal(v.sensors,'AK09974C');assert.equal(v.features,'');
  await client.disconnect();

  dev=staleDevice();
  client=new C.SerialClient({requestPort:async()=>dev.port},{timeout:300,settleMs:100});
  await client.connect();
  assert.equal(await client.readVersion(),null);
  assert.equal(client.state,'connected','old firmware must stay connected');
  await client.disconnect();
});
