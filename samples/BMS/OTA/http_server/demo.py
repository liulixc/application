import http.server
import socketserver
import socket
import os  # 确保已导入 os 模块

PORT = 8080
FIRMWARE_FILE = "test.fwpkg"  # 修改为 .fwpkg 文件

class OTAHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == f"/{FIRMWARE_FILE}":  # 动态匹配文件名
            try:
                # 获取文件大小（与 HTTP 头中的 Content-Length 一致）
                file_size = os.path.getsize(FIRMWARE_FILE)
                
                # 发送 HTTP 响应头
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")  # 适用于二进制文件
                self.send_header("Content-Length", str(file_size))
                self.end_headers()
                print(f"Sent Content-Length: {file_size}")  # 新增日志
                # 发送文件内容
                with open(FIRMWARE_FILE, "rb") as f:
                    self.wfile.write(f.read())
            except FileNotFoundError:
                self.send_error(404, f"File '{FIRMWARE_FILE}' not found")
            except Exception as e:
                self.send_error(500, str(e))
        else:
            self.send_error(404, "Path not found")

def get_ip_address():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    except Exception:
        ip = '127.0.0.1'
    finally:
        s.close()
    return ip

if __name__ == "__main__":
    # 启动前检查固件文件是否存在并显示大小
    try:
        # 检查文件是否存在
        if not os.path.exists(FIRMWARE_FILE):
            raise FileNotFoundError(f"Firmware file '{FIRMWARE_FILE}' not found!")
        
        # 获取并打印文件大小
        firmware_size = os.path.getsize(FIRMWARE_FILE)
        print(f"Firmware Info:")
        print(f"  Name: {FIRMWARE_FILE}")
        print(f"  Size: {firmware_size} bytes")
        
        # 启动服务器
        ip = get_ip_address()
        print(f"\nOTA Server running at http://{ip}:{PORT}")
        with socketserver.TCPServer(("", PORT), OTAHandler) as httpd:
            try:
                httpd.serve_forever()
            except KeyboardInterrupt:
                print("Server stopped by user.")
    except Exception as e:
        print(f"Error: {str(e)}")
        exit(1)