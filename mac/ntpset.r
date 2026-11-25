#include "Dialogs.r"
#include "Processes.r"

/* Settings (edit with ResEdit): server IP, zone in minutes east of UTC or "auto", DST rule
 * ("EU" or "none"), timeout in seconds. */
resource 'STR#' (128, "NTPSet settings") {
    {
        "time.metas.ch";
        "60";
        "EU";
        "10"
    }
};

/* result alert, only shown when Option is held at launch */
resource 'ALRT' (128) {
    { 60, 60, 190, 460 }, 128,
    { OK, visible, silent; OK, visible, silent; OK, visible, silent; OK, visible, silent },
    alertPositionMainScreen
};

resource 'DITL' (128) {
    {
        { 98, 318, 118, 388 }, Button { enabled, "OK" };
        { 10, 70, 90, 390 },   StaticText { disabled, "^0" }
    }
};

resource 'SIZE' (-1) {
    reserved, ignoreSuspendResumeEvents, reserved, cannotBackground, needsActivateOnFGSwitch,
    backgroundAndForeground, dontGetFrontClicks, ignoreChildDiedEvents, is32BitCompatible,
    notHighLevelEventAware, onlyLocalHLEvents, notStationeryAware, dontUseTextEditServices,
    reserved, reserved, reserved,
    256 * 1024,
    192 * 1024
};
