import subprocess
import json
import sys

def send_message(proc, msg):
    content = json.dumps(msg)
    header = f"Content-Length: {len(content)}\r\n\r\n"
    proc.stdin.write(header.encode('utf-8'))
    proc.stdin.write(content.encode('utf-8'))
    proc.stdin.flush()

def read_message(proc):
    content_length = 0
    while True:
        line = proc.stdout.readline().decode('utf-8')
        if not line:
            break
        line = line.strip()
        if not line:
            break
        if line.startswith("Content-Length:"):
            content_length = int(line.split(":")[1].strip())

    if content_length > 0:
        content = proc.stdout.read(content_length).decode('utf-8')
        return json.loads(content)
    return None

def main():
    proc = subprocess.Popen(
        ['build/tools/yuan-lsp/yuan-lsp'],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=sys.stderr
    )

    # Send initialize
    print("Sending initialize...")
    send_message(proc, {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {}
    })

    response = read_message(proc)
    print("Received:", json.dumps(response, indent=2))

    # Send textDocument/didOpen
    print("\nSending didOpen with bad syntax...")
    send_message(proc, {
        "jsonrpc": "2.0",
        "method": "textDocument/didOpen",
        "params": {
            "textDocument": {
                "uri": "file:///test.yu",
                "text": "func main() { let x: i32 = \"abc\"; }",
                "version": 1
            }
        }
    })

    response = read_message(proc)
    print("Received:", json.dumps(response, indent=2))

    # Exit
    send_message(proc, {"jsonrpc": "2.0", "method": "exit"})
    proc.wait()

if __name__ == "__main__":
    main()
