"""JSON file storage for the kids-calendar backend.

Data lives in <DATA_DIR>/courses.json as:
{
  "meixi":       [ {course}, ... ],
  "zhuangzhuang": [ {course}, ... ]
}
Thread-safe, atomic writes (tmp + rename). Good enough for a home NAS.
"""

import json
import os
import tempfile
import threading
import uuid

KIDS = ("meixi", "zhuangzhuang")

_lock = threading.RLock()
_data_dir = None
_db = None


def init(data_dir):
    global _data_dir, _db
    _data_dir = data_dir
    os.makedirs(_data_dir, exist_ok=True)
    path = _path()
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            _db = json.load(f)
    else:
        _db = {}
    for kid in KIDS:
        _db.setdefault(kid, [])
        # drop unknown top-level keys silently on next save


def _path():
    return os.path.join(_data_dir, "courses.json")


def _save():
    fd, tmp = tempfile.mkstemp(dir=_data_dir, suffix=".tmp")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        json.dump(_db, f, ensure_ascii=False, indent=1)
    os.replace(tmp, _path())


def list_courses(kid=None):
    with _lock:
        if kid:
            return [dict(c) for c in _db.get(kid, [])]
        return {k: [dict(c) for c in v] for k, v in _db.items()}


def list_today(kid, weekday):
    """weekday uses C tm_wday convention: 0=Sunday .. 6=Saturday."""
    with _lock:
        courses = [dict(c) for c in _db.get(kid, []) if c.get("day_of_week") == weekday]
    courses.sort(key=lambda c: c.get("start_time", ""))
    return courses


def upsert(course):
    """Insert or update by id. Empty id -> generate one. Returns the stored course."""
    with _lock:
        kid = course.get("kid")
        if kid not in KIDS:
            raise ValueError(f"unknown kid: {kid!r}")
        if not course.get("id"):
            course["id"] = "c_" + uuid.uuid4().hex[:12]
        items = _db[kid]
        for i, old in enumerate(items):
            if old.get("id") == course["id"]:
                items[i] = course
                _save()
                return dict(course)
        items.append(course)
        _save()
        return dict(course)


def delete(course_id):
    with _lock:
        for kid in KIDS:
            items = _db[kid]
            for i, old in enumerate(items):
                if old.get("id") == course_id:
                    del items[i]
                    _save()
                    return True
    return False
