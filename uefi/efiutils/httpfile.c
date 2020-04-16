/*******************************************************************************
 * Copyright (c) 2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * httpfile.c -- Provides access to files through the HTTP protocol
 */

#include <string.h>
#include <stdlib.h>
#include "efi_private.h"
#include "ServiceBinding.h"
#include "Http.h"

static EFI_GUID HttpServiceBindingProto =
   EFI_HTTP_SERVICE_BINDING_PROTOCOL_GUID;
static EFI_GUID HttpProto = EFI_HTTP_PROTOCOL_GUID;
static EFI_GUID DevicePathProto = DEVICE_PATH_PROTOCOL;
static EFI_GUID LoadFileProto = LOAD_FILE_PROTOCOL;

static EFI_SERVICE_BINDING_PROTOCOL *HttpServiceBinding;
static EFI_HANDLE HttpHandle;
static EFI_HTTP_PROTOCOL *Http;
static EFI_EVENT HttpEvent;
static bool HttpDone;
static UINT16 LocalPort;

#define NUM_HEADERS 2
#define TIMEOUT_MS 10000
#define MAX_RETRIES 2

static const char *HttpStatusStrings[] = {
   [HTTP_STATUS_UNSUPPORTED_STATUS] = "Unknown",
   [HTTP_STATUS_100_CONTINUE] = "100 Continue",
   [HTTP_STATUS_101_SWITCHING_PROTOCOLS] = "101 Switching Protocols",
   [HTTP_STATUS_200_OK] = "200 OK",
   [HTTP_STATUS_201_CREATED] = "201 Created",
   [HTTP_STATUS_202_ACCEPTED] = "202 Accepted",
   [HTTP_STATUS_203_NON_AUTHORITATIVE_INFORMATION] =
      "203 Non-Authoritative Information",
   [HTTP_STATUS_204_NO_CONTENT] = "204 No Content",
   [HTTP_STATUS_205_RESET_CONTENT] = "205 Reset Content",
   [HTTP_STATUS_206_PARTIAL_CONTENT] = "206 Partial Content",
   [HTTP_STATUS_300_MULTIPLE_CHIOCES] = "300 Multiple Choices",
   [HTTP_STATUS_301_MOVED_PERMANENTLY] = "301 Moved Permanently",
   [HTTP_STATUS_302_FOUND] = "302 Found",
   [HTTP_STATUS_303_SEE_OTHER] = "303 See Other",
   [HTTP_STATUS_304_NOT_MODIFIED] = "304 Not Modified",
   [HTTP_STATUS_305_USE_PROXY] = "305 Use Proxy",
   [HTTP_STATUS_307_TEMPORARY_REDIRECT] = "307 Temporary Redirect",
   [HTTP_STATUS_400_BAD_REQUEST] = "400 Bad Request",
   [HTTP_STATUS_401_UNAUTHORIZED] = "401 Unauthorized",
   [HTTP_STATUS_402_PAYMENT_REQUIRED] = "402 Payment Required",
   [HTTP_STATUS_403_FORBIDDEN] = "403 Forbidden",
   [HTTP_STATUS_404_NOT_FOUND] = "404 Not Found",
   [HTTP_STATUS_405_METHOD_NOT_ALLOWED] = "405 Method Not Allowed",
   [HTTP_STATUS_406_NOT_ACCEPTABLE] = "406 Not Acceptable",
   [HTTP_STATUS_407_PROXY_AUTHENTICATION_REQUIRED] =
      "407 Proxy Authentication Required",
   [HTTP_STATUS_408_REQUEST_TIME_OUT] = "408 Request Timeout",
   [HTTP_STATUS_409_CONFLICT] = "409 Conflict",
   [HTTP_STATUS_410_GONE] = "410 Gone",
   [HTTP_STATUS_411_LENGTH_REQUIRED] = "411 Length Required",
   [HTTP_STATUS_412_PRECONDITION_FAILED] = "412 Precondition Failed",
   [HTTP_STATUS_413_REQUEST_ENTITY_TOO_LARGE] = "413 Request Entity Too Large",
   [HTTP_STATUS_414_REQUEST_URI_TOO_LARGE] = "414 Request-URI Too Large",
   [HTTP_STATUS_415_UNSUPPORTED_MEDIA_TYPE] = "415 Unsupported Media Type",
   [HTTP_STATUS_416_REQUESTED_RANGE_NOT_SATISFIED] =
      "416 Requested Range Not Satisfiable",
   [HTTP_STATUS_417_EXPECTATION_FAILED] = "417 Expectation Failed",
   [HTTP_STATUS_500_INTERNAL_SERVER_ERROR] = "500 Internal Server Error",
   [HTTP_STATUS_501_NOT_IMPLEMENTED] = "501 Not Implemented",
   [HTTP_STATUS_502_BAD_GATEWAY] = "502 Bad Gateway",
   [HTTP_STATUS_503_SERVICE_UNAVAILABLE] = "503 Service Unavailable",
   [HTTP_STATUS_504_GATEWAY_TIME_OUT] = "504 Gateway Time Out",
   [HTTP_STATUS_505_HTTP_VERSION_NOT_SUPPORTED] =
      "505 HTTP Version Not Supported",
};

/*
 * Info about current image if it was loaded via HTTP.
 */
static struct {
   EFI_HANDLE volume;
   char *url;
   EFI_DEVICE_PATH *ipdp;
   int ipv;
} HttpImageInfo;

/*
 * Exported LoadFile protocol.
 */
static EFIAPI EFI_STATUS
http_efi_load_file(EFI_LOAD_FILE_PROTOCOL *This,
                   EFI_DEVICE_PATH_PROTOCOL *FileDevpath,
                   BOOLEAN BootPolicy,
                   UINTN *BufferSize,
                   VOID *Buffer);

EFI_LOAD_FILE_PROTOCOL HttpLoadFile = {
   http_efi_load_file
};

/*-- is_http_boot --------------------------------------------------------------
 *
 *      Check whether the current running image was loaded directly via HTTP.
 *      "Directly" means the HTTP URL was for the image itself, not for a
 *      ramdisk containing the image.
 *
 * Results
 *      True/false
 *
 * Side effects
 *      Cache information about the image.
 *----------------------------------------------------------------------------*/
bool is_http_boot(void)
{
   static bool first_time = true;
   static bool is_http = false;
   EFI_STATUS Status;
   EFI_LOADED_IMAGE *Image;
   EFI_DEVICE_PATH *DevPath, *NewDevPath;
   EFI_DEVICE_PATH *node, *ipv_node, *tmp;

   if (!first_time) {
      return is_http;
   }
   first_time = false;

   /*
    * NOTE: When DEBUG is not defined at build time, this function is typically
    * first called before logging has been initialized, so the Log calls in it
    * have no effect.  (The call chain is efi_main -> efi_create_argv ->
    * get_boot_file.)
    */

   Status = image_get_info(ImageHandle, &Image);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error getting image info: %s",
          error_str[error_efi_to_generic(Status)]);
      return false;
   }

   Status = devpath_get(Image->DeviceHandle, &DevPath);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error getting image devpath: %s",
          error_str[error_efi_to_generic(Status)]);
      return false;
   }
   log_devpath(LOG_DEBUG, "Image->DeviceHandle", DevPath);

   Status = devpath_duplicate(DevPath, &NewDevPath);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error duplicating image devpath: %s",
          error_str[error_efi_to_generic(Status)]);
      return false;
   }

   ipv_node = NULL;
   FOREACH_DEVPATH_NODE(NewDevPath, node) {
      if (node->Type == MESSAGING_DEVICE_PATH) {

         if (node->SubType == MSG_IPv4_DP ||
             node->SubType == MSG_IPv6_DP) {
            size_t len;
            EFI_DEVICE_PATH *ipdp;

            /* Save a copy of IPv4 or IPv6 devpath node */
            len = DevPathNodeLength(node);
            ipdp = sys_malloc(len);
            memcpy(ipdp, node, len);
            HttpImageInfo.ipv = (ipdp->SubType == MSG_IPv4_DP) ? 4 : 6;
            HttpImageInfo.ipdp = ipdp;
            ipv_node = node;

         } else if (ipv_node != NULL && node->SubType == MSG_URI_DP) {
            size_t len;
            char *url;

            /* Save a copy of the URL */
            len = DevPathNodeLength(node) - sizeof(*node);
            url = sys_malloc(len + 1);
            memcpy(url, &((URI_DEVICE_PATH *)node)->Uri, len);
            url[len] = '\0';
            HttpImageInfo.url = url;
            is_http = true;
         }

      } else if (node->Type == MEDIA_DEVICE_PATH &&
                 node->SubType == MEDIA_RAM_DISK_DP) {
         /* Ramdisk node found: not direct HTTP boot */
         is_http = false;
         break;
      }
   }

   if (is_http) {
      /* Get "volume" (NIC handle) */
      SetDevPathEndNode(ipv_node);
      tmp = NewDevPath;
      Status = bs->LocateDevicePath(&HttpServiceBindingProto,
                                    &tmp, &HttpImageInfo.volume);
      if (EFI_ERROR(Status)) {
         Log(LOG_ERR, "Error getting NIC handle: %s",
             error_str[error_efi_to_generic(Status)]);
         is_http = false;
      }
   }
   sys_free(NewDevPath);

   if (is_http) {
      Log(LOG_DEBUG, "is_http_boot: Image loaded directly via HTTP, IPv%u",
          HttpImageInfo.ipv);
      log_devpath(LOG_DEBUG, "   DevPath", DevPath);
      log_handle_devpath(LOG_DEBUG, "   NIC handle", HttpImageInfo.volume);
      Log(LOG_DEBUG, "   URL %s", HttpImageInfo.url);
   } else if (HttpImageInfo.url != NULL) {
      Log(LOG_DEBUG, "is_http_boot: Image from ramdisk loaded via HTTP, IPv%u",
          HttpImageInfo.ipv);
      log_devpath(LOG_DEBUG, "   DevPath", DevPath);
      Log(LOG_DEBUG, "   URL %s", HttpImageInfo.url);
   } else {
      Log(LOG_DEBUG, "is_http_boot: Image not loaded via HTTP");
   }

   return is_http;
}

/*-- get_http_boot_url ---------------------------------------------------------
 *
 *      Get the URL of the HTTP boot file.
 *
 * Parameter
 *      OUT buffer: pointer to the freshly allocated URL.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_http_boot_url(char **buffer)
{
   if (is_http_boot()) {
      *buffer = strdup(HttpImageInfo.url);
      return ERR_SUCCESS;
   } else {
      return ERR_UNSUPPORTED;
   }
}

/*-- get_http_boot_volume ------------------------------------------------------
 *
 *      Get a suitable "boot volume handle" for HTTP boot.  This ends up being
 *      the NIC that the current image was loaded from.
 *
 * Parameter
 *      OUT Volume: the boot volume handle
 *
 * Results
 *      EFI_SUCCESS, or a UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS get_http_boot_volume(EFI_HANDLE *Volume)
{
   if (is_http_boot()) {
      *Volume = HttpImageInfo.volume;
      return EFI_SUCCESS;
   } else {
      return EFI_UNSUPPORTED;
   }
}

/*-- get_http_boot_ipv ---------------------------------------------------------
 *
 *      Get the IP version the current image was loaded with.
 *
 * Parameters
 *      IN  handle: EFI handle
 *      OUT ipv:    4 or 6.
 *
 * Results
 *      EFI_SUCCESS, or EFI_UNSUPPORTED if no IP version found.
 *----------------------------------------------------------------------------*/
static EFI_STATUS get_http_boot_ipv(int *ipv)
{
   if (is_http_boot()) {
      *ipv = HttpImageInfo.ipv;
      return EFI_SUCCESS;
   } else {
      return EFI_UNSUPPORTED;
   }
}

/*-- make_http_child_dh --------------------------------------------------------
 *
 *      Create something suitable to pass as ChildHandle->DeviceHandle when
 *      starting a child image that was chainloaded via HTTP.
 *
 * Parameter
 *      IN  url:       URL the child was loaded from.
 *      OUT ChildDH:   device handle.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS make_http_child_dh(const char *url, EFI_HANDLE *ChildDH)
{
   /*
    * This library relies on device path of the Image->DeviceHandle that its
    * parent (initially the UEFI boot manager) passed into it, in order to
    * determine what URL it was loaded from and what IP version was used.  In
    * order to pass similar information to a child that may be using the same
    * library, this routine creates a handle with a similarly formatted device
    * path.  Just passing on the same handle the current app received would not
    * generally work, as the URL is different.  (In the future the IP
    * version could be different too, but that is not currently supported.)
    */
   EFI_STATUS Status;
   EFI_DEVICE_PATH *NicPath;
   EFI_DEVICE_PATH *node;
   EFI_DEVICE_PATH *ChildPath;
   size_t s1, s2, s3;
   char *p = NULL;

   Status = devpath_get(HttpImageInfo.volume, &NicPath);
   if (EFI_ERROR(Status)) {
      return Status;
   }
   if (HttpImageInfo.volume == NULL || HttpImageInfo.ipdp == NULL) {
      return EFI_UNSUPPORTED;
   }

   FOREACH_DEVPATH_NODE(NicPath, node) {
      if (IsDevPathEndType(node)) {
         break;
      }
   }
   s1 = (char *)node - (char *)NicPath;
   s2 = DevPathNodeLength(HttpImageInfo.ipdp);
   s3 = sizeof(EFI_DEVICE_PATH) + strlen(url) + 1;

   p = sys_malloc(s1 + s2 + s3 + sizeof(EFI_DEVICE_PATH));
   memcpy(p, NicPath, s1);
   memcpy(p + s1, HttpImageInfo.ipdp, s2);
   node = (EFI_DEVICE_PATH *)(p + s1 + s2);
   node->Type = MESSAGING_DEVICE_PATH;
   node->SubType = MSG_URI_DP;
   SetDevPathNodeLength(node, s3);
   memcpy(p + s1 + s2 + sizeof(EFI_DEVICE_PATH), url,
          s3 - sizeof(EFI_DEVICE_PATH));
   node = (EFI_DEVICE_PATH *)(p + s1 + s2 + s3);
   SetDevPathEndNode(node);
   ChildPath = (EFI_DEVICE_PATH *)p;

   *ChildDH = NULL;
   Status = bs->InstallProtocolInterface(ChildDH, &DevicePathProto,
                                         EFI_NATIVE_INTERFACE, ChildPath);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "make_http_child_dh: error creating handle: %s",
          error_str[error_efi_to_generic(Status)]);
      goto out;
   }

   /*
    * Additionally, install a LoadFile protocol on the handle that makes this
    * image's HTTP connection available to child images.  In particular, this
    * allows an old build of mboot.efi that doesn't have its own HTTP code to
    * load boot modules over HTTP via the parent menu.efi's HTTP connection.
    */
   Status = bs->InstallProtocolInterface(ChildDH, &LoadFileProto,
                                         EFI_NATIVE_INTERFACE, &HttpLoadFile);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "make_http_child_dh: error installing LoadFileProto: %s",
          error_str[error_efi_to_generic(Status)]);
      goto out;
   }

 out:
   if (EFI_ERROR(Status) && ChildPath != NULL) {
      sys_free(ChildPath);
   }
   return Status;
}

/*-- get_url_hostname ----------------------------------------------------------
 *
 *      Get the hostname from a URL.
 *
 * Parameters
 *      IN  url:      URL
 *      OUT hostname: freshly allocated hostname
 *
 * Results
 *      EFI_SUCCESS or an error.
 *----------------------------------------------------------------------------*/
static EFI_STATUS get_url_hostname(const char *url, char **hostname)
{
   const char *p, *q;
   char *h;
   size_t len;

   p = url;

   /* Strip scheme:// */
   q = strstr(url, "://");
   if (q == NULL) {
      return EFI_INVALID_PARAMETER;
   }
   p = q + 3;

   /* Strip path */
   q = strchr(p, '/');
   if (q == NULL) {
      len = strlen(p);
   } else {
      len = q - p;
   }

   /* Strip userinfo@ */
   q = strchr(p, '@');
   if (q != NULL) {
      p = q + 1;
   }

   /* Strip :port */
   q = strrchr(p, ']');
   if (q == NULL) {
      q = p;
   }
   q = strrchr(q, ':');
   if (q != NULL) {
      len = q - p;
   }

   h = sys_malloc(len + 1);
   if (h == NULL) {
      return EFI_OUT_OF_RESOURCES;
   }
   memcpy(h, p, len);
   h[len] = '\0';
   *hostname = h;
   return EFI_SUCCESS;
}

/*-- http_callback -------------------------------------------------------------
 *
 *      Callback for Http->Request and Http->Response
 *
 * Parameters
 *      IN  Event:    Event that was notified
 *      IN  Context:  Context parameter
 *----------------------------------------------------------------------------*/
VOID EFIAPI http_callback(UNUSED_PARAM(EFI_EVENT Event), VOID *Context)
{
   *(bool *)Context = true;
}

/*-- http_init -----------------------------------------------------------------
 *
 *      Initialize for loading files via HTTP.
 *
 * Parameters
 *      IN  NicHandle: Handle for the NIC to use.
 *
 * Results
 *      EFI_SUCCESS, or EFI_UNSUPPORTED if no IP version is associated.
 *----------------------------------------------------------------------------*/
static EFI_STATUS http_init(EFI_HANDLE NicHandle)
{
   EFI_STATUS Status;
   int ipv = 0;
   EFI_HTTP_CONFIG_DATA HttpConfigData;
   EFI_HTTPv4_ACCESS_POINT IPv4Node;
   EFI_HTTPv6_ACCESS_POINT IPv6Node;

   if (Http != NULL) {
      return EFI_SUCCESS; // already initialized
   }

   if (HttpEvent == NULL) {
      bs->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK,
                      http_callback, &HttpDone, &HttpEvent);
   }

   /*
    * Pick a random-ish range of dynamic local ports.
    */
   if (LocalPort == 0) {
      UINT64 Count;
      bs->GetNextMonotonicCount(&Count);
      LocalPort = 49160 + (Count % 1627) * 10;
   }

   /*
    * Find which IP version to use.
    */
   Status = get_http_boot_ipv(&ipv);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error determining IP version: %s",
          error_str[error_efi_to_generic(Status)]);
      goto out;
   }

   if (ipv == 4) {
      /*
       * Ensure we have an IP address.  This seems to be needed; otherwise
       * Http->Request typically fails with EFI_NO_MAPPING (which is not
       * documented as a possibility for it, by the way).  From study of a
       * packet trace, it appears that the Http->Request does automatically
       * cause DHCP to be started, but it forgets to wait for the DHCP
       * transaction to finish before trying to connect to the HTTP server.
       */
      Status = get_ipv4_addr(NicHandle);
      if (EFI_ERROR(Status)) {
         Log(LOG_ERR, "Error getting IP address: %s",
             error_str[error_efi_to_generic(Status)]);
         goto out;
      }
   }

   /*
    * Find and initialize HTTP protocol.  Passing NicHandle to
    * get_protocol_interface causes the same NIC to be used that loaded
    * this EFI app.
    */
   Status = get_protocol_interface(NicHandle, &HttpServiceBindingProto,
                                   (void**)&HttpServiceBinding);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error getting HttpServiceBinding protocol: %s",
          error_str[error_efi_to_generic(Status)]);
      goto out;
   }

   Status = HttpServiceBinding->CreateChild(HttpServiceBinding, &HttpHandle);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error creating Http child handle: %s",
          error_str[error_efi_to_generic(Status)]);
      goto out;
   }

   Status = get_protocol_interface(HttpHandle, &HttpProto, (void **)&Http);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error getting Http protocol: %s",
          error_str[error_efi_to_generic(Status)]);
      goto out;
   }

   /*
    * Configure Http
    */
   memset(&HttpConfigData, 0, sizeof(HttpConfigData));
   HttpConfigData.HttpVersion = HttpVersion11;
   HttpConfigData.TimeOutMillisec = TIMEOUT_MS;
   HttpConfigData.LocalAddressIsIPv6 = (ipv == 6);
   if (HttpConfigData.LocalAddressIsIPv6) {
      memset(&IPv6Node, 0, sizeof(IPv6Node));
      IPv6Node.LocalPort = LocalPort++;
      HttpConfigData.AccessPoint.IPv6Node = &IPv6Node;
   } else {
      memset(&IPv4Node, 0, sizeof(IPv4Node));
      IPv4Node.UseDefaultAddress = true;
      IPv4Node.LocalPort = LocalPort++;
      HttpConfigData.AccessPoint.IPv4Node = &IPv4Node;
   }
   Status = Http->Configure(Http, &HttpConfigData);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error in Http->Configure: %s",
          error_str[error_efi_to_generic(Status)]);
      goto out;
   }

out:
   if (EFI_ERROR(Status)) {
      http_cleanup();
   }
   return Status;
}

/*-- http_cleanup --------------------------------------------------------------
 *
 *      Clean up cached Http instance.
 *----------------------------------------------------------------------------*/
void http_cleanup(void)
{
   if (Http != NULL) {
      Log(LOG_DEBUG, "http_cleanup: Http->Configure(Http, NULL)");
      Http->Configure(Http, NULL);
      Http = NULL;
   }

   if (HttpHandle != NULL) {
      Log(LOG_DEBUG, "http_cleanup: Destroying HttpHandle");
      HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpHandle);
      HttpHandle = NULL;
   }

   HttpServiceBinding = NULL;
}

/*-- http_status ---------------------------------------------------------------
 *
 *      Translate EFI_HTTP_STATUS_CODE to a human-readable string.
 *
 * Parameter
 *      IN code: EFI_HTTP_STATUS_CODE value
 *
 * Results
 *      Static ASCII string
 *----------------------------------------------------------------------------*/
static const char *http_status(EFI_HTTP_STATUS_CODE code)
{
   if (code >= ARRAYSIZE(HttpStatusStrings)) {
      return "Out of range";
   } else {
      return HttpStatusStrings[code];
   }
}

/*-- http_file_load_try --------------------------------------------------------
 *
 *      Try once to load a file into memory or get its length, using HTTP.
 *
 * Parameters
 *      IN     try:       retry number (0 = first try).
 *      IN     Url:       URL to load.
 *      IN     hostname:  hostname from URL.
 *      IN     callback:  routine to be called periodically while the file
 *                        is being loaded.
 *      IN/OUT Buffer:    buffer where the file is loaded:
 *                           if Buffer=NULL, just get the file's length;
 *                           else if *Buffer=NULL, allocate a buffer;
 *                           else use the given *Buffer (size in *BufSize).
 *      IN/OUT BufSize:   length of Buffer, size of file:
 *                           if Buffer=NULL, OUT only;
 *                           else if *Buffer=NULL, OUT only;
 *                           else IN/OUT.
 *
 * Results
 *      EFI_SUCCESS, or an EFI error status.  EFI_ACCESS_DENIED indicates that a
 *      retry is needed because the connection was closed.
 *----------------------------------------------------------------------------*/
static EFI_STATUS http_file_load_try(int try, CHAR16 *Url, char *hostname,
                                     int (*callback)(size_t), VOID **Buffer,
                                     UINTN *BufSize)
{
   EFI_STATUS Status;
   EFI_HTTP_TOKEN ReqToken;
   EFI_HTTP_MESSAGE ReqMessage;
   EFI_HTTP_REQUEST_DATA ReqData;
   EFI_HTTP_HEADER ReqHeaders[NUM_HEADERS];
   EFI_HTTP_TOKEN RespToken;
   EFI_HTTP_MESSAGE RespMessage;
   EFI_HTTP_RESPONSE_DATA RespData;
   EFI_HTTP_STATUS_CODE HttpStatus = HTTP_STATUS_200_OK;
   unsigned i;
   uint8_t *buf = NULL;
   size_t size = (size_t)-1;
   size_t size_recd;

   /*
    * Needed early to prep for possible "goto out" on error.
    */
   memset(&ReqToken, 0, sizeof(ReqToken));
   memset(&ReqMessage, 0, sizeof(ReqMessage));
   memset(&ReqData, 0, sizeof(ReqData));
   memset(&RespToken, 0, sizeof(RespToken));
   memset(&RespMessage, 0, sizeof(RespMessage));
   memset(&RespData, 0, sizeof(RespData));

   /*
    * Send HTTP GET request.
    */
   ReqToken.Event = HttpEvent;
   ReqToken.Status = EFI_NOT_READY;
   ReqToken.Message = &ReqMessage;
   ReqMessage.Data.Request = &ReqData;
   ReqMessage.HeaderCount = NUM_HEADERS;
   ReqMessage.Headers = ReqHeaders;
   ReqData.Method = (Buffer == NULL) ? HttpMethodHead : HttpMethodGet;
   ReqData.Url = Url;
   ReqHeaders[0].FieldName = (CHAR8 *)"User-Agent";
   ReqHeaders[0].FieldValue = (CHAR8 *)"esx-boot/2.0";
   ReqHeaders[1].FieldName = (CHAR8 *)"Host";
   ReqHeaders[1].FieldValue = hostname;

   HttpDone = false;
   Status = Http->Request(Http, &ReqToken);
   if (EFI_ERROR(Status)) {
      Log(try == 0 ? LOG_DEBUG : LOG_WARNING,
          "Error in Http->Request: %s",
          error_str[error_efi_to_generic(Status)]);
      goto out;
   }
   while (!HttpDone) {
      Http->Poll(Http);
   }
   if (EFI_ERROR(ReqToken.Status)) {
      Status = ReqToken.Status;
      Log(LOG_ERR, "Async error from Http->Request: %s",
          error_str[error_efi_to_generic(Status)]);
      goto out;
   }

   /*
    * Pick up first part of response -- namely, just the headers -- to get the
    * file length.
    */
   RespToken.Event = HttpEvent;
   RespToken.Status = EFI_SUCCESS;
   RespToken.Message = &RespMessage;
   RespMessage.Data.Response = &RespData;
   RespMessage.BodyLength = 0;
   RespMessage.Body = NULL;
   RespData.StatusCode = HTTP_STATUS_UNSUPPORTED_STATUS;

   HttpDone = false;
   Status = Http->Response(Http, &RespToken);
   if (EFI_ERROR(Status)) {
      Log(try == 0 ? LOG_DEBUG : LOG_WARNING,
          "Error in Http->Response (header): %s",
          error_str[error_efi_to_generic(Status)]);
      goto out;
   }
   while (!HttpDone) {
      Http->Poll(Http);
   }
   if (EFI_ERROR(RespToken.Status)) {
      if (RespToken.Status == EFI_HTTP_ERROR) {
         /*
          * In this case we still must proceed to read the body, but we'll
          * throw it away later.  The body will typically be an HTML file
          * complaining about the error; e.g., a "404 Not Found" page.
          */
         HttpStatus = RespData.StatusCode;
         Log(LOG_DEBUG, "HTTP error from Http->Response: %s",
             http_status(HttpStatus));
      } else {
         Status = RespToken.Status;
         Log(LOG_ERR, "Async error from Http->Response: %s",
             error_str[error_efi_to_generic(Status)]);
         goto out;
      }
   }
   for (i = 0; i < RespMessage.HeaderCount; ++i) {
      /* Get the length of the file from the ContentLength header */
      if (strcmp(RespMessage.Headers[i].FieldName, "Content-Length") == 0) {
         size = strtol(RespMessage.Headers[i].FieldValue, NULL, 10);
         break;
      }
   }
   if (RespMessage.Headers != NULL) {
      sys_free(RespMessage.Headers);
      RespMessage.Headers = NULL;
   }
   if (size == (size_t)-1) {
      Log(LOG_ERR, "No http Content-Length header");
      Status = EFI_PROTOCOL_ERROR;
      goto out;
   }

   /*
    * If just getting the file length, we are done now.
    */
   if (Buffer == NULL) {
      goto out;
   }

   /*
    * Allocate buffer to store the file contents if needed.
    */
   if (*Buffer != NULL) {
      if (*BufSize < size) {
         Log(LOG_DEBUG, "Buffer for http file too small (%zu < %zu)",
             *BufSize, size);
         *BufSize = size;
         Status = EFI_BUFFER_TOO_SMALL;
         goto out;
      }
      buf = *Buffer;
   } else {
      buf = sys_malloc(size);
      if (buf == NULL) {
         Status = EFI_OUT_OF_RESOURCES;
         Log(LOG_ERR, "Out of memory to receive http file");
         goto out;
      }
   }

   /*
    * Loop reading the body into the buffer.
    */
   size_recd = 0;
   while (size_recd < size) {
      memset(&RespMessage, 0, sizeof(RespMessage));
      RespMessage.Body = &buf[size_recd];
      RespMessage.BodyLength = size - size_recd;

      HttpDone = false;
      Status = Http->Response(Http, &RespToken);
      if (EFI_ERROR(Status)) {
         Log(try == 0 ? LOG_DEBUG : LOG_WARNING,
             "Error in Http->Response (body): %s",
             error_str[error_efi_to_generic(Status)]);
         goto out;
      }
      while (!HttpDone) {
         Http->Poll(Http);
      }
      if (callback != NULL) {
         callback(RespMessage.BodyLength);
      }
      size_recd += RespMessage.BodyLength;
   }

 out:
   if (RespMessage.Headers != NULL) {
      sys_free(RespMessage.Headers);
   }
   if (!EFI_ERROR(Status) && HttpStatus != HTTP_STATUS_200_OK) {
      Status = EFI_HTTP_ERROR;
   }
   if (EFI_ERROR(Status)) {
      if (Http != NULL) {
         Http->Cancel(Http, NULL);
      }
      if (buf != NULL && buf != *Buffer) {
         sys_free(buf);
      }
   } else {
      if (Buffer != NULL) {
         *Buffer = buf;
      }
      *BufSize = (UINTN)size;
   }
   return Status;
}

/*-- http_file_load ------------------------------------------------------------
 *
 *      Load a file into memory or get its length, using HTTP.
 *
 * Parameters
 *      IN     NicHandle: handle to the "volume" from which to load the file.
 *      IN     filepath:  the ASCII absolute path of the file to retrieve;
 *                        must be in URL format.
 *      IN     callback:  routine to be called periodically while the file
 *                        is being loaded.
 *      IN/OUT Buffer:    buffer where the file is loaded:
 *                           if Buffer=NULL, just get the file's length;
 *                           else if *Buffer=NULL, allocate a buffer;
 *                           else use the given *Buffer (size in *BufSize).
 *      IN/OUT BufSize:   length of Buffer, size of file:
 *                           if Buffer=NULL, OUT only;
 *                           else if *Buffer=NULL, OUT only;
 *                           else IN/OUT.
 *
 * Results
 *      EFI_SUCCESS, or an EFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS http_file_load(EFI_HANDLE NicHandle, const char *filepath,
                          int (*callback)(size_t), VOID **Buffer,
                          UINTN *BufSize)
{
   EFI_STATUS Status;
   char *hostname = NULL;
   CHAR16 *Url = NULL;
   unsigned try;

   if (!is_http_boot()) {
      return EFI_UNSUPPORTED;
   }

   Status = get_url_hostname(filepath, &hostname);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "http_file_load: Could not get hostname from '%s': %s",
          filepath, error_str[error_efi_to_generic(Status)]);
      return Status;
   }
   ascii_to_ucs2(filepath, &Url);
   efi_set_watchdog_timer(WATCHDOG_DISABLE);

   for (try = 0; try <= MAX_RETRIES; try++) {
      Status = http_init(NicHandle);
      if (EFI_ERROR(Status)) {
         break;
      }
      Status = http_file_load_try(try, Url, hostname,
                                  callback, Buffer, BufSize);
      if (EFI_ERROR(Status) && Status != EFI_HTTP_ERROR) {
         /*
          * The HTTP 1.1 connection may need to be reopened.  The UEFI spec
          * says: "If the HTTP driver does not have an open underlying TCP
          * connection with the host specified in the response URL, Response()
          * will return EFI_ACCESS_DENIED. This is consistent with RFC 2616
          * recommendation that HTTP clients should attempt to maintain an open
          * TCP connection between client and host."  Although the spec says
          * Response, testing has shown EFI_ACCESS_DENIED can be returned on
          * Request.  Looking for the error here covers both possibilities.
          *
          * Another issue seen (with HPE Gen10 firmware) is that if an HTTPS
          * server closes the HTTP 1.1 connection, we get an undefined
          * EFI_STATUS value (0x8000000000000068) instead of EFI_ACCESS_DENIED.
          * So try reopening the connection for all errors other than
          * EFI_HTTP_ERROR.
          */
         http_cleanup();
         Log(try == 0 ? LOG_DEBUG : LOG_WARNING,
             "Retrying (%d retries left)", MAX_RETRIES - try);
         continue;
      }
      break;
   }

   efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);
   sys_free(Url);
   sys_free(hostname);
   return Status;
}

/*-- http_file_get_size --------------------------------------------------------
 *
 *      Get the size of a file using HTTP.
 *
 * Parameters
 *      IN  NicHandle: handle to the "volume" from which to load the file
 *      IN  filepath:  the ASCII absolute path of the file; must be in URL
 *                     format.
 *      OUT FileSize:  the 64-bit file size, in bytes.
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS http_file_get_size(EFI_HANDLE NicHandle, const char *filepath,
                              UINTN *FileSize)
{
   return http_file_load(NicHandle, filepath, NULL, NULL, FileSize);
}

/*-- http_efi_load_file --------------------------------------------------------
 *
 *      Implement the LoadFile protocol on top of UEFI HTTP.
 *
 * Parameters
 *      IN  This:        unused
 *      IN  FileDevpath: device path of the file to be loaded
 *      IN  BootPolicy:  unused
 *      IN/OUT BufSize:  length of Buffer, size of file:
 *                          if Buffer=NULL, OUT only;
 *                          else IN/OUT.
 *      IN/OUT Buffer:   buffer where the file is loaded:
 *                          if Buffer=NULL, just get the file's length;
 *                          else use the given Buffer (size in *BufSize).
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFIAPI EFI_STATUS
http_efi_load_file(UNUSED_PARAM(EFI_LOAD_FILE_PROTOCOL *This),
                   EFI_DEVICE_PATH_PROTOCOL *FileDevpath,
                   UNUSED_PARAM(BOOLEAN BootPolicy),
                   UINTN *BufSize,
                   VOID *Buffer)
{
   EFI_STATUS Status;
   CHAR16 *FilePath;
   char *filepath;
   EFI_HANDLE NicHandle;
   char *p;

   Status = get_http_boot_volume(&NicHandle);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   Status = devpath_get_filepath(FileDevpath, &FilePath);
   if (EFI_ERROR(Status)) {
      return Status;
   }
   filepath = (char *)FilePath;
   ucs2_to_ascii(FilePath, &filepath, FALSE);

   /*
    * The original ASCII URL has been damaged by going through
    * filepath_unix_to_efi -> make_file_devpath -> devpath_get_filepath ->
    * uc2_to_ascii.  As a result it has an extra backslash at the front, and
    * every slash (or sequence of slashes) has become a single backslash.
    * Repair the damage as best we can.  Example:
    *    \http:\boot.example.org\esx67\s.b00 ->
    *    http://boot.example.org/esx67/s.b00
    */
   p = strstr(filepath, ":\\");
   if (p != NULL) {
      /*
       * Copy the URL scheme, colon, and single trailing backslash one byte
       * backward, thus overwriting the unwanted leading backslash and leaving
       * two trailing backslashes.
       */
      memmove(filepath, filepath + 1, p - filepath + 1);
   }
   for (p = filepath; *p != '\0'; p++) {
      /* Change all backslashes to forward slashes. */
      if (*p == '\\') {
         *p = '/';
      }
   }

   Status = http_file_load(NicHandle, filepath, NULL,
                           Buffer == NULL ? NULL : &Buffer, BufSize);

   sys_free(FilePath);
   return Status;
}
