"""Smoke tests for the CSSON Python binding. Run: python3 -m pytest, or just
`python3 test_csson.py` (falls back to a plain runner if pytest is absent)."""
import csson

DOC = """cssonv1 {
  --org: "Acme";            /* a comment that must survive edits */
  --port: 8080;
  dept { --name: "Eng"; --n: 3; }
  dept { --name: "Ops"; }
}"""


def test_version():
    assert csson.version() == "1"


def test_loads_canonical():
    assert csson.loads(DOC) == {
        "org": "Acme", "port": 8080,
        "dept": [{"n": 3, "name": "Eng"}, {"name": "Ops"}],
    }


def test_get_pointer():
    assert csson.get(DOC, "/dept/0/n") == 3
    assert csson.get(DOC, "/org") == "Acme"


def test_set_preserves_comments():
    out = csson.set(DOC, "/org", "Beta")
    assert "/* a comment that must survive edits */" in out
    assert csson.loads(out)["org"] == "Beta"


def test_remove():
    out = csson.remove(DOC, "/port")
    assert "port" not in csson.loads(out)


def test_patch_atomic():
    out = csson.patch(DOC, [{"op": "replace", "path": "/org", "value": "Z"},
                            {"op": "add", "path": "/dept/0/lead", "value": "Alice"}])
    data = csson.loads(out)
    assert data["org"] == "Z" and data["dept"][0]["lead"] == "Alice"


def test_dumps_roundtrip():
    obj = {"app": {"port": 8080, "name": "x"}}
    assert csson.loads(csson.dumps(obj)) == {"app": [{"port": 8080, "name": "x"}]}


def test_validate():
    assert csson.validate("<integer>", "5") is True
    assert csson.validate("<integer>", "5.5") is False
    assert csson.validate("<color>", "red") is True


def test_error_on_no_root():
    try:
        csson.loads("body { --x: 1; }")
        assert False, "expected CssonError"
    except csson.CssonError as e:
        assert "root" in str(e)


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    passed = 0
    for fn in fns:
        fn()
        passed += 1
        print(f"  ok   {fn.__name__}")
    print(f"\n{passed} passed")
