const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const C=require('../core.js');

function firmware(options={}){
  let rx,state=options.initialState||'idle',text='',cfg=structuredClone(C.IMPORT_DEFAULTS),closed=false;
  const commands=[],encoder=new TextEncoder(),port={
    readable:new ReadableStream({start(controller){rx=controller;}}),
    writable:new WritableStream({write(bytes){
      const command=new TextDecoder().decode(bytes).trimEnd();commands.push(command);assert(command.length<=62);
      let response='';
      if(command==='ML IMPORT_CANCEL'){state='idle';text='';response=options.oldFirmware?'Unknown: ML IMPORT_CANCEL':'IMPORT_CANCELLED';}
      else if(command.startsWith('ML IMPORT_START ')){state='receiving';response='READY';}
      else if(command.startsWith('ML IMPORT_DATA ')){assert.equal(state,'receiving');text+=command.slice(15);response='ACK_'+(text.length+(options.badAck?1:0));}
      else if(command==='ML IMPORT_END'){assert.equal(state,'receiving');cfg=C.prepareImport(text).config;state='complete';response=options.flashFail?'IMPORT_ERROR: save to flash failed':'IMPORT_OK: config applied and saved';}
      else if(state!=='idle')return; // main.c consumes all normal commands until CANCEL.
      else if(command==='ML FLAG')response='FLAG_RES: 0x'+cfg.algo.flag.toString(16);
      else if(command==='ML MAPPING')response=`X=${cfg.algo.xmap.toFixed(3)} Y=${cfg.algo.ymap.toFixed(3)} Z=${cfg.algo.zmap.toFixed(3)}`;
      else if(command==='ML LOGLEVEL')response=`LogLevel=${options.badReadback?0:cfg.log.level} (0=NONE 1=E 2=W 3=I 4=D)`;
      else if(command==='ML GETCFG'){
        const snapshot=structuredClone(cfg);
        if(options.badReadback)snapshot.log.level=0;
        if(options.badSensor)snapshot.sensor.gain=0;
        if(options.badThreshold)snapshot.algo.dead=1;
        if(options.missingField)delete snapshot.log;
        response=options.legacy?JSON.stringify(snapshot).slice(0,99):options.unsupported?'Unknown: ML GETCFG':
          'CFG_BEGIN\r\n[I][SYS] reading\r\n'+JSON.stringify(snapshot)+'\r\n'+(options.noEnd?'':'CFG_END');
      }
      else if(command==='ML START')response=options.startFail?'Failed to start':'Sensor started';
      else if(command==='ML STOP')response='Stopped';
      else if(command==='ML CALIB')response='Calibration OK';
      const output=encoder.encode('[I][DATA] sensor0:0.25, -0.5, 1.25\r\n'+response+'\r\n');
      queueMicrotask(()=>{if(!closed)for(let i=0;i<output.length;i+=5)rx.enqueue(output.slice(i,i+5));});
    }}),async open(){},async close(){closed=true;},getInfo(){return{};}
  };
  return{client:new C.SerialClient({requestPort:async()=>port},{timeout:100}),commands,get state(){return state;},get config(){return cfg;}};
}

test('all repository JSON examples normalize to the import schema',()=>{
  const repo=path.resolve(__dirname,'../../..');
  for(const filename of ['firmware/config_example.json','firmware/config_custom.json','firmware/config_compact.json','tools/sensor_config.json']){
    const p=C.prepareImport(fs.readFileSync(path.join(repo,filename),'utf8'));assert(p.bytes<1024);assert.equal(p.chunks.join(''),p.wire);assert(p.chunks.every(c=>c.length<=47));assert.deepEqual(C.prepareImport(p.wire).config,p.config);
  }
});
test('validation rejects unknown fields, wrong types, out of range, duplicate aliases and unsafe floats',()=>{
  for(const input of ['{broken','[]','null','{}','{"sensor":{"gain":8}}','{"sensor":{"osr":1.5}}','{"log":{"level":"3"}}','{"algo":{"flag":"0x100"}}','{"sensor":{"typo":1}}','{"algo":{"xmap":1e100}}','{"sensor":{"resx":1,"res_x":2}}','{"algo":{},"algorithm":{"flag":1}}'])assert.throws(()=>C.prepareImport(input),undefined,input);
  assert.throws(()=>C.prepareImport({algo:{xmap:Infinity}}));
  assert.throws(()=>C.prepareImport(' '.repeat(65537)));
});
test('missing fields use firmware defaults and BOM plus aliases are supported',()=>{
  const p=C.prepareImport('\uFEFF{"algo":{"FLAG_RES":"0X1F","X_MAPPING_VALUE":0.123456},"log":{"level":0}}');
  assert.equal(p.config.algo.flag,31);assert.equal(p.config.algo.xmap,.123456);assert.equal(p.config.sensor.gain,7);assert(p.missing.includes('sensor.gain'));assert(!p.missing.includes('log.level'));
});
test('new log-prefixed sensor data and compact configuration replies parse correctly',()=>{
  assert.deepEqual(C.parseLine('[I][DATA] sensor0:0.12,-0.05,0.33').values,[.12,-.05,.33]);
  assert.deepEqual(C.parseLine('X=0.100 Y=0.200 Z=-0.300').values,[.1,.2,-.3]);
});
test('full import waits for each ACK, exits COMPLETE, starts and verifies runtime fields',async()=>{
  const f=firmware(),progress=[];await f.client.connect();let samples=0;f.client.on('sample',()=>samples++);
  const result=await f.client.importConfig({sensor:{gain:5},algo:{xmap:.15},log:{level:4}},{startAfter:true,onProgress:p=>progress.push(p.stage)});
  assert.equal(result.config.sensor.gain,5);assert.equal(result.logLevel,4);assert.equal(result.verification,'full-config');assert.equal(f.state,'idle');
  const end=f.commands.indexOf('ML IMPORT_END');assert.deepEqual(f.commands.slice(end),['ML IMPORT_END','ML IMPORT_CANCEL','ML START','ML GETCFG']);assert(samples>5);assert.equal(progress.at(-1),'done');await f.client.disconnect();
});
test('mismatched ACK aborts before END and resets import state',async()=>{
  const f=firmware({badAck:true});await f.client.connect();await assert.rejects(f.client.importConfig(C.IMPORT_DEFAULTS),/字节数不一致/);assert(!f.commands.includes('ML IMPORT_END'));assert.equal(f.commands.at(-1),'ML IMPORT_CANCEL');assert.equal(f.state,'idle');await f.client.disconnect();
});
test('user cancellation waits for current chunk and does not apply',async()=>{
  const f=firmware(),abort=new AbortController();await f.client.connect();await assert.rejects(f.client.importConfig(C.IMPORT_DEFAULTS,{signal:abort.signal,onProgress:p=>{if(p.stage==='sending')abort.abort();}}),{name:'AbortError'});assert(!f.commands.includes('ML IMPORT_END'));assert.equal(f.state,'idle');await f.client.disconnect();
});
test('Flash failure reports uncertain device state and resets ERROR state',async()=>{
  const f=firmware({flashFail:true});await f.client.connect();await assert.rejects(f.client.importConfig(C.IMPORT_DEFAULTS),error=>error.submitted===true&&/save to flash failed/.test(error.message));assert.equal(f.state,'idle');await f.client.disconnect();
});
test('readback or start failure after IMPORT_OK is not reported as success',async()=>{
  for(const options of [{badReadback:true},{badSensor:true},{badThreshold:true},{startFail:true}]){const f=firmware(options);await f.client.connect();await assert.rejects(f.client.importConfig(C.IMPORT_DEFAULTS,{startAfter:true}),error=>error.applied===true);assert.equal(f.state,'idle');await f.client.disconnect();}
});
test('current firmware start/stop/calibration acknowledgements work',async()=>{
  const f=firmware();await f.client.connect();await f.client.start();await f.client.stop();await f.client.calibrate();await f.client.disconnect();
});

test('reconnection can clear an interrupted import before automatic config read',async()=>{
  const f=firmware({initialState:'receiving'});await f.client.connect();await f.client.resetImport();
  const actual=await f.client.readFullConfig();assert.equal(actual.algo.flag,59);assert.equal(actual.log.level,3);
  assert.equal(f.commands[0],'ML IMPORT_CANCEL');assert.equal(f.state,'idle');await f.client.disconnect();
});

test('full snapshot reads every field amid split serial traffic without writes',async()=>{
  const f=firmware();await f.client.connect();assert.deepEqual(await f.client.readFullConfig(),C.IMPORT_DEFAULTS);
  assert.deepEqual(f.commands,['ML GETCFG']);await f.client.disconnect();
});
test('missing fields, legacy replies and unsupported readback fail before import writes',async()=>{
  for(const options of [{missingField:true},{legacy:true},{unsupported:true}]){
    const f=firmware(options);await f.client.connect();await assert.rejects(f.client.importConfig(C.IMPORT_DEFAULTS),/完整配置读取失败/);
    assert(!f.commands.some(c=>c.startsWith('ML IMPORT_START')));await f.client.disconnect();
  }
});
test('missing snapshot end marker times out instead of accepting partial data',async()=>{
  const f=firmware({noEnd:true});await f.client.connect();await assert.rejects(f.client.readFullConfig(),/超时/);await f.client.disconnect();
});
test('full configuration round-trip preserves small and precise float32 values',async()=>{
  const f=firmware();await f.client.connect();
  const result=await f.client.importConfig({algo:{xmap:1e-8,stab:.123456789}});
  assert.equal(Math.fround(result.config.algo.stab),Math.fround(.123456789));await f.client.disconnect();
});

test('old firmware rejects import reset immediately but remains usable for monitoring',async()=>{
  const f=firmware({oldFirmware:true,legacy:true});await f.client.connect();
  assert.equal(await f.client.resetImport({allowUnsupported:true}),false);
  await assert.rejects(f.client.readFullConfig(),/旧版配置回包/);
  assert.equal(f.client.connected,true);assert.equal((await f.client.readConfig()).flags,59);
  await assert.rejects(f.client.resetImport(),/不支持配置导入/);assert.equal(f.client.connected,true);
  await f.client.disconnect();
});
