const {test}=require('node:test');
const assert=require('node:assert/strict');
const {spawn}=require('node:child_process');
const path=require('node:path');
test('Windows backend serves offline UI, lists ports, rejects unauthorized and invalid requests', {skip:process.platform!=='win32',timeout:15000},async()=>{
  const server=spawn(path.resolve(__dirname,'../FIELD.exe'),['--no-browser','--port=0'],{windowsHide:true});
  try{
    const origin=await new Promise((resolve,reject)=>{server.stdout.on('data',chunk=>{const m=chunk.toString().match(/http:\/\/127\.0\.0\.1:\d+/);if(m)resolve(m[0]);});server.on('error',reject);server.on('exit',code=>reject(new Error('Server exited '+code)));});
    const page=await fetch(origin);assert.equal(page.status,200);assert.match(await page.text(),/FIELD/);assert.match(page.headers.get('content-security-policy'),/connect-src 'self'/);
    const session=await(await fetch(origin+'/api/session')).json();assert.equal(session.transport,'windows-serial');const headers={'X-Field-Key':session.token,'Content-Type':'application/json'};
    const ports=await(await fetch(origin+'/api/ports',{headers})).json();assert(Array.isArray(ports.ports));assert(ports.ports.every(p=>/^COM\d+$/.test(p)));
    assert.equal((await fetch(origin+'/api/ports')).status,403);
    assert.equal((await fetch(origin+'/api/session',{headers:{Origin:'https://untrusted.example'}})).status,403);
    assert.equal((await fetch(origin+'/bridge/Server.cs')).status,404);
    const invalid=await fetch(origin+'/api/connect',{method:'POST',headers,body:JSON.stringify({port:'COM999999',baudRate:115200})});assert.equal(invalid.status,400);assert.match((await invalid.json()).error,/端口/);
    const injection=await fetch(origin+'/api/write',{method:'POST',headers,body:JSON.stringify({command:'ML STOP\nML START'})});assert.equal(injection.status,400);assert.match((await injection.json()).error,/命令/);
    for(const command of ['ML IMPORT_START 250','ML IMPORT_DATA '+ 'a'.repeat(47),'ML IMPORT_END','ML IMPORT_CANCEL','ML LOGLEVEL']){
      const result=await fetch(origin+'/api/write',{method:'POST',headers,body:JSON.stringify({command})});assert.equal(result.status,400);assert.match((await result.json()).error,/会话/);
    }
    for(const command of ['ML IMPORT_START 1024','ML IMPORT_DATA '+ 'a'.repeat(48),'ML IMPORT_END\n']){
      const result=await fetch(origin+'/api/write',{method:'POST',headers,body:JSON.stringify({command})});assert.equal(result.status,400);assert.match((await result.json()).error,/命令/);
    }
    const read=await fetch(origin+'/api/read',{headers});assert.equal(read.status,400);
    const badBaud=await fetch(origin+'/api/connect',{method:'POST',headers,body:JSON.stringify({port:'COM999999',baudRate:0})});assert.equal(badBaud.status,400);
  }finally{server.kill();}
});
