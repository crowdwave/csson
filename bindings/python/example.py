#!/usr/bin/env python3
"""Runnable tour of the CSSON Python API. Run from this directory:

    python3 example.py

(Needs the native libcsson — see README. In the repo it's auto-found at
core/build/libcsson.so once you've built csson_shared.)
"""
import csson

CONFIG = """/* deployment config — comments are first-class and survive edits */
cssonv1 {
  --environment: production;
  --port: 8080;
  --timeout: 30s;

  database {
    --engine: postgres;
    --pool-min: 2;
    --pool-max: 10;
  }
  replica { --host: "db-1"; }
  replica { --host: "db-2"; }
}"""

print("CSSON version:", csson.version(), "\n")

# 1. Parse to a plain dict (canonical JSON).
data = csson.loads(CONFIG)
print("parsed:", data, "\n")

# 2. Read one value at a JSON Pointer.
print("pool-max:", csson.get(CONFIG, "/database/0/pool-max"))
print("replica[1].host:", csson.get(CONFIG, "/replica/1/host"), "\n")

# 3. Edit — comment-preserving, returns the new source text.
updated = csson.set(CONFIG, "/port", 9090)
print("port now 9090 and comment kept:",
      '"port":9090' in csson.dumps(csson.loads(updated)).replace(" ", "").replace("\n", "")
      or csson.loads(updated)["port"] == 9090,
      "| comment kept:", "/* deployment config" in updated, "\n")

# 4. RFC 6902 patch (atomic).
patched = csson.patch(CONFIG, [
    {"op": "replace", "path": "/environment", "value": "staging"},
    {"op": "add", "path": "/database/0/ssl", "value": True},
])
print("patched env:", csson.loads(patched)["environment"],
      "| ssl:", csson.loads(patched)["database"][0]["ssl"], "\n")

# 5. Build CSSON from a Python object.
print("dumps:\n" + csson.dumps({"service": {"name": "api", "replicas": 3}}))

# 6. Validate a value against an @property syntax.
print("\nvalidate <integer> 5   ->", csson.validate("<integer>", "5"))
print("validate <integer> 5.5 ->", csson.validate("<integer>", "5.5"))
print("validate <color> red   ->", csson.validate("<color>", "red"))
