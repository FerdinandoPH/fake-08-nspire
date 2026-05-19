#include "nspire_path.h"

#include <fstream>

std::string nspire_resolve_path(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    {
        std::ifstream in(path, std::ios::in | std::ios::binary);
        if (in) {
            return path;
        }
    }

    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".tns") == 0) {
        return path;
    }

    return path + ".tns";
}
