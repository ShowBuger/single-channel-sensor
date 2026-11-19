const {test}=require('node:test');
const assert=require('node:assert/strict');
const C=require('../core.js');

test('v2 sample parsing retains timing and reference diagnostics',()=>{
  const e=C.parseLine('S ch=0 seq=4294967295 t=1234 rate=100 valid=1 x=-1.2 y=3e-2 z=4 tr=1200 skew=34 timing=trigger err=0 rx=2 ry=3 rz=-4');
  assert.equal(e.kind,'sample');assert.equal(e.valid,true);
  assert.deepEqual(e.values,[-1.2,.03,4]);assert.equal(e.seq,0xFFFFFFFF);
  assert.equal(e.timestampUs,1234);assert.equal(e.fields.rz,'-4');
});
test('invalid v2 samples are counted without emitting zero into the signal',()=>{
  const client=new C.SerialClient({}),samples=[],invalid=[];
  client.on('sample',v=>samples.push(v));client.on('sample-invalid',v=>invalid.push(v));
  client.handleLine('S ch=0 seq=1 t=10 rate=100 valid=0 x=0 y=0 z=0 err=2');
  client.handleLine('S ch=0 seq=2 t=20 rate=100 valid=1 x=10 y=0 z=0 err=0');
  assert.equal(invalid.length,1);assert.deepEqual(samples,[[10,0,0]]);
});
test('malformed v2 metadata is rejected and unknown extension fields are accepted',()=>{
  const valid='S ch=0 seq=1 t=10 rate=100 valid=1 x=1 y=2 z=3';
  for(const bad of [valid+' x=4',valid.replace('seq=1','seq=-1'),valid.replace('valid=1','valid=2'),
                    valid.replace('rate=100','rate=0'),valid.replace('x=1','x=NaN'),valid.replace(' t=10','')]){
    assert.equal(C.parseLine(bad).kind,'invalid',bad);
  }
  assert.equal(C.parseLine(valid+' future=some-value').kind,'sample');
});
test('new algorithm configuration round-trips without adding fields to old files',()=>{
  const old=C.prepareImport({product:'MA01'});
  assert(!Object.hasOwn(JSON.parse(old.wire).algo,'profile'));
  assert.equal(C.prepareImport({product:'MA03'}).config.algo.profile,1);
  assert.throws(()=>C.prepareImport({product:'MA03',algo:{profile:'legacy'}}),/MA03/);
  const p=C.prepareImport({product:'MA03',algo:{profile:'v2',window_ms:500,tau_lp_ms:220,kx:1.2,ky:-1,kz:-1,spike_k:5}});
  assert.equal(p.config.algo.profile,1);assert.equal(p.config.algo.kx,1.2);
  assert.equal(C.prepareImport(p.wire).wire,p.wire);assert(p.bytes<=1023);
  for(const algo of [{profile:2},{window_ms:501},{tau_ms:0},{kx:101},{spike_k:0},{spike_hold_ms:10}]){
    assert.throws(()=>C.prepareImport({product:'MA03',algo}));
  }
});
test('v2 sample lines are excluded from the communication archive',()=>{
  const log=new C.SerialLog();log.begin('COM1',921600);
  const n=log.count;
  log.append('RX','S ch=0 seq=1 t=10 rate=100 valid=1 x=1 y=2 z=3');
  assert.equal(log.count,n);
});
