#include "eva_radio_catalog.h"

static const eva_radio_city_t s_cities[] = {
    { .id = 2, .name = "上海", .display_name = "SHANGHAI" },
    { .id = 3, .name = "广州", .display_name = "GUANGZHOU" },
    { .id = 4, .name = "深圳", .display_name = "SHENZHEN" },
    { .id = 5, .name = "杭州", .display_name = "HANGZHOU" },
    { .id = 7, .name = "成都", .display_name = "CHENGDU" },
    { .id = 8, .name = "武汉", .display_name = "WUHAN" },
    { .id = 9, .name = "苏州", .display_name = "SUZHOU" },
    { .id = 10, .name = "青岛", .display_name = "QINGDAO" },
    { .id = 11, .name = "济南", .display_name = "JINAN" },
    { .id = 12, .name = "合肥", .display_name = "HEFEI" },
    { .id = 13, .name = "厦门", .display_name = "XIAMEN" },
    { .id = 14, .name = "北京", .display_name = "BEIJING" },
    { .id = 15, .name = "重庆", .display_name = "CHONGQING" },
    { .id = 16, .name = "西安", .display_name = "XIAN" },
    { .id = 17, .name = "长沙", .display_name = "CHANGSHA" },
    { .id = 18, .name = "郑州", .display_name = "ZHENGZHOU" },
    { .id = 19, .name = "宁波", .display_name = "NINGBO" },
    { .id = 20, .name = "无锡", .display_name = "WUXI" },
    { .id = 21, .name = "沈阳", .display_name = "SHENYANG" },
};

/* 2026-10-30 宿主机取样：HTTP 200、audio/mpeg、至少8个连续MP3帧。
   台名来自公开目录，不代表官方身份认证或设备端长期可用性。 */
static const eva_radio_station_t s_stations[] = {
    { 2, 2, "上海东广新闻台", "SHANGHAI EAST RADIO NEWS", "新闻", "http://lhttp.qingting.fm/live/275/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 3, 2, "上海经典947音乐广播", "SHANGHAI CLASSIC 947", "音乐", "http://lhttp.qingting.fm/live/267/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 4, 3, "MYFM全国音乐频道·广州", "MYFM CHINA MUSIC", "音乐", "http://lhttp.qingting.fm/live/20194/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 5, 4, "深圳新闻广播", "SHENZHEN NEWS RADIO", "新闻", "http://lhttp.qingting.fm/live/1270/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 6, 5, "杭州音乐时尚广播", "HANGZHOU MUSIC", "音乐", "http://lhttp.qingting.fm/live/15318146/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 7, 8, "武汉音乐广播", "WUHAN MUSIC RADIO", "音乐", "http://lhttp.qingting.fm/live/1297/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 8, 7, "CRI HITFM·成都", "CRI HIT FM", "音乐", "http://lhttp.qingting.fm/live/15318703/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 9, 8, "武汉新闻广播", "WUHAN NEWS RADIO", "新闻", "http://lhttp.qingting.fm/live/20198/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 10, 9, "苏州音乐广播", "SUZHOU MUSIC RADIO", "音乐", "http://lhttp.qingting.fm/live/2803/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 11, 10, "青岛新闻广播", "QINGDAO NEWS RADIO", "新闻", "http://lhttp.qingting.fm/live/1673/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 12, 11, "济南私家车广播", "JINAN CAR RADIO", "交通", "http://lhttp.qingting.fm/live/1670/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 13, 12, "合肥新闻广播", "HEFEI NEWS RADIO", "新闻", "http://lhttp.qingting.fm/live/20212380/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 14, 13, "厦门综合广播", "XIAMEN GENERAL RADIO", "综合", "http://lhttp.qingting.fm/live/1737/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 15, 2, "上海动感101音乐广播", "RADIO 274", "音乐", "http://lhttp.qingting.fm/live/274/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 16, 2, "上海交通台", "RADIO 266", "交通", "http://lhttp.qingting.fm/live/266/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 17, 2, "上海人民广播电台", "RADIO 270", "综合", "http://lhttp.qingting.fm/live/270/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 18, 2, "上海音乐广播", "RADIO 273", "音乐", "http://lhttp.qingting.fm/live/273/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 19, 2, "上海浦东FM106.5东上海之声", "RADIO 21355", "综合", "http://lhttp.qingting.fm/live/21355/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 20, 2, "上海浦东FM100.1新娱乐广播", "RADIO 5022341", "综合", "http://lhttp.qingting.fm/live/5022341/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 21, 3, "广州交通电台", "RADIO 4955", "交通", "http://lhttp.qingting.fm/live/4955/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 22, 3, "广州新闻电台", "RADIO 4848", "新闻", "http://lhttp.qingting.fm/live/4848/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 23, 4, "深圳飞扬音乐971", "RADIO 1271", "音乐", "http://lhttp.qingting.fm/live/1271/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 24, 4, "深圳快乐1062(交通广播)", "RADIO 1272", "交通", "http://lhttp.qingting.fm/live/1272/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 25, 4, "深圳私家车广播", "RADIO 1273", "综合", "http://lhttp.qingting.fm/live/1273/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 26, 4, "深圳龙岗频道", "RADIO 20160", "综合", "http://lhttp.qingting.fm/live/20160/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 27, 5, "杭州动听968音乐调频", "RADIO 4866", "音乐", "http://lhttp.qingting.fm/live/4866/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 28, 5, "杭州交通广播", "RADIO 1133", "交通", "http://lhttp.qingting.fm/live/1133/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 29, 5, "杭州西湖之声", "RADIO 1163", "综合", "http://lhttp.qingting.fm/live/1163/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 30, 5, "杭州临安广播", "RADIO 20005", "综合", "http://lhttp.qingting.fm/live/20005/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 31, 8, "武汉交通广播·武汉市应急广播电台", "RADIO 4665", "交通", "http://lhttp.qingting.fm/live/4665/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 32, 9, "苏州儿童广播", "RADIO 2807", "儿童", "http://lhttp.qingting.fm/live/2807/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 33, 9, "苏州生活广播", "RADIO 2801", "综合", "http://lhttp.qingting.fm/live/2801/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 34, 9, "苏州交通经济广播", "RADIO 2806", "交通", "http://lhttp.qingting.fm/live/2806/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 35, 9, "苏州新闻广播", "RADIO 2808", "新闻", "http://lhttp.qingting.fm/live/2808/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 36, 9, "苏州戏曲广播", "RADIO 20211622", "戏曲", "http://lhttp.qingting.fm/live/20211622/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 37, 10, "青岛故事广播", "RADIO 4956", "故事", "http://lhttp.qingting.fm/live/4956/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 38, 10, "青岛交通广播", "RADIO 1676", "交通", "http://lhttp.qingting.fm/live/1676/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 39, 10, "青岛经济广播", "RADIO 1674", "经济", "http://lhttp.qingting.fm/live/1674/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 40, 10, "青岛音乐•体育广播", "RADIO 1677", "音乐", "http://lhttp.qingting.fm/live/1677/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 41, 10, "青岛胶州广播", "RADIO 20211644", "综合", "http://lhttp.qingting.fm/live/20211644/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 42, 11, "济南故事广播", "RADIO 1672", "故事", "http://lhttp.qingting.fm/live/1672/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 43, 11, "济南交通广播", "RADIO 1669", "交通", "http://lhttp.qingting.fm/live/1669/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 44, 11, "济南经济广播", "RADIO 1668", "经济", "http://lhttp.qingting.fm/live/1668/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 45, 11, "济南新闻广播", "RADIO 1667", "新闻", "http://lhttp.qingting.fm/live/1667/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 46, 11, "济南音乐广播(MUSIC887)", "RADIO 1671", "音乐", "http://lhttp.qingting.fm/live/1671/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 47, 12, "合肥故事广播", "RADIO 1961", "故事", "http://lhttp.qingting.fm/live/1961/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 48, 12, "合肥交通广播", "RADIO 1960", "交通", "http://lhttp.qingting.fm/live/1960/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 49, 12, "合肥汽车音乐", "RADIO 1975", "音乐", "http://lhttp.qingting.fm/live/1975/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 50, 13, "厦门经济交通广播", "RADIO 1738", "交通", "http://lhttp.qingting.fm/live/1738/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 51, 13, "厦门音乐广播", "RADIO 1739", "音乐", "http://lhttp.qingting.fm/live/1739/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 52, 14, "北京好音乐广播", "RADIO 5021381", "音乐", "http://lhttp.qingting.fm/live/5021381/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 53, 14, "北京文艺广播", "RADIO 333", "文艺", "http://lhttp.qingting.fm/live/333/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 54, 14, "北京交通广播", "RADIO 336", "交通", "http://lhttp.qingting.fm/live/336/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 55, 14, "北京音乐广播", "RADIO 332", "音乐", "http://lhttp.qingting.fm/live/332/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 56, 14, "北京新闻广播", "RADIO 339", "新闻", "http://lhttp.qingting.fm/live/339/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 57, 14, "北京城市广播", "RADIO 345", "综合", "http://lhttp.qingting.fm/live/345/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 58, 15, "重庆都市广播", "RADIO 1502", "综合", "http://lhttp.qingting.fm/live/1502/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 59, 15, "重庆交通广播", "RADIO 1500", "交通", "http://lhttp.qingting.fm/live/1500/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 60, 15, "重庆新闻广播", "RADIO 1498", "新闻", "http://lhttp.qingting.fm/live/1498/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 61, 15, "重庆音乐广播", "RADIO 647", "音乐", "http://lhttp.qingting.fm/live/647/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 62, 16, "西安交通广播", "RADIO 1611", "交通", "http://lhttp.qingting.fm/live/1611/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 63, 16, "西安新闻广播", "RADIO 1610", "新闻", "http://lhttp.qingting.fm/live/1610/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 64, 16, "西安音乐广播", "RADIO 1612", "音乐", "http://lhttp.qingting.fm/live/1612/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 65, 17, "长沙新闻广播", "RADIO 4877", "新闻", "http://lhttp.qingting.fm/live/4877/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 66, 17, "长沙城市之声", "RADIO 4237", "综合", "http://lhttp.qingting.fm/live/4237/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 67, 17, "长沙BIGRADIO流行音乐广播", "RADIO 20847", "音乐", "http://lhttp.qingting.fm/live/20847/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 68, 17, "长沙市望城区人民广播电台", "RADIO 5022076", "综合", "http://lhttp.qingting.fm/live/5022076/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 69, 17, "长沙交通广播", "RADIO 3967", "交通", "http://lhttp.qingting.fm/live/3967/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 70, 17, "长沙品味音乐广播", "RADIO 4930", "音乐", "http://lhttp.qingting.fm/live/4930/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 71, 18, "郑州电台怀旧好声音", "RADIO 1223", "综合", "http://lhttp.qingting.fm/live/1223/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 72, 18, "郑州私家车", "RADIO 1222", "综合", "http://lhttp.qingting.fm/live/1222/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 73, 18, "郑州车道931", "RADIO 1221", "综合", "http://lhttp.qingting.fm/live/1221/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 74, 18, "郑州活力944", "RADIO 4921", "综合", "http://lhttp.qingting.fm/live/4921/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 75, 18, "郑州汽车广播", "RADIO 1211", "综合", "http://lhttp.qingting.fm/live/1211/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 76, 18, "郑州新闻广播", "RADIO 1220", "新闻", "http://lhttp.qingting.fm/live/1220/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 77, 19, "宁波交通广播", "RADIO 1140", "交通", "http://lhttp.qingting.fm/live/1140/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 78, 19, "宁波经济广播", "RADIO 1152", "经济", "http://lhttp.qingting.fm/live/1152/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 79, 19, "宁波私家车音乐台", "RADIO 1142", "音乐", "http://lhttp.qingting.fm/live/1142/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 80, 19, "宁波新闻广播", "RADIO 1138", "新闻", "http://lhttp.qingting.fm/live/1138/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 81, 20, "无锡交通广播", "RADIO 2780", "交通", "http://lhttp.qingting.fm/live/2780/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 82, 20, "无锡经济广播", "RADIO 2778", "经济", "http://lhttp.qingting.fm/live/2778/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 83, 20, "无锡梁溪之声广播", "RADIO 2782", "综合", "http://lhttp.qingting.fm/live/2782/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 84, 20, "无锡汽车音乐广播", "RADIO 2779", "音乐", "http://lhttp.qingting.fm/live/2779/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 85, 20, "无锡新闻广播", "RADIO 2777", "新闻", "http://lhttp.qingting.fm/live/2777/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 86, 20, "无锡综合广播", "RADIO 2776", "综合", "http://lhttp.qingting.fm/live/2776/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 87, 21, "沈阳都市广播", "RADIO 1099", "综合", "http://lhttp.qingting.fm/live/1099/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 88, 21, "沈阳音乐广播 路上好朋友", "RADIO 1101", "音乐", "http://lhttp.qingting.fm/live/1101/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 89, 21, "沈阳新民广播FM103.9", "RADIO 5022535", "综合", "http://lhttp.qingting.fm/live/5022535/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
    { 90, 21, "沈阳新闻广播", "RADIO 20024", "新闻", "http://lhttp.qingting.fm/live/20024/64k.mp3", "https://github.com/qist/tvbox/blob/master/radio.txt", "2026-10-30", 64, EVA_RADIO_CODEC_MP3 },
};

size_t eva_radio_city_count(void)
{
    return sizeof(s_cities) / sizeof(s_cities[0]);
}

bool eva_radio_city_at(size_t index, const eva_radio_city_t **city)
{
    if (!city || index >= eva_radio_city_count()) return false;
    *city = &s_cities[index];
    return true;
}

size_t eva_radio_station_count(void)
{
    return sizeof(s_stations) / sizeof(s_stations[0]);
}

bool eva_radio_station_at(size_t index, const eva_radio_station_t **station)
{
    if (!station || index >= eva_radio_station_count()) return false;
    *station = &s_stations[index];
    return true;
}

size_t eva_radio_stations_for_city(uint16_t city_id)
{
    size_t count = 0;
    for (size_t i = 0; i < eva_radio_station_count(); i++)
        if (s_stations[i].city_id == city_id) count++;
    return count;
}

bool eva_radio_station_for_city_at(uint16_t city_id, size_t index,
                                   const eva_radio_station_t **station)
{
    if (!station) return false;
    for (size_t i = 0; i < eva_radio_station_count(); i++) {
        if (s_stations[i].city_id != city_id) continue;
        if (index == 0) {
            *station = &s_stations[i];
            return true;
        }
        index--;
    }
    return false;
}
