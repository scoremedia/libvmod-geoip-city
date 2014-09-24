#include <stdlib.h>
#include <GeoIP.h>
#include <GeoIPCity.h>

#include "vrt.h"
#include "cache/cache.h"
#include "vcc_if.h"

struct GeoIPHandles {
    GeoIP* base;
    GeoIP* city_db;
};

static pthread_key_t key;
static pthread_once_t key_is_initialized = PTHREAD_ONCE_INIT;

static void make_key(void);
static void store_record(GeoIPRecord * record);
static GeoIPRecord * fetch_record(void);
static void clear_record(void);

static void
make_key(void) {
    AZ(pthread_key_create(&key, free));
}

static void
store_record(GeoIPRecord * record)
{
    // Clean up any existing pointers
    GeoIPRecord * existing_pointer = fetch_record();
    if (existing_pointer)
        clear_record();

    AZ(pthread_setspecific(key, record));
}

static GeoIPRecord*
fetch_record(void)
{
    return (GeoIPRecord*)pthread_getspecific(key);
}

static void
clear_record(void)
{
    GeoIPRecord * record = fetch_record();
    if (record != NULL)
        free(record);

    pthread_setspecific(key, NULL);
}

static void
free_geoip_handles(void * ptr)
{
    struct GeoIPHandles * handles = (struct GeoIPHandles*)ptr;

    if (handles && handles->base)
        GeoIP_delete(handles->base);

    if (handles && handles->city_db)
        GeoIP_delete(handles->city_db);

    free(handles);
}

/*
 * Start vmod functions
 */
int
init_function(struct vmod_priv *priv_vcl, const struct VCL_conf *conf)
{
    priv_vcl->priv = malloc(sizeof(struct GeoIPHandles));
    if (!priv_vcl->priv)
        return (1);

    struct GeoIPHandles * handles = (struct GeoIPHandles*)priv_vcl->priv;

    handles->base = GeoIP_new(GEOIP_MMAP_CACHE);
    handles->city_db = GeoIP_open(GeoIPDBFileName[GEOIP_CITY_EDITION_REV1], GEOIP_MMAP_CACHE);

    priv_vcl->free = &free_geoip_handles;

    pthread_once(&key_is_initialized, make_key);

    return (0);
}

VCL_VOID
vmod_locate(const struct vrt_ctx *ctx, struct vmod_priv * priv_vcl, const char * ip)
{
    struct GeoIPHandles * handles = (struct GeoIPHandles*)priv_vcl->priv;
    GeoIP * db = handles->city_db;
    GeoIPRecord * record = GeoIP_record_by_addr(db, ip);

    store_record(record);
}

VCL_VOID
vmod_locate_ip(const struct vrt_ctx * ctx, struct vmod_priv * priv_vcl, const struct suckaddr * ip)
{
    vmod_locate(ctx, priv_vcl, VRT_IP_string(ctx, ip));
}

VCL_VOID
vmod_clean_up(const struct vrt_ctx * ctx)
{
    clear_record();
}

VCL_STRING vmod_country_code(const struct vrt_ctx *ctx)
{
    GeoIPRecord * record = fetch_record();
    if (!(record && record->country_code))
        return WS_Copy(ctx->ws, "", -1);

    return WS_Copy(ctx->ws, record->country_code, -1);
}

VCL_STRING
vmod_country_code3(const struct vrt_ctx *ctx)
{
    GeoIPRecord * record = fetch_record();
    if (!(record && record->country_code3))
        return WS_Copy(ctx->ws, "", -1);

    return WS_Copy(ctx->ws, record->country_code3, -1);
}

VCL_STRING
vmod_country_name(const struct vrt_ctx *ctx)
{
    GeoIPRecord * record = fetch_record();
    if (!(record && record->country_name))
        return WS_Copy(ctx->ws, "", -1);

    return WS_Copy(ctx->ws, record->country_name, -1);
}

VCL_STRING
vmod_region(const struct vrt_ctx *ctx)
{
    GeoIPRecord * record = fetch_record();
    if (!(record && record->region))
        return WS_Copy(ctx->ws, "", -1);

    return WS_Copy(ctx->ws, record->region, -1);
}

VCL_STRING
vmod_city(const struct vrt_ctx *ctx)
{
    GeoIPRecord * record = fetch_record();
    if (!(record && record->city))
        return WS_Copy(ctx->ws, "", -1);

    return WS_Copy(ctx->ws, record->city, -1);
}

VCL_STRING
vmod_postal_code(const struct vrt_ctx *ctx)
{
    GeoIPRecord * record = fetch_record();
    if (!(record && record->postal_code))
        return WS_Copy(ctx->ws, "", -1);

    return WS_Copy(ctx->ws, record->postal_code, -1);
}

VCL_STRING
vmod_latitude(const struct vrt_ctx *ctx)
{
    GeoIPRecord * record = fetch_record();
    if (!(record && record->latitude))
        return WS_Copy(ctx->ws, "", -1);

    int int_latitude = record->latitude;
    int int_latitude_dec = (record->latitude - int_latitude) * 100000;

    char * latitude = WS_Alloc(ctx->ws, 32 * sizeof(char));

    sprintf(latitude, "%d.%04d", int_latitude, int_latitude_dec);

    return latitude;
}

VCL_STRING
vmod_longitude(const struct vrt_ctx *ctx)
{
    GeoIPRecord * record = fetch_record();
    if (!(record && record->longitude))
        return WS_Copy(ctx->ws, "", -1);

    int int_longitude = record->longitude;
    int int_longitude_dec = (record->longitude - int_longitude) * 100000;

    char * longitude = WS_Alloc(ctx->ws, 32 * sizeof(char));

    sprintf(longitude, "%d.%04d", int_longitude, int_longitude_dec);

    return longitude;
}

VCL_STRING
vmod_area_code(const struct vrt_ctx *ctx)
{
    GeoIPRecord * record = fetch_record();
    if (!(record && record->area_code))
        return WS_Copy(ctx->ws, "", -1);

    char * area_code = WS_Alloc(ctx->ws, 32 * sizeof(char));

    sprintf(area_code, "%d", record->area_code);

    return area_code;
}

VCL_STRING
vmod_metro_code(const struct vrt_ctx *ctx)
{
    GeoIPRecord * record = fetch_record();
    if (!(record && record->metro_code))
        return WS_Copy(ctx->ws, "", -1);

    char * metro_code = WS_Alloc(ctx->ws, 32 * sizeof(char));

    sprintf(metro_code, "%d", record->metro_code);

    return metro_code;
}

VCL_STRING
vmod_continent_code(const struct vrt_ctx *ctx)
{
    GeoIPRecord * record = fetch_record();
    if (!(record && record->continent_code))
        return WS_Copy(ctx->ws, "", -1);

    return WS_Copy(ctx->ws, record->continent_code, -1);
}
