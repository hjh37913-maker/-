"""Prepare an explicitly offline Android UI demo from the research dashboard."""
import argparse
from pathlib import Path
import shutil

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('output', type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'assets').mkdir(exist_ok=True)
html = (root / 'web/index.html').read_text(encoding='utf-8')
for old, new in [
    ('href="/style.css"', 'href="style.css"'),
    ('src="/app.js"', 'src="app.js"'),
    ('src="/assets/nightfall-hero.jpg"', 'src="assets/nightfall-hero.jpg"'),
    ('target="_blank"', 'target="_self"'),
    ('<script src="app.js" defer></script>', '<script>window.NIGHTFALL_ANDROID_DEMO = true;</script><script src="app.js" defer></script>'),
    ('Пока это не игровой лаунчер.', 'Android-демо: только интерфейс.'),
    ('Добавление файла не означает, что его можно запустить. PS5 и GTA VI ещё не поддерживаются.', 'Здесь нет ядра и запуска программ или игр. PS5 и GTA VI не поддерживаются.'),
    ('Поддерживаются только оригинальные небольшие тестовые файлы raw и ELF. Не запускайте неизвестные или защищённые файлы: это не песочница.', 'В этой Android-версии запуска программ и игр нет. Доступен только просмотр библиотеки.'),
]:
    if old not in html: raise SystemExit('Missing expected HTML marker: ' + old)
    html = html.replace(old, new)
(args.output / 'index.html').write_text(html, encoding='utf-8')
js = (root / 'web/app.js').read_text(encoding='utf-8')
marker = "  $('detailsDialog').showModal();"
if marker not in js: raise SystemExit('Missing details marker')
js = js.replace(marker, "  if(window.NIGHTFALL_ANDROID_DEMO) { $('runItem').disabled=true; $('runItem').textContent='В Android-демо недоступно'; }\n"+marker)
start = js.index("fetch('/api/status')")
end = js.index('\ndraw();', start)
js = js[:start] + "$('engineStatus').textContent='Android демо · без ядра'; $('engineDot').classList.add('off');\n" + js[end:]
(args.output / 'app.js').write_text(js, encoding='utf-8')
shutil.copyfile(root / 'web/style.css', args.output / 'style.css')
shutil.copyfile(root / 'web/assets/nightfall-hero.jpg', args.output / 'assets/nightfall-hero.jpg')
