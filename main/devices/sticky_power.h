#pragma once

// Logs whether this boot followed a wake from the center power button.
void sticky_power_log_wakeup_reason();

// Powers down unused rails and enters deep sleep. The center power button is
// the wake source and the e-paper image remains visible.
[[noreturn]] void sticky_power_enter_deep_sleep();
