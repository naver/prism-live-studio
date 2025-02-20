#include "PLSChannelSupportVideEncoder.h"

QMap<QString, QList<QString>> channelSupportVideoEncoderMap = {{YOUTUBE_RTMP, {"h264", "hevc", "av1"}},
								     {YOUTUBE_HLS, {"h264", "hevc"}},
								     {TWITCH_SERVICE, {"h264"}},
							         {WHIP_SERVICE, {"h264"}},
								     {AFREECATV_SERVICE, {"h264"}},
								     {FACEBOOK_SERVICE, {"h264"}},
								     {NAVER_SHOPPING_LIVE, {"h264", "hevc"}},
								     {BAND, {"h264"}},
								     {NAVER_TV, {"h264"}},
								     {CHZZK, {"h264"}},
								     {CUSTOM_RTMP, {"h264", "hevc"}},
								     {"DEFALUT", {"h264"}}};
