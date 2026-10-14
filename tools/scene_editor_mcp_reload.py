"""Keep stdio alive while atomically replacing validated, trusted local catalogs."""
import hashlib
import json
import queue
import re
import sys
import threading
from scene_editor_mcp_compat import RESERVED


class Catalog:
    def __init__(self, path):
        self.path = path
        self.signature = None
        self.digest = None
        self.methods = {}
        self.tools = []
        self.rejected = None
        self.refresh(required=True)

    def refresh(self, required=False):
        try:
            stat = self.path.stat()
            signature = (stat.st_ino, stat.st_mtime_ns, stat.st_size)
            if signature == self.signature:
                return False
            self.signature = signature
            if stat.st_size > 1048576:
                raise ValueError('catalog exceeds 1 MiB')
            source = self.path.read_bytes()
            digest = hashlib.sha256(source).hexdigest()
            if digest == self.digest:
                return False
            # This is application-owned executable source, never scene/user input.
            # Compile exact bytes; avoid importlib's timestamp/pycache stale reads.
            namespace = {'__name__': 'optic_live_catalog', '__file__': str(self.path)}
            exec(compile(source, str(self.path), 'exec'), namespace)
            methods = namespace['METHODS']
            tools = namespace['tool_list']()
            if not isinstance(methods, dict) or not methods or not isinstance(tools, list):
                raise ValueError('catalog must contain tools and method mappings')
            names = [tool['name'] for tool in tools]
            if RESERVED.intersection(names):
                raise ValueError('catalog cannot shadow fixed compatibility tools')
            if len(names) != len(set(names)) or set(names) != set(methods):
                raise ValueError('catalog names and method mappings disagree')
            for tool in tools:
                name = tool['name']
                if not isinstance(name, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,128}', name):
                    raise ValueError('invalid tool name')
                mapping = methods[name]
                if (not isinstance(mapping, tuple) or len(mapping) != 3
                        or not isinstance(mapping[0], str) or not isinstance(mapping[2], dict)
                        or not isinstance(tool.get('description'), str)):
                    raise ValueError('invalid editor method mapping')
                schema = tool['inputSchema']
                if (schema.get('type') != 'object' or schema.get('additionalProperties') is not False
                        or not isinstance(schema.get('properties'), dict)
                        or not isinstance(schema.get('required'), list)
                        or set(schema['properties']) != set(schema['required'])
                        or len(schema['required']) != len(set(schema['required']))):
                    raise ValueError('tools must retain strict required argument schemas')
            json.dumps(tools, allow_nan=False)
            changed = tools != self.tools or methods != self.methods
            self.methods, self.tools, self.digest = methods, tools, digest
            self.rejected = None
            return changed
        except Exception as error:
            if required:
                raise
            diagnostic = str(error)
            if diagnostic != self.rejected:
                print('optiC catalog update rejected: ' + diagnostic, file=sys.stderr, flush=True)
                self.rejected = diagnostic
            return False


def requests(stream):
    """One bounded reader; the owner thread alone writes protocol output/state."""
    inbox = queue.Queue(maxsize=16)
    end = object()

    def read():
        try:
            while True:
                line = stream.readline(131074)
                if not line:
                    break
                inbox.put(line)
        finally:
            inbox.put(end)

    threading.Thread(target=read, name='optic-mcp-input', daemon=True).start()
    while True:
        try:
            line = inbox.get(timeout=.5)
        except queue.Empty:
            yield None
            continue
        if line is end:
            return
        yield line
