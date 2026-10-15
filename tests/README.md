# Host checks

From the repository root:

```bash
python3 -m pip install -r tools/requirements.txt
python3 -m unittest discover -s tests -v
node --check web/app.js
```

The tests import the original serial mock's command handler without opening a port. They cover wake/manual-mode semantics and the derived voice reference. They do not simulate the complete Mega loop or prove voice-module compatibility.
