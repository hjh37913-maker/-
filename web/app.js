'use strict';
const $ = id => document.getElementById(id);
const KEY = 'nightfall.library.v1';
let items = [];
let attached = new Map();
let current = null;
try {
  const data = JSON.parse(localStorage.getItem(KEY) || '[]');
  if (Array.isArray(data)) items = data.filter(x => x && typeof x.id === 'string' && typeof x.name === 'string' && typeof x.fileName === 'string' && (x.size === null || Number.isFinite(x.size)));
} catch (_) { /* Keep the interface usable when browser storage is unavailable. */ }
function persist() {
  try { localStorage.setItem(KEY, JSON.stringify(items)); return true; }
  catch (_) { toast('Не удалось сохранить библиотеку в браузере'); return false; }
}
let toastTimer;
function toast(message) { const el = $('toast'); el.textContent = message; el.classList.add('show'); clearTimeout(toastTimer); toastTimer = setTimeout(() => el.classList.remove('show'), 3000); }
function sizeLabel(size) { if (size === null) return 'Внешний источник'; return size >= 1024*1024 ? (size/(1024*1024)).toFixed(1)+' МиБ' : (size/1024).toFixed(1)+' КиБ'; }
function draw() {
  const query = $('search').value.trim().toLocaleLowerCase();
  const shown = items.filter(x => x.name.toLocaleLowerCase().includes(query));
  $('itemCount').textContent = String(items.length).padStart(2,'0');
  $('empty').hidden = items.length !== 0;
  $('noResults').hidden = !items.length || shown.length !== 0;
  $('cards').replaceChildren();
  for (const item of shown) {
    const card = document.createElement('button'); card.type='button'; card.className='game-card';
    const art = document.createElement('div'); art.className='game-art'; art.textContent='ЛОКАЛЬНЫЙ ФАЙЛ';
    const info = document.createElement('div'); info.className='game-info';
    const name = document.createElement('strong'); name.textContent=item.name;
    const meta = document.createElement('small'); meta.textContent=sizeLabel(item.size)+' · Совместимость неизвестна';
    info.append(name,meta); card.append(art,info); card.addEventListener('click',()=>details(item.id)); $('cards').append(card);
  }
}
function details(id) {
  const item=items.find(x=>x.id===id); if(!item) return;
  current=id; $('detailTitle').textContent=item.name;
  $('detailMeta').textContent=(item.fileName || 'Файл не выбран')+' · '+sizeLabel(item.size)+' · совместимость неизвестна';
  $('reattachArea').hidden=attached.has(id); $('reattach').value=''; $('runConsent').checked=false; $('runOutput').hidden=true;
  $('runMode').value=item.fileName.toLowerCase().endsWith('.elf')?'elf':'raw';
  const source=$('sourceLink'); source.hidden=true; source.removeAttribute('href');
  try { const url=new URL(item.source); if(url.protocol==='https:') { source.href=url.href; source.hidden=false; } } catch(_) {}
  $('detailsDialog').showModal();
}
for(const id of ['addButton','openAdd','emptyAdd']) $(id).addEventListener('click',()=>{ $('addForm').reset(); $('addDialog').showModal(); });
for(const button of document.querySelectorAll('[data-close]')) button.addEventListener('click',()=>button.closest('dialog').close());
$('fileInput').addEventListener('change',()=>{ if(!$('titleInput').value && $('fileInput').files[0]) $('titleInput').value=$('fileInput').files[0].name; });
$('addForm').addEventListener('submit',e=>{
  e.preventDefault(); const file=$('fileInput').files[0];
  let source=$('sourceInput').value.trim();
  if(source) { try { if(new URL(source).protocol!=='https:') throw Error(); } catch(_) { toast('Укажите корректную HTTPS-ссылку'); return; } }
  if(!file && !source) { toast('Выберите файл или укажите HTTPS-ссылку'); return; }
  if(!file && !$('titleInput').value.trim()) { toast('Укажите название для ссылки'); return; }
  const id=globalThis.crypto?.randomUUID?.() || Date.now().toString(36)+'-'+Math.random().toString(36).slice(2);
  const item={id,name:($('titleInput').value.trim() || file.name).slice(0,120), fileName:file ? file.name : '', size:file ? file.size : null, source};
  items.push(item); if(file) attached.set(id,file); persist(); draw(); $('addDialog').close(); toast('Добавлено в библиотеку');
});
$('search').addEventListener('input',draw);
$('removeItem').addEventListener('click',()=>{ items=items.filter(x=>x.id!==current); attached.delete(current); persist(); draw(); $('detailsDialog').close(); toast('Запись удалена; файл не затронут'); });
$('reattach').addEventListener('change',()=>{ const file=$('reattach').files[0]; if(file) { attached.set(current,file); $('detailMeta').textContent='Выбран для запуска: '+file.name+' · '+sizeLabel(file.size); toast('Файл выбран для этого сеанса'); } });
$('runItem').addEventListener('click',async()=>{
  const file=attached.get(current), mode=$('runMode').value;
  if(!file) { $('reattachArea').hidden=false; toast('Сначала выберите файл заново'); return; }
  if(!$('runConsent').checked) { toast('Подтвердите, что это ваш тестовый файл'); return; }
  if(file.size===0 || file.size>(mode==='raw'?65536:1048576)) { toast('Размер файла превышает ограничение тестового режима'); return; }
  const button=$('runItem'); button.disabled=true; button.textContent='Выполняется…';
  const output=$('runOutput'); output.hidden=false; output.textContent='Выполняем оригинальную тестовую программу…';
  try {
    const response=await fetch('/api/run?mode='+encodeURIComponent(mode), {method:'POST',headers:{'Content-Type':'application/octet-stream'},body:file});
    const data=await response.json();
    output.textContent=response.ok ? ('Код выхода: '+data.exit_code+'\n'+data.stdout+(data.stderr?'\n'+data.stderr:'')) : ('Ошибка: '+(data.error || response.status));
  } catch (_) { output.textContent='Сервер недоступен. Запустите web/server.py и проверьте сборку ядра.'; }
  finally { button.disabled=false; button.textContent='Запустить тест'; }
});
$('openControls').addEventListener('click',()=>{ $('inputEvent').textContent='Ожидание ввода'; $('controlsDialog').showModal(); $('inputArea').focus(); });
$('inputArea').addEventListener('keydown',e=>{ if(e.key==='Escape') return; e.preventDefault(); $('inputEvent').textContent='Клавиша: '+e.code; });
$('inputArea').addEventListener('pointerdown',e=>{ e.preventDefault(); $('inputArea').focus(); $('inputEvent').textContent='Кнопка мыши: '+(e.button+1); });
$('inputArea').addEventListener('pointermove',e=>{ if(e.pointerType==='mouse' && document.activeElement===$('inputArea')) $('inputEvent').textContent='Мышь: '+Math.round(e.offsetX)+' × '+Math.round(e.offsetY); });
$('inputArea').addEventListener('contextmenu',e=>e.preventDefault());
for(const id of ['helpButton','openAbout']) $(id).addEventListener('click',e=>{ e.preventDefault(); $('aboutDialog').showModal(); });
fetch('/api/status').then(r=>r.json()).then(s=>{ $('engineStatus').textContent=s.engine==='available'?'Ядро найдено':'Ядро не собрано'; $('engineDot').classList.toggle('off',s.engine!=='available'); }).catch(()=>{ $('engineStatus').textContent='Нет соединения'; $('engineDot').classList.add('off'); });
draw();
