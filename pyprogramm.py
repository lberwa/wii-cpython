import requests
import subprocess
import re
import os
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

# --- Konfiguration ---
OLLAMA_URL = "http://localhost:11434/api/generate"

# Modelle: Code-Modell und Text/Erklärungs-Modell
MODEL_CODE = "qwen2.5-coder:7b"    # für Code-Analyse und Generierung
MODEL_TEXT = "phi4-mini"            # für Erklärungen oder Analyse in natürlicher Sprache

MAX_WORKERS = 8  # parallele KI-Anfragen

SYSTEM_PROMPT = """
You are an autonomous Linux terminal and code assistant.
Follow these rules:

- Answer EXCLUSIVELY with shell commands between %%% ... %%%.
- Combine multiple commands only if absolutely necessary.
- Do not write explanations outside of %%% ... %%%.
- If you need user input (e.g., file path), write:
  %%%ASK_USER: <your message asking the user>%%%
  Then wait for user input before continuing.
- If a command fails, try to generate a corrected command automatically.
- Provide step-by-step progress and concise status updates.
- When done with all commands, write %%%STOP%%% and append a short summary of findings.
"""

SYSTEM_PROMPT_ANALYSIS = """
You are a code and text analysis AI.
Analyze code snippets, grep/find/git results, or any user-provided context.
Answer in German, clearly and concisely.
Use natural language explanations if needed.
"""

# --- KI-Anfrage ---
def ask_ki(prompt, history, model):
    payload = {"model": model, "prompt": f"{SYSTEM_PROMPT}\n\nKontext:\n{history}\n\nUser: {prompt}\nAntwort:", "stream": False}
    try:
        r = requests.post(OLLAMA_URL, json=payload, timeout=120)
        return r.json().get("response", "")
    except Exception as e:
        return f"Fehler: {e}"

# --- Shell-Befehle ausführen ---
def run_cmd(cmd):
    if cmd.startswith("cd "):
        try:
            target = cmd.replace("cd ", "").split("&&")[0].strip()
            os.chdir(os.path.expanduser(target))
            return f"cwd: {os.getcwd()}"
        except Exception as e:
            return f"cd FEHLER: {e}"
    try:
        return subprocess.check_output(cmd, shell=True, text=True, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        return e.output

# --- Extrahiere Befehl aus KI-Antwort ---
def extract_command(ki_response):
    match = re.search(r"%+\s*([^%\n\r]+)", ki_response)
    if match:
        return match.group(1).strip()
    return None

# --- Filtern nur neuer Zeilen ---
def summarize_result(result, already_seen):
    lines = result.strip().splitlines()
    new_lines = []
    for l in lines:
        if l not in already_seen:
            new_lines.append(l)
            already_seen.add(l)
    return new_lines, already_seen

# --- Automatisches Modell-Switching ---
def choose_model(ki_response):
    if ki_response and ("%%%" in ki_response and any(cmd in ki_response for cmd in ["grep", "find", "git", "cd", "ls", "make"])):
        return MODEL_CODE
    return MODEL_TEXT

# --- Hauptprogramm ---
print("--- KI-Shell autonom & Analyse (dynamic model) ---")

history_cmd = ""
history_analysis = ""
already_seen_lines = set()

executor = ThreadPoolExecutor(max_workers=MAX_WORKERS)

while True:
    user_input = input("\n> ")
    if user_input.lower() in ["exit", "quit"]:
        break

    history_cmd += f"\nUser: {user_input}\n"

    step = 0
    user_prompt = user_input

    while step < 50:
        step += 1
        print(f"\n[Schritt {step}] KI wird befragt…")

        # parallele KI-Anfragen starten
        futures = [executor.submit(ask_ki, user_prompt, history_cmd, MODEL_CODE) for _ in range(MAX_WORKERS)]
        results = []
        for future in as_completed(futures):
            resp = future.result()
            results.append(resp)

        # Erste brauchbare Antwort auswählen
        ki_resp = next((r for r in results if r and "Fehler" not in r), results[0])
        print(f"[KI-Antwort] {ki_resp}")

        # Prüfen, ob Modell-Switch nötig ist
        model_to_use = choose_model(ki_resp)

        cmd = extract_command(ki_resp)
        if not cmd:
            print("[Status] Kein Befehl erkannt, KI versucht Fortsetzung…")
            user_prompt = "Analysiere den Kontext erneut und generiere den nächsten Schritt."
            time.sleep(0.3)
            continue

        if cmd.upper().startswith("ASK_USER"):
            user_prompt = input(f"[KI benötigt Info] {cmd.split(':',1)[1].strip()}\n> ")
            history_cmd += f"User liefert Info: {user_prompt}\n"
            continue

        if cmd.upper() == "STOP":
            print("[Status] KI sagt STOP – alle Schritte abgeschlossen.")
            break

        print(f"[Status] Führe Befehl aus: {cmd}")
        result = run_cmd(cmd)

        new_lines, already_seen_lines = summarize_result(result, already_seen_lines)
        if not new_lines:
            print("[Status] Keine neuen Informationen, KI kann stoppen.")
            break

        summary_text = "\n".join(new_lines[:10])
        if len(new_lines) > 10:
            summary_text += f"\n... ({len(new_lines)-10} weitere Zeilen gekürzt)"
        print(f"[Ergebnis] {len(new_lines)} neue Zeilen:\n{summary_text}")

        history_cmd += f"KI-Befehl: {cmd}\nErgebnis:\n{summary_text}\n"

        # --- Analyse-KI mit ggf. anderem Modell ---
        analysis_prompt = SYSTEM_PROMPT_ANALYSIS + "\nNeue Infos:\n" + summary_text
        analysis_model = MODEL_TEXT
        analysis_resp = ask_ki(analysis_prompt, history_analysis, model=analysis_model)
        print(f"[Analyse der KI]:\n{analysis_resp}\n")
        history_analysis += f"{summary_text}\nAnalyse:\n{analysis_resp}\n"

        # Nächster Schritt Input
        user_prompt = "Analysiere das Ergebnis und fahre fort, falls nötig."
        time.sleep(0.1)
