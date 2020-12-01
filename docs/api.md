# wayz_iotkit API


```c
/**
 * Wifi parameter initialized
 * 
 * @param ssid: Wifi name
 * 
 * @param passwd: Wifi password
 * 
 * @return twifi_info: Dynamic allocation wifi info structure
*/
twifi_info *wifi_param_init(const char *ssid, const char *passwd);

/**
 * device parameter initialized
 * 
 * @param dev_name: device name
 * 
 * @param manufacturer: device manufacturer
 * 
 * @param product: product name
 * 
 * @param SN: product serial number
 * 
 * @param tenant: tenant device
 * 
 * @return tdeviec_info: Dynamic allocation device info structure
*/
tdeviec_info *dev_para_init(const char *dev_name, const char *manufacturer, const char *product, \
                const char *SN, const char *tenant);

/**
 * Wifi station mac address
 * 
 * @param data: Wifi station mac address data
 * 
 * @return void
*/
void get_sta_mac_addr(char *data);

/**
 * Connected to the Internet to register
 *
 * @param wlan_info: wifi name , wifi passwd
 *
 * @param dev_info: device info ,(dev_name、manufacturer、SN、product、tenant)
 * 
 * @param key: Visiting the website key
 *
 * @return =0: wifi connect failure
 *         =1: device register success
 *         =2: device register failure
 */
char dev_register_init(twifi_info *wlan_info, tdeviec_info *dev_info, char *key);

/**
 * Get the positioning result function
 * 
 * @param wlan_info Wifi related information
 * 
 * @param key Visiting the website key
 * 
 * @param post_data post gnss and cellulars data, obtain positioning results
 * 
 * @param location get location result
 * 
 * @return >0: success
 *         =0: location failure
 * 
*/
char get_position_info(twifi_info *wlan_info, char *key, tpost_data *post_data, tlocation_info *location);

/**
 * print location result
 * 
 * @param location location info
 * 
 * @return void:
 * 
*/
void location_print(tlocation_info location);
```
