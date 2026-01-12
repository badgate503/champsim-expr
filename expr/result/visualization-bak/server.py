import http.server
import socketserver
import os
import json
from urllib.parse import urlparse, parse_qs, unquote

PORT = 8000
BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

RESULT_BASE = os.path.abspath(os.path.join(os.path.dirname(__file__), '../data'))

def safe_result_dir(group_name):
    # group_name may be None or a simple folder name like 'baseline' or 'prophet'
    if not group_name:
        group_name = 'baseline'
    # prevent traversal
    group_name = os.path.normpath(group_name)
    if group_name.startswith('..') or os.path.isabs(group_name):
        return None
    candidate = os.path.abspath(os.path.join(RESULT_BASE, group_name))
    if not candidate.startswith(RESULT_BASE):
        return None
    return candidate

class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        # support /result_list or /result_list/<group>
        parsed = urlparse(self.path)
        path = parsed.path
        # support /result_groups -> list subdirectories under result/
        if path.startswith('/result_groups'):
            try:
                groups = [d for d in os.listdir(RESULT_BASE) if os.path.isdir(os.path.join(RESULT_BASE, d))]
                groups.sort()
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(groups).encode())
                return
            except Exception as e:
                self.send_response(500)
                self.end_headers()
                return
        if path.startswith('/result_list'):
            parts = path.split('/')
            group = None
            if len(parts) >= 3 and parts[2]:
                group = unquote(parts[2])
            
            result_dir = safe_result_dir(group)
            if not result_dir or not os.path.isdir(result_dir):
                self.send_response(404)
                self.end_headers()
                return
            files = [f for f in os.listdir(result_dir) if f.endswith('.txt')]
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(files).encode())
            return

        # support /result_data?file=...&group=...
        if path.startswith('/result_data'):
            query = parse_qs(parsed.query)
            filename = query.get('file', [None])[0]
            group = query.get('group', [None])[0]
            result_dir = safe_result_dir(group)
            if not filename or not result_dir:
                self.send_response(404)
                self.end_headers()
                return
            filename = os.path.normpath(unquote(filename))
            if filename.startswith('..') or os.path.isabs(filename):
                self.send_response(400)
                self.end_headers()
                return
            file_path = os.path.abspath(os.path.join(result_dir, filename))
            if not file_path.startswith(result_dir) or not os.path.isfile(file_path):
                self.send_response(404)
                self.end_headers()
                return
            with open(file_path, 'r') as f:
                data = f.read()
            self.send_response(200)
            self.send_header('Content-Type', 'text/plain; charset=utf-8')
            self.end_headers()
            self.wfile.write(data.encode())
            return

        # tracelist
        if path == '/tracelist':
            tracelist_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..','..', 'utils/tracelist'))
            if os.path.isfile(tracelist_path):
                with open(tracelist_path, 'r') as f:
                    data = f.read()
                self.send_response(200)
                self.send_header('Content-Type', 'text/plain; charset=utf-8')
                self.end_headers()
                self.wfile.write(data.encode())
                return
            else:
                self.send_response(404)
                self.end_headers()
                return

        # serve files under /result/ from BASE_DIR/result/
        if path.startswith('/data/'):
            file_path = os.path.join(BASE_DIR, path.lstrip('/'))
            print(file_path)
            if os.path.isfile(file_path):
                self.send_response(200)
                if file_path.endswith('.txt'):
                    ctype = 'text/plain; charset=utf-8'
                elif file_path.endswith('.png'):
                    ctype = 'image/png'
                else:
                    ctype = 'application/octet-stream'
                self.send_header('Content-Type', ctype)
                self.end_headers()
                with open(file_path, 'rb') as f:
                    self.wfile.write(f.read())
                return
            else:
                self.send_response(404)
                self.end_headers()
                return

        # Serve static files (index.html, etc.)
        if self.path == '/':
            self.path = '/index.html'
        return http.server.SimpleHTTPRequestHandler.do_GET(self)
        
if __name__ == '__main__':
    os.chdir(os.path.dirname(__file__))
    with socketserver.TCPServer(("", PORT), Handler) as httpd:
        print(f"Serving at http://localhost:{PORT}")
        httpd.serve_forever()
