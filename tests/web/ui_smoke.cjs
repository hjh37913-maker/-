// Optional DOM smoke test; requires jsdom 26. No browser rendering or real device is tested.
const {JSDOM} = require('jsdom');
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '../..');
const dom = new JSDOM(fs.readFileSync(path.join(root,'web/index.html'),'utf8'), {url:'http://localhost:8765/',runScripts:'outside-only'});
const w=dom.window, d=w.document;
for(const dialog of d.querySelectorAll('dialog')) { dialog.showModal=function(){this.setAttribute('open','')}; dialog.close=function(){this.removeAttribute('open')}; }
let calls=[];
w.fetch=async (url,options)=>{calls.push({url,options}); return {ok:true,json:async()=>url==='/api/status'?{engine:'available'}:{exit_code:0,stdout:'halted RAX=42',stderr:''}};};
w.eval(fs.readFileSync(path.join(root,'web/app.js'),'utf8'));
const $=id=>d.getElementById(id);
const flush=()=>new Promise(resolve=>setTimeout(resolve,0));
(async()=>{
 await flush(); assert.equal($('engineStatus').textContent,'Ядро найдено');
 $('addButton').click(); assert($('addDialog').hasAttribute('open'));
 const file=new w.File([new Uint8Array([0xf4])],'<unsafe>.elf');
 Object.defineProperty($('fileInput'),'files',{configurable:true,value:[file]});
 $('titleInput').value='<img src=x onerror=alert(1)>';
 $('addForm').dispatchEvent(new w.Event('submit',{bubbles:true,cancelable:true}));
 assert.equal($('itemCount').textContent,'01'); assert.equal(d.querySelectorAll('.game-card').length,1);
 assert.equal(d.querySelectorAll('.game-info img').length,0); assert.equal(d.querySelector('.game-info strong').textContent,'<img src=x onerror=alert(1)>');
 $('search').value='zzz'; $('search').dispatchEvent(new w.Event('input')); assert.equal($('noResults').hidden,false);
 $('search').value=''; $('search').dispatchEvent(new w.Event('input')); d.querySelector('.game-card').click();
 $('runItem').click(); assert.equal(calls.filter(x=>x.url.startsWith('/api/run')).length,0);
 $('runConsent').checked=true; $('runItem').click(); await flush();
 assert.equal(calls.filter(x=>x.url==='/api/run?mode=elf').length,1); assert.match($('runOutput').textContent,/halted RAX=42/);
 $('removeItem').click(); assert.equal($('itemCount').textContent,'00');
 $('addButton').click(); Object.defineProperty($('fileInput'),'files',{configurable:true,value:[]}); $('titleInput').value='Источник'; $('sourceInput').value='https://example.org/game';
 $('addForm').dispatchEvent(new w.Event('submit',{bubbles:true,cancelable:true})); d.querySelector('.game-card').click(); assert.equal($('sourceLink').href,'https://example.org/game');
 $('runItem').click(); assert.equal($('reattachArea').hidden,false);
 $('detailsDialog').close(); $('openControls').click();
 $('inputArea').dispatchEvent(new w.KeyboardEvent('keydown',{code:'KeyW',key:'w',bubbles:true,cancelable:true})); assert.match($('inputEvent').textContent,/KeyW/);
 $('inputArea').dispatchEvent(new w.MouseEvent('pointerdown',{button:0,bubbles:true,cancelable:true})); assert.match($('inputEvent').textContent,/1/);
 console.log('DOM smoke: add, search, safe text, test execution, source link, remove, keyboard and mouse passed');
 dom.window.close();
})().catch(error=>{console.error(error);process.exitCode=1;dom.window.close()});
