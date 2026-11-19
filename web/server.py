"""Local research dashboard. Serves trusted original test programs only; not a sandbox."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parent
ASSETS = {'/': ('index.html', 'text/html; charset=utf-8'), '/app.js': ('app.js', 'text/javascript; charset=utf-8'), '/style.css': ('style.css', 'text/css; charset=utf-8'), '/assets/nightfall-hero.jpg': ('assets/nightfall-hero.jpg', 'image/jpeg')}
MAX_FILE = 1024 * 1024

class Handler(BaseHTTPRequestHandler):
    def respond(self, code: int, data: bytes, content_type: str) -> None:
        self.send_response(code)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.end_headers()
        self.wfile.write(data)

    def json_response(self, code: int, obj: dict) -> None:
        self.respond(code, json.dumps(obj, ensure_ascii=False).encode('utf-8'), 'application/json; charset=utf-8')

    def do_GET(self) -> None:
        path = urlsplit(self.path).path
        if path == '/api/status':
            self.json_response(200, {'engine': 'available' if self.server.cli.is_file() else 'missing', 'ps5': False, 'max_file_bytes': MAX_FILE})
            return
        if path not in ASSETS:
            self.json_response(404, {'error': 'Not found'})
            return
        name, mime = ASSETS[path]
        try:
            self.respond(200, (ROOT / name).read_bytes(), mime)
        except OSError:
            self.json_response(500, {'error': 'Dashboard asset unavailable'})

    def do_POST(self) -> None:
        url = urlsplit(self.path)
        if url.path != '/api/run':
            self.json_response(404, {'error': 'Not found'}); return
        # Permit same-origin browser requests and local CLI clients without Origin.
        origin = self.headers.get('Origin')
        if origin:
            parsed = urlsplit(origin)
            if parsed.scheme not in ('http', 'https') or parsed.netloc.lower() != self.headers.get('Host', '').lower() or parsed.path not in ('', '/'):
                self.json_response(403, {'error': 'Origin rejected'}); return
        params = parse_qs(url.query)
        modes = params.get('mode', [])
        if len(modes) != 1 or modes[0] not in ('raw', 'elf'):
            self.json_response(400, {'error': 'Choose raw or elf mode'}); return
        try:
            size = int(self.headers.get('Content-Length', ''))
        except ValueError:
            self.json_response(400, {'error': 'Invalid Content-Length'}); return
        limit = 65536 if modes[0] == 'raw' else MAX_FILE
        if size <= 0 or size > limit:
            self.json_response(413, {'error': f'File must be between 1 and {limit} bytes'}); return
        if not self.server.cli.is_file():
            self.json_response(503, {'error': 'Interpreter not built; see README'}); return
        try:
            self.connection.settimeout(10)
            data = self.rfile.read(size)
            if len(data) != size:
                self.json_response(400, {'error': 'Incomplete upload'}); return
            with tempfile.TemporaryDirectory(prefix='nightfall-test-') as directory:
                file = Path(directory) / 'guest-test'
                file.write_bytes(data)
                run = subprocess.run([str(self.server.cli), '--run-'+modes[0], str(file), '10000'], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=15)
            self.json_response(200, {'exit_code': run.returncode, 'stdout': run.stdout[:16384], 'stderr': run.stderr[:16384]})
        except (OSError, subprocess.TimeoutExpired):
            self.json_response(500, {'error': 'Interpreter could not finish; inspect server logs'})


def create_server(address: tuple[str, int], cli: Path) -> ThreadingHTTPServer:
    server = ThreadingHTTPServer(address, Handler)
    server.cli = cli.resolve()
    return server


def main() -> None:
    parser = argparse.ArgumentParser(description='NIGHTFALL local research dashboard; never expose to untrusted users.')
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--cli', type=Path, default=ROOT.parent / 'build' / ('nightfall_cli.exe' if os.name == 'nt' else 'nightfall_cli'))
    args = parser.parse_args()
    with create_server((args.host, args.port), args.cli) as server:
        print(f'NIGHTFALL research dashboard on {args.host}:{server.server_port}; use only trusted test files.', flush=True)
        server.serve_forever()

if __name__ == '__main__':
    main()
