const{test}=require('node:test');const assert=require('node:assert/strict');const M=require('../algorithm-model.js');
const line=(id=0,seq=1,extra='')=>`N seq=${seq} t=1234 p=1 s=${id} run=1 valid=1 err=0 x=1 y=-2.5 z=3e-2 rx=4 ry=5 rz=6${extra}`;
test('node parser rejects malformed values, duplicate metadata and invalid ranges',()=>{
  assert.deepEqual(M.parse(line()).actual,[1,-2.5,.03]);
  for(const bad of [line(10),line(1,-1),line(0,1,' x=2'),line().replace('x=1','x=NaN'),line().replace('run=1','run=2'),line().replace('p=1','p=3')])assert.equal(M.parse(bad),null,bad);
  assert.equal(M.parse('sensor0:1,2,3'),null);
});
test('frame assembly never mixes sequences, and keeps history bounded',()=>{
  const f=new M.Frames(2);f.push(M.parse(line(0,0)));
  for(let seq=1;seq<=3;seq++)for(let id=0;id<10;id++)f.push(M.parse(line(id,seq)));
  assert.equal(f.incomplete,1);assert.deepEqual(f.history.map(v=>v.seq),[2,3]);
  assert.equal(f.history[0].nodes[9].seq,2);
});
test('legacy execution order and absent nodes are explicit',()=>{
  assert.deepEqual(M.order(0),[0,2,4,6,7,5,8,9]);
  const f=new M.Frames();let result;
  for(const id of M.order(0))result=f.push(M.parse(line(id).replace('p=1','p=0')));
  assert.equal(Object.keys(result.nodes).length,8);
  const state=M.parse('NODES profile=0 flags=59 bypass=0 calibrated=1 trace=1 running=1');
  assert.equal(M.availability(state,1),'absent');assert.equal(M.availability(state,5),'bypass');
  assert.equal(M.availability({...state,calibrated:0},4),'uncalibrated');
  assert.equal(M.availability({...state,bypass:16},4),'bypass');
  assert.equal(M.availability(state,0),'fixed');
});
test('invalid frames create gaps and do not expose placeholder zeros as values',()=>{
  const f=new M.Frames();const n=M.parse(line().replace('valid=1','valid=0').replace('err=0','err=2'));
  const result=f.push(n);assert.equal(result.valid,false);assert.deepEqual(result.nodes,{});assert.equal(f.invalid,1);assert.equal(f.history.length,1);
  assert.equal(M.toCSV(f.history).split('\r\n').length,1);
});
test('CSV preserves per-stage channel data and execution state',()=>{
  const f=new M.Frames();for(let i=0;i<10;i++)f.push(M.parse(line(i)));
  const csv=M.toCSV(f.history);assert(csv.includes('reference_z'));assert(csv.includes('1,1234,1,4,1,1,1,-2.5,0.03,4,5,6'));
});
test('shared serial layer classifies telemetry separately from samples and archived commands',()=>{
  const C=require('../core.js'),log=new C.SerialLog();log.begin('COM1',921600);const before=log.count;
  assert.equal(C.parseLine(line()).kind,'trace');log.append('RX',line());assert.equal(log.count,before);
});
