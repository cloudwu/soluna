---@meta soluna.image

---
--- Soluna image module
---

---@class soluna.image
local image = {}

---
--- Loads an image from binary data
---
---@param data string Image file data (PNG, JPG, etc.)
---@return userdata? imagedata Image data, or nil on error
---@return integer|string width_or_error Image width, or error message on failure
---@return integer? height Image height
function image.load(data) end

---
--- Resizes an image by scale factors
---
--- The image data can be either RGBA (4 channels) or grayscale (1 channel).
--- Size is validated: for RGBA data must be width*height*4, for grayscale must be width*height.
---
---@param data userdata Image data (external string from image.load)
---@param width integer Source image width
---@param height integer Source image height
---@param scale_x number Horizontal scale factor (e.g., 0.5 for half width)
---@param scale_y? number Vertical scale factor (default: same as scale_x)
---@return userdata imagedata Resized image data
---@return integer width New width (width * scale_x, rounded)
---@return integer height New height (height * scale_y, rounded)
function image.resize(data, width, height, scale_x, scale_y) end

return image
