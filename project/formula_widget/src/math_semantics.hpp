#pragma once

#include "math_ast.hpp"

#include <string>

namespace formula {

    struct SemanticResult {
        bool ok = false;
        std::string sympy;
        std::string mainVar;
        std::string error;
    };

    SemanticResult toSympy(const MathNode& root);

}
