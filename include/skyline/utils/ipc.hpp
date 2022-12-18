#pragma once

#include "types.h"

namespace skyline {
    namespace utils {
        class Ipc {
        public:

            static Result getOwnProcessHandle(Handle*);
        };
    };
};