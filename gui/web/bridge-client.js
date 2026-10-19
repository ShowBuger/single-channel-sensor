/* Local Windows transport adapter for FieldCore.SerialClient. */
(function(root){
'use strict';
const localize=(zh,en)=>globalThis.FieldI18n?FieldI18n.t(zh,en):zh;
class LocalSerialApi {
  constructor(){this.token='';this.selected='';this.port=null;}
  async init(){if(location.protocol==='file:')throw new Error(localize('请双击“启动上位机.cmd”，由本地程序自动打开界面并识别 COM 端口。','Double-click "Start FIELD.cmd" to open the interface and detect COM ports.'));const r=await fetch('/api/session',{cache:'no-store',signal:AbortSignal.timeout(2500)});if(!r.ok)throw new Error(localize('当前为静态预览。请双击“启动上位机.cmd”启用串口。','Static preview. Double-click "Start FIELD.cmd" to enable serial communication.'));const data=await r.json();if(data.transport!=='windows-serial')throw new Error(localize('本地通信服务不匹配。','Local communication service mismatch.'));this.token=data.token;return this;}
  async call(path,body,connection='',signal){const r=await fetch('/api/'+path,{method:body===undefined?'GET':'POST',cache:'no-store',headers:{'X-Field-Key':this.token,'X-Field-Connection':connection,...(body!==undefined?{'Content-Type':'application/json'}:{})},body:body===undefined?undefined:JSON.stringify(body),signal:signal||AbortSignal.timeout(5000)});const data=await r.json();if(!r.ok)throw new Error(globalThis.FieldI18n?.translate(data.error)||data.error||localize('本地通信失败','Local communication failed'));return data;}
  async getPorts(){if(!this.token)await this.init();return(await this.call('ports')).ports;}
  async requestPort(){if(!this.token)await this.init();if(!this.selected)throw new Error(localize('请选择检测到的串口。','Select a detected serial port.'));this.port=new LocalPort(this,this.selected);return this.port;}
}
class LocalPort {
  constructor(api,name){this.api=api;this.name=name;this.id='';this.active=false;this.abort=null;this.readable=null;this.writable=null;}
  getInfo(){return{portName:this.name};}
  async open(options){const r=await this.api.call('connect',{port:this.name,baudRate:options.baudRate});this.id=r.connection;this.active=true;this.abort=new AbortController();this.readable=new ReadableStream({start:controller=>{this.controller=controller;this.pollTask=this.poll(controller);},cancel:()=>{this.active=false;this.abort.abort();}});this.writable=new WritableStream({write:bytes=>{const command=new TextDecoder().decode(bytes).replace(/\n$/,'');return this.api.call('write',{command},this.id);}});}
  async poll(controller){try{while(this.active){const r=await this.api.call('read',undefined,this.id,this.abort.signal);if(!this.active)return;if(r.data)controller.enqueue(Uint8Array.from(atob(r.data),c=>c.charCodeAt(0)));if(!r.connected)throw new Error(r.error||localize('串口已断开','Serial port disconnected'));}}catch(error){if(this.active){this.active=false;controller.error(error);}}}
  async close(){this.active=false;this.abort?.abort();if(this.id){const id=this.id;this.id='';try{await this.api.call('disconnect',{},id);}catch(error){if(!/会话已改变|session changed/i.test(error.message))throw error;}}await this.pollTask;}
}
root.LocalSerialApi=LocalSerialApi;
})(globalThis);
