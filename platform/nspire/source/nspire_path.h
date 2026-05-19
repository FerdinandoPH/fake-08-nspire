#pragma once

#include <string>

// On the TI-Nspire (Ndless), every user file on disk has a `.tns` suffix.
// Paths constructed by the common code (e.g. "<cartdir>/foo.p8") therefore
// fail to open as-is. This helper tries the path verbatim first and falls
// back to appending `.tns` if it isn't already present.
std::string nspire_resolve_path(const std::string& path);
