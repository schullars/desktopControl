# PrintWindow error 87 fallback

Windows window capture first uses PW_RENDERFULLCONTENT. Only a failed call with
ERROR_INVALID_PARAMETER (87) causes one retry using flags=0, after clearing the
bitmap. LastError is cleared before each call. Other failures are not retried;
both error codes are retained if the retry fails. No image is encoded on failure.
The command interface, capture bounds, image formats and privilege model remain
unchanged. There is no screen-crop substitute or application-specific handling.

Six injected tests cover first-call success, fallback success, other errors,
unset error, retry failure and bitmap-clear failure.

2026-11-08 target verification reported by the RustDesk integration session:
Windows Server 2008 R2 SP1 (6.1.7601), signed candidate SHA256
4C7C12134882DFD6FC545ADDFBDD07C7B81713EF1FF5A7E054B914B1A5D0F0F4.
UIAccess capture of an elevated MMC window succeeded for BMP and JPEG. Both
1032x732 images were manually checked for complete title bar, menus and content;
no black/blank output, clipping or occluding-window contamination was observed.
Session start, window listing, activation and session stop also succeeded.
The tested candidate additionally contains previously approved DPI/result-file
changes; this commit deliberately contains only the PrintWindow fix and tests.
