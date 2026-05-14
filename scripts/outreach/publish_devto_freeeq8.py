#!/usr/bin/env python3
import json
import os
import re
import sys
import urllib.request
from pathlib import Path

POST_PATH = Path("docs/outreach/api-publishing/FREEEQ8_DEVTO_POST.md")

def parse_frontmatter(text: str):
    match = re.match(r"^---\n(.*?)\n---\n(.*)$", text, re.S)
    if not match:
        raise SystemExit("Missing YAML-style frontmatter block.")
    raw_meta, body = match.groups()
    meta = {}
    for line in raw_meta.splitlines():
        if not line.strip() or ":" not in line:
            continue
        key, value = line.split(":", 1)
        value = value.strip()
        if value.lower() == "true":
            value = True
        elif value.lower() == "false":
            value = False
        meta[key.strip()] = value
    return meta, body.strip()

def main():
    api_key = os.environ.get("DEVTO_API_KEY")
    if not api_key:
        raise SystemExit("Set DEVTO_API_KEY first. Example: export DEVTO_API_KEY='your_key_here'")

    text = POST_PATH.read_text(encoding="utf-8")
    meta, body = parse_frontmatter(text)

    tags = [t.strip() for t in str(meta.get("tags", "")).split(",") if t.strip()]
    payload = {
        "article": {
            "title": meta["title"],
            "published": bool(meta.get("published", False)),
            "body_markdown": body,
            "tags": tags,
            "canonical_url": meta.get("canonical_url"),
            "description": meta.get("description", ""),
        }
    }

    req = urllib.request.Request(
        "https://dev.to/api/articles",
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Content-Type": "application/json",
            "api-key": api_key,
        },
        method="POST",
    )

    try:
        with urllib.request.urlopen(req, timeout=30) as res:
            print(res.read().decode("utf-8"))
    except Exception as e:
        print(f"DEV.to publish failed: {e}", file=sys.stderr)
        raise

if __name__ == "__main__":
    main()
