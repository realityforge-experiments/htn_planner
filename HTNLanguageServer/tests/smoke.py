# Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com

"""Exercise the actual LSP executable over stdio on Windows and Linux."""

import argparse
import io
import json
from pathlib import Path
import subprocess
import tempfile


def frame(message):
    payload = json.dumps(message).encode("utf-8")
    return f"Content-Length: {len(payload)}\r\n\r\n".encode("ascii") + payload


def decode_frames(data):
    stream = io.BytesIO(data)
    messages = []
    while header := stream.readline():
        assert header.startswith(b"Content-Length: "), f"Unexpected stdout: {header!r}"
        length = int(header.split(b":", 1)[1])
        assert stream.readline() == b"\r\n"
        payload = stream.read(length)
        assert len(payload) == length, "Truncated LSP response"
        messages.append(json.loads(payload))
    return messages


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("server", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    logs = root / "build" / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    source = """(:domain LspSmoke top_level_domain
    (:method (helper ?inp_value)
        (branch () ((!result ?inp_value))))
    (:method (run) top_level_method
        (branch () ((helper 42))))
)
"""
    with tempfile.TemporaryDirectory(prefix="lsp smoke ", dir=logs) as directory:
        # Spaces in the URI also exercise percent encoding on both platforms.
        document = Path(directory) / "smoke.domain"
        document.write_text(source, encoding="utf-8")
        uri = document.as_uri()
        position = {"line": 4, "character": source.splitlines()[4].index("helper") + 2}
        target = {"textDocument": {"uri": uri}, "position": position}
        completion_target = {"textDocument": {"uri": uri}, "position": {
            "line": 2, "character": source.splitlines()[2].index("?inp_value") + 4}}
        messages = [
            {"id": 1, "method": "initialize", "params": {}},
            {"method": "initialized", "params": {}},
            {"method": "textDocument/didOpen", "params": {"textDocument": {
                "uri": uri, "languageId": "htn", "version": 1, "text": source}}},
            {"id": 2, "method": "textDocument/definition", "params": target},
            {"id": 3, "method": "textDocument/completion", "params": completion_target},
            {"id": 4, "method": "htn/compile", "params": {"textDocument": {"uri": uri}}},
            {"method": "textDocument/didChange", "params": {
                "textDocument": {"uri": uri, "version": 2},
                "contentChanges": [{"text": source.replace("(helper 42)", "(missing_method 42)")}]}},
            {"id": 5, "method": "htn/compile", "params": {"textDocument": {"uri": uri}}},
            {"method": "textDocument/didClose", "params": {"textDocument": {"uri": uri}}},
            {"id": 6, "method": "shutdown"},
            {"method": "exit"},
        ]
        request = b"".join(frame({"jsonrpc": "2.0", **message}) for message in messages)
        result = subprocess.run([str(args.server.resolve())], input=request, capture_output=True,
                                cwd=root, timeout=30)
        assert result.returncode == 0, result.stderr.decode("utf-8", errors="replace")
        replies = decode_frames(result.stdout)
        responses = {message["id"]: message["result"] for message in replies if "id" in message}
        assert responses[1]["capabilities"]["definitionProvider"]
        assert responses[2]["uri"] == uri
        assert responses[2]["range"]["start"]["line"] == 1
        assert any(item["label"] == "?inp_value" for item in responses[3]), responses[3]
        assert responses[4]["success"], responses[4]
        assert not responses[5]["success"], responses[5]
        assert any("missing_method" in item["message"] for item in responses[5]["diagnostics"])
        diagnostics = [message["params"] for message in replies
                       if message.get("method") == "textDocument/publishDiagnostics"]
        assert len(diagnostics) == 3
        assert not diagnostics[0]["diagnostics"]
        assert diagnostics[1]["diagnostics"]
        assert not diagnostics[2]["diagnostics"]
        assert all(item["uri"] == uri for item in diagnostics)
        assert responses[6] is None
    print("PASS: LSP initialize, diagnostics, definition, completion, compile, dirty buffer, close and shutdown")


if __name__ == "__main__":
    main()
