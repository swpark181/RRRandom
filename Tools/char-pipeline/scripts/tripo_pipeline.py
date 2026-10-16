"""Image -> 3D model -> rigged FBX (-> optional preset animations) via Tripo API V3.

Usage:
    python tripo_pipeline.py ../input/hero.png
    python tripo_pipeline.py ../input/hero.png --animations preset:idle,preset:walk,preset:run
    python tripo_pipeline.py ../input/hero.png --resume      # reuse finished tasks from state.json

Requires env var TRIPO_API_KEY. Standard library only (runs on Unreal's bundled Python too).
Each finished task id is saved to output/<name>/state.json so a failed run can resume
without spending credits again.
"""
import argparse
import json
import mimetypes
import os
import sys
import time
import urllib.error
import urllib.request
import uuid
from pathlib import Path

BASE = "https://openapi.tripo3d.com/v3"
ROOT = Path(__file__).resolve().parent.parent


def api_key():
    key = os.environ.get("TRIPO_API_KEY")
    if not key and sys.platform == "win32":
        # setx로 방금 등록한 값은 이미 떠 있는 프로세스에 반영되지 않으므로 레지스트리에서 직접 읽는다.
        import winreg
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, "Environment") as k:
                key = winreg.QueryValueEx(k, "TRIPO_API_KEY")[0]
        except OSError:
            pass
    if not key:
        sys.exit("TRIPO_API_KEY 환경 변수가 없습니다.")
    return key


def request(method, path, body=None, data=None, content_type=None, retries=5):
    headers = {"Authorization": f"Bearer {api_key()}"}
    if body is not None:
        data = json.dumps(body).encode()
        content_type = "application/json"
    if content_type:
        headers["Content-Type"] = content_type
    for attempt in range(retries):
        req = urllib.request.Request(BASE + path, data=data, headers=headers, method=method)
        try:
            with urllib.request.urlopen(req, timeout=120) as r:
                res = json.loads(r.read())
        except urllib.error.HTTPError as e:
            detail = e.read().decode(errors="replace")
            if e.code == 429 and attempt < retries - 1:
                time.sleep(10 * (attempt + 1))
                continue
            if e.code == 401:
                sys.exit("인증 실패(401): TRIPO_API_KEY를 확인하세요.")
            sys.exit(f"{method} {path} 실패 (HTTP {e.code}): {detail}")
        if res.get("code") != 0:
            sys.exit(f"{method} {path} 실패: {res}")
        return res["data"]
    sys.exit(f"{method} {path}: 재시도 한도 초과")


def upload(path):
    boundary = uuid.uuid4().hex
    mime = mimetypes.guess_type(path.name)[0] or "application/octet-stream"
    data = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="file"; filename="{path.name}"\r\n'
        f"Content-Type: {mime}\r\n\r\n"
    ).encode() + path.read_bytes() + f"\r\n--{boundary}--\r\n".encode()
    return request("POST", "/files", data=data,
                   content_type=f"multipart/form-data; boundary={boundary}")["file_token"]


def wait(task_id, label):
    last = None
    while True:
        d = request("GET", f"/tasks/{task_id}")
        status, progress = d.get("status"), d.get("progress")
        if (status, progress) != last:
            print(f"  [{label}] {status} {progress}%", flush=True)
            last = (status, progress)
        if status == "success":
            return d
        if status not in ("queued", "running"):
            sys.exit(f"[{label}] {status}: {d.get('error_code')} {d.get('error_message')}")
        time.sleep(3)


def model_urls(output):
    """Generation tasks return output.model_url; rig/retarget return output.model_urls."""
    urls = []
    for key in ("model_url", "model_urls"):
        v = output.get(key)
        if isinstance(v, str):
            urls.append(v)
        elif isinstance(v, list):
            urls += [u for u in v if isinstance(u, str)]
        elif isinstance(v, dict):
            urls += [u for u in v.values() if isinstance(u, str)]
    return urls


def download(url, dest):
    with urllib.request.urlopen(url, timeout=300) as r:
        dest.write_bytes(r.read())
    print(f"  -> {dest.relative_to(ROOT)}")


def ext_of(url, fallback):
    suffix = Path(url.split("?")[0]).suffix
    return suffix or fallback


def run_step(state, key, label, create):
    """Create a task (unless state already has one) and wait for it."""
    if key not in state:
        state[key] = create()["task_id"]
        save(state)
    return wait(state[key], label)


def save(state):
    state["_path"].write_text(json.dumps({k: v for k, v in state.items() if k != "_path"}, indent=2))


def main():
    p = argparse.ArgumentParser()
    p.add_argument("image", type=Path)
    p.add_argument("--name", help="출력 폴더 이름 (기본: 이미지 파일 이름)")
    p.add_argument("--model-version", default="v3.1-20260211")
    p.add_argument("--face-limit", type=int, default=20000, help="Tripo 상한 20000")
    p.add_argument("--no-smart-low-poly", action="store_true",
                   help="게임용 저폴리 토폴로지 생성 끄기 (기본: 켬)")
    p.add_argument("--texture-quality", default="detailed", choices=["standard", "detailed", "extreme"])
    p.add_argument("--rig-version", default="v2.5-20260210")
    p.add_argument("--animations", default="", help="예: preset:idle,preset:walk (최대 5개, 개당 과금)")
    p.add_argument("--resume", action="store_true")
    args = p.parse_args()

    image = args.image.resolve()
    if not image.exists():
        sys.exit(f"이미지가 없습니다: {image}")
    name = args.name or image.stem
    out = ROOT / "output" / name
    out.mkdir(parents=True, exist_ok=True)

    state_path = out / "state.json"
    state = json.loads(state_path.read_text()) if args.resume and state_path.exists() else {}
    state["_path"] = state_path

    print("1/4 이미지 -> 3D 모델")
    gen = {
        "model_version": args.model_version,
        "face_limit": args.face_limit,
        "texture": True,
        "pbr": True,
        "texture_quality": args.texture_quality,
    }
    if not args.model_version.startswith("P1"):  # P1 모델은 smart_low_poly 필드 자체를 거부
        gen["smart_low_poly"] = not args.no_smart_low_poly
    d = run_step(state, "model_task", "model", lambda: request("POST", "/generation/image-to-model", {
        "file": {"type": image.suffix.lstrip(".").lower().replace("jpg", "jpeg"),
                 "file_token": upload(image)},
        **gen,
    }))
    for i, url in enumerate(model_urls(d["output"])):
        download(url, out / f"{name}_model{'' if i == 0 else i}{ext_of(url, '.glb')}")
    if d["output"].get("rendered_image_url"):
        download(d["output"]["rendered_image_url"], out / f"{name}_preview.webp")

    print("2/4 리깅 가능 여부 확인")
    d = run_step(state, "rigcheck_task", "rig-check", lambda: request(
        "POST", "/animations/rig-check", {"original_model_task_id": state["model_task"]}))
    riggable, rig_type = d["output"].get("riggable"), d["output"].get("rig_type") or "biped"
    print(f"  riggable={riggable}, rig_type={rig_type}")
    if not riggable:
        sys.exit("리깅 불가 판정입니다. 팔다리가 몸에서 떨어진 T/A-pose 이미지로 다시 시도하세요.")

    print("3/4 리깅 (Mixamo 스켈레톤, FBX)")
    d = run_step(state, "rig_task", "rig", lambda: request("POST", "/animations/rig", {
        "original_model_task_id": state["model_task"],
        "model_version": args.rig_version,
        "rig_type": rig_type,
        "spec": "mixamo",
        "out_format": "fbx",
    }))
    for i, url in enumerate(model_urls(d["output"])):
        download(url, out / f"{name}_rigged{'' if i == 0 else i}{ext_of(url, '.fbx')}")

    anims = [a.strip() for a in args.animations.split(",") if a.strip()]
    if anims:
        print(f"4/4 프리셋 애니메이션 {anims}")
        d = run_step(state, "retarget_task", "retarget", lambda: request("POST", "/animations/retarget", {
            "original_model_task_id": state["rig_task"],
            "animations": anims[:5],
            "out_format": "fbx",
            "animate_in_place": True,
        }))
        for i, url in enumerate(model_urls(d["output"])):
            download(url, out / f"{name}_anim{i}{ext_of(url, '.fbx')}")
    else:
        print("4/4 프리셋 애니메이션 생략 (언리얼에서 리타겟 예정)")

    print(f"\n완료: {out}")


if __name__ == "__main__":
    main()
