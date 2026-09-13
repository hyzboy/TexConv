#pragma once
// TexConv 各模块统一错误码。
// 跨 DLL 仅以 int 传递,枚举值稳定,不得改动已有值。

enum TexResult
{
    TEX_OK                  = 0,    ///< 成功
    TEX_ERR_PARAM           = 1,    ///< 参数非法(空指针/非法路径/非法枚举值)
    TEX_ERR_LOAD_IMAGE      = 2,    ///< 图像加载失败(不存在/损坏/无解码委托)
    TEX_ERR_UNSUPPORTED     = 3,    ///< 源图通道数与目标格式组合不受支持
    TEX_ERR_ENCODE          = 4,    ///< 编码失败
    TEX_ERR_IO              = 5,    ///< 文件读写失败
    TEX_ERR_NO_PROVIDER     = 6,    ///< 指定的编码后端不存在/未加载
    TEX_ERR_CANCELLED       = 7,    ///< 进度回调请求取消
    TEX_ERR_INTERNAL        = 8,    ///< 内部错误
};
