/* Firmware trace protocol and bounded, sequence-consistent frame assembly. */
(function(root,factory){const api=factory();if(typeof module==='object'&&module.exports)module.exports=api;else root.AlgorithmModel=api;})(globalThis,function(){
'use strict';
const stages=[
  ['有效性校验','有限数值与传感器状态',0],['坐标旋转','统一实际与参考坐标',0],
  ['零偏校正','减去静态标定偏置',1],['尖峰抑制','中值预测与 MAD 判断',128],
  ['背景补偿','时间对齐与带符号增益',8],['低通滤波','降低高频噪声',4],
  ['回差死区','按噪声阈值抑制小信号',2],['Z 轴整形','负半轴取绝对值',64],
  ['标度映射','各轴映射系数',16],['静止归零','稳定后按时间衰减',32]
].map(([name,description,bit],id)=>({id,name,description,bit}));
const order=profile=>profile===1?[0,1,2,3,4,5,6,7,8,9]:[0,2,4,6,7,5,8,9];
const integer=(x,max)=>/^\d+$/.test(x||'')&&Number(x)<=max;
function parse(line){
  if(!/^(?:N |NODES )/.test(line))return null;
  const fields={};
  for(const token of line.split(/\s+/).slice(1)){
    const m=/^([a-z_]+)=(\S+)$/.exec(token);
    if(!m||Object.hasOwn(fields,m[1]))return null;fields[m[1]]=m[2];
  }
  const check=(k,max)=>integer(fields[k],max);
  if(line.startsWith('NODES ')){
    if(!check('profile',1)||!check('flags',255)||!check('bypass',1022)||
       !['calibrated','trace','running'].every(k=>check(k,1)))return null;
    return{kind:'state',...Object.fromEntries(Object.entries(fields).map(([k,v])=>[k,Number(v)]))};
  }
  if(!check('seq',0xFFFFFFFF)||!check('t',0xFFFFFFFF)||!check('s',9)||!check('p',1)||
     !['run','valid'].every(k=>check(k,1))||!check('err',255))return null;
  const numbers=['x','y','z','rx','ry','rz'].map(k=>Number(fields[k]));
  if(!['x','y','z','rx','ry','rz'].every(k=>/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$/.test(fields[k]||''))||!numbers.every(Number.isFinite))return null;
  return{kind:'node',seq:Number(fields.seq),t:Number(fields.t),profile:Number(fields.p),
    id:Number(fields.s),run:fields.run==='1',valid:fields.valid==='1',error:Number(fields.err),actual:numbers.slice(0,3),reference:numbers.slice(3)};
}
function availability(state,id){
  if(!order(state.profile).includes(id))return'absent';
  if(id===0)return'fixed';
  if((state.bypass&(1<<id)) || (stages[id].bit && !(state.flags&stages[id].bit)))return'bypass';
  if(!state.calibrated && [2,4,6,8,9].includes(id))return'uncalibrated';
  if(!state.calibrated && state.profile===0 && id===5)return'uncalibrated';
  return'on';
}
class Frames{
  constructor(capacity=300){this.capacity=capacity;this.clear();}
  clear(){this.pending=null;this.history=[];this.incomplete=0;this.invalid=0;}
  push(n){
    if(!n||n.kind!=='node')return null;
    if(!n.valid){this.invalid++;this.pending=null;const frame={seq:n.seq,t:n.t,profile:n.profile,valid:false,error:n.error,nodes:{}};this.history.push(frame);if(this.history.length>this.capacity)this.history.shift();return frame;}
    if(!order(n.profile).includes(n.id))return null;
    if(!this.pending||this.pending.seq!==n.seq||this.pending.profile!==n.profile){
      if(this.pending)this.incomplete++;
      this.pending={seq:n.seq,t:n.t,profile:n.profile,valid:true,nodes:{}};
    }
    if(n.t!==this.pending.t||Object.hasOwn(this.pending.nodes,n.id))return null;
    this.pending.nodes[n.id]=n;
    if(!order(n.profile).every(id=>this.pending.nodes[id]))return null;
    const frame=this.pending;this.pending=null;this.history.push(frame);
    if(this.history.length>this.capacity)this.history.shift();return frame;
  }
}
function toCSV(frames){
  const rows=['seq,t_us,profile,stage,executed,valid,actual_x,actual_y,actual_z,reference_x,reference_y,reference_z'];
  for(const f of frames)for(const id of order(f.profile)){const n=f.nodes[id];if(n)rows.push([f.seq,f.t,f.profile,id,+n.run,+f.valid,...n.actual,...n.reference].join(','));}
  return'\uFEFF'+rows.join('\r\n');
}
return{stages,order,parse,availability,Frames,toCSV};
});
