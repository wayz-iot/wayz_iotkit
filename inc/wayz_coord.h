/*
 * wayz_coord.h —— 坐标系转换(独立工具)
 *
 * 说明: 定位上报链路本身不做坐标换算, 云端按请求参数 response_sprf
 *       直接返回 gcj02 / wgs84 / bd09 中的一种(见 wayz_set_response_sprf)。
 *       本模块保留下来, 供应用侧自行换算使用(例如把 GPS 的 WGS-84 坐标
 *       转成 gcj02 后在国内地图上显示)。
 *
 * 实现为公开的 Krasovsky 偏移算法, 与国内主流实现一致。
 */
#if defined(PKG_WAYZ_IOTKIT_COORD)

#ifndef __WAYZ_COORD_H__
#define __WAYZ_COORD_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * gcj02(国测局/火星坐标) -> WGS-84
 *
 * @param gcj_lat  gcj02 纬度
 * @param gcj_lng  gcj02 经度
 * @param wgs_lat  输出: WGS-84 纬度
 * @param wgs_lng  输出: WGS-84 经度
 *
 * @return void
 */
void wayz_gcj02_to_wgs84(double gcj_lat, double gcj_lng, double *wgs_lat, double *wgs_lng);

/**
 * WGS-84 -> gcj02(国测局/火星坐标)
 *
 * @param wgs_lat  WGS-84 纬度
 * @param wgs_lng  WGS-84 经度
 * @param gcj_lat  输出: gcj02 纬度
 * @param gcj_lng  输出: gcj02 经度
 *
 * @return void
 */
void wayz_wgs84_to_gcj02(double wgs_lat, double wgs_lng, double *gcj_lat, double *gcj_lng);

#ifdef __cplusplus
}
#endif

#endif /* __WAYZ_COORD_H__ */

#endif /* PKG_WAYZ_IOTKIT_COORD */
