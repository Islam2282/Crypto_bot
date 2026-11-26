"""Собирает 3D-страницу: вставляет скетч Arduino в шаблон.

Выход:
  web/model-3d.html           — страница для публикации (без <html>/<head>)
  model-3d-standalone.html    — то же, открывается двойным щелчком в браузере
"""
import html
import pathlib

here = pathlib.Path(__file__).resolve().parent
root = here.parent
sketch = (root / "firmware/servo_lid/servo_lid.ino").read_text(encoding="utf-8")
page = (here / "model-3d.template.html").read_text(encoding="utf-8").replace("{{SKETCH}}", html.escape(sketch, quote=False))
(here / "model-3d.html").write_text(page, encoding="utf-8")
standalone = (
    '<!doctype html>\n<html lang="ru">\n<head>\n<meta charset="utf-8">\n'
    '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n'
    "<style>body{margin:0}</style>\n</head>\n<body>\n" + page + "\n</body>\n</html>\n"
)
(root / "model-3d-standalone.html").write_text(standalone, encoding="utf-8")
print("built", len(page), "bytes")
