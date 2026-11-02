import os
import sys
import subprocess
import json
import random
import multiprocessing
from flask import Flask, render_template, request, jsonify

app = Flask(__name__, template_folder='templates', static_folder='static')
app.config['TEMPLATES_AUTO_RELOAD'] = True
app.config['SEND_FILE_MAX_AGE_DEFAULT'] = 0

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
EXE_PATH = os.path.join(BASE_DIR, "monte_carlo_pi.exe")

def ensure_compiled():
    """Memastikan file eksekusi C monte_carlo_pi.exe siap dijalankan"""
    if not os.path.exists(EXE_PATH):
        print("[*] Mengompilasi main.c...")
        env = os.environ.copy()
        mingw_bin = r"C:\Program Files\GNU Octave\Octave-10.1.0\mingw64\bin"
        if mingw_bin not in env.get("PATH", ""):
            env["PATH"] = mingw_bin + ";" + env.get("PATH", "")
        subprocess.run(
            ["gcc", "-O3", "-fopenmp", "-pthread", "main.c", "-o", "monte_carlo_pi.exe", "-lm"],
            cwd=BASE_DIR, env=env, check=True
        )

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/system-info', methods=['GET'])
def get_system_info():
    try:
        cpu_count = multiprocessing.cpu_count()
    except Exception:
        cpu_count = 8

    return jsonify({
        "status": "online",
        "backend": "C (C99) + OpenMP",
        "rng": "Xorshift128+ 64-bit",
        "cpu_cores": cpu_count,
        "default_samples": 1000000000,
        "exe_exists": os.path.exists(EXE_PATH)
    })

@app.route('/api/run', methods=['POST'])
def run_simulation():
    data = request.get_json() or {}
    threads = int(data.get('threads', 4))
    samples = int(data.get('samples', 1000000000))
    method = data.get('method', 'openmp')

    if threads < 1: threads = 1
    if samples < 1000: samples = 1000

    try:
        cmd = [EXE_PATH, "--json-run", str(threads), str(samples), method]
        result = subprocess.run(cmd, cwd=BASE_DIR, capture_output=True, text=True, check=True)
        out = result.stdout.strip()
        data_json = json.loads(out)
        return jsonify(data_json)
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/compare', methods=['POST'])
def run_comparison():
    data = request.get_json() or {}
    samples = int(data.get('samples', 1000000000))
    if samples < 1000: samples = 1000

    try:
        cmd = [EXE_PATH, "--json-compare", str(samples)]
        result = subprocess.run(cmd, cwd=BASE_DIR, capture_output=True, text=True, check=True)
        out = result.stdout.strip()
        data_json = json.loads(out)
        return jsonify(data_json)
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/benchmark', methods=['POST'])
def run_benchmark():
    data = request.get_json() or {}
    samples = int(data.get('samples', 1000000000))
    if samples < 1000: samples = 1000

    try:
        cmd = [EXE_PATH, "--json-benchmark", str(samples)]
        result = subprocess.run(cmd, cwd=BASE_DIR, capture_output=True, text=True, check=True)
        out = result.stdout.strip()
        data_json = json.loads(out)
        return jsonify(data_json)
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/race-demo', methods=['POST'])
def run_race_demo():
    data = request.get_json() or {}
    samples = int(data.get('samples', 20000000))
    if samples < 10000: samples = 10000

    try:
        cmd = [EXE_PATH, "--json-race", str(samples)]
        result = subprocess.run(cmd, cwd=BASE_DIR, capture_output=True, text=True, check=True)
        out = result.stdout.strip()
        data_json = json.loads(out)
        return jsonify(data_json)
    except Exception as e:
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/api/visual-sample', methods=['GET'])
def get_visual_sample():
    count = int(request.args.get('count', 2500))
    points = []
    inside_count = 0
    for _ in range(count):
        x = random.random()
        y = random.random()
        is_inside = (x * x + y * y <= 1.0)
        if is_inside:
            inside_count += 1
        points.append({
            "x": round(x, 4),
            "y": round(y, 4),
            "inside": is_inside
        })
    return jsonify({
        "total": count,
        "inside": inside_count,
        "pi_sample": 4.0 * (inside_count / count),
        "points": points
    })

if __name__ == '__main__':
    ensure_compiled()
    print("=================================================================")
    print("  DASHBOARD WEB SIMULASI MONTE CARLO ESTIMASI PI (KOMPUTER PARALEL)")
    print("  Buka browser di URL: http://localhost:5000")
    print("=================================================================")
    app.run(host='0.0.0.0', port=5000, debug=False)
