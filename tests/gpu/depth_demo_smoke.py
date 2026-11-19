"""Check the standalone CPU rasterizer output, not hardware rendering."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory() as tmp:
    output = pathlib.Path(tmp) / 'depth.ppm'
    run = subprocess.run([sys.argv[1], str(output)], capture_output=True, text=True, check=True)
    assert 'not a PS5 GPU' in run.stdout
    data = output.read_bytes()
    header = b'P6\n320 180\n255\n'
    assert data.startswith(header) and len(data) == len(header) + 320 * 180 * 3
    def pixel(x, y):
        pos = len(header) + (y * 320 + x) * 3
        return data[pos:pos+3]
    assert pixel(0, 0) == bytes((12,18,32))
    assert pixel(150,90) == bytes((239,189,94))
    assert bytes((92,216,206)) in data[len(header):]
    print('depth demo PPM dimensions and known occlusion pixels verified')
