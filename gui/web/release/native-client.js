/* WebView2 transport: local native messages, no HTTP listener. */
(() => {
'use strict';
const pending=new Map();let sequence=0;
const bridge=globalThis.chrome?.webview;
bridge?.addEventListener('message',({data})=>{
  const request=pending.get(data.id);if(!request)return;
  pending.delete(data.id);clearTimeout(request.timer);request.cleanup();
  data.ok?request.resolve(data.result):request.reject(new Error(data.error||'本地通信失败'));
});
class LocalSerialApi {
  constructor(){this.token='native';this.selected='';this.port=null;}
  async init(){if(!bridge)throw new Error('请使用 FIELD 发布版程序打开。');return this;}
  async call(operation,body,connection='',signal){
    await this.init();if(signal?.aborted)throw new DOMException('已取消','AbortError');
    const id=++sequence;
    return new Promise((resolve,reject)=>{
      const cancel=()=>{const request=pending.get(id);if(!request)return;clearTimeout(request.timer);pending.delete(id);request.cleanup();reject(new DOMException('已取消','AbortError'));};
      const cleanup=()=>signal?.removeEventListener('abort',cancel);
      const timer=setTimeout(()=>{pending.delete(id);cleanup();reject(new Error('本地通信超时，请重新连接。'));},6000);
      pending.set(id,{resolve,reject,timer,cleanup});signal?.addEventListener('abort',cancel,{once:true});
      try{bridge.postMessage({id,operation,body:body||{},connection});}catch(error){pending.delete(id);clearTimeout(timer);cleanup();reject(error);}
    });
  }
  async getPorts(){return(await this.call('ports')).ports;}
  async requestPort(){await this.init();if(!this.selected)throw new Error('请选择检测到的串口。');this.port=new LocalPort(this,this.selected);return this.port;}
}
class LocalPort {
  constructor(api,name){this.api=api;this.name=name;this.id='';this.active=false;this.abort=null;}
  getInfo(){return{portName:this.name};}
  async open(options){
    const result=await this.api.call('connect',{port:this.name,baudRate:options.baudRate});
    this.id=result.connection;this.active=true;this.abort=new AbortController();
    this.readable=new ReadableStream({start:controller=>{this.pollTask=this.poll(controller);},cancel:()=>{this.active=false;this.abort.abort();}});
    this.writable=new WritableStream({write:bytes=>this.api.call('write',{command:new TextDecoder().decode(bytes).replace(/\n$/,'')},this.id)});
  }
  async poll(controller){try{while(this.active){const result=await this.api.call('read',undefined,this.id,this.abort.signal);if(!this.active)return;if(result.data)controller.enqueue(Uint8Array.from(atob(result.data),c=>c.charCodeAt(0)));if(!result.connected)throw new Error(result.error||'串口已断开');}}catch(error){if(this.active){this.active=false;controller.error(error);}}}
  async close(){this.active=false;this.abort?.abort();if(this.id){const id=this.id;this.id='';try{await this.api.call('disconnect',{},id);}catch(error){if(!/会话已改变/.test(error.message))throw error;}}await this.pollTask;}
}
globalThis.LocalSerialApi=LocalSerialApi;
})();
