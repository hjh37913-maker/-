"""Integration tests for the loopback dashboard server using the real CLI."""
import http.client
import importlib.util
from pathlib import Path
import sys
import threading
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('dashboard_server', ROOT / 'web/server.py')
server_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server_module)
CLI = Path(sys.argv.pop(1)).resolve()

class DashboardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = server_module.create_server(('127.0.0.1', 0), CLI)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown(); cls.server.server_close(); cls.thread.join()
    def request(self, method, path, body=None, headers=None):
        connection = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=10)
        connection.request(method, path, body=body, headers=headers or {})
        response=connection.getresponse(); data=response.read(); status=response.status; connection.close()
        return status, data
    def oversized_headers(self, mode, size):
        # A server may reject before the body is sent; test the size preflight
        # without racing a large send against connection closure.
        connection = http.client.HTTPConnection('127.0.0.1', self.server.server_port, timeout=10)
        try:
            connection.putrequest('POST', '/api/run?mode='+mode)
            connection.putheader('Content-Length', str(size))
            connection.endheaders()
            response=connection.getresponse(); response.read(); return response.status
        finally: connection.close()
    def test_static_and_status(self):
        status, data=self.request('GET','/'); self.assertEqual(status,200); self.assertIn(b'NIGHTFALL',data)
        self.assertEqual(self.request('GET','/assets/nightfall-hero.jpg')[0],200)
        self.assertEqual(self.request('GET','/../README.md')[0],404)
        status, data=self.request('GET','/api/status'); self.assertEqual(status,200); self.assertIn(b'"engine": "available"',data)
    def test_run_original_raw(self):
        import json
        data=(ROOT/'examples/register-demo.bin').read_bytes()
        origin=f'http://127.0.0.1:{self.server.server_port}'
        status, body=self.request('POST','/api/run?mode=raw',data,{'Origin':origin,'Content-Type':'application/octet-stream'})
        result=json.loads(body); self.assertEqual(status,200); self.assertEqual(result['exit_code'],0); self.assertIn('halted RAX=14',result['stdout'])
    def test_run_original_elf(self):
        import json
        data=(ROOT/'examples/indirect-call.elf').read_bytes()
        status, body=self.request('POST','/api/run?mode=elf',data)
        self.assertEqual(status,200); result=json.loads(body); self.assertEqual(result['exit_code'],0); self.assertIn('halted RAX=42',result['stdout'])
    def test_invalid_code_is_not_reported_as_successful_execution(self):
        import json
        status, body=self.request('POST','/api/run?mode=raw',b'\x0f\x0b')
        self.assertEqual(status,200); result=json.loads(body); self.assertNotEqual(result['exit_code'],0); self.assertIn('invalid_instruction',result['stdout'])
    def test_rejections(self):
        self.assertEqual(self.request('POST','/api/run?mode=raw',b'a',{'Origin':'https://attacker.invalid'})[0],403)
        self.assertEqual(self.request('POST','/api/run?mode=raw&mode=elf',b'a')[0],400)
        self.assertEqual(self.request('POST','/api/run?mode=bogus',b'a')[0],400)
        self.assertEqual(self.request('POST','/api/run?mode=raw',b'')[0],413)
        self.assertEqual(self.oversized_headers('raw',65537),413)
        self.assertEqual(self.oversized_headers('elf',1048577),413)
        self.assertEqual(self.request('POST','/other',b'a')[0],404)
    def test_missing_interpreter(self):
        original=self.server.cli; self.server.cli=ROOT/'file-that-does-not-exist'
        try:
            self.assertEqual(self.request('POST','/api/run?mode=raw',b'a')[0],503)
        finally: self.server.cli=original

if __name__=='__main__': unittest.main()
