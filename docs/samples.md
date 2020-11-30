# wayz_iotkit 示例程序 #

| 示例程序路径                   | 说明 |
| ----                          | ---- |
| samples/location_client.c.c   | wayz_iotkit 测试例程 |

## 运行示例 ##

## 1、申请ACCESS_KEY

- 需要在平台申请ACCESS_KEY，目前只能提供人工申请的方式
在示例代码中的宏中填写申请的ACCESS_KEY字段，例如：
```c
#define  ACCESS_KEY     "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
```
## 2、填写wifi、设备相关信息
修改将所要连接网络的AP即用户名、密码等；也可以在选择组件时指定wifi相关信息。例如：
```c
#ifdef PKG_WAYZ_IOTKIT_WIFI_SSID
#define  WAYZ_WIFI_SSID      PKG_WAYZ_IOTKIT_WIFI_SSID
#else
#define  WAYZ_WIFI_SSID      "thread"
#endif

#ifdef PKG_WAYZ_IOTKIT_WIFI_PASSWORD
#define  WAYZ_WIFI_PWD      PKG_WAYZ_IOTKIT_WIFI_PASSWORD
#else
#define  WAYZ_WIFI_PWD      "12345678"
#endif
```
修改设备相关信息，例如：
```c
#define  DEV_NAME       "PANDDRA"      // 设备名称
#define  VENDER         "ALIENTEK"	   // 设备生产商
#define  PRODUCT        "FINDU01"	   // 设备所属产品
#define  SN             "1234567"	   // 产品序列号
#define  TENANT         "WAYZ"		   // 设备所属租户，通常是开放平台的用户 ID
```

## 3、在`main()`主函数调用例程
在将下面代码嵌入main()中，即可实现调用：
```c
extern int location_client_start(void);
location_client_start();
```


## 示例结果 ##
```c
timestamp: 1606293694990 
gcj02:
	latitude:30.515105
	longitude:114.401555
wgs84:
	latitude:30.517407914397
	longitude:114.396014616712
POI: {"id": "7SkEZdfXQfS","type": "Residential","name": "中建东湖明珠国际公馆","categories":[{"id": 10200,"name": "住宅"}]}
```