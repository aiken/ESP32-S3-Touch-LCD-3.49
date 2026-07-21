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
    changed = False
    for kid in KIDS:
        items = _db.setdefault(kid, [])
        for i, c in enumerate(items):
            nc = _normalize(c)
            if nc != c:
                items[i] = nc
                changed = True
    if changed:
        _save()


def _path():
    return os.path.join(_data_dir, "courses.json")


def _save():
    fd, tmp = tempfile.mkstemp(dir=_data_dir, suffix=".tmp")
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        json.dump(_db, f, ensure_ascii=False, indent=1)
    os.replace(tmp, _path())


def _normalize(course):
    """Accept legacy `day_of_week: int` and convert to `days: [int]`."""
    c = dict(course)
    if "days" not in c or not isinstance(c.get("days"), list) or not c["days"]:
        c["days"] = [c.get("day_of_week", 1)]
    c["days"] = sorted({int(d) for d in c["days"] if 0 <= int(d) <= 6})
    c.pop("day_of_week", None)
    return c


def list_courses(kid=None):
    with _lock:
        if kid:
            return [dict(c) for c in _db.get(kid, [])]
        return {k: [dict(c) for c in v] for k, v in _db.items()}


def list_today(kid, weekday):
    """weekday uses C tm_wday convention: 0=Sunday .. 6=Saturday.
    Recurring courses match when today's weekday is in their days[]."""
    with _lock:
        courses = [dict(c) for c in _db.get(kid, []) if weekday in c.get("days", [])]
    courses.sort(key=lambda c: c.get("start_time", ""))
    return courses


def upsert(course):
    """Insert or update by id. Empty id -> generate one. Returns the stored course."""
    course = _normalize(course)
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


def clone(course_id, target_kid=None):
    """Duplicate one course (new id), optionally into the other kid."""
    with _lock:
        for kid in KIDS:
            for old in _db[kid]:
                if old.get("id") == course_id:
                    c = dict(old)
                    c["id"] = "c_" + uuid.uuid4().hex[:12]
                    c["kid"] = target_kid if target_kid in KIDS else kid
                    _db[c["kid"]].append(c)
                    _save()
                    return dict(c)
    return None


def clone_all(from_kid, to_kid):
    """Clone ALL courses of from_kid into to_kid (append, new ids)."""
    if from_kid not in KIDS or to_kid not in KIDS or from_kid == to_kid:
        return 0
    with _lock:
        n = 0
        for old in _db[from_kid]:
            c = dict(old)
            c["id"] = "c_" + uuid.uuid4().hex[:12]
            c["kid"] = to_kid
            _db[to_kid].append(c)
            n += 1
        if n:
            _save()
    return n
