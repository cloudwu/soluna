---@meta

---单个 sprite id 或动画帧 id 列表
---Single sprite id or animation frame id list.
---@alias Sprite integer|integer[]

---sprite bundle 名称到 id 的映射
---Sprite bundle name-to-id mapping.
---@alias SpriteBundle table<string, Sprite?>

---窗口图标图片描述
---Window icon image descriptor.
---@class soluna.IconImage
---@field data string|userdata|lightuserdata RGBA 像素数据 / RGBA pixel buffer
---@field w? integer 宽度；也可使用 `width` / Width; `width` is also accepted
---@field h? integer 高度；也可使用 `height` / Height; `height` is also accepted
---@field width? integer 宽度；`w` 的别名 / Width alias for `w`
---@field height? integer 高度；`h` 的别名 / Height alias for `h`
---@field stride? integer 每行字节数，默认 `width * 4` / Row stride in bytes, default `width * 4`
---@field size? integer `lightuserdata` 数据大小 / Buffer size for `lightuserdata`

---运行时预加载 sprite 图片
---Runtime preloaded sprite image.
---@class soluna.PreloadSprite
---@field filename string 虚拟文件名 / Virtual filename
---@field content string RGBA 像素数据 / RGBA pixel data
---@field w integer 宽度 / Width
---@field h integer 高度 / Height

---音频播放选项
---Audio playback options.
---@class soluna.AudioPlayOptions
---@field group? string audio bus 名称 / Audio bus name
---@field volume? number 线性音量倍率 / Linear volume multiplier
---@field pan? number 声像，范围通常为 `[-1.0, 1.0]` / Stereo pan, usually in `[-1.0, 1.0]`
---@field pitch? number pitch 倍率 / Pitch multiplier
---@field loop? boolean 是否循环播放 / Whether playback loops
---@field stream? boolean 是否流式播放 / Whether to stream instead of preloading

---音频播放实例
---Audio playback instance.
---@class soluna.AudioVoice
local AudioVoice = {}

---停止播放
---Stops playback.
---@param fade_seconds? number fade out 秒数 / Fade-out seconds
---@return boolean ok voice 有效且请求成功时为 true / true when the voice is valid and the request succeeds
function AudioVoice:stop(fade_seconds)
end

---返回是否仍在播放
---Returns whether the voice is still playing.
---@return boolean playing 是否播放中 / Whether it is playing
function AudioVoice:playing()
end

---设置 voice 音量
---Sets voice volume.
---@param volume number 线性音量倍率 / Linear volume multiplier
---@return boolean ok voice 有效时为 true / true when the voice is valid
function AudioVoice:set_volume(volume)
end

---设置 voice 声像
---Sets voice pan.
---@param pan number 声像 / Stereo pan
---@return boolean ok voice 有效时为 true / true when the voice is valid
function AudioVoice:set_pan(pan)
end

---设置 voice pitch
---Sets voice pitch.
---@param pitch number pitch 倍率 / Pitch multiplier
---@return boolean ok voice 有效时为 true / true when the voice is valid
function AudioVoice:set_pitch(pitch)
end

---设置 voice 是否循环
---Sets whether the voice loops.
---@param loop boolean 是否循环 / Whether to loop
---@return boolean ok voice 有效时为 true / true when the voice is valid
function AudioVoice:set_loop(loop)
end

---跳转播放位置
---Seeks to a playback position.
---@param seconds number 目标秒数 / Target seconds
---@return boolean ok voice 有效且 seek 成功时为 true / true when valid and seek succeeds
function AudioVoice:seek(seconds)
end

---返回当前播放位置
---Returns the current playback position.
---@return number? seconds 当前秒数 / Current seconds
---@return string? err 错误信息 / Error message
function AudioVoice:tell()
end

---音频 bus 句柄
---Audio bus handle.
---@class soluna.AudioBus
local AudioBus = {}

---设置 bus 音量
---Sets bus volume.
---@param volume number 线性音量倍率 / Linear volume multiplier
---@return boolean ok bus 存在时为 true / true when the bus exists
function AudioBus:set_volume(volume)
end

---Soluna 主模块
---Soluna root module.
---@class soluna
---@field platform "windows"|"macos"|"linux"|"wasm" 当前平台 / Current platform
---@field version string 运行时版本字符串 / Runtime version string
---@field version_api integer API 版本号 / API version number
local soluna = {}

---返回 `.game` 设置表
---Returns the `.game` settings table.
---@return table settings 游戏设置 / Game settings
function soluna.settings()
end

---设置窗口标题
---Sets the window title.
---@param text string 标题文字 / Window title
function soluna.set_window_title(text)
end

---设置窗口图标
---Sets one icon image or an icon image list.
---@param data soluna.IconImage|soluna.IconImage[] 图标数据 / Icon data
function soluna.set_icon(data)
end

---返回并创建游戏数据目录
---Returns and creates the game data directory.
---@param name? string 项目名，默认来自 `settings.project` / Project name, default from `settings.project`
---@return string path 绝对路径，结尾带 `/` / Absolute path ending with `/`
function soluna.gamedir(name)
end

---加载 sprite bundle
---Loads a sprite bundle.
---@param filename string|table `.dl` 文件路径或已解析 bundle 表 / `.dl` path or parsed bundle table
---@return SpriteBundle sprites sprite 名称映射 / Sprite name mapping
function soluna.load_sprites(filename)
end

---预加载运行时生成的 RGBA sprite 图片
---Preloads runtime-generated RGBA sprite images.
---@param sprites soluna.PreloadSprite|soluna.PreloadSprite[] 单个 sprite 或列表 / One sprite or a list
function soluna.preload(sprites)
end

---加载音频定义 bundle
---Loads an audio definition bundle.
---@param filename string `sounds.dl` 文件路径 / `sounds.dl` path
function soluna.load_sounds(filename)
end

---播放音频并返回 voice
---Plays a sound and returns a voice handle.
---@param name string `sounds.dl` 中的音频名 / Sound name from `sounds.dl`
---@param opts? soluna.AudioPlayOptions 播放选项覆盖 / Playback option overrides
---@return soluna.AudioVoice? voice voice 句柄 / Voice handle
---@return string? err 错误信息 / Error message
function soluna.play_sound(name, opts)
end

---返回 audio bus 句柄
---Returns an audio bus handle.
---@param name string bus 名称 / Bus name
---@return soluna.AudioBus? bus bus 句柄 / Bus handle
---@return string? err 错误信息 / Error message
function soluna.audio_bus(name)
end

return soluna
