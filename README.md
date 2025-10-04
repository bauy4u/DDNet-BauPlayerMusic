# DDNet-BauPlayerMusic

一个为 DDNet 游戏服务器添加音乐播放功能的项目,支持从网易云音乐搜索、下载和播放歌曲。

## 功能特性

- 🎵 **歌曲搜索**: 通过网易云音乐 API 搜索歌曲
- 📥 **自动下载**: 自动下载并转换为 Opus 格式
- 📝 **歌词显示**: 支持 LRC 歌词同步显示
- 🎼 **播放队列**: 支持歌曲队列管理和自动播放
- ☁️ **地图上传**: 支持将地图上传到对象存储

## 后端服务 (mds.py)

### 环境要求

- Python 3.11.9
- FFmpeg (用于音频转换)
- Chrome 浏览器 (用于首次登录)

### 依赖安装

```bash
pip install flask requests selenium boto3 pydub
```

### 配置说明

编辑 `mds.py` 中的配置项:

```python
# 音乐下载目录
DOWNLOAD_DIR = "DDNet服务器目录\data\musicso"

# S3 对象存储配置 (用于地图上传)
S3_ENDPOINT_URL = ''
S3_ACCESS_KEY = ''
S3_SECRET_KEY = ''
S3_BUCKET_NAME = ''
S3_PUBLIC_DOMAIN = ''

# 地图文件路径
WEBMAPS_BASE_PATH = '任意文件夹(保存地图用)'
```

### 启动服务

```bash
python mds.py
```

首次运行会自动打开浏览器要求登录网易云音乐账号,登录后 Cookies 会被保存到 `netease_cookies.json`。

服务启动后监听 `http://127.0.0.1:5000`

### API 接口

#### 1. 搜索歌曲
```
GET /search?name=歌曲名称
```

返回格式:
```json
[
  {
    "title": "歌曲名",
    "artist": "歌手名",
    "page_url": "歌曲ID"
  }
]
```

#### 2. 下载歌曲
```
POST /download
Content-Type: application/json

{
  "title": "歌曲名",
  "artist": "歌手名",
  "page_url": "歌曲ID"
}
```

返回格式:
```json
{
  "status": "success",
  "message": "下载信息",
  "duration": 180.5
}
```

#### 3. 上传地图
```
POST /upload_map
Content-Type: application/json

{
  "map_name": "地图名称",
  "hash": "地图哈希值"
}
```

## 游戏内使用

在服务器内使用前，确保已使用
```
http_allow_insecure 1
```
开启http通信
 
### 玩家命令

- `/song <歌名>` - 搜索歌曲
- `/choose <编号>` - 选择搜索结果中的歌曲并发起投票
- `/mls` - 查看当前播放队列
- `/skip` - 发起投票跳过当前歌曲

### 服务器命令

- `lyrics_start` - 开始显示歌词
- `lyrics_stop` - 停止显示歌词
- `lyrics_load <song_id>` - 加载指定歌曲的歌词
- `queue_status` - 查看队列状态
- `queue_clear` - 清空播放队列
- `queue_skip` - 跳过当前歌曲
- `queue_restart` - 重新开始播放队列

## 工作流程

1. 玩家使用 `/song` 搜索歌曲
2. 使用 `/choose` 选择歌曲并发起投票
3. 投票通过后,歌曲自动添加到播放队列
4. 系统自动预加载歌曲并转换为 Opus 格式
5. 歌曲准备完成后自动播放
6. 支持歌词同步显示(如果有 LRC 文件)
7. 播放完成后自动切换到下一首

## 技术细节

- 音频格式: MP3 → Opus (128kbps VBR)
- 歌词格式: LRC
- 队列持久化: `data/musico/playlist.txt`
- 冷却时间: 全局 10 秒,个人 100 秒

## 注意事项

- 首次使用需要登录网易云音乐账号
- 确保 FFmpeg 已正确安装并在 PATH 中
- 下载的音频文件会保存在配置的目录中
- 队列状态会自动保存,服务器重启后可恢复
```

