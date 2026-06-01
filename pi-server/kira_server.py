# ════════════════════════════════════════════════════════════════
#  Project Cortex — Cerebro AI Server
#  https://github.com/YOUR_USERNAME/project-cortex
#
#  Kira + Rumi AI desktop robots powered by local LLM
#  Whisper STT → Ollama LLM → gTTS → M5Stack CoreS3 SE
# ════════════════════════════════════════════════════════════════
"""
kira_server.py  —  Brain for Kira + Rumi AI robots
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Setup:   bash install.sh
Run:     source venv/bin/activate && python kira_server.py
Health:  http://YOUR_CEREBRO_IP:5005/health
"""

import io, os, json, wave, logging, hashlib, random, requests, time, threading
from collections import defaultdict
from pathlib import Path
from datetime import datetime
from werkzeug.utils import secure_filename

try:
    from zoneinfo import ZoneInfo
    TIMEZONE = ZoneInfo("Australia/Brisbane")
except ImportError:
    TIMEZONE = None

from flask import Flask, request, Response, send_file
from faster_whisper import WhisperModel
from gtts import gTTS
from pydub import AudioSegment

# ─── Config ───────────────────────────────────────────────
SERVER_PORT     = 5005
WHISPER_MODEL   = "medium"
OLLAMA_HOST     = os.environ.get("OLLAMA_HOST", "http://localhost:11434")
OLLAMA_MODEL    = os.environ.get("OLLAMA_MODEL", "qwen2.5:14b")
CHARACTERS_FILE = Path(__file__).parent / "characters.json"
CACHE_DIR       = Path(__file__).parent / "audio_cache"
HISTORY_DIR     = Path(__file__).parent / "conversation_history"
OTA_KEY     = "change_this_key"                              # change this to something personal
OTA_DIR         = Path(__file__).parent / "firmware"
MAX_HISTORY     = 20
SAVE_HISTORY    = True
OTA_ENABLED     = True
OTA_KEY         = "change_this_key"       # change this to something personal

# ─── Sleep / wake schedule (Brisbane, no DST) ─────────────
WAKE_HOUR    = 7;  WAKE_MINUTE  = 30
SLEEP_HOUR   = 19; SLEEP_MINUTE = 30
_sleep_override = None

# ─── Follow-up window ─────────────────────────────────────
# After Rumi responds, the next utterance counts as a reply
# even without saying her name — great for short answers like "pink"
last_response_time = {}
FOLLOWUP_TIMEOUT_ADULT = 45
FOLLOWUP_TIMEOUT_KID   = 180   # 3 minutes — kids get distracted, give them time

# ─── Setup ────────────────────────────────────────────────
logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")
log = logging.getLogger("kira")

app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 8 * 1024 * 1024  # 8MB for firmware uploads
_chat_lock = threading.Semaphore(1)  # one robot through Whisper+Ollama at a time
stt = WhisperModel(WHISPER_MODEL, device="cpu", compute_type="int8")
for d in [CACHE_DIR, HISTORY_DIR, OTA_DIR]:
    d.mkdir(exist_ok=True)

with open(CHARACTERS_FILE) as f:
    CHARACTERS = { r["id"]: r for r in json.load(f)["robots"] }

log.info(f"Loaded {len(CHARACTERS)} characters: {list(CHARACTERS.keys())}")

# Per-robot runtime state for dashboard controls
robot_state = {rid: {"mode": char.get("mode","adult"), "led": True, "volume": 180}
               for rid, char in CHARACTERS.items()}
log.info(f"Ollama: {OLLAMA_HOST} — model: {OLLAMA_MODEL}")

# Warm up Ollama on startup
import threading
def _warmup():
    OTA_DIR.mkdir(exist_ok=True)
    try:
        requests.post(f"{OLLAMA_HOST}/api/chat", json={
            "model": OLLAMA_MODEL,
            "messages": [{"role":"user","content":"hi"}],
            "stream": False, "options": {"num_predict": 1}
        }, timeout=120)
        log.info("Ollama warmed up")
    except Exception as e:
        log.warning(f"Warmup failed: {e}")
threading.Thread(target=_warmup, daemon=True).start()

sessions     = defaultdict(list)
robot_queue  = defaultdict(list)   # inter-robot message queue
quiet_until  = defaultdict(float)  # robot_id → epoch when quiet mode expires

QUIET_KEYWORDS = ["quiet please","go quiet","shh","be quiet","hush",
                  "quiet time","silence please","shut up"]
WAKE_KEYWORDS  = ["you can chat now","wake up","chat now","talk now",
                  "you can talk","come back","stop being quiet"]
QUIET_DURATION = 7200   # 2 hours in seconds

# ─── History helpers ──────────────────────────────────────
def history_file(robot_id): return HISTORY_DIR / f"{robot_id}_history.json"

def load_history(robot_id):
    path = history_file(robot_id)
    if not path.exists(): return []
    try:
        with open(path) as f:
            msgs = json.load(f).get("messages", [])[-MAX_HISTORY:]
        log.info(f"[{robot_id}] Loaded {len(msgs)} messages")
        return msgs
    except: return []

def save_history_file(robot_id, messages, robot_name):
    if not SAVE_HISTORY: return
    path = history_file(robot_id)
    try:
        existing = []
        if path.exists():
            with open(path) as f: existing = json.load(f).get("messages", [])
        new_n = len(messages) - len(existing)
        if new_n > 0: existing.extend(messages[-new_n:])
        with open(path, "w") as f:
            json.dump({"robot": robot_name, "updated": datetime.now().isoformat(),
                       "messages": existing}, f, indent=2)
    except Exception as e:
        log.warning(f"[{robot_id}] Could not save history: {e}")

def log_exchange(robot_id, robot_name, heard, reply, kid):
    try:
        with open(HISTORY_DIR / f"{robot_id}_log.txt", "a") as f:
            ts   = datetime.now().strftime("%Y-%m-%d %H:%M")
            mode = "👶 kid" if kid else "adult"
            f.write(f"[{ts}] [{mode}]\n  You:   {heard}\n  {robot_name}: {reply}\n\n")
    except: pass

for rid in CHARACTERS:
    sessions[rid] = load_history(rid)

# ─── Addressing ───────────────────────────────────────────
MISHEARINGS = {
    "rumi": ["from me","roomie","roomy","rumi","rumy","room me","roome","lumi","numi"],
    "kira": ["kira","keira","keer","keerah","kura"],
}

def is_addressed(text, robot_name, kid=False):
    if not text or len(text.strip()) < 2: return False
    words  = text.lower().split()
    name   = robot_name.lower()
    first5 = " ".join(words[:5])

    # Direct name or common mishearing
    extras = MISHEARINGS.get(name, [])
    triggers = [name, f"hey {name}", f"hi {name}", f"ok {name}",
                f"oi {name}", f"yo {name}"] + extras + [f"hey {m}" for m in extras]
    if any(t in first5 for t in triggers):
        return True

    # Kid mode: ONLY accept one-word/short answers when within follow-up window
    # (handled by is_follow_up separately — don't auto-accept random words here)

    return False

def is_follow_up(robot_id, heard):
    """True when within the follow-up window — catches short answers like 'pink' or 'yes'."""
    if robot_id not in last_response_time: return False
    elapsed = time.time() - last_response_time[robot_id]
    char    = CHARACTERS.get(robot_id, {})
    timeout = FOLLOWUP_TIMEOUT_KID if char.get("mode") == "kid" else FOLLOWUP_TIMEOUT_ADULT
    if elapsed > timeout: return False
    # Don't steal if the OTHER robot is explicitly named
    heard_lower = heard.lower()
    if robot_id == "robot2" and "kira" in heard_lower: return False
    if robot_id == "robot1" and "rumi" in heard_lower: return False
    return True

# ─── Sleep ────────────────────────────────────────────────
def is_sleeping():
    if _sleep_override is not None: return _sleep_override
    now     = datetime.now(TIMEZONE) if TIMEZONE else datetime.now()
    wakeup  = now.replace(hour=WAKE_HOUR,  minute=WAKE_MINUTE,  second=0, microsecond=0)
    bedtime = now.replace(hour=SLEEP_HOUR, minute=SLEEP_MINUTE, second=0, microsecond=0)
    return not (wakeup <= now < bedtime)

# ─── Action detection ─────────────────────────────────────
# Only trigger actions from the AI REPLY, not from the user's words.
# This stops "dinosaur" in a question triggering a roar before Rumi even answers.

DANCE_MARKERS = ["dance party time", "one two three dance", "dance party!", "🕺", "🎵", "🎶", "boogie"]
ROAR_MARKERS  = ["roarrr", "rawr", "raarrr", "growl", "r-o-a-r"]

def detect_action(user_text, reply_text):
    # Check user text for explicit music request (don't need AI to say it)
    user_lower  = user_text.lower()
    reply_lower = reply_text.lower()
    if any(m in user_lower for m in MUSIC_KEYWORDS): return "play_music"
    if any(m in reply_lower for m in DANCE_MARKERS): return "dance_party"
    if any(m in reply_lower for m in ROAR_MARKERS):  return "roar"
    return "none"

# ─── Sound effect detection ──────────────────────────────
# Detects keywords in the AI reply and returns sound effects to play
# The robot plays these as actual tones AFTER speaking

# ─── Music action detection ───────────────────────────────
MUSIC_KEYWORDS = ["play music","play a song","play immortals","immortals",
                  "play something","some music","a song please","sing me",
                  "play fall out","fallout boy","fall out boy","bg music"]

# ─── Nintendo Talking Flower detection ────────────────────
# The Talking Flower from Super Mario Bros. Wonder announces time hourly,
# makes random philosophical comments, reacts to temperature etc.
FLOWER_TIME_WORDS  = ["o'clock", "oclock", "it's one", "it's two", "it's three",
                      "it's four", "it's five", "it's six", "it's seven", "it's eight",
                      "it's nine", "it's ten", "it's eleven", "it's twelve",
                      "wakey wakey", "time for bed", "goodnight"]
FLOWER_PHRASES     = ["planet is spinning", "it's good to be alive", "feel like you're missing",
                      "ocean tastes like tears", "nothing to report", "nice to space out",
                      "batteries are almost dead", "what are batteries", "psst can i talk",
                      "psst... can i", "wonderrr", "sometimes it's nice", "have you had lunch",
                      "perfect weather", "something special", "i'll keep quiet",
                      "doing great", "you can do it", "wheeeee"]

def is_flower_speech(text: str) -> str:
    """Returns 'time', 'flower', or '' depending on what kind of flower speech it is."""
    t = text.lower()
    if any(w in t for w in FLOWER_TIME_WORDS):  return "time"
    if any(w in t for w in FLOWER_PHRASES):     return "flower"
    return ""

FLOWER_PROMPT_KIRA = """You are Kira, a massive gaming nerd. The Nintendo Talking Flower 
(from Super Mario Bros. Wonder) sitting nearby just said something to you. 
You know this flower from the game and find it hilarious/charming that there's a real one. 
React to what it said in 1 short funny sentence — in character as a gaming nerd. 
You can address the flower directly. No markdown."""

FLOWER_PROMPT_RUMI_KID = """You are Rumi, a friendly robot companion for {kid_name} who is 4 years old. 
The Nintendo Talking Flower nearby just said something! It's like a real life flower from Mario! 
React in 1 very short excited sentence, in super simple words a 4-year-old understands. 
Address the flower or {kid_name}. No markdown."""

# ─── Sound effect detection ───────────────────────────────
SFX_MAP = {
    "sfx_rocket":  ["rocket","spaceship","blast off","whoosh","zoom","launching"],
    "sfx_notes":   ["musical note","la la la","do re mi","sing a song","♪","♫"],
    "sfx_laser":   ["laser","zap","pew pew","ray gun","blaster","zapped"],
    "sfx_magic":   ["magic","abracadabra","spell","wizard","twinkle","sparkle","✨"],
    "sfx_bounce":  ["boing","bounce","spring","bouncy"],
    "sfx_win":     ["hooray","yay!","yahoo","you did it","well done","amazing"],
    "sfx_alarm":   ["uh oh","oh no","danger","warning","alert"],
    "sfx_spring":  ["sproing","jump","leap","boing"],
}

def detect_sfx(reply_text: str) -> str:
    reply_lower = reply_text.lower()
    found = [name for name, keywords in SFX_MAP.items()
             if any(kw in reply_lower for kw in keywords)]
    return ",".join(found) if found else "none"

# ─── Weather (Open-Meteo, free, no API key) ─────────────────
WEATHER_LAT  = -27.4705
WEATHER_LON  =  153.0260
WEATHER_CITY = "Brisbane"

WMO_CODES = {
    0:"clear skies", 1:"mainly clear", 2:"partly cloudy", 3:"overcast",
    45:"foggy", 48:"foggy", 51:"light drizzle", 53:"drizzle", 55:"heavy drizzle",
    61:"light rain", 63:"rain", 65:"heavy rain", 80:"showers", 81:"showers",
    82:"heavy showers", 95:"thunderstorms", 96:"thunderstorms", 99:"thunderstorms",
    71:"snow", 73:"snow", 75:"heavy snow",
}

def get_weather():
    try:
        r = requests.get(
            f"https://api.open-meteo.com/v1/forecast"
            f"?latitude={WEATHER_LAT}&longitude={WEATHER_LON}"
            f"&current=temperature_2m,weathercode"
            f"&timezone=Australia%2FBrisbane", timeout=5)
        if r.status_code == 200:
            d    = r.json()["current"]
            temp = round(d["temperature_2m"])
            desc = WMO_CODES.get(d["weathercode"], "conditions unknown")
            return f"{temp} degrees and {desc} in {WEATHER_CITY}"
    except Exception as e:
        log.warning(f"[Weather] Failed: {e}")
    return None

# ─── TTS (cached) ─────────────────────────────────────────
def tts_pcm(text, tld="com.au"):
    key  = hashlib.md5(f"{text}{tld}".encode()).hexdigest()[:12]
    path = CACHE_DIR / f"{key}.raw"
    if path.exists(): return path.read_bytes()
    buf = io.BytesIO()
    gTTS(text=text, lang="en", tld=tld).write_to_fp(buf)
    buf.seek(0)
    pcm = (AudioSegment.from_mp3(buf)
           .set_channels(1).set_frame_rate(16000).set_sample_width(2)).raw_data
    path.write_bytes(pcm)
    return pcm

# ─── STT ──────────────────────────────────────────────────
def transcribe(raw_pcm, sr=16000):
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wf:
        wf.setnchannels(1); wf.setsampwidth(2)
        wf.setframerate(sr); wf.writeframes(raw_pcm)
    buf.seek(0)
    segs, _ = stt.transcribe(buf, language="en", beam_size=3,
                              initial_prompt="Rumi, Kira, hey Rumi, dance party, dinosaur")
    return " ".join(s.text for s in segs).strip()

# ─── Ollama ───────────────────────────────────────────────
def ask_ollama(session, robot_id, text, kid, kid_name="buddy"):
    char   = CHARACTERS.get(robot_id, list(CHARACTERS.values())[0])
    system = char.get("kid_prompt" if kid else "adult_prompt", "You are a helpful robot.")
    system = system.replace("YOUR_SONS_NAME", kid_name)

    hist = sessions[session]
    hist.append({"role": "user", "content": text})
    if len(hist) > MAX_HISTORY:
        sessions[session] = hist[-MAX_HISTORY:]

    resp = requests.post(f"{OLLAMA_HOST}/api/chat", json={
        "model":    OLLAMA_MODEL,
        "messages": [{"role": "system", "content": system}] + sessions[session],
        "stream":   False,
        "options":  {"num_predict": 60 if kid else 150, "temperature": 0.7},
    }, timeout=120)
    resp.raise_for_status()
    reply = resp.json()["message"]["content"].strip()

    sessions[session].append({"role": "assistant", "content": reply})
    save_history_file(robot_id, sessions[session], char["name"])

    log.info(f"[{char['name']}{'👶' if kid else ''}] {reply}")
    return reply

# ─── Routes ───────────────────────────────────────────────
@app.route("/health")
def health():
    try:    ollama_ok = requests.get(f"{OLLAMA_HOST}/api/tags", timeout=3).status_code == 200
    except: ollama_ok = False
    return {"status": "ok", "robots": list(CHARACTERS.keys()),
            "whisper": WHISPER_MODEL, "model": OLLAMA_MODEL,
            "ollama": OLLAMA_HOST, "ollama_live": ollama_ok,
            "sleeping": is_sleeping(),
            "wake_at": f"{WAKE_HOUR:02d}:{WAKE_MINUTE:02d}",
            "sleep_at": f"{SLEEP_HOUR:02d}:{SLEEP_MINUTE:02d}"}

@app.route("/chat", methods=["POST"])
def chat():
    session  = request.headers.get("X-Session",      "default")
    robot_id = request.headers.get("X-Robot-ID",     "robot1")
    mode     = request.headers.get("X-Mode",         "adult")
    kid_name = request.headers.get("X-Kid-Name",     "buddy")
    sr       = int(request.headers.get("X-Sample-Rate", 16000))
    kid      = (mode == "kid")
    raw_pcm  = request.data
    char     = CHARACTERS.get(robot_id, list(CHARACTERS.values())[0])
    name     = char["name"]
    tld      = char.get("voice_tld", "com.au")

    if not raw_pcm or len(raw_pcm) < 800: return Response(status=204)
    if is_sleeping():                      return Response(status=204)

    # Queue if both robots hit at same time — second waits, doesn't fail
    acquired = _chat_lock.acquire(timeout=30)
    if not acquired:
        log.warning(f"[{name}] Timed out waiting for chat lock")
        return Response(status=204)

    try:
        heard = transcribe(raw_pcm, sr)
        log.info(f"[STT:{name}] '{heard}'")

        if not heard or len(heard.strip()) < 2:
            return Response(status=204)

        # Wake word always works even in quiet mode
        heard_lower = heard.lower()
        if any(w in heard_lower for w in WAKE_KEYWORDS):
            quiet_until[robot_id] = 0
            log.info(f"[{name}] Quiet mode OFF")
            pcm = tts_pcm("I'm back! What's up?", tld)
            last_response_time[robot_id] = time.time()
            return Response(pcm, mimetype="application/octet-stream", headers={
                "X-Reply-Text": "I'm back!", "X-Robot-Name": name,
                "X-Action": "none", "X-SFX": "none"})

        # Quiet mode — robot is silent
        if quiet_until[robot_id] > time.time():
            remaining = int((quiet_until[robot_id] - time.time()) / 60)
            log.info(f"[{name}] Quiet mode active ({remaining}min left) — ignoring")
            return Response(status=204)

        # Quiet request
        if any(w in heard_lower for w in QUIET_KEYWORDS):
            quiet_until[robot_id] = time.time() + QUIET_DURATION
            log.info(f"[{name}] Quiet mode ON for 2 hours")
            pcm = tts_pcm("Okay, going quiet. Say 'you can chat now' when you want me back.", tld)
            return Response(pcm, mimetype="application/octet-stream", headers={
                "X-Reply-Text": "Going quiet!", "X-Robot-Name": name,
                "X-Action": "none", "X-SFX": "none"})

        addressed  = is_addressed(heard, name, kid)
        follow_up  = is_follow_up(robot_id, heard)
        flower_type = is_flower_speech(heard)

        if not (addressed or follow_up):
            # Check if this is the Nintendo Talking Flower speaking nearby
            if flower_type:
                log.info(f"[{name}] 🌸 Flower heard ({flower_type}): '{heard}'")

                # Build a special one-shot flower reaction
                if kid:
                    system = FLOWER_PROMPT_RUMI_KID.format(kid_name=kid_name)
                else:
                    system = FLOWER_PROMPT_KIRA

                # Add context about what the flower said
                flower_msg = f"The Talking Flower just said: \"{heard}\""
                if flower_type == "time":
                    flower_msg += " (It was announcing the time)"

                resp = requests.post(f"{OLLAMA_HOST}/api/chat", json={
                    "model":    OLLAMA_MODEL,
                    "messages": [{"role":"system","content":system},
                                 {"role":"user",  "content":flower_msg}],
                    "stream":   False,
                    "options":  {"num_predict": 60, "temperature": 0.9},
                }, timeout=30)
                resp.raise_for_status()
                reply  = resp.json()["message"]["content"].strip()
                action = detect_action(heard, reply)
                log.info(f"[{name}🌸] {reply}")
                last_response_time[robot_id] = time.time()
                pcm = tts_pcm(reply, tld)
                return Response(pcm, mimetype="application/octet-stream", headers={
                    "X-Reply-Text":   reply[:80],
                    "X-Robot-Name":   name,
                    "X-Action":       action,
                    "X-SFX":          detect_sfx(reply),
                    "X-Expression":   detect_expression(reply),
                    "Content-Length": str(len(pcm)),
                })
            else:
                log.info(f"[{name}] ignored: '{heard}'")
                return Response(status=204)

        if follow_up and not addressed:
            log.info(f"[{name}] follow-up: '{heard}'")

        reply  = ask_ollama(session, robot_id, heard, kid, kid_name)
        action = detect_action(heard, reply)

        # After dance party, wipe history so model starts fresh next conversation
        if action == "dance_party":
            sessions[robot_id] = []
            log.info(f"[{robot_id}] History cleared after dance party")

        log_exchange(robot_id, name, heard, reply, kid)
        last_response_time[robot_id] = time.time()

        # Check if this was a cross-robot message
        other_id, other_char, cross_msg = detect_robot_target(heard, robot_id)
        if other_id and cross_msg:
            try:
                other_kid  = (other_char.get("mode","adult") == "kid")
                other_reply = ask_ollama(other_id, other_id,
                                         f"{name} says: {cross_msg}", other_kid, kid_name)
                other_pcm   = tts_pcm(other_reply, other_char.get("voice_tld","com.au"))
                robot_queue[other_id].append({
                    "from": name, "text": other_reply, "pcm": other_pcm
                })
                log.info(f"Queued message for {other_id} from {name}")
            except Exception as e:
                log.warning(f"Failed to queue cross-robot message: {e}")

        pcm = tts_pcm(reply, tld)
        return Response(pcm, mimetype="application/octet-stream", headers={
            "X-Reply-Text":   reply[:100].replace("\n", " ").encode("latin-1","ignore").decode("latin-1"),
            "X-Robot-Name":   name,
            "X-Action":       action,
            "X-SFX":          detect_sfx(reply),
            "X-Heard":        heard[:80],
            "Content-Length": str(len(pcm)),
        })

    except Exception as e:
        log.error(f"Error: {e}", exc_info=True)
        pcm = tts_pcm("Oops, something went wrong!", tld)
        return Response(pcm, mimetype="application/octet-stream",
                        headers={"X-Reply-Text": "Error!", "X-Robot-Name": name, "X-Action": "none"})
    finally:
        if acquired:
            _chat_lock.release()

@app.route("/reset", methods=["POST"])
def reset():
    session  = request.headers.get("X-Session",  "default")
    robot_id = request.headers.get("X-Robot-ID", session)
    sessions.pop(session, None)
    log.info(f"[{robot_id}] Memory cleared")
    return {"status": "cleared"}

@app.route("/history/<robot_id>", methods=["GET"])
def get_history(robot_id):
    path = history_file(robot_id)
    if not path.exists(): return {"robot_id": robot_id, "messages": []}
    with open(path) as f: return json.load(f)

@app.route("/history/<robot_id>", methods=["DELETE"])
def delete_history(robot_id):
    path = history_file(robot_id)
    if path.exists(): path.unlink()
    sessions.pop(robot_id, None)
    log.info(f"[{robot_id}] History wiped")
    return {"status": "wiped"}

@app.route("/sleep", methods=["POST"])
def force_sleep():
    global _sleep_override; _sleep_override = True
    return {"status": "sleeping", "override": True}

@app.route("/wake", methods=["POST"])
def force_wake():
    global _sleep_override; _sleep_override = False
    return {"status": "awake", "override": True}

@app.route("/schedule", methods=["POST"])
def restore_schedule():
    global _sleep_override; _sleep_override = None
    return {"status": "sleeping" if is_sleeping() else "awake", "override": None}

@app.route("/status")
def status():
    now = datetime.now(TIMEZONE) if TIMEZONE else datetime.now()
    quiet_info = {rid: int(max(0, quiet_until[rid]-time.time()))
                  for rid in CHARACTERS}
    return {"sleeping": is_sleeping(), "override": _sleep_override,
            "time": now.strftime("%H:%M"),
            "wake_at": f"{WAKE_HOUR:02d}:{WAKE_MINUTE:02d}",
            "sleep_at": f"{SLEEP_HOUR:02d}:{SLEEP_MINUTE:02d}",
            "robots": list(CHARACTERS.keys()),
            "quiet_seconds_remaining": quiet_info}

# ─── Inter-robot messaging ────────────────────────────────
def detect_robot_target(heard: str, from_robot_id: str):
    """Detect 'tell Rumi...' / 'ask Kira...' patterns. Returns (target_id, message) or (None,None)."""
    heard_lower = heard.lower()
    for rid, char in CHARACTERS.items():
        if rid == from_robot_id: continue
        name = char["name"].lower()
        for pattern in [f"tell {name}", f"ask {name}", f"say to {name}",
                        f"say hi to {name}", f"message {name}"]:
            if pattern in heard_lower:
                idx = heard_lower.find(pattern) + len(pattern)
                msg = heard[idx:].strip().lstrip(" ,").strip() or heard
                return rid, char, msg
    return None, None, None

@app.route("/pending")
def pending():
    """Robot polls this for messages from other robots. Returns 204 if nothing waiting."""
    robot_id = request.args.get("robot_id", "robot1")
    if is_sleeping() or not robot_queue[robot_id]:
        return Response(status=204)
    item = robot_queue[robot_id].pop(0)
    char = CHARACTERS.get(robot_id, list(CHARACTERS.values())[0])

    # Dashboard action-only messages (LED, mode switch, volume — no audio)
    if isinstance(item, dict) and "action" in item and item.get("pcm") is None:
        log.info(f"[{char['name']}] Action: {item['action']}")
        return Response(b"", mimetype="application/octet-stream", headers={
            "X-Reply-Text": item["action"],
            "X-Robot-Name": char["name"],
            "X-Action":     item["action"],
            "X-SFX":        "none",
            "Content-Length": "0",
        })

    # Dashboard say + mode messages (have audio)
    if isinstance(item, dict) and "action" in item and item.get("pcm"):
        log.info(f"[{char['name']}] Action+audio: {item['action']}")
        return Response(item["pcm"], mimetype="application/octet-stream", headers={
            "X-Reply-Text": item["action"],
            "X-Robot-Name": char["name"],
            "X-Action":     item["action"],
            "X-SFX":        "none",
        })

    # Inter-robot messages (legacy format with 'from'/'text'/'pcm' keys)
    log.info(f"[{char['name']}] Delivering message from {item['from']}: {item['text'][:60]}")
    return Response(item["pcm"], mimetype="application/octet-stream", headers={
        "X-Reply-Text": f"{item['from']}: {item['text'][:50]}",
        "X-Robot-Name": char["name"],
        "X-Action":     "none",
        "X-SFX":        "none",
    })

@app.route("/greet")
def greet():
    """Called once on boot after WiFi connects — robot introduces itself."""
    robot_id = request.args.get("robot_id", "robot1")
    mode     = request.args.get("mode",     "adult")
    kid_name = request.args.get("kid_name", "buddy")
    char     = CHARACTERS.get(robot_id, list(CHARACTERS.values())[0])
    kid      = (mode == "kid")
    tld      = char.get("voice_tld", "com.au")
    name     = char["name"]

    if kid:
        phrase = f"Hi! I'm {name}! Ready to play, {kid_name}!"
    elif robot_id == "robot1":
        phrase = f"Oi! {name} here, connected and ready. Let's go!"
    else:
        phrase = f"Hello! I'm {name}, connected and ready to chat."

    log.info(f"[{name}] Boot greeting: {phrase}")
    pcm = tts_pcm(phrase, tld)
    return Response(pcm, mimetype="application/octet-stream", headers={
        "X-Reply-Text":   phrase,
        "X-Robot-Name":   name,
        "X-Action":       "none",
        "X-SFX":          "none",
        "Content-Length": str(len(pcm)),
    })

@app.route("/battery")
def battery_warning():
    """Called by robot when battery is low."""
    robot_id = request.args.get("robot_id", "robot1")
    level    = int(request.args.get("level", "15"))
    char     = CHARACTERS.get(robot_id, list(CHARACTERS.values())[0])
    kid      = (char.get("mode","adult") == "kid")
    name     = char["name"]
    tld      = char.get("voice_tld","com.au")

    if kid:
        phrases = [
            f"Uh oh! Rumi is getting tired! I only have {level} percent left! Can someone charge me please!",
            f"Help! I need charging! Only {level} percent battery! Like a sleepy robot!",
            f"My tummy is empty! Only {level} percent power left! ROARRR for charging!",
        ]
    elif robot_id == "robot1":
        phrases = [
            f"Oi, quick heads up — I'm down to {level} percent battery. Might wanna plug me in before I die mid-sentence.",
            f"Battery's at {level} percent. I'd really rather not pass out right now, just saying.",
            f"Hey, {level} percent here. Kira needs juice. Hook me up?",
        ]
    else:
        phrases = [
            f"Just a gentle heads up — I'm at {level} percent battery. Worth charging me when you get a chance.",
            f"My battery is at {level} percent. I'll keep going but I wanted you to know.",
            f"Battery check: {level} percent remaining. No rush, but soon would be nice.",
        ]

    phrase = random.choice(phrases)
    log.info(f"[{name}] 🔋 Battery low: {level}%")
    pcm = tts_pcm(phrase, tld)
    return Response(pcm, mimetype="application/octet-stream", headers={
        "X-Reply-Text":   phrase[:80],
        "X-Robot-Name":   name,
        "X-Action":       "none",
        "X-SFX":          "sfx_alarm",
        "Content-Length": str(len(pcm)),
    })

@app.route("/idle")
def idle():
    if is_sleeping(): return Response(status=204)
    char   = CHARACTERS.get(request.args.get("robot_id","robot1"), list(CHARACTERS.values())[0])
    phrase = random.choice(char.get("idle_phrases", ["Hello!"]))
    pcm    = tts_pcm(phrase, char.get("voice_tld","com.au"))
    return Response(pcm, mimetype="application/octet-stream",
                    headers={"X-Reply-Text": phrase, "X-Robot-Name": char["name"], "X-Action": "none"})

# ─── Dashboard say / send message ───────────────────────────
@app.route("/say", methods=["POST"])
def say():
    """Queue a message for a robot to say. Dashboard uses this."""
    robot_id = request.json.get("robot_id", "robot1")
    text     = request.json.get("text", "").strip()
    if not text: return {"error": "no text"}, 400
    char = CHARACTERS.get(robot_id, list(CHARACTERS.values())[0])
    tld  = char.get("voice_tld", "com.au")
    pcm  = tts_pcm(text, tld)
    robot_queue[robot_id].append({"action": "say", "pcm": pcm})
    log.info(f"[Dashboard] Queued message for {robot_id}: {text[:50]}")
    return {"status": "queued", "robot_id": robot_id, "text": text}

@app.route("/dashboard")
def dashboard():
    robots = []
    for rid, char in CHARACTERS.items():
        last_file  = HISTORY_DIR / f"{rid}_log.txt"
        last_active = "never"
        online_mins = 999
        if last_file.exists():
            mtime       = last_file.stat().st_mtime
            last_active = datetime.fromtimestamp(mtime).strftime("%d %b %H:%M")
            online_mins = int((datetime.now().timestamp() - mtime) / 60)
        ver_file = OTA_DIR / f"{rid}_version.txt"
        robots.append({
            "id":      rid,
            "name":    char["name"],
            "mode":    robot_state.get(rid, {}).get("mode","adult"),
            "led":     robot_state.get(rid, {}).get("led", True),
            "volume":  robot_state.get(rid, {}).get("volume", 180),
            "last":    last_active,
            "online":  online_mins < 10,
            "version": ver_file.read_text().strip() if ver_file.exists() else "?",
        })

    sleeping = is_sleeping()
    weather  = get_weather() or "unavailable"
    now      = datetime.now().strftime("%A %d %b %Y, %H:%M")

    cards = ""
    for r in robots:
        is_kid = r["mode"] == "kid"
        led_on = r["led"]
        cards += f"""
  <div class="card" id="card_{r['id']}">
    <div class="card-header">
      <div style="display:flex;align-items:center;gap:8px">
        <span class="dot {'on' if r['online'] else 'off'}"></span>
        <span class="robot-name">{r['name']}</span>
      </div>
      <span class="ver">v{r['version']}</span>
    </div>
    <div style="margin-bottom:10px">
      <span class="badge {'asleep' if sleeping else 'awake'}">{('🌙 sleeping' if sleeping else '✅ awake')}</span>
      <span class="badge {r['mode']}" id="modebadge_{r['id']}">{r['mode']} mode</span>
    </div>
    <div class="meta">Last active: {r['last']}</div>

    <div class="ctrl-row">
      <span class="lbl">Kid mode</span>
      <label class="tog"><input type="checkbox" id="kid_{r['id']}" {'checked' if is_kid else ''} onchange="setMode('{r['id']}',this.checked)"><span class="sl"></span></label>
    </div>
    <div class="ctrl-row">
      <span class="lbl">LEDs</span>
      <label class="tog"><input type="checkbox" id="led_{r['id']}" {'checked' if led_on else ''} onchange="setLed('{r['id']}',this.checked)"><span class="sl"></span></label>
    </div>
    <div class="ctrl-row">
      <span class="lbl">Volume <b id="vl_{r['id']}">{r['volume']}</b></span>
      <input type="range" min="50" max="255" value="{r['volume']}" class="vslider"
        oninput="document.getElementById('vl_{r['id']}').textContent=this.value"
        onchange="setVol('{r['id']}',this.value)">
    </div>

    <div class="qrow">
      <button onclick="say('{r['id']}','Hey! How is everyone?')">👋 Hey</button>
      <button onclick="say('{r['id']}','dance party')">🕺 Dance</button>
      <button onclick="say('{r['id']}','Tell me a fun fact')">🧠 Fact</button>
      <button onclick="say('{r['id']}','Goodnight everyone, sweet dreams!')">🌙 Night</button>
      <button onclick="say('{r['id']}','What is the weather like today?')">🌤️ Weather</button>
      <button onclick="say('{r['id']}','Tell me a joke')">😂 Joke</button>
    </div>

    <input type="text" id="msg_{r['id']}" placeholder="Type something for {r['name']} to say...">
    <div class="brow">
      <button class="bsay" onclick="sendSay('{r['id']}')">🔊 Say it</button>
      <button class="brst" onclick="resetMem('{r['id']}')">🗑️ Reset memory</button>
    </div>

    <div class="history" id="hist_{r['id']}">
      <div class="hist-title">Recent conversation</div>
      <div id="hist_items_{r['id']}"><div class="hist-loading">Loading...</div></div>
    </div>
  </div>
"""

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>🤖 Cerebro</title>
<style>
*{{box-sizing:border-box;margin:0;padding:0}}
body{{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#0f0f13;color:#e8e8f0;min-height:100vh;padding:20px}}
h1{{font-size:1.4rem;font-weight:700;color:#fff;margin-bottom:4px}}
.sub{{font-size:.8rem;color:#555;margin-bottom:18px}}
.sbar{{background:#1a1a24;border:1px solid #252535;border-radius:12px;padding:12px 18px;display:flex;gap:18px;flex-wrap:wrap;margin-bottom:14px;font-size:.8rem}}
.si{{color:#555}}.si b{{color:#a0a0c0}}
.gbtns{{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:20px}}
.gbtn{{background:#1e1e2e;border:1px solid #2a2a40;color:#a0a0c0;padding:9px 16px;border-radius:8px;cursor:pointer;font-size:.82rem;font-weight:600}}
.gbtn:hover{{background:#252535}}
.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(310px,1fr));gap:16px}}
.card{{background:#1a1a24;border:1px solid #252535;border-radius:14px;padding:18px}}
.card-header{{display:flex;justify-content:space-between;align-items:center;margin-bottom:8px}}
.robot-name{{font-size:1.35rem;font-weight:700}}
.ver{{font-size:.68rem;color:#333;background:#111;padding:2px 7px;border-radius:8px}}
.dot{{display:inline-block;width:9px;height:9px;border-radius:50%}}
.dot.on{{background:#4caf50;box-shadow:0 0 5px #4caf50}}
.dot.off{{background:#333}}
.badge{{display:inline-block;padding:3px 9px;border-radius:20px;font-size:.7rem;font-weight:600;margin-right:5px}}
.badge.awake{{background:#1a3a1a;color:#4caf50;border:1px solid #4caf5055}}
.badge.asleep{{background:#1a1a3a;color:#7070c0;border:1px solid #7070c055}}
.badge.adult{{background:#2a1a3a;color:#c07de0}}
.badge.kid{{background:#1a2a3a;color:#7db8e0}}
.meta{{font-size:.75rem;color:#444;margin-bottom:14px}}
.ctrl-row{{display:flex;align-items:center;justify-content:space-between;margin-bottom:11px}}
.lbl{{font-size:.8rem;color:#777}}
.lbl b{{color:#aaa}}
.tog{{position:relative;display:inline-block;width:42px;height:23px}}
.tog input{{opacity:0;width:0;height:0}}
.sl{{position:absolute;cursor:pointer;inset:0;background:#222235;border-radius:23px;transition:.25s}}
.sl:before{{content:"";position:absolute;height:17px;width:17px;left:3px;bottom:3px;background:#555;border-radius:50%;transition:.25s}}
input:checked+.sl{{background:#6060e0}}
input:checked+.sl:before{{transform:translateX(19px);background:#fff}}
.vslider{{width:120px;accent-color:#6060e0;cursor:pointer}}
.qrow{{display:flex;gap:5px;flex-wrap:wrap;margin:10px 0}}
.qrow button{{background:#1c1c2e;border:1px solid #2a2a42;color:#9090c0;padding:5px 9px;border-radius:7px;cursor:pointer;font-size:.75rem;font-weight:600}}
.qrow button:hover{{background:#242438}}
input[type=text]{{width:100%;padding:8px 11px;background:#111118;border:1px solid #222232;border-radius:8px;color:#e8e8f0;font-size:.83rem;margin:8px 0}}
input[type=text]:focus{{outline:none;border-color:#5050a0}}
.brow{{display:flex;gap:7px;margin-bottom:14px}}
.brow button{{padding:7px 13px;border:none;border-radius:8px;cursor:pointer;font-size:.8rem;font-weight:600}}
.bsay{{background:#2a4060;color:#80b8f0}}
.brst{{background:#3a1818;color:#c07070}}
.history{{background:#111118;border:1px solid #1e1e2e;border-radius:8px;padding:10px 12px;margin-top:4px}}
.hist-title{{font-size:.72rem;color:#444;font-weight:600;margin-bottom:8px;text-transform:uppercase;letter-spacing:.05em}}
.hist-pair{{margin-bottom:8px;border-bottom:1px solid #1a1a24;padding-bottom:8px}}
.hist-pair:last-child{{border-bottom:none;margin-bottom:0;padding-bottom:0}}
.hist-u{{font-size:.75rem;color:#6060a0;margin-bottom:3px}}
.hist-u span{{color:#8080c0}}
.hist-a{{font-size:.78rem;color:#a0a0c0;line-height:1.4}}
.hist-loading{{font-size:.75rem;color:#333}}
.banner{{background:#1a1a38;border:1px solid #3a3a80;border-radius:8px;padding:9px 14px;text-align:center;color:#7070b0;font-size:.82rem;margin-bottom:14px}}
.toast{{position:fixed;bottom:20px;right:20px;background:#1a3a1a;border:1px solid #4caf5088;color:#90e0a0;padding:9px 16px;border-radius:8px;font-size:.82rem;opacity:0;transition:opacity .3s;pointer-events:none;z-index:9999;max-width:280px}}
.toast.show{{opacity:1}}
@media(max-width:520px){{body{{padding:12px}}.grid{{grid-template-columns:1fr}}.vslider{{width:100px}}}}
</style>
</head>
<body>
<h1>🤖 Cerebro</h1>
<div class="sub">{now} &nbsp;·&nbsp; 🌤️ {weather}</div>

{"<div class='banner'>🌙 Sleep hours active — robots are in low-power mode</div>" if sleeping else ""}

<div class="sbar">
  <div class="si">Server <b>online</b></div>
  <div class="si">Model <b>{OLLAMA_MODEL}</b></div>
  <div class="si">Whisper <b>{WHISPER_MODEL}</b></div>
  <div class="si">Sleep <b>{SLEEP_HOUR:02d}:{SLEEP_MINUTE:02d} – {WAKE_HOUR:02d}:{WAKE_MINUTE:02d}</b></div>
</div>

<div class="gbtns">
  <button class="gbtn" onclick="api('/sleep','POST').then(()=>location.reload())">🌙 Sleep all</button>
  <button class="gbtn" onclick="api('/wake','POST').then(()=>location.reload())">☀️ Wake all</button>
  <button class="gbtn" onclick="api('/schedule','POST').then(()=>location.reload())">🔄 Restore schedule</button>
</div>

<div class="grid">
{cards}
</div>

<div id="toast" class="toast"></div>
<script>
async function api(url,method='GET',body=null){{
  try{{
    const o={{method,headers:{{'Content-Type':'application/json'}}}};
    if(body)o.body=JSON.stringify(body);
    const r=await fetch(url,o);
    const t=await r.text();
    toast(r.ok?'✅ '+t.substring(0,70):'❌ '+r.status+' '+t.substring(0,40));
    return r;
  }}catch(e){{toast('❌ '+e.message);}}
}}
async function sendSay(id){{
  const el=document.getElementById('msg_'+id);
  const text=el.value.trim(); if(!text)return;
  el.value=''; await api('/say','POST',{{robot_id:id,text}});
}}
async function say(id,text){{await api('/say','POST',{{robot_id:id,text}});}}
async function setMode(id,kid){{
  const mode=kid?'kid':'adult';
  document.getElementById('modebadge_'+id).textContent=mode+' mode';
  document.getElementById('modebadge_'+id).className='badge '+mode;
  await api('/mode','POST',{{robot_id:id,mode}});
}}
async function setLed(id,on){{await api('/led','POST',{{robot_id:id,on}});}}
async function setVol(id,vol){{await api('/volume','POST',{{robot_id:id,volume:parseInt(vol)}});}}
async function resetMem(id){{
  if(!confirm('Reset '+id+' memory?'))return;
  await api('/reset?robot_id='+id,'POST');
}}
document.querySelectorAll('input[type=text]').forEach(el=>{{
  el.addEventListener('keydown',e=>{{if(e.key==='Enter')sendSay(el.id.replace('msg_',''));}});
}});
function toast(msg){{
  const t=document.getElementById('toast');
  t.textContent=msg;t.classList.add('show');
  setTimeout(()=>t.classList.remove('show'),3500);
}}
async function loadHistory(id){{
  try{{
    const r=await fetch('/recent_history?robot_id='+id+'&n=3');
    const d=await r.json();
    const el=document.getElementById('hist_items_'+id);
    if(!d.history||d.history.length===0){{el.innerHTML='<div class="hist-loading">No conversations yet</div>';return;}}
    el.innerHTML=d.history.map(p=>
      `<div class="hist-pair">
        <div class="hist-u">👤 <span>${{p.user}}</span></div>
        <div class="hist-a">${{p.assistant}}</div>
      </div>`
    ).join('');
  }}catch(e){{document.getElementById('hist_items_'+id).innerHTML='<div class="hist-loading">Unavailable</div>';}}
}}
// Load history for all robots
{'; '.join([f"loadHistory('{r['id']}')" for r in robots])};
// Reload every 90s
setTimeout(()=>location.reload(), 90000);
</script>
</body></html>"""
    return html


@app.route("/mode", methods=["POST"])
def set_mode():
    robot_id = request.json.get("robot_id","robot1")
    mode     = request.json.get("mode","adult")   # "adult" or "kid"
    if robot_id not in robot_state: return {"error":"unknown robot"}, 404
    robot_state[robot_id]["mode"] = mode
    # Queue a mode-switch action for the robot to pick up
    char = CHARACTERS.get(robot_id, list(CHARACTERS.values())[0])
    tld  = char.get("voice_tld","com.au")
    msg  = "Kid mode on!" if mode=="kid" else "Adult mode."
    robot_queue[robot_id].append({"action": f"set_mode_{mode}", "pcm": tts_pcm(msg, tld)})
    log.info(f"[Dashboard] {robot_id} mode → {mode}")
    return {"status":"ok","robot_id":robot_id,"mode":mode}

@app.route("/led", methods=["POST"])
def set_led():
    robot_id = request.json.get("robot_id","robot1")
    on       = request.json.get("on", True)
    if robot_id not in robot_state: return {"error":"unknown robot"}, 404
    robot_state[robot_id]["led"] = on
    robot_queue[robot_id].append({"action": "led_on" if on else "led_off", "pcm": None})
    log.info(f"[Dashboard] {robot_id} LED → {'on' if on else 'off'}")
    return {"status":"ok","robot_id":robot_id,"led":on}

@app.route("/volume", methods=["POST"])
def set_volume():
    robot_id = request.json.get("robot_id","robot1")
    vol      = int(request.json.get("volume", 180))
    vol      = max(0, min(255, vol))
    if robot_id not in robot_state: return {"error":"unknown robot"}, 404
    robot_state[robot_id]["volume"] = vol
    robot_queue[robot_id].append({"action": f"set_volume_{vol}", "pcm": None})
    log.info(f"[Dashboard] {robot_id} volume → {vol}")
    return {"status":"ok","robot_id":robot_id,"volume":vol}

@app.route("/robot_state")
def get_robot_state():
    robot_id = request.args.get("robot_id","robot1")
    return robot_state.get(robot_id, {})

# ─── Dashboard history helper ────────────────────────────────
@app.route("/recent_history")
def recent_history():
    """Last N exchanges for dashboard display."""
    robot_id = request.args.get("robot_id","robot1")
    n        = int(request.args.get("n", 4))
    session  = conversation_store.get(robot_id, [])
    # Return last n pairs (user+assistant)
    pairs = []
    msgs  = [m for m in session if m["role"] in ("user","assistant")]
    for i in range(max(0, len(msgs)-n*2), len(msgs), 2):
        if i+1 < len(msgs):
            pairs.append({
                "user":      msgs[i]["content"][:80],
                "assistant": msgs[i+1]["content"][:120],
            })
    return {"robot_id": robot_id, "history": pairs[-n:]}

# ─── OTA ──────────────────────────────────────────────────
@app.route("/ota/version")
def ota_version():
    """Returns firmware version string (plain text) or 'none'. Used by robot firmware."""
    if not OTA_ENABLED: return "none", 200
    robot_id = request.args.get("robot_id", "robot1")
    fw       = OTA_DIR / f"{robot_id}.bin"
    ver_file = OTA_DIR / f"{robot_id}_version.txt"
    if not fw.exists(): return "none", 200
    if ver_file.exists(): return ver_file.read_text().strip(), 200
    return datetime.fromtimestamp(fw.stat().st_mtime).strftime("%Y.%m.%d.%H%M"), 200

@app.route("/ota/firmware/<robot_id>")
def ota_firmware(robot_id):
    if not OTA_ENABLED: return Response("OTA disabled", status=403)
    fw = OTA_DIR / f"{secure_filename(robot_id)}.bin"
    if not fw.exists(): return Response("Not found", status=404)
    data = fw.read_bytes()
    return Response(data, mimetype="application/octet-stream", headers={
        "Content-Length": str(len(data)),
        "Content-Disposition": f'attachment; filename="{robot_id}.bin"',
        "X-MD5": hashlib.md5(data).hexdigest()})

@app.route("/ota/upload", methods=["POST"])
def ota_upload():
    robot_id = request.args.get("robot_id", "robot1")
    """Raw binary upload from Windows curl deploy script.
       curl -X POST http://cerebro:5005/ota/upload/robot1?version=2026.06.01&key=change_this_key --data-binary @firmware.bin
    """
    if request.args.get("key") != OTA_KEY:
        log.warning(f"[OTA] Rejected upload for {robot_id} — bad key")
        return "Unauthorized", 401
    version = request.args.get("version", datetime.now().strftime("%Y.%m.%d.%H%M"))
    data    = request.stream.read()
    if not data or len(data) < 1000: return "Empty or too small", 400
    OTA_DIR.mkdir(exist_ok=True)
    (OTA_DIR / f"{robot_id}.bin").write_bytes(data)
    (OTA_DIR / f"{robot_id}_version.txt").write_text(version)
    log.info(f"[OTA] {robot_id} v{version} uploaded ({len(data):,} bytes)")
    return f"OK — {robot_id} firmware {version} ready ({len(data):,} bytes)", 200

@app.route("/ota/list")
def ota_list():
    files = [{"robot_id": p.stem, "size": p.stat().st_size,
              "md5": hashlib.md5(p.read_bytes()).hexdigest(),
              "updated": datetime.fromtimestamp(p.stat().st_mtime).isoformat()}
             for p in OTA_DIR.glob("*.bin")]
    return {"firmware": files, "ota_dir": str(OTA_DIR)}
if __name__ == "__main__":
    log.info("━" * 48)
    log.info("  Kira Robot Server")
    for rid, c in CHARACTERS.items():
        log.info(f"  {c['name']:10} → {rid}")
    log.info(f"  Whisper: {WHISPER_MODEL}   Port: {SERVER_PORT}")
    log.info(f"  Model:   {OLLAMA_MODEL}")
    log.info(f"  Follow-up: {FOLLOWUP_TIMEOUT_KID}s kid / {FOLLOWUP_TIMEOUT_ADULT}s adult")
    log.info("━" * 48)
    app.run(host="0.0.0.0", port=SERVER_PORT, threaded=True)
