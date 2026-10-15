"""Serve the supplied dashboard with explicit synthetic data; no hardware I/O."""
import argparse
import copy
import json
import threading
import time
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

WEB_DIR = Path(__file__).resolve().parents[1] / 'web'
INITIAL_STATE = {
    'temperature': 25.4, 'humidity': 62.0, 'iaq': '42',
    'waterLevel': 'normal', 'status': 'standby', 'uv': 'off',
    'autoMode': 'off', 'powerLevel': 0, 'targetHumidity': 65,
    'online': True, 'secondsSinceUpdate': 0, 'lastUpdateMs': 0,
    'lastRawJson': '{}', 'preview': True,
}


class PreviewServer(ThreadingHTTPServer):
    def __init__(self, address):
        self.state = copy.deepcopy(INITIAL_STATE)
        self.state_lock = threading.Lock()
        self.changed_at = time.monotonic()
        super().__init__(address, partial(Handler, directory=str(WEB_DIR)))


class Handler(SimpleHTTPRequestHandler):
    def json_response(self, payload, status=200):
        data = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        parsed = urlsplit(self.path)
        route, args = parsed.path, parse_qs(parsed.query)
        if route == '/state':
            with self.server.state_lock:
                state = dict(self.server.state)
                state['secondsSinceUpdate'] = int(time.monotonic() - self.server.changed_at)
            return self.json_response(state)
        commands = {
            '/uv_on': ('uv', 'on'), '/uv_off': ('uv', 'off'),
            '/auto_on': ('autoMode', 'on'), '/auto_off': ('autoMode', 'off'),
            '/power_running': ('status', 'running'), '/power_standby': ('status', 'standby'),
        }
        if route in commands or route in {'/manual_change', '/set_target', '/set_strength'}:
            with self.server.state_lock:
                if route in commands:
                    key, value = commands[route]
                    self.server.state[key] = value
                elif route != '/manual_change':
                    key, parameter = ('targetHumidity', 'value') if route == '/set_target' else ('powerLevel', 'level')
                    try:
                        value = int(args.get(parameter, [''])[0])
                    except ValueError:
                        return self.json_response({'error': 'Expected integer parameter'}, 400)
                    self.server.state[key] = max(0, min(100, value))
                if not route.startswith('/power_'):
                    self.server.state['status'] = 'running'
                self.server.changed_at = time.monotonic()
            return self.json_response({'ok': True, 'preview': True})
        if route in {'/', '/index.html'}:
            text = (WEB_DIR / 'index.html').read_text()
            banner = '<aside style="padding:8px;text-align:center;background:#fff0bf;color:#3f3000;font:12px sans-serif">SOFTWARE PREVIEW — SYNTHETIC DATA — NO HARDWARE CONNECTED</aside>'
            data = text.replace('<body>', '<body>' + banner).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)
            return
        return super().do_GET()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8080)
    args = parser.parse_args()
    server = PreviewServer(('127.0.0.1', args.port))
    print(f'Synthetic software preview: http://127.0.0.1:{server.server_port}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
