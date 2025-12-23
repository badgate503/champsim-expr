import http.server
import socketserver
import os
import json
from urllib.parse import urlparse, parse_qs, unquote

PORT = 8000
RESULT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '../result'))

class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith('/result_list'):
            files = [f for f in os.listdir(RESULT_DIR) if f.endswith('.txt')]
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()
            self.wfile.write(json.dumps(files).encode())
        elif self.path.startswith('/result_data'):
            query = parse_qs(urlparse(self.path).query)
            filename = query.get('file', [None])[0]
            if filename and os.path.isfile(os.path.join(RESULT_DIR, filename)):
                with open(os.path.join(RESULT_DIR, filename), 'r') as f:
                    data = f.read()
                self.send_response(200)
                self.send_header('Content-Type', 'text/plain; charset=utf-8')
                self.end_headers()
                self.wfile.write(data.encode())
            else:
                self.send_response(404)
                self.end_headers()
        elif self.path == '/tracelist':
            tracelist_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '../tracelist'))
            if os.path.isfile(tracelist_path):
                with open(tracelist_path, 'r') as f:
                    data = f.read()
                self.send_response(200)
                self.send_header('Content-Type', 'text/plain; charset=utf-8')
                self.end_headers()
                self.wfile.write(data.encode())
            else:
                self.send_response(404)
                self.end_headers()
        else:
            # Serve static files (index.html, etc.)
            if self.path == '/':
                self.path = '/index.html'
            return http.server.SimpleHTTPRequestHandler.do_GET(self)

if __name__ == '__main__':
    os.chdir(os.path.dirname(__file__))
    with socketserver.TCPServer(("", PORT), Handler) as httpd:
        print(f"Serving at http://localhost:{PORT}")
        httpd.serve_forever()
