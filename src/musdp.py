from flask import Flask, request, jsonify
import time
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.chrome.service import Service
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
import urllib.parse
import os
import pydub # 导入pydub
import glob

# --- 配置 ---
DRIVER_PATH = './chromedriver.exe'
BASE_URL = "https://www.gequbao.com"
# 假设您Chrome的默认下载文件夹在这里
DOWNLOAD_FOLDER = 'E:\download'
# 最终保存Opus文件的文件夹
OPUS_FOLDER = './opus_songs' 

# 确保Opus文件夹存在
os.makedirs(OPUS_FOLDER, exist_ok=True)

app = Flask(__name__)

# 全局只使用一个浏览器实例，避免重复启动，提高效率
# 注意：Flask默认是单线程处理请求的，所以这样是安全的。
# 如果部署到多线程环境，需要为每个线程创建一个driver实例。
service = Service(DRIVER_PATH)
options = webdriver.ChromeOptions()
# options.add_argument("--headless") # 正式部署时建议使用无头模式
options.add_argument("--log-level=3")
options.add_experimental_option('excludeSwitches', ['enable-logging'])
driver = webdriver.Chrome(service=service, options=options)
print("全局Chrome浏览器实例已启动。")


@app.route('/search', methods=['GET'])
def search_song():
    song_name = request.args.get('name')
    if not song_name:
        return jsonify({"error": "缺少歌曲名称参数 'name'"}), 400

    print(f"收到搜索请求: {song_name}")
    
    encoded_song_name = urllib.parse.quote(song_name)
    search_url = f"{BASE_URL}/s/{encoded_song_name}"
    driver.get(search_url)

    wait = WebDriverWait(driver, 10)
    try:
        results_container = wait.until(EC.presence_of_element_located((By.CLASS_NAME, 'card-text')))
        song_rows = results_container.find_elements(By.CLASS_NAME, 'row')
    except:
        return jsonify([]) # 返回空列表表示未找到

    search_results = []
    for row in song_rows:
        try:
            link_tag = row.find_element(By.CLASS_NAME, 'music-link')
            title = link_tag.find_element(By.CLASS_NAME, 'music-title').text
            artist = link_tag.find_element(By.CLASS_NAME, 'text-jade').text
            page_url = link_tag.get_attribute('href')
            
            # 只返回精确匹配的结果
            if song_name.lower() in title.lower():
                search_results.append({
                    'title': title,
                    'artist': artist,
                    'page_url': page_url
                })
        except:
            continue
            
    return jsonify(search_results)


@app.route('/download', methods=['POST'])
def download_and_convert():
    data = request.json
    if not data or 'page_url' not in data:
        return jsonify({"error": "请求体中缺少 'page_url'"}), 400

    title = data.get('title', 'unknown')
    artist = data.get('artist', 'unknown')
    page_url = data['page_url']
    
    print(f"收到下载请求: {title} - {artist}")
    
    # 清理下载文件夹，防止找到旧文件
    for f in glob.glob(os.path.join(DOWNLOAD_FOLDER, '*.mp3')):
        os.remove(f)

    # --- Selenium下载 ---
    driver.execute_script(f"window.open('{page_url}', '_blank');") # 在新标签页打开
    time.sleep(1)
    driver.switch_to.window(driver.window_handles[-1])
    wait = WebDriverWait(driver, 10)

    try:
        download_button = wait.until(EC.element_to_be_clickable((By.ID, 'btn-download-mp3')))
        download_button.click()
        time.sleep(1)

        low_quality_button = wait.until(EC.element_to_be_clickable((By.PARTIAL_LINK_TEXT, '备用')))
        low_quality_button.click()
        print("已触发浏览器下载...")

        # --- 等待MP3下载完成 ---
        # 这是一个简单的等待方法：不断检查下载文件夹，直到出现MP3文件且大小不再变化
        downloaded_mp3_path = None
        timeout = 60 # 60秒超时
        start_time = time.time()
        last_size = -1
        while time.time() - start_time < timeout:
            mp3_files = glob.glob(os.path.join(DOWNLOAD_FOLDER, '*.mp3'))
            if mp3_files:
                current_path = mp3_files[0]
                current_size = os.path.getsize(current_path)
                if current_size > 0 and current_size == last_size:
                    downloaded_mp3_path = current_path
                    print(f"MP3文件下载完成: {downloaded_mp3_path}")
                    break
                last_size = current_size
            time.sleep(1)
        
        driver.close() # 关闭当前标签页
        driver.switch_to.window(driver.window_handles[0]) # 切回主标签页

        if not downloaded_mp3_path:
            return jsonify({"status": "error", "message": "下载MP3文件超时或失败"}), 500

        # --- Pydub格式转换 ---
        print("开始将MP3转换为Opus...")
        safe_filename = f"{title} - {artist}".replace('/', '_').replace('\\', '_')
        opus_path = os.path.join(OPUS_FOLDER, f"{safe_filename}.opus")
        
        audio = pydub.AudioSegment.from_mp3(downloaded_mp3_path)
        audio.export(opus_path, format="opus")
        print(f"Opus文件已保存: {opus_path}")

        # --- 清理工作 ---
        os.remove(downloaded_mp3_path)
        print("临时MP3文件已删除。")

        return jsonify({"status": "success", "opus_path": opus_path})

    except Exception as e:
        print(f"处理下载时发生错误: {e}")
        # 如果出错，尝试关闭多余的标签页
        if len(driver.window_handles) > 1:
            driver.close()
            driver.switch_to.window(driver.window_handles[0])
        return jsonify({"status": "error", "message": str(e)}), 500


if __name__ == '__main__':
    # 启动Flask服务器
    # host='0.0.0.0' 允许您网络内的其他设备访问
    # debug=True 方便调试，但正式部署时建议关闭
    app.run(host='127.0.0.1', port=5000)