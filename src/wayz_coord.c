/*
 * wayz_coord.c —— 坐标系转换(独立工具)
 *
 * gcj02(国测局/火星坐标) <-> WGS-84 的公开转换算法(Krasovsky 偏移)。
 * 该算法在国内地图/定位领域被广泛使用, 此处实现与主流实现一致。
 *
 * 定位上报链路不使用本模块(云端按 response_sprf 直接返回指定坐标系),
 * 这里保留供应用侧自行换算。
 */
#include "wayz_coord.h"

#include <math.h>

#define WAYZ_COORD_A         6378245.0
#define WAYZ_COORD_EE        0.00669342162296594323
#define WAYZ_COORD_PI        3.14159265358979324

static int wayz_out_of_china(double lat, double lon)
{
    if (lon < 72.004 || lon > 137.8347)
    {
        return 1;
    }
    if (lat < 0.8293 || lat > 55.8271)
    {
        return 1;
    }

    return 0;
}

static double wayz_transform_lat(double x, double y)
{
    double ret = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * x * y + 0.2 * sqrt(fabs(x));
    ret += (20.0 * sin(6.0 * x * WAYZ_COORD_PI) + 20.0 * sin(2.0 * x * WAYZ_COORD_PI)) * 2.0 / 3.0;
    ret += (20.0 * sin(y * WAYZ_COORD_PI) + 40.0 * sin(y / 3.0 * WAYZ_COORD_PI)) * 2.0 / 3.0;
    ret += (160.0 * sin(y / 12.0 * WAYZ_COORD_PI) + 320 * sin(y * WAYZ_COORD_PI / 30.0)) * 2.0 / 3.0;

    return ret;
}

static double wayz_transform_lon(double x, double y)
{
    double ret = 300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * x * y + 0.1 * sqrt(fabs(x));
    ret += (20.0 * sin(6.0 * x * WAYZ_COORD_PI) + 20.0 * sin(2.0 * x * WAYZ_COORD_PI)) * 2.0 / 3.0;
    ret += (20.0 * sin(x * WAYZ_COORD_PI) + 40.0 * sin(x / 3.0 * WAYZ_COORD_PI)) * 2.0 / 3.0;
    ret += (150.0 * sin(x / 12.0 * WAYZ_COORD_PI) + 300.0 * sin(x / 30.0 * WAYZ_COORD_PI)) * 2.0 / 3.0;

    return ret;
}

/**
 * WGS-84 -> gcj02 的正向换算(内部使用)
 */
static void wayz_wgs84_to_gcj02_calc(double wgs_lat, double wgs_lng, double *gcj_lat, double *gcj_lng)
{
    double dLat, dLon, radLat, magic, sqrtMagic;

    if (wayz_out_of_china(wgs_lat, wgs_lng))
    {
        /* 境外坐标不做偏移 */
        *gcj_lat = wgs_lat;
        *gcj_lng = wgs_lng;
        return;
    }

    dLat = wayz_transform_lat(wgs_lng - 105.0, wgs_lat - 35.0);
    dLon = wayz_transform_lon(wgs_lng - 105.0, wgs_lat - 35.0);
    radLat = wgs_lat / 180.0 * WAYZ_COORD_PI;
    magic = sin(radLat);
    magic = 1 - WAYZ_COORD_EE * magic * magic;
    sqrtMagic = sqrt(magic);
    dLat = (dLat * 180.0) / ((WAYZ_COORD_A * (1 - WAYZ_COORD_EE)) / (magic * sqrtMagic) * WAYZ_COORD_PI);
    dLon = (dLon * 180.0) / (WAYZ_COORD_A / sqrtMagic * cos(radLat) * WAYZ_COORD_PI);

    *gcj_lat = wgs_lat + dLat;
    *gcj_lng = wgs_lng + dLon;
}

void wayz_gcj02_to_wgs84(double gcj_lat, double gcj_lng, double *wgs_lat, double *wgs_lng)
{
    double tmp_lat, tmp_lng;

    wayz_wgs84_to_gcj02_calc(gcj_lat, gcj_lng, &tmp_lat, &tmp_lng);

    /* 正向换算的结果再用一次 "x2 - x" 反算回去 */
    *wgs_lat = gcj_lat * 2 - tmp_lat;
    *wgs_lng = gcj_lng * 2 - tmp_lng;
}

void wayz_wgs84_to_gcj02(double wgs_lat, double wgs_lng, double *gcj_lat, double *gcj_lng)
{
    wayz_wgs84_to_gcj02_calc(wgs_lat, wgs_lng, gcj_lat, gcj_lng);
}
