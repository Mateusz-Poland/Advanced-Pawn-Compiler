#if defined _geolocation_included
    #endinput
#endif
#define _geolocation_included

/* 
    check libs
*/

#if !defined _samp_included
	#error "Please include <a_samp> or <a_npc> before geolocation."
#endif

#if !defined HTTP
	#error "Please include <a_http> before geolocation."
#endif

#if !defined sscanf
	#error "Please include <sscanf2> before geolocation."
#endif

/* 
    definitions
*/

#if !defined MAX_GEO_IP_LENGTH
    const MAX_GEO_IP_LENGTH = 16;
#endif

#if !defined MAX_GEO_COUNTRY_CODE_LENGTH
    const MAX_GEO_COUNTRY_CODE_LENGTH = 9;
#endif

#if !defined MAX_GEO_COUNTRY_NAME_LENGTH
    const MAX_GEO_COUNTRY_NAME_LENGTH = 32;
#endif

#if !defined MAX_GEO_REGION_CODE_LENGTH
    const MAX_GEO_REGION_CODE_LENGTH = 9;
#endif

#if !defined MAX_GEO_REGION_NAME_LENGTH
    const MAX_GEO_REGION_NAME_LENGTH = 32;
#endif

#if !defined MAX_GEO_DISTRICT_NAME_LENGTH
    const MAX_GEO_DISTRICT_NAME_LENGTH = 64;
#endif

#if !defined MAX_GEO_CITY_NAME_LENGTH
    const MAX_GEO_CITY_NAME_LENGTH = 32;
#endif

#if !defined MAX_GEO_LAT_LON_LENGTH
    const MAX_GEO_LAT_LON_LENGTH = 12;
#endif

#if !defined MAX_GEO_TIMEZONE_NAME_LENGTH
    const MAX_GEO_TIMEZONE_NAME_LENGTH = 64;
#endif

#if !defined MAX_GEO_PROVIDER_NAME_LENGTH
    const  MAX_GEO_PROVIDER_NAME_LENGTH = 32;
#endif

#if !defined MAX_GEO_MOBILE_STATUS_LENGTH
    const MAX_GEO_MOBILE_STATUS_LENGTH = 9;
#endif

#if !defined MAX_GEO_PROXY_LENGTH
    const MAX_GEO_PROXY_LENGTH = 9;
#endif

#define GEO_IP_INFO_PRIMARY_URL          "ip-api.com/csv/"
#define GEO_IP_INFO_PRIMARY_URL_FIELDS   "?fields=61439"

/*
	enums
*/

enum E_PLAYER_GEO_DATA
{
    E_PLAYER_GEO_IP[MAX_GEO_IP_LENGTH + 1],
    E_PLAYER_COUNTRY_CODE[MAX_GEO_COUNTRY_CODE_LENGTH + 1],
    E_PLAYER_COUNTRY[MAX_GEO_COUNTRY_NAME_LENGTH + 1],
    E_PLAYER_REGION_CODE[MAX_GEO_REGION_CODE_LENGTH + 1],
    E_PLAYER_REGION[MAX_GEO_REGION_NAME_LENGTH + 1],
    E_PLAYER_DISTRICT[MAX_GEO_DISTRICT_NAME_LENGTH + 1],
    E_PLAYER_CITY[MAX_GEO_CITY_NAME_LENGTH + 1],
    E_PLAYER_LAT[MAX_GEO_LAT_LON_LENGTH + 1],
    E_PLAYER_LON[MAX_GEO_LAT_LON_LENGTH + 1],
    E_PLAYER_TIMEZONE[MAX_GEO_TIMEZONE_NAME_LENGTH + 1],
    E_PLAYER_PROVIDER[MAX_GEO_PROVIDER_NAME_LENGTH + 1], 
    E_PLAYER_MOBILE_STATUS[MAX_GEO_MOBILE_STATUS_LENGTH + 1],
    E_PLAYER_PROXY[MAX_GEO_PROXY_LENGTH + 1]
};
static g_player_geo_data[MAX_PLAYERS][E_PLAYER_GEO_DATA];

static const g_player_geo_data_reset[E_PLAYER_GEO_DATA] = 
{
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined",
    "Undefined"
};

/* 
    forwards
*/

forward OnPlayerIpInformationRequested(playerid, response_code, data[]);

/*
	get functions 
*/

#define GetPlayerIpEx(%0)               g_player_geo_data[%0][E_PLAYER_GEO_IP]
#define GetPlayerIpCountryCode(%0)      g_player_geo_data[%0][E_PLAYER_COUNTRY_CODE]
#define GetIPCountry(%0)          	g_player_geo_data[%0][E_PLAYER_COUNTRY]
#define GetPlayerIpRegionCode(%0)       g_player_geo_data[%0][E_PLAYER_REGION_CODE]
#define GetPlayerIpRegion(%0)           g_player_geo_data[%0][E_PLAYER_REGION]
#define GetIPCity(%0)             g_player_geo_data[%0][E_PLAYER_CITY]
#define GetPlayerIpDistrict(%0)         g_player_geo_data[%0][E_PLAYER_DISTRICT]
#define GetPlayerIpLatitude(%0)         g_player_geo_data[%0][E_PLAYER_LAT]
#define GetPlayerIpLongtitude(%0)       g_player_geo_data[%0][E_PLAYER_LON]
#define GetPlayerIpLat(%0)              GetPlayerIpLatitude(%0)
#define GetPlayerIpLon(%0)              GetPlayerIpLongtitude(%0)
#define GetPlayerIpTimeZone(%0)         g_player_geo_data[%0][E_PLAYER_TIMEZONE]
#define GetPlayerIpProvider(%0)         g_player_geo_data[%0][E_PLAYER_PROVIDER]
#define GetPlayerIpMobileStatus(%0)     g_player_geo_data[%0][E_PLAYER_MOBILE_STATUS]
#define GetPlayerProxy(%0)      g_player_geo_data[%0][E_PLAYER_PROXY]



/* 
    callbacks
*/

public OnPlayerIpInformationRequested(playerid, response_code, data[])
{
    if (response_code == 200) {
        sscanf(data, "p<,>e<s[32]s[3]s[6]s[32]s[32]s[64]s[12]s[12]s[64]s[32]s[5]s[5]>",
            g_player_geo_data[playerid]
        );
    } else {
        g_player_geo_data[playerid] = g_player_geo_data_reset;
    }

    return true;
}

/* 
    hook OnPlayerConnect
*/

public OnPlayerConnect(playerid)
{
    GetPlayerIp(playerid, g_player_geo_data[playerid][E_PLAYER_GEO_IP], MAX_GEO_IP_LENGTH);

    new query[28 + MAX_GEO_IP_LENGTH + 1];

    strcat(query, GEO_IP_INFO_PRIMARY_URL);
    strcat(query, g_player_geo_data[playerid][E_PLAYER_GEO_IP]);
    strcat(query, GEO_IP_INFO_PRIMARY_URL_FIELDS);

    HTTP(playerid, HTTP_GET, query, "", !"OnPlayerIpInformationRequested");

    #if defined geo_OnPlayerConnect
        return geo_OnPlayerConnect(playerid);
    #else
        return true;
    #endif
}
#if defined _ALS_OnPlayerConnect
    #undef OnPlayerConnect
#else
    #define _ALS_OnPlayerConnect
#endif

#define OnPlayerConnect geo_OnPlayerConnect
#if defined geo_OnPlayerConnect
    forward geo_OnPlayerConnect(playerid);
#endif

/* 
    hook OnPlayerDisconnect
*/

public OnPlayerDisconnect(playerid, reason)
{
    g_player_geo_data[playerid] = g_player_geo_data_reset;

    #if defined geo_OnPlayerDisconnect
        return geo_OnPlayerDisconnect(playerid, reason);
    #else
        return true;
    #endif
}
#if defined _ALS_OnPlayerDisconnect
    #undef OnPlayerDisconnect
#else
    #define _ALS_OnPlayerDisconnect
#endif

#define OnPlayerDisconnect geo_OnPlayerDisconnect
#if defined geo_OnPlayerDisconnect
    forward geo_OnPlayerDisconnect(playerid, reason);
#endif