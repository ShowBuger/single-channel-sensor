/* JSON import preview/controller. User files are data, never executable content. */
(() => {
'use strict';
const $=id=>document.getElementById(id),C=FieldCore;
class ImportPanel {
  constructor({device,operation,onApplied,onUncertain,onRead,toast,log}){
    Object.assign(this,{device,operation,onApplied,onUncertain,onRead,toast,log});this.sourceLabel="文件值";this.prepared=null;this.busy=false;this.controller=null;this.fileVersion=0;
    $('readDeviceConfig').onclick=()=>this.operation(this.onRead);
    $('chooseJson').onclick=()=>$('jsonFile').click();
    $('jsonFile').onchange=async e=>{const file=e.target.files[0];if(!file)return;const version=++this.fileVersion;this.prepared=null;this.sync();$('jsonPreview').replaceChildren();$('jsonEditor').value='';this.sourceLabel='文件值';$('jsonFileName').textContent=file.name;try{if(file.size>65536)throw new Error('文件超过 64 KB，请选择传感器 JSON 配置文件。');const text=await file.text();if(version!==this.fileVersion)return;$('jsonEditor').value=text;this.validate();}catch(error){this.status(error.message,'error');}finally{e.target.value='';}};
    $('jsonEditor').oninput=()=>{this.sourceLabel='编辑值';$('jsonFileName').textContent='已编辑 · 待应用配置';this.fileVersion++;this.prepared=null;$('jsonPreview').replaceChildren();this.status('内容已修改，请重新校验。');this.sync();};
    $('validateJson').onclick=()=>this.validate();
    $('downloadTemplate').onclick=()=>this.download(C.IMPORT_DEFAULTS,'FIELD_config_default.json');
    $('exportJson').onclick=()=>{if(this.prepared)this.download(JSON.parse(this.prepared.wire),'FIELD_config.json');};
    $('applyJson').onclick=()=>this.apply();
    $('cancelImport').onclick=()=>{this.controller?.abort();$('cancelImport').disabled=true;this.status('等待当前分块回执后取消…');};
    $('importDialog').addEventListener('cancel',e=>{if(this.busy)e.preventDefault();});
    this.sync();
  }
  status(text,kind='info'){$('importStatus').textContent=text;$('importStatus').dataset.kind=kind;}
  loadDeviceConfig(config){
    this.fileVersion++;this.sourceLabel='设备读回';this.prepared=C.prepareImport(config);
    $('jsonEditor').value=JSON.stringify(JSON.parse(this.prepared.wire),null,2);
    $('jsonFileName').textContent='设备配置 · '+new Date().toLocaleTimeString('zh-CN',{hour12:false});
    this.render();this.status('设备完整配置已读取，未修改设备。可编辑后应用，或导出配置。','success');this.sync(this.externalBusy);
  }
  sync(externalBusy=false){this.externalBusy=externalBusy;const locked=this.busy||externalBusy;for(const id of ['chooseJson','jsonEditor','validateJson','downloadTemplate','importClose','startAfterImport'])$(id).disabled=locked;$('exportJson').disabled=locked||!this.prepared;$('readDeviceConfig').disabled=locked||!this.device.connected;$('applyJson').disabled=locked||!this.device.connected||!this.prepared;$('cancelImport').hidden=!this.busy;if(!this.busy)$('cancelImport').disabled=false;$('importConnection').textContent=this.device.connected?'已连接 · 可应用到设备':'未连接 · 可导入预览，连接后应用';}
  validate(){try{this.prepared=C.prepareImport($('jsonEditor').value);this.render();this.status(`校验通过 · ${this.prepared.bytes} 字节 · ${this.prepared.chunks.length} 个传输块`,'success');}catch(error){this.prepared=null;$('jsonPreview').replaceChildren();$('importNotes').textContent='修正配置后重新校验。';this.status(error.message,'error');}this.sync(this.externalBusy);}
  render(){
    const {config,missing}=this.prepared;
    const groups=[['产品信息',[['产品型号',config.product,'product']]]];
    for(const[group,title]of [['sensor','传感器寄存器参数'],['algo','算法参数'],['log','系统日志']]){
      const rows=Object.entries(C.IMPORT_FIELDS[group]).map(([key,rule])=>{
        const value=group==='algo'&&key==='flag'?'0x'+config.algo.flag.toString(16).toUpperCase().padStart(2,'0'):
          group==='log'?`${config.log.level} · ${['NONE','ERROR','WARN','INFO','DEBUG'][config.log.level]}`:String(config[group][key]);
        return[rule[0],value,group+'.'+key];
      });
      groups.push([title,rows]);
    }
    $('jsonPreview').replaceChildren();
    for(const[title,rows]of groups){
      const section=document.createElement('section'),heading=document.createElement('h3'),table=document.createElement('table');
      heading.textContent=title;section.append(heading);
      for(const[label,text,field]of rows){
        const tr=document.createElement('tr'),name=document.createElement('th'),value=document.createElement('td'),source=document.createElement('td');
        name.textContent=label;value.className='mono';value.textContent=text;
        source.textContent=missing.includes(field)?'默认补全':this.sourceLabel;source.className='muted';
        tr.append(name,value,source);table.append(tr);
      }
      section.append(table);$('jsonPreview').append(section);
    }
    $('importNotes').textContent=(missing.length?`缺少的 ${missing.length} 个字段使用固件默认值，不会保留设备旧值。 `:'这是完整配置，将替换设备对应参数。 ')+(config.log.level<3?'当前固件在日志等级低于 INFO 时不输出采样数据；应用后曲线会停止更新。':'应用可能重启传感器并重新标定，请保持传感器静止。');
  }
  download(value,name){const url=URL.createObjectURL(new Blob([JSON.stringify(value,null,2)+'\n'],{type:'application/json'})),a=document.createElement('a');a.href=url;a.download=name;document.body.append(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);}
  async apply(){if(!this.prepared||!this.device.connected||this.busy)return;const prepared=this.prepared;this.controller=new AbortController();this.busy=true;$('importProgress').hidden=false;$('importProgressBar').value=0;this.sync();await this.operation(async()=>{try{
      const result=await this.device.importConfig(prepared.config,{signal:this.controller.signal,startAfter:$('startAfterImport').checked,onProgress:p=>{const stages={prepare:'准备导入',sending:'正在发送',applying:'设备应用与保存中',starting:'启动采集与标定中',verifying:'读取参数校验中',done:'应用完成'};$('importProgressBar').value=p.stage==='done'?100:p.sent/p.total*90;this.status(`${stages[p.stage]} · ${p.sent} / ${p.total} 字节`);$('cancelImport').disabled=!['prepare','sending'].includes(p.stage);}});
      this.onApplied(result);this.loadDeviceConfig(result.config);this.status('设备已确认保存；完整配置读回一致。寄存器参数写入芯片仍需下位机支持。','success');this.log('配置已保存，完整配置读回一致；寄存器应用待下位机支持。','配置');this.toast('配置已保存，运行参数已核对');
    }catch(error){if(error.submitted)this.onUncertain?.();this.status(error.message,'error');throw error;}finally{this.busy=false;this.controller=null;this.sync();}});}
}
globalThis.FieldImportPanel=ImportPanel;
})();
