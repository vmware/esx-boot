#ifndef _WATCHDOG_H
#define _WATCHDOG_H

#define RUNTIME_WATCHDOG_PROTOCOL_GUID \
  { \
    0xfb7ef6e8, 0x822b, 0x47b7, \
    {0x94, 0x70, 0x2f, 0xcc, 0x73, 0x58, 0xb2, 0xcf } \
  }

typedef struct _RUNTIME_WATCHDOG_PROTOCOL RUNTIME_WATCHDOG_PROTOCOL;

typedef enum {
  //
  // SBSA GWDT, detected using ACPI GTDT.
  //
  ARM_GENERIC_WATCHDOG,
  //
  // Vendor-specific WDT, detected using vendor-specific ACPI table (not DSDT).
  //
  VENDOR_WATCHDOG,
} RUNTIME_WATCHDOG_TYPE;

/**
  Enables/disables/restarts the watchdog timer countdown.
  If the countdown completes prior to another SetWatchdog call,
  the system will reset.


  The armed timer survives ExitBootServices.

  @param[in] This           The protocol instance.
  @param[in] TimeoutSeconds If non-zero and watchdog is disabled,
                               enables watchdog with the timeout.
                            If non-zero and watchdog is enabled,
                               restarts watchdog with new timeout.
                            If zero and watchdog disabled, does nothing.
                            If zero and watchdog enabled, disables watchdog.

  @retval EFI_SUCCESS        Operation completed successfully
  @retval EFI_DEVICE_ERROR   The command was unsuccessful
  @retval EFI_NOT_SUPPORTED  Disabling or re-arming with a different
                             timeout is unsupported.

**/
typedef
EFI_STATUS
(EFIAPI *RUNTIME_WATCHDOG_SET) (
  IN  struct _RUNTIME_WATCHDOG_PROTOCOL *This,
  IN  UINTN TimeoutSeconds
  );

struct _RUNTIME_WATCHDOG_PROTOCOL {
  RUNTIME_WATCHDOG_TYPE Type;
  //
  // Used to help find the matching WDT where multiple WDTs are present.
  // For Type == ARM_GENERIC_WATCHDOG, this matches the GTDT
  // WatchdogControlFrame physical address.
  //
  EFI_PHYSICAL_ADDRESS  Base;
  //
  // The Min/Max TimeoutSeconds provide guidance of the
  // supported range to be passed to SetWatchdog.
  //
  UINTN MinTimeoutSeconds;  // Must be >= 1
  UINTN MaxTimeoutSeconds;  // Must be >= MinTimeoutSeconds
  RUNTIME_WATCHDOG_SET  SetWatchdog;
};

extern EFI_GUID gRuntimeWatchdogProtocolGuid;

#endif /* !_WATCHDOG_H */
