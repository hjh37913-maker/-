// Optional jsdom check for generated Android assets. It does not emulate Android.
const {JSDOM}=require('jsdom');
const fs=require('node:fs'), path=require('node:path'), assert=require('node:assert/strict');
const dir=process.argv[2]; if(!dir) throw Error('Pass generated Android asset directory');
const dom=new JSDOM(fs.readFileSync(path.join(dir,'index.html'),'utf8'),{url:'https://demo.invalid/',runScripts:'outside-only'});
const w=dom.window,d=w.document;
for(const dialog of d.querySelectorAll('dialog')) { dialog.showModal=function(){this.setAttribute('open','')}; dialog.close=function(){this.removeAttribute('open')}; }
w.NIGHTFALL_ANDROID_DEMO=true; w.fetch=()=>{throw Error('Offline demo must not call backend')};
w.eval(fs.readFileSync(path.join(dir,'app.js'),'utf8'));
const $=id=>d.getElementById(id);
assert.match($('engineStatus').textContent,/без ядра/);
$('addButton').click(); $('titleInput').value='Test'; $('sourceInput').value='https://example.org/';
$('addForm').dispatchEvent(new w.Event('submit',{bubbles:true,cancelable:true})); d.querySelector('.game-card').click();
assert.equal($('runItem').disabled,true); assert.match($('runItem').textContent,/недоступно/);
console.log('Android generated assets: offline status and disabled execution passed (simulated DOM only)');
w.close();
