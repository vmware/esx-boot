/*******************************************************************************
 * Copyright (c) 2019-2020 VMware, Inc.  All rights reserved.
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

/*
 * Cached information about current or most recent HTTP transaction.
 */
static EFI_HANDLE HttpVolume; // handle implying the NIC and IP version
static EFI_SERVICE_BINDING_PROTOCOL *HttpServiceBinding;
static EFI_HANDLE HttpHandle;
static EFI_HTTP_PROTOCOL *Http;
static EFI_EVENT HttpEvent;
static bool HttpDone;
static UINT16 LocalPort;

/*
 * Current criteria for using UEFI HTTP, initialized to the default.
 * In the future the default may be changed to
 * http_if_plain_http_allowed.  Remember to change the usage comment
 * in mboot.c and the usage help in mboot/config.c if changing the
 * default here.
 */
static http_criteria_t httpCriteria = http_if_http_booted;

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

static EFI_STATUS
http_file_load_try(const CHAR16 *Url, const char *hostname,
                   int (*callback)(size_t), VOID **Buffer, UINTN *BufSize);

/*-- get_http_nic_and_ipv ------------------------------------------------------
 *
 *      Get a NIC handle and IP version implied by the given handle's devpath.
 *
 * Parameters
 *      IN  volumeIn:  EFI handle
 *      OUT nicOut:    EFI handle
 *      OUT ipvOut:    4 or 6.
 *
 * Results
 *      EFI_SUCCESS, or an EFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS get_http_nic_and_ipv(EFI_HANDLE volumeIn,
                                EFI_HANDLE *nicOut, int *ipvOut)
{
   static EFI_HANDLE volume = NULL;
   static EFI_HANDLE nic = NULL;
   static int ipv = 0;
   static EFI_STATUS Status;
   EFI_DEVICE_PATH *DevPath, *node, *tmp;
   EFI_DEVICE_PATH *NewDevPath = NULL;

   /* Cache */
   if (volume != NULL && volume == volumeIn) {
      goto done;
   }
   volume = volumeIn;

   Status = devpath_get(volume, &DevPath);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error getting volume devpath: %s",
          error_str[error_efi_to_generic(Status)]);
      goto done;
   }
   log_devpath(LOG_DEBUG, "volume", DevPath);

   Status = devpath_duplicate(DevPath, &NewDevPath);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error duplicating volume devpath: %s",
          error_str[error_efi_to_generic(Status)]);
      goto done;
   }

   FOREACH_DEVPATH_NODE(NewDevPath, node) {
      if (node->Type == MESSAGING_DEVICE_PATH &&
          (node->SubType == MSG_IPv4_DP || node->SubType == MSG_IPv6_DP)) {
         ipv = (node->SubType == MSG_IPv4_DP) ? 4 : 6;
         SetDevPathEndNode(node);
         break;
      }
   }

   tmp = NewDevPath;
   Status = bs->LocateDevicePath(&HttpServiceBindingProto, &tmp, &nic);
   if (!EFI_ERROR(Status)) {
      log_devpath(LOG_DEBUG, "HTTP NIC in volume devpath", NewDevPath);
   } else {
      Log(LOG_DEBUG, "No HTTP NIC in volume devpath: %s",
          error_str[error_efi_to_generic(Status)]);
   }

   if (ipv != 0) {
      Log(LOG_DEBUG, "IP version in volume devpath: %u", ipv);
   } else {
      Log(LOG_DEBUG, "No IP version in volume devpath");
      Status = EFI_NOT_FOUND;
   }

 done:
   sys_free(NewDevPath);
   *nicOut = nic;
   *ipvOut = ipv;
   return Status;
}

/*-- hide_pxe ------------------------------------------------------------------
 *
 *      Apply a workaround in case the child was built with an older, buggy
 *      version of httpfile.c.  The older code is too sloppy when looking up a
 *      NIC handle based on the devpath of the Image->DeviceHandle and will
 *      find the PXE handle instead if it exists.  To prevent this, uninstall
 *      the PXE handle's device path so that it can't be looked up by devpath.
 *
 *      Essentially, this workaround is needed only when menu.efi is loaded via
 *      PXE *and* menu.efi then uses native UEFI HTTP to load an old build of
 *      mboot.efi that was linked with a buggy version of is_http_boot().
 *      Therefore it can be exposed only if menu.efi has httpCriteria >=
 *      http_if_plain_http_allowed.  The bug (PR 2451182) was introduced in
 *      efiboot/main @808773 and was fixed in efiboot/main @926760.  Buggy
 *      builds were used on bora/main for a time, and on bora/esx-7.0.0.0, but
 *      ESXi 7.0 GA was released from bora/esx-7.0.0.1, so no buggy build was
 *      ever shipped in a GA product.  Thus it's not clear we really need to
 *      keep this workaround, but it seems harmless to keep it.
 *
 * Results
 *      PXE handle's device path.
 *----------------------------------------------------------------------------*/
EFI_STATUS hide_pxe(void)
{
   EFI_STATUS Status;
   EFI_HANDLE BootVolume;
   EFI_DEVICE_PATH *DevPath;

   Status = get_boot_volume(&BootVolume);
   if (EFI_ERROR(Status)) {
      return Status;
   }
   Status = devpath_get(BootVolume, &DevPath);
   if (EFI_ERROR(Status)) {
      return Status;
   }
   return bs->UninstallProtocolInterface(BootVolume,
                                         &DevicePathProto, DevPath);
}

/*-- make_http_child_dh --------------------------------------------------------
 *
 *      Create something suitable to pass as ChildHandle->DeviceHandle when
 *      starting a child image that was chainloaded via HTTP.
 *
 * Parameter
 *      IN  Volume:    Handle implying the NIC and IP version used.
 *      IN  url:       URL the child was loaded from.
 *      OUT ChildDH:   device handle.
 *
 * Results
 *      EFI_SUCCESS, or an EFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS make_http_child_dh(EFI_HANDLE Volume, const char *url,
                              EFI_HANDLE *ChildDH)
{
   /*
    * This library relies on the device path of the Image->DeviceHandle that
    * its parent (initially the UEFI boot manager) passed into it, in order to
    * determine what URL it was loaded from and what IP version was used.  In
    * order to pass similar information to a child that may be using the same
    * library, this routine creates a handle with a similarly formatted device
    * path.  Just passing on the same handle the current app received would not
    * generally work, as the URL is different.  (In the future the IP
    * version could be different too, but that is not currently supported.)
    */
   EFI_STATUS Status;
   EFI_DEVICE_PATH *VolumePath;
   EFI_DEVICE_PATH *node;
   EFI_DEVICE_PATH *ChildPath;
   size_t s1, s2;
   char *p = NULL;

   Status = devpath_get(Volume, &VolumePath);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   FOREACH_DEVPATH_NODE(VolumePath, node) {
      if (IsDevPathEndType(node) ||
          (node->Type == MESSAGING_DEVICE_PATH &&
           node->SubType == MSG_URI_DP)) {
         break;
      }
   }
   s1 = (char *)node - (char *)VolumePath;
   s2 = sizeof(EFI_DEVICE_PATH) + strlen(url) + 1;

   p = sys_malloc(s1 + s2 + sizeof(EFI_DEVICE_PATH));
   memcpy(p, VolumePath, s1);
   node = (EFI_DEVICE_PATH *)(p + s1);
   node->Type = MESSAGING_DEVICE_PATH;
   node->SubType = MSG_URI_DP;
   SetDevPathNodeLength(node, s2);
   memcpy(p + s1 + sizeof(EFI_DEVICE_PATH), url, s2 - sizeof(EFI_DEVICE_PATH));
   node = (EFI_DEVICE_PATH *)(p + s1 + s2);
   SetDevPathEndNode(node);
   ChildPath = (EFI_DEVICE_PATH *)p;

   *ChildDH = NULL;
   Status = bs->InstallProtocolInterface(ChildDH, &DevicePathProto,
                                         EFI_NATIVE_INTERFACE, ChildPath);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Error creating child handle: %s",
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
      Log(LOG_ERR, "Error installing LoadFile protocol: %s",
          error_str[error_efi_to_generic(Status)]);
      goto out;
   }

   if (is_pxe_boot(NULL)) {
      hide_pxe();
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
 *      EFI_SUCCESS if successful,
 *      EFI_INVALID_PARAMETER if not a URL,
 *      or an EFI error status.
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
 *      IN  Volume: Handle implying the NIC and IP version to use.
 *
 * Results
 *      EFI_SUCCESS, or an EFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS http_init(EFI_HANDLE Volume)
{
   static EFI_STATUS Status; // static to allow caching errors
   EFI_HANDLE NicHandle;
   int ipv = 0;
   EFI_HTTP_CONFIG_DATA HttpConfigData;
   EFI_HTTPv4_ACCESS_POINT IPv4Node;
   EFI_HTTPv6_ACCESS_POINT IPv6Node;

   if (HttpVolume == Volume) {
      // Return cached info
      return Status;
   } else {
      // Clear cached info
      http_cleanup();
   }

   /*
    * Find which NIC and IP version to use.
    */
   Status = get_http_nic_and_ipv(Volume, &NicHandle, &ipv);
   if (EFI_ERROR(Status)) {
      /*
       * Fail silently in this case, and cache the error.  It can occur when
       * checking whether HTTP is available on a machine where it is not
       * available, or when booting from disk or ISO image.
       */
      HttpVolume = Volume;
      goto out;
   }

   /*
    * Find and initialize HTTP protocol.  Passing NicHandle to
    * get_protocol_interface causes the same NIC to be used that loaded
    * this EFI app.
    */
   Status = get_protocol_interface(NicHandle, &HttpServiceBindingProto,
                                   (void**)&HttpServiceBinding);
   if (EFI_ERROR(Status)) {
      /*
       * This shouldn't be possible, because get_http_nic_and_ipv has already
       * looked for HttpServiceBindingProto on the NicHandle.
       */
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
    * Pick a random-ish range of dynamic local ports.
    */
   if (LocalPort == 0) {
      UINT64 Count;
      bs->GetNextMonotonicCount(&Count);
      LocalPort = 49160 + (Count % 1627) * 10;
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

   if (HttpEvent == NULL) {
      bs->CreateEvent(EVT_NOTIFY_SIGNAL, TPL_CALLBACK,
                      http_callback, &HttpDone, &HttpEvent);
   }

out:
   if (EFI_ERROR(Status)) {
      http_cleanup();
   } else {
      HttpVolume = Volume;
   }
   return Status;
}

/*-- plain_http_allowed --------------------------------------------------------
 *
 *      Given that HTTP support is present, check whether plain http:// URLs
 *      are allowed.  UEFI implementations sometimes forbid plain http:// and
 *      allow only https://, governed by either a compile-time or runtime
 *      option that is not directly visible to apps.
 *
 *      Assumes http_init has been called.
 *
 * Results
 *      True/false
 *----------------------------------------------------------------------------*/
bool plain_http_allowed(EFI_HANDLE Volume)
{
   EFI_STATUS Status;
   UINTN BufSize;
   static EFI_HANDLE VolCached = NULL;
   static bool allowed;

   if (VolCached == NULL) {
      /*
       * Ask for the size of something from 0.0.0.0 and check which
       * error status we get back.  Using 0.0.0.0 here avoids sending
       * anything out on the wire regardless of whether plain http is
       * or isn't supported.
       */
      Log(LOG_DEBUG, "Probing for plain http:// URL support...");
      Status = http_file_load_try(L"http://0.0.0.0/probe", "0.0.0.0",
                                  NULL, NULL, &BufSize);
      VolCached = Volume;
      allowed = Status != EFI_ACCESS_DENIED;
      Log(LOG_DEBUG, "UEFI firmware on this system %sallows plain http:// URLs",
          allowed ? "" : "dis");
   }
   return allowed;
}

/*-- set_http_criteria ---------------------------------------------------------
 *
 *      Adjust the criteria for when UEFI HTTP may be used.
 *
 *      http_never = never use native UEFI HTTP.
 *
 *      http_if_http_booted = attempt native UEFI HTTP if the current image
 *      was loaded via native UEFI HTTP.
 *
 *      http_if_plain_http_allowed = attempt native UEFI HTTP if the system
 *      supports it and allows plain http URLs.
 *
 *      http_always = attempt native UEFI HTTP if the system supports it, even
 *      if it allows only https URLs.
 *----------------------------------------------------------------------------*/
void set_http_criteria(http_criteria_t criteria)
{
   Log(LOG_DEBUG, "set_http_criteria: %u -> %u", httpCriteria, criteria);
   httpCriteria = criteria;
}

/*-- has_http ------------------------------------------------------------------
 *
 *      Check whether the NIC and IP version implied by the given volume is
 *      capable and usable for native UEFI HTTP.
 *
 * Results
 *      True/false
 *----------------------------------------------------------------------------*/
bool has_http(EFI_HANDLE Volume)
{
   switch (httpCriteria) {
   case http_never:
      return false;

   case http_if_http_booted:
      return is_http_boot() && http_init(Volume) == EFI_SUCCESS;

   case http_if_plain_http_allowed:
      return http_init(Volume) == EFI_SUCCESS && plain_http_allowed(Volume);

   case http_always:
      return http_init(Volume) == EFI_SUCCESS;
   }
   return false;
}

/*-- http_cleanup --------------------------------------------------------------
 *
 *      Clean up cached Http instance.
 *----------------------------------------------------------------------------*/
void http_cleanup(void)
{
   if (Http != NULL) {
      Http->Configure(Http, NULL);
      Http = NULL;
   }

   if (HttpHandle != NULL) {
      HttpServiceBinding->DestroyChild(HttpServiceBinding, HttpHandle);
      HttpHandle = NULL;
   }

   HttpServiceBinding = NULL;
   HttpVolume = NULL;
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
 *      EFI_SUCCESS, or an EFI error status.  EFI_ACCESS_DENIED or
 *      EFI_CONNECTION_FIN indicates that a retry is needed because the
 *      connection was closed.
 *----------------------------------------------------------------------------*/
static EFI_STATUS http_file_load_try(const CHAR16 *Url,
                                     const char *hostname,
                                     int (*callback)(size_t),
                                     VOID **Buffer, UINTN *BufSize)
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
    * Send HTTP HEAD or GET request.
    */
   ReqToken.Event = HttpEvent;
   ReqToken.Status = EFI_NOT_READY;
   ReqToken.Message = &ReqMessage;
   ReqMessage.Data.Request = &ReqData;
   ReqMessage.HeaderCount = NUM_HEADERS;
   ReqMessage.Headers = ReqHeaders;
   ReqData.Method = (Buffer == NULL) ? HttpMethodHead : HttpMethodGet;
   ReqData.Url = (CHAR16 *)Url;
   ReqHeaders[0].FieldName = (CHAR8 *)"User-Agent";
   ReqHeaders[0].FieldValue = (CHAR8 *)"esx-boot/2.0";
   ReqHeaders[1].FieldName = (CHAR8 *)"Host";
   ReqHeaders[1].FieldValue = (CHAR8 *)hostname;

   HttpDone = false;
   Status = Http->Request(Http, &ReqToken);
   if (EFI_ERROR(Status)) {
      Log(LOG_DEBUG, "Error in Http->Request: %s",
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
      Log(LOG_DEBUG, "Error in Http->Response (header): %s",
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
         Log(LOG_DEBUG, "Error in Http->Response (body): %s",
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
 *      IN     Volume:    handle to the "volume" from which to load the file.
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
EFI_STATUS http_file_load(EFI_HANDLE Volume, const char *filepath,
                          int (*callback)(size_t), VOID **Buffer,
                          UINTN *BufSize)
{
   EFI_STATUS Status;
   char *hostname = NULL;
   CHAR16 *Url = NULL;
   unsigned try;
   EFI_HANDLE NicHandle;
   int ipv;

   Status = get_url_hostname(filepath, &hostname);
   if (EFI_ERROR(Status)) {
      /*
       * Don't log in this case.  It is normal when firmware_file_* is looping
       * though methods and tries http_file_load on a non-URL.
       */
      goto out;
   }

   if (!has_http(Volume) ||
       (strncasecmp(filepath, "http:", 5) == 0 &&
        !plain_http_allowed(Volume))) {
      /*
       * Don't log in this case.  It is normal when firmware_file_* is looping
       * though methods and tries http_file_load on a machine where HTTP is not
       * available in firmware or the httpCriteria setting doesn't allow it.
       */
      Status = EFI_UNSUPPORTED;
      goto out;
   }

   ascii_to_ucs2(filepath, &Url);
   efi_set_watchdog_timer(WATCHDOG_DISABLE);

   Status = get_http_nic_and_ipv(Volume, &NicHandle, &ipv);
   if (EFI_ERROR(Status)) {
      goto out;
   }
   if (ipv == 4) {
      /*
       * Ensure we have an IP address.  This seems to be needed;
       * otherwise Http->Request typically fails with EFI_NO_MAPPING
       * (which is not documented as a possibility for it, by the
       * way).  From study of a packet trace, it appears that the
       * Http->Request does automatically cause DHCP to be started,
       * but it forgets to wait for the DHCP transaction to finish
       * before trying to connect to the HTTP server.
       */
      Status = get_ipv4_addr(NicHandle);
      if (EFI_ERROR(Status)) {
         Log(LOG_ERR, "Error getting IPv4 address: %s",
             error_str[error_efi_to_generic(Status)]);
         goto out;
      }
   }

   for (try = 0; try <= MAX_RETRIES; try++) {
      Status = http_init(Volume);
      if (EFI_ERROR(Status)) {
         break;
      }
      Status = http_file_load_try(Url, hostname, callback, Buffer, BufSize);
      if (EFI_ERROR(Status) && Status != EFI_HTTP_ERROR) {
         /*
          * The HTTP 1.1 connection may need to be reopened.  The UEFI spec
          * says: "If the HTTP driver does not have an open underlying TCP
          * connection with the host specified in the response URL, Response()
          * will return EFI_ACCESS_DENIED. This is consistent with RFC 2616
          * recommendation that HTTP clients should attempt to maintain an open
          * TCP connection between client and host."  Although the spec says
          * Response, testing has shown this can occur on Request as well.
          * Looking for the error here covers both possibilities.
          *
          * Testing has also shown (at least with HPE Gen10 firmware) that if
          * an HTTPS server closes the HTTP 1.1 connection, EFI_CONNECTION_FIN
          * is returned instead of EFI_ACCESS_DENIED.
          *
          * Further, some UEFI implementations disallow plain http:// URLs,
          * allowing only https://.  This behavior depends on a compile-time or
          * runtime option that is not directly readable by UEFI apps.  This
          * case also returns EFI_ACCESS_DENIED on Request, so that return
          * status is ambiguous.
          *
          * To be conservative, we try reopening the connection here for all
          * errors other than EFI_HTTP_ERROR.  If the final retry fails with
          * EFI_ACCESS_DENIED (when filepath is a plain http:// URL), we can
          * assume that the UEFI implementation disallows plain http.
          */
         http_cleanup();
         Log(LOG_DEBUG, "%d retries left", MAX_RETRIES - try);
         continue;
      }
      break;
   }

 out:
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
 *      IN  Volume:    handle to the volume from which to load the file
 *      IN  filepath:  the ASCII absolute path of the file; must be in URL
 *                     format.
 *      OUT FileSize:  the 64-bit file size, in bytes.
 *
 * Results
 *      EFI_SUCCESS, or an EFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS http_file_get_size(EFI_HANDLE Volume, const char *filepath,
                              UINTN *FileSize)
{
   return http_file_load(Volume, filepath, NULL, NULL, FileSize);
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
 *      EFI_SUCCESS, or an EFI error status.
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
   EFI_HANDLE Volume;
   char *p;

   Status = get_boot_volume(&Volume);
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

   Status = http_file_load(Volume, filepath, NULL,
                           Buffer == NULL ? NULL : &Buffer, BufSize);

   sys_free(FilePath);
   return Status;
}
