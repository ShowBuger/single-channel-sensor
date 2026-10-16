const fs=require('node:fs/promises');
const path=require('node:path');
const zlib=require('node:zlib');
const acorn=require('acorn');
const {minify}=require('terser');
const root=path.resolve(__dirname,'..'),out=path.join(__dirname,'build/assets');

function publicCore(source){
  const removed=new Set(['validateConfig','configCommands','SerialLog','readbackEqual','IMPORT_DEFAULTS','IMPORT_FIELDS','ALIASES','productDefaults','sensorFieldActive','normalizeSensorType','prepareImport']);
  const methods=new Set(['readConfigDirect','readConfig','readFullConfigDirect','readFullConfig','resetImport','readLogLevelDirect','readLogLevel','writeConfig','importConfig']);
  const edits=[],found=new Set();
  function walk(node){
    if(!node||typeof node!=='object')return;
    if((node.type==='FunctionDeclaration'||node.type==='ClassDeclaration')&&removed.has(node.id?.name)){
      found.add(node.id.name);edits.push([node.start,node.end,'']);return;
    }
    if(node.type==='VariableDeclaration'&&node.declarations.every(d=>removed.has(d.id.name))){
      node.declarations.forEach(d=>found.add(d.id.name));edits.push([node.start,node.end,'']);return;
    }
    if(node.type==='ClassDeclaration'&&node.id.name==='SerialClient'){
      for(const method of node.body.body)if(methods.has(method.key.name)){found.add(method.key.name);edits.push([method.start,method.end,'']);}
      return;
    }
    if(node.type==='ReturnStatement'&&node.argument?.type==='ObjectExpression'&&node.argument.properties.some(p=>p.key.name==='SerialClient')){
      const properties=node.argument.properties.filter(p=>!removed.has(p.key.name)).map(p=>source.slice(p.start,p.end));
      edits.push([node.argument.start,node.argument.end,'{'+properties.join(',')+'}']);return;
    }
    for(const value of Object.values(node))if(Array.isArray(value))value.forEach(walk);else if(value&&typeof value==='object')walk(value);
  }
  walk(acorn.parse(source,{ecmaVersion:'latest'}));
  for(const name of [...removed,...methods])if(!found.has(name))throw Error('Release stripping contract changed: '+name);
  edits.sort((a,b)=>b[0]-a[0]);
  for(const[start,end,replacement]of edits)source=source.slice(0,start)+replacement+source.slice(end);
  acorn.parse(source,{ecmaVersion:'latest'});
  return source;
}

function replaceOnce(source,pattern,replacement){
  let count=0;const result=source.replace(pattern,()=>{count++;return replacement;});
  if(count!==1)throw Error('Release HTML contract changed: '+pattern+' ('+count+')');
  return result;
}
async function build(){
  await fs.mkdir(out,{recursive:true});
  let html=await fs.readFile(path.join(root,'index.html'),'utf8');
  html=replaceOnce(html,/<button data-open="importDialog">[\s\S]*?<\/button>/g,'');
  html=replaceOnce(html,/<dialog id="importDialog"[\s\S]*?<\/dialog>/g,'');
  html=replaceOnce(html,/<button[^>]*id="exportSerialLog"[^>]*>[\s\S]*?<\/button>/g,'');
  html=replaceOnce(html,/<div><strong[^>]*id="serialLogCount"[\s\S]*?<\/div>/g,'');
  html=replaceOnce(html,/<p>串口日志包含本页[\s\S]*?<\/p>/g,'<p>下方显示最近 150 条设备事件。</p>');
  html=replaceOnce(html,/<script src="core.js"><\/script>[\s\S]*?<script src="app.js"><\/script>/g,'<script src="bundle.js"></script>');
  html=html.replace('<title>传感上位机</title>','<title>FIELD · 传感监测</title>').replace('MLX90393 · 单点形变示意','单点形变示意');
  html=html.replace(/<!--[^]*?-->/g,'').replace(/>\s+</g,'><').trim();
  let app=await fs.readFile(path.join(root,'app.js'),'utf8');
  app=replaceOnce(app,/const INTERNAL_BUILD=true;/g,'const INTERNAL_BUILD=false;');
  const core=publicCore(await fs.readFile(path.join(root,'core.js'),'utf8'));
  const transport=await fs.readFile(path.join(__dirname,'native-client.js'),'utf8');
  const result=await minify([core,transport,app].join('\n'),{
    toplevel:true,compress:{passes:3},mangle:{toplevel:true},sourceMap:false,format:{comments:false,ascii_only:true}
  });
  if(!result.code)throw Error('Empty release bundle');
  for(const marker of ['ML IMPORT_','ML CHFLAG','ML SETMAP','FieldImportPanel','FIELD_serial_'])if(result.code.includes(marker))throw Error('Internal feature leaked into release: '+marker);
  for(const[name,content]of [['index.html',html],['bundle.js',result.code]]){
    await fs.writeFile(path.join(out,name),content,'utf8');
    await fs.writeFile(path.join(out,name+'.gz'),zlib.gzipSync(content,{level:9}));
  }
  console.log('Release assets generated:',out);
}
module.exports={publicCore,build};
if(require.main===module)build().catch(error=>{console.error(error);process.exitCode=1;});
