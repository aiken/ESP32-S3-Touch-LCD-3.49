"""Kids-calendar backend — FastAPI app.

Endpoints:
  Web editor (session cookie after login):
    POST /api/login        {password} -> sets session cookie
    POST /api/logout
    GET  /api/courses      ?kid=meixi|zhuangzhuang (optional)
    POST /api/courses      course JSON or [course, ...]  (upsert)
    DELETE /api/courses    ?id=<course_id>
  Device (X-Device-Key header):
    GET  /api/today        ?kid=...   -> today's courses (server date)
    GET  /api/sync         ?kid=...   -> full course list for the kid
    GET  /api/health

Env config:
  ADMIN_PASSWORD  web editor password        (default: admin123)
  DEVICE_KEY      device API key             (default: devkey-change-me)
  DATA_DIR        where courses.json lives   (default: ./data)
  PORT            listen port                (default: 3000)
"""

import os
import secrets
from datetime import date

from fastapi import Depends, FastAPI, HTTPException, Request, Response
from fastapi.responses import FileResponse, JSONResponse
from pydantic import BaseModel, Field

from app import storage

ADMIN_PASSWORD = os.environ.get("ADMIN_PASSWORD", "admin123")
DEVICE_KEY = os.environ.get("DEVICE_KEY", "devkey-change-me")
DATA_DIR = os.environ.get("DATA_DIR", os.path.join(os.path.dirname(__file__), "..", "data"))

app = FastAPI(title="kids-calendar", docs_url="/docs", redoc_url=None)
storage.init(os.path.abspath(DATA_DIR))

_sessions = set()

KID_NAMES = {"meixi": "美熹", "zhuangzhuang": "壮壮"}


# ---------- auth ----------

def require_session(request: Request):
    token = request.cookies.get("session")
    if not token or token not in _sessions:
        raise HTTPException(status_code=401, detail="not logged in")


def require_device_key(request: Request):
    if request.headers.get("X-Device-Key", "") != DEVICE_KEY:
        raise HTTPException(status_code=401, detail="bad device key")


# ---------- models ----------

class LoginBody(BaseModel):
    password: str


class Course(BaseModel):
    id: str = ""
    kid: str = "meixi"
    name: str
    days: list[int] = []                 # recurring weekdays, 0=Sunday .. 6=Saturday
    day_of_week: int | None = None       # legacy single-day input, converted
    start_time: str = "08:00"
    end_time: str = "09:00"
    teacher: str = ""
    location: str = ""
    color: str = "#4A90D9"
    remind_before: int = 10


# ---------- web auth ----------

@app.post("/api/login")
def login(body: LoginBody, response: Response):
    if body.password != ADMIN_PASSWORD:
        raise HTTPException(status_code=401, detail="wrong password")
    token = secrets.token_hex(16)
    _sessions.add(token)
    response.set_cookie("session", token, httponly=True, samesite="lax",
                        max_age=30 * 24 * 3600)
    return {"status": "ok"}


@app.post("/api/logout")
def logout(request: Request, response: Response):
    _sessions.discard(request.cookies.get("session", ""))
    response.delete_cookie("session")
    return {"status": "ok"}


# ---------- web editor API (session) ----------

@app.get("/api/courses")
def get_courses(kid: str = "", _=Depends(require_session)):
    if kid:
        if kid not in storage.KIDS:
            raise HTTPException(400, "unknown kid")
        return {"status": "ok", "courses": storage.list_courses(kid)}
    return {"status": "ok", "kids": KID_NAMES, "courses": storage.list_courses()}


@app.post("/api/courses")
async def post_courses(request: Request, _=Depends(require_session)):
    body = await request.json()
    items = body if isinstance(body, list) else [body]
    saved = []
    for raw in items:
        try:
            course = Course(**raw).model_dump()
        except Exception as e:
            raise HTTPException(422, f"invalid course: {e}")
        try:
            saved.append(storage.upsert(course))
        except ValueError as e:
            raise HTTPException(400, str(e))
    return {"status": "ok", "saved": saved}


@app.delete("/api/courses")
def delete_course(id: str, _=Depends(require_session)):
    if not storage.delete(id):
        raise HTTPException(404, "course not found")
    return {"status": "ok"}


@app.post("/api/courses/clone")
def clone_course(body: dict, _=Depends(require_session)):
    """Clone one course; body: {"id": "...", "target_kid": "meixi|zhuangzhuang"(opt)}"""
    result = storage.clone(body.get("id", ""), body.get("target_kid"))
    if not result:
        raise HTTPException(404, "course not found")
    return {"status": "ok", "saved": result}


@app.post("/api/kids/clone")
def clone_kid(body: dict, _=Depends(require_session)):
    """Clone ALL courses from one kid to the other; body: {"from": "meixi", "to": "zhuangzhuang"}"""
    n = storage.clone_all(body.get("from", ""), body.get("to", ""))
    if n == 0:
        raise HTTPException(400, "nothing cloned (check from/to)")
    return {"status": "ok", "cloned": n}


# ---------- device API (device key) ----------

@app.get("/api/health")
def health():
    return {"status": "ok", "date": date.today().isoformat()}


@app.get("/api/today")
def api_today(kid: str = "meixi", _=Depends(require_device_key)):
    if kid not in storage.KIDS:
        raise HTTPException(400, "unknown kid")
    today = date.today()
    weekday = (today.weekday() + 1) % 7   # C tm_wday: 0=Sunday
    courses = storage.list_today(kid, weekday)
    for c in courses:
        c["day_of_week"] = weekday  # display hint for simple clients
    return {
        "status": "ok",
        "date": today.isoformat(),
        "weekday": weekday,
        "kid": kid,
        "courses": courses,
    }


@app.get("/api/sync")
def api_sync(kid: str = "meixi", _=Depends(require_device_key)):
    if kid not in storage.KIDS:
        raise HTTPException(400, "unknown kid")
    return {
        "status": "ok",
        "kid": kid,
        "courses": storage.list_courses(kid),
    }


# ---------- static web editor ----------

@app.get("/", include_in_schema=False)
def index():
    return FileResponse(os.path.join(os.path.dirname(__file__), "static", "index.html"))


@app.exception_handler(404)
def not_found(request, exc):
    return JSONResponse({"status": "error", "detail": "not found"}, status_code=404)


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=int(os.environ.get("PORT", "3000")))
