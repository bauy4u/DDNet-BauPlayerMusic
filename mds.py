# -*- coding: utf-8 -*-

import os
import json
import time
import requests
import subprocess
import threading
from selenium import webdriver
from flask import Flask, request, jsonify
import boto3
from pydub import AudioSegment

# --- 1. 配置部分 ---
# 网易云音乐相关配置
BASE_URL = "http://music.163.com"
SEARCH_API = f"{BASE_URL}/api/search/get/web"
SONG_URL_API = f"{BASE_URL}/api/song/enhance/player/url"
LYRIC_API = f"{BASE_URL}/api/song/lyric"
DOWNLOAD_DIR = "DDNet服务器目录\data\musicso"
COOKIE_FILE = "netease_cookies.json"

# S3 对象存储配置
S3_ENDPOINT_URL = ''
S3_ACCESS_KEY = ''
S3_SECRET_KEY = ''
S3_BUCKET_NAME = ''
S3_PUBLIC_DOMAIN = ''

# DDNet webmaps 文件在服务器上的基础路径
WEBMAPS_BASE_PATH = '任意文件夹(保存地图用)'

# --- 2. 核心功能函数 ---

def search_songs(keyword, limit=10):
    params = {'s': keyword, 'type': 1, 'limit': limit, 'offset': 0}
    try:
        response = requests.post(SEARCH_API, data=params)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        print(f"[错误] 搜索歌曲 '{keyword}' 时出错: {e}")
        return None

def get_song_url(song_id, cookies):
    params = {'ids': f"[{song_id}]", 'br': 320000}
    try:
        response = requests.get(SONG_URL_API, params=params, cookies=cookies)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        print(f"[错误] 获取歌曲 ID '{song_id}' 的链接时出错: {e}")
        return None

def _download_and_save_lyrics(song_id, title):
    lrc_path = os.path.join(DOWNLOAD_DIR, f"{song_id}.lrc")
    if os.path.exists(lrc_path):
        print(f"[跳过] 歌词文件 {os.path.basename(lrc_path)} 已存在。")
        return
    params = {'id': song_id, 'lv': -1, 'tv': -1}
    try:
        response = requests.get(LYRIC_API, params=params)
        response.raise_for_status()
        data = response.json()
        if data.get('lrc') and data['lrc'].get('lyric'):
            lyrics = data['lrc']['lyric']
            with open(lrc_path, "w", encoding="utf-8") as f:
                f.write(lyrics)
            print(f"[歌词] 已成功保存歌词到 {os.path.basename(lrc_path)}")
        else:
            print(f"[歌词] 歌曲 '{title}' 没有找到歌词。")
    except Exception as e:
        print(f"[错误] 下载歌曲 '{title}' 的歌词时失败: {e}")

def save_cookies(cookies):
    with open(COOKIE_FILE, 'w') as f:
        json.dump(cookies, f)

def load_cookies():
    if os.path.exists(COOKIE_FILE):
        with open(COOKIE_FILE, 'r') as f:
            return json.load(f)
    return None

def login_and_get_cookies():
    print("[登录] 浏览器窗口将会打开，请您登录网易云音乐。")
    print("[登录] 请在网页上完成登录后，回到此命令行窗口并按 Enter 键继续。")
    try:
        driver = webdriver.Chrome()
        driver.get("https://music.163.com/#/login")
        input("[登录] 在您成功登录后，请按 Enter 键...")
        cookies = {cookie['name']: cookie['value'] for cookie in driver.get_cookies()}
        save_cookies(cookies)
        print("[登录] Cookies 已成功保存！")
        driver.quit()
        return cookies
    except Exception as e:
        print(f"[错误] 登录过程中发生错误: {e}")
        return None

# [!! 修改 !!]
# 将 pydub 读取格式从 "opus" 修改为 "ogg"，以修复 FFmpeg 兼容性问题
def _perform_download_and_conversion(song_id, title, artist, cookies):
    """
    同步执行下载和转换，并返回包含时长信息的结果。
    """
    log_display_name = f"{title} - {artist}"
    print(f"[处理开始] 开始处理: {log_display_name} (ID: {song_id})")
    opus_path = os.path.join(DOWNLOAD_DIR, f"{song_id}.opus")

    if os.path.exists(opus_path):
        _download_and_save_lyrics(song_id, title)
        duration_seconds = 0
        try:
            # [!! 关键修正 !!] 使用 ogg 格式读取 opus 文件
            audio = AudioSegment.from_file(opus_path, format="ogg")
            duration_seconds = len(audio) / 1000.0
            print(f"[时长] 已存在文件 '{os.path.basename(opus_path)}' 的时长为: {duration_seconds:.2f} 秒")
        except Exception as e:
            print(f"[错误] 无法获取已存在文件 '{os.path.basename(opus_path)}' 的时长: {e}")

        success_msg = f"歌曲 '{log_display_name}' ({os.path.basename(opus_path)}) 已存在，跳过下载。"
        return True, {"message": success_msg, "duration": duration_seconds}

    _download_and_save_lyrics(song_id, title)

    song_url_data = get_song_url(song_id, cookies)
    if not (song_url_data and song_url_data.get('data') and song_url_data['data'][0].get('url')):
        error_msg = f"[错误] 无法获取 '{log_display_name}' 的有效下载链接。"
        print(error_msg)
        return False, {"message": error_msg}

    download_url = song_url_data['data'][0]['url']
    mp3_path = os.path.join(DOWNLOAD_DIR, f"{song_id}.mp3")

    try:
        response = requests.get(download_url, stream=True)
        response.raise_for_status()
        with open(mp3_path, 'wb') as f:
            for chunk in response.iter_content(chunk_size=8192):
                f.write(chunk)
        print(f"[下载完成] {os.path.basename(mp3_path)}")
    except requests.exceptions.RequestException as e:
        error_msg = f"[错误] 下载 '{log_display_name}' 时失败: {e}"
        print(error_msg)
        return False, {"message": error_msg}

    command = [
        'ffmpeg', '-i', mp3_path, '-c:a', 'libopus', '-vbr', 'on',
        '-b:a', '128k', '-y', '-loglevel', 'error', opus_path
    ]
    try:
        subprocess.run(command, check=True)
        print(f"[转换成功] {os.path.basename(opus_path)}")
    except Exception as e:
        error_msg = f"[错误] FFmpeg 转换时失败: {e}"
        print(error_msg)
        return False, {"message": error_msg}
    
    duration_seconds = 0
    try:
        # [!! 关键修正 !!] 使用 ogg 格式读取 opus 文件
        audio = AudioSegment.from_file(opus_path, format="ogg")
        duration_seconds = len(audio) / 1000.0
        print(f"[时长] 获取到 '{os.path.basename(opus_path)}' 的时长为: {duration_seconds:.2f} 秒")
    except Exception as e:
        print(f"[错误] 无法使用 pydub 获取音频时长: {e}")
        
    if os.path.exists(mp3_path):
        os.remove(mp3_path)
    
    final_filename = os.path.basename(opus_path)
    success_msg = f"歌曲 '{log_display_name}' 已成功保存为 '{final_filename}'。"
    return True, {"message": success_msg, "duration": duration_seconds}

# --- 3. Flask API 服务 ---

app = Flask(__name__)
NETEASE_COOKIES = {}

@app.route('/search', methods=['GET'])
def api_search_compatible():
    query = request.args.get('name')
    if not query:
        return jsonify({"error": "缺少歌曲名称参数 'name'"}), 400
    print(f"[API /search] 收到搜索请求: '{query}'")
    search_results = search_songs(query)
    if not search_results or not search_results.get('result') or not search_results['result'].get('songs'):
        return jsonify([])
    songs = search_results['result']['songs']
    response_list = []
    for song in songs:
        response_list.append({
            "title": song['name'],
            "artist": ", ".join([artist['name'] for artist in song['artists']]),
            "page_url": str(song['id'])
        })
    return jsonify(response_list)

@app.route('/download', methods=['POST'])
def api_download_compatible():
    if not request.is_json:
        return jsonify({"status": "error", "message": "请求格式错误..."}), 400
    
    data = request.json
    page_url = data.get('page_url')
    title = data.get('title')
    artist = data.get('artist')

    if not all([page_url, title, artist]):
        return jsonify({"status": "error", "message": "请求体中缺少参数..."}), 400
    
    song_id = page_url
    print(f"[API /download] 收到下载请求: {title} - {artist} (ID: {song_id})")
    
    success, result_data = _perform_download_and_conversion(
        song_id, title, artist, NETEASE_COOKIES
    )
    
    print("[等待] 操作完成，额外等待 1 秒...")
    time.sleep(1)
    
    if success:
        print("[响应] 返回成功信息及歌曲时长。")
        return jsonify({
            "status": "success",
            "message": result_data.get("message"),
            "duration": result_data.get("duration", 0)
        })
    else:
        print("[响应] 返回失败信息。")
        return jsonify({
            "status": "error", 
            "message": result_data.get("message")
        }), 500

@app.route('/upload_map', methods=['POST'])
def api_upload_map():
    if not request.is_json:
        return jsonify({"success": False, "error": "请求格式错误..."}), 400

    data = request.json
    map_name = data.get('map_name')
    hash_value = data.get('hash')

    if not all([map_name, hash_value]):
        return jsonify({"success": False, "error": "请求体中缺少参数..."}), 400
        
    print(f"[API /upload_map] 收到上传请求，地图名: {map_name}, 哈希: {hash_value}")

    safe_map_name = os.path.basename(map_name)
    safe_hash_value = os.path.basename(hash_value)
    file_name = f"{safe_map_name}_{safe_hash_value}.map"
    local_file_path = os.path.join(WEBMAPS_BASE_PATH, file_name)

    if not os.path.exists(local_file_path):
        error_msg = f"文件未找到: {local_file_path}"
        print(f"[错误] {error_msg}")
        return jsonify({"success": False, "error": error_msg}), 404

    object_key = file_name

    try:
        s3 = boto3.client(
            's3',
            endpoint_url=S3_ENDPOINT_URL,
            aws_access_key_id=S3_ACCESS_KEY,
            aws_secret_access_key=S3_SECRET_KEY
        )
        s3.upload_file(
            local_file_path, S3_BUCKET_NAME, object_key,
            ExtraArgs={'ACL': 'public-read'}
        )
        
        final_url = f"https://{S3_PUBLIC_DOMAIN}/{object_key}"
        success_msg = f"成功将 '{local_file_path}' 上传到 {final_url}"
        print(f"[成功] {success_msg}")
        
        return jsonify({"success": True, "url": final_url})

    except Exception as e:
        error_msg = f"上传到对象存储时发生错误: {e}"
        print(f"[错误] {error_msg}")
        return jsonify({"success": False, "error": error_msg}), 500

# --- 4. 主程序入口 ---
if __name__ == '__main__':
    os.makedirs(DOWNLOAD_DIR, exist_ok=True)
    os.makedirs(WEBMAPS_BASE_PATH, exist_ok=True)
    
    NETEASE_COOKIES = load_cookies()
    if not NETEASE_COOKIES:
        print("[初始化] 未找到本地 Cookies 文件，需要进行首次登录。")
        NETEASE_COOKIES = login_and_get_cookies()
        if not NETEASE_COOKIES:
            print("[致命错误] 未能获取 Cookies，API 服务无法启动。程序退出。")
            exit(1)
    else:
        print("[初始化] 成功从本地加载 Cookies。")

    print("\n===================================================")
    print(" DDNet听歌房 辅助脚本 已启动")
    print("---------------------------------------------------")
    print(" > 音乐下载功能: 已激活 (支持返回时长)")
    print(" > 地图上传功能: 已激活")
    print("---------------------------------------------------")
    print(f" 监听地址: http://127.0.0.1:5000")
    print(f" Opus/LRC 目录: {os.path.abspath(DOWNLOAD_DIR)}")
    print(f" Web 地图目录: {os.path.abspath(WEBMAPS_BASE_PATH)}")
    print("===================================================\n")
    
    app.run(host='127.0.0.1', port=5000, debug=False)